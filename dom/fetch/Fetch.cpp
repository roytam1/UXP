/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"
#include "FetchConsumer.h"

#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsProxyRelease.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/MutableBlobStreamListener.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"

#include "FetchObserver.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

// This class helps the proxying of AbortSignal changes cross threads.
class AbortSignalProxy final : public AbortSignal::Follower
{
  // This is created and released on the main-thread.
  RefPtr<AbortSignal> mSignalMainThread;

  // This value is used only for the creation of AbortSignal on the
  // main-thread. They are not updated.
  const bool mAborted;

  // This runnable propagates changes from the AbortSignal on workers to the
  // AbortSignal on main-thread.
  class AbortSignalProxyRunnable final : public Runnable
  {
    RefPtr<AbortSignalProxy> mProxy;

  public:
    explicit AbortSignalProxyRunnable(AbortSignalProxy* aProxy)
      : mProxy(aProxy)
    {}

    NS_IMETHOD
    Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      AbortSignal* signal = mProxy->GetOrCreateSignalForMainThread();
      signal->Abort();
      return NS_OK;
    }
  };

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbortSignalProxy)

  explicit AbortSignalProxy(AbortSignal* aSignal)
    : mAborted(aSignal->Aborted())
  {
    Follow(aSignal);
  }

  void
  Aborted() override
  {
    RefPtr<AbortSignalProxyRunnable> runnable =
      new AbortSignalProxyRunnable(this);
    NS_DispatchToMainThread(runnable);
  }

  AbortSignal*
  GetOrCreateSignalForMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mSignalMainThread) {
      mSignalMainThread = new AbortSignal(mAborted);
    }
    return mSignalMainThread;
  }

  AbortSignal*
  GetSignalForTargetThread()
  {
    return mFollowingSignal;
  }

  void
  Shutdown()
  {
    Unfollow();
  }

private:
  ~AbortSignalProxy()
  {
    NS_ReleaseOnMainThread(mSignalMainThread.forget());
  }
};

class WorkerFetchResolver final : public FetchDriverObserver
{
  friend class MainThreadFetchRunnable;
  friend class WorkerDataAvailableRunnable;
  friend class WorkerFetchResponseEndBase;
  friend class WorkerFetchResponseEndRunnable;
  friend class WorkerFetchResponseRunnable;

  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  RefPtr<AbortSignalProxy> mSignalProxy;
  RefPtr<FetchObserver> mFetchObserver;

public:
  // Returns null if worker is shutting down.
  static already_AddRefed<WorkerFetchResolver>
  Create(workers::WorkerPrivate* aWorkerPrivate, Promise* aPromise,
         AbortSignal* aSignal, FetchObserver* aObserver)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(aWorkerPrivate, aPromise);
    if (!proxy) {
      return nullptr;
    }

    RefPtr<AbortSignalProxy> signalProxy;
    if (aSignal) {
      signalProxy = new AbortSignalProxy(aSignal);
    }

    RefPtr<WorkerFetchResolver> r =
      new WorkerFetchResolver(proxy, signalProxy, aObserver);
    return r.forget();
  }

  AbortSignal*
  GetAbortSignalForMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mSignalProxy) {
      return nullptr;
    }

    return mSignalProxy->GetOrCreateSignalForMainThread();
  }

  AbortSignal*
  GetAbortSignalForTargetThread()
  {
    mPromiseProxy->GetWorkerPrivate()->AssertIsOnWorkerThread();

    if (!mSignalProxy) {
      return nullptr;
    }

    return mSignalProxy->GetSignalForTargetThread();
  }

  void
  OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void
  OnResponseEnd(FetchDriverObserver::EndReason eReason) override;

  void
  OnDataAvailable() override;

private:
   WorkerFetchResolver(PromiseWorkerProxy* aProxy,
                       AbortSignalProxy* aSignalProxy,
                       FetchObserver* aObserver)
    : mPromiseProxy(aProxy)
    , mSignalProxy(aSignalProxy)
    , mFetchObserver(aObserver)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mPromiseProxy);
  }

  ~WorkerFetchResolver()
  {}

  virtual void
  FlushConsoleReport() override;
};

class MainThreadFetchResolver final : public FetchDriverObserver
{
  RefPtr<Promise> mPromise;
  RefPtr<Response> mResponse;
  RefPtr<FetchObserver> mFetchObserver;
  RefPtr<AbortSignal> mSignal;

  nsCOMPtr<nsIDocument> mDocument;

  NS_DECL_OWNINGTHREAD
public:
  MainThreadFetchResolver(Promise* aPromise, FetchObserver* aObserver, AbortSignal* aSignal)
    : mPromise(aPromise)
    , mFetchObserver(aObserver)
    , mSignal(aSignal)
  {}

  void
  OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void SetDocument(nsIDocument* aDocument)
  {
    mDocument = aDocument;
  }

  void OnResponseEnd(FetchDriverObserver::EndReason aReason) override
  {
    if (aReason == eAborted) {
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    mFetchObserver = nullptr;

    FlushConsoleReport();
  }

  void
  OnDataAvailable() override;

private:
  ~MainThreadFetchResolver();

  void FlushConsoleReport() override
  {
    mReporter->FlushConsoleReports(mDocument);
  }
};

class MainThreadFetchRunnable : public Runnable
{
  RefPtr<WorkerFetchResolver> mResolver;
  RefPtr<InternalRequest> mRequest;

public:
  MainThreadFetchRunnable(WorkerFetchResolver* aResolver,
                          InternalRequest* aRequest)
    : mResolver(aResolver)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mResolver);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    RefPtr<FetchDriver> fetch;
    RefPtr<PromiseWorkerProxy> proxy = mResolver->mPromiseProxy;

    {
      // Acquire the proxy mutex while getting data from the WorkerPrivate...
      MutexAutoLock lock(proxy->Lock());
      if (proxy->CleanedUp()) {
        NS_WARNING("Aborting Fetch because worker already shut down");
        return NS_OK;
      }

      nsCOMPtr<nsIPrincipal> principal = proxy->GetWorkerPrivate()->GetPrincipal();
      MOZ_ASSERT(principal);
      nsCOMPtr<nsILoadGroup> loadGroup = proxy->GetWorkerPrivate()->GetLoadGroup();
      MOZ_ASSERT(loadGroup);
      fetch = new FetchDriver(mRequest, principal, loadGroup);
      nsAutoCString spec;
      if (proxy->GetWorkerPrivate()->GetBaseURI()) {
        proxy->GetWorkerPrivate()->GetBaseURI()->GetAsciiSpec(spec);
      }
      fetch->SetWorkerScript(spec);
    }

    RefPtr<AbortSignal> signal = mResolver->GetAbortSignalForMainThread();

    // ...but release it before calling Fetch, because mResolver's callback can
    // be called synchronously and they want the mutex, too.
    return fetch->Fetch(signal, mResolver);
  }
};

already_AddRefed<Promise>
FetchRequest(nsIGlobalObject* aGlobal, const RequestOrUSVString& aInput,
             const RequestInit& aInit, ErrorResult& aRv)
{
  RefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Double check that we have chrome privileges if the Request's content
  // policy type has been overridden.  Note, we must do this before
  // entering the global below.  Otherwise the IsCallerChrome() will
  // always fail.
  MOZ_ASSERT_IF(aInput.IsRequest() &&
                aInput.GetAsRequest().IsContentPolicyTypeOverridden(),
                nsContentUtils::IsCallerChrome());

  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> jsGlobal(cx, aGlobal->GetGlobalJSObject());
  GlobalObject global(cx, jsGlobal);

  RefPtr<Request> request = Request::Constructor(global, aInput, aInit, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<InternalRequest> r = request->GetInternalRequest();

  RefPtr<AbortSignal> signal = request->GetSignal();
  
  if (signal && signal->Aborted()) {
    // An already aborted signal should reject immediately.
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  RefPtr<FetchObserver> observer;
  if (aInit.mObserve.WasPassed()) {
    observer = new FetchObserver(aGlobal, signal);
    aInit.mObserve.Value().HandleEvent(*observer);
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsIPrincipal* principal;
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
      principal = doc->NodePrincipal();
      loadGroup = doc->GetDocumentLoadGroup();
    } else {
      principal = aGlobal->PrincipalOrNull();
      if (NS_WARN_IF(!principal)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
      nsresult rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), principal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aRv.Throw(rv);
        return nullptr;
      }
    }

    RefPtr<MainThreadFetchResolver> resolver =
      new MainThreadFetchResolver(p, observer, signal);
    RefPtr<FetchDriver> fetch = new FetchDriver(r, principal, loadGroup);
    fetch->SetDocument(doc);
    resolver->SetDocument(doc);
    aRv = fetch->Fetch(signal, resolver);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);

    if (worker->IsServiceWorker()) {
      r->SetSkipServiceWorker();
    }

    RefPtr<WorkerFetchResolver> resolver =
      WorkerFetchResolver::Create(worker, p, signal, observer);
    if (!resolver) {
      NS_WARNING("Could not add WorkerFetchResolver workerHolder to worker");
      aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
      return nullptr;
    }

    RefPtr<MainThreadFetchRunnable> run = new MainThreadFetchRunnable(resolver, r);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(run));
  }

  return p.forget();
}

void
MainThreadFetchResolver::OnResponseAvailableInternal(InternalResponse* aResponse)
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (aResponse->Type() != ResponseType::Error) {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Complete);
    }

    nsCOMPtr<nsIGlobalObject> go = mPromise->GetParentObject();
    mResponse = new Response(go, aResponse, mSignal);
    mPromise->MaybeResolve(mResponse);
  } else {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Errored);
    }

    ErrorResult result;
    result.ThrowTypeError<MSG_FETCH_FAILED>();
    mPromise->MaybeReject(result);
  }
}

void
MainThreadFetchResolver::OnDataAvailable()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (!mFetchObserver) {
    return;
  }

  if (mFetchObserver->State() == FetchState::Requesting) {
    mFetchObserver->SetState(FetchState::Responding);
  }
}

MainThreadFetchResolver::~MainThreadFetchResolver()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
}

class WorkerFetchResponseRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<WorkerFetchResolver> mResolver;
  // Passed from main thread to worker thread after being initialized.
  RefPtr<InternalResponse> mInternalResponse;
public:
  WorkerFetchResponseRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver,
                              InternalResponse* aResponse)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , mResolver(aResolver)
    , mInternalResponse(aResponse)
  {
     MOZ_ASSERT(mResolver);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mResolver->mPromiseProxy->WorkerPromise();

    if (mInternalResponse->Type() != ResponseType::Error) {
      if (mResolver->mFetchObserver) {
        mResolver->mFetchObserver->SetState(FetchState::Complete);
      }

      RefPtr<nsIGlobalObject> global = aWorkerPrivate->GlobalScope();
      RefPtr<Response> response = new Response(global, mInternalResponse, mResolver->GetAbortSignalForTargetThread());
      promise->MaybeResolve(response);
    } else {
      if (mResolver->mFetchObserver) {
        mResolver->mFetchObserver->SetState(FetchState::Errored);
      }

      ErrorResult result;
      result.ThrowTypeError<MSG_FETCH_FAILED>();
      promise->MaybeReject(result);
    }
    return true;
  }
};

class WorkerDataAvailableRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<WorkerFetchResolver> mResolver;
public:
  WorkerDataAvailableRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , mResolver(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    if (mResolver->mFetchObserver &&
        mResolver->mFetchObserver->State() == FetchState::Requesting) {
      mResolver->mFetchObserver->SetState(FetchState::Responding);
    }

    return true;
  }
};

class WorkerFetchResponseEndBase
{
protected:
  RefPtr<WorkerFetchResolver> mResolver;

public:
  explicit WorkerFetchResponseEndBase(WorkerFetchResolver* aResolver)
    : mResolver(aResolver)
  {
    MOZ_ASSERT(aResolver);
  }

  void
  WorkerRunInternal(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mResolver->mPromiseProxy->CleanUp();

    mResolver->mFetchObserver = nullptr;

    if (mResolver->mSignalProxy) {
      mResolver->mSignalProxy->Shutdown();
      mResolver->mSignalProxy = nullptr;
    }
  }
};

class WorkerFetchResponseEndRunnable final : public MainThreadWorkerRunnable
                                           , public WorkerFetchResponseEndBase
{
  FetchDriverObserver::EndReason mReason;

public:
  WorkerFetchResponseEndRunnable(WorkerPrivate* aWorkerPrivate,
                                 WorkerFetchResolver* aResolver,
                                 FetchDriverObserver::EndReason aReason)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , WorkerFetchResponseEndBase(aResolver)
    , mReason(aReason)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (mReason == FetchDriverObserver::eAborted) {
      RefPtr<Promise> promise = mResolver->mPromiseProxy->WorkerPromise();
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  nsresult
  Cancel() override
  {
    // Execute Run anyway to make sure we cleanup our promise proxy to avoid
    // leaking the worker thread
    Run();
    return WorkerRunnable::Cancel();
  }
};

class WorkerFetchResponseEndControlRunnable final : public MainThreadWorkerControlRunnable
                                                  , public WorkerFetchResponseEndBase
{
public:
  WorkerFetchResponseEndControlRunnable(WorkerPrivate* aWorkerPrivate,
                                        WorkerFetchResolver* aResolver)
    : MainThreadWorkerControlRunnable(aWorkerPrivate)
    , WorkerFetchResponseEndBase(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  // Control runnable cancel already calls Run().
};

void
WorkerFetchResolver::OnResponseAvailableInternal(InternalResponse* aResponse)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerFetchResponseRunnable> r =
    new WorkerFetchResponseRunnable(mPromiseProxy->GetWorkerPrivate(), this,
                                    aResponse);

  if (!r->Dispatch()) {
    NS_WARNING("Could not dispatch fetch response");
  }
}

void
WorkerFetchResolver::OnDataAvailable()
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerDataAvailableRunnable> r =
    new WorkerDataAvailableRunnable(mPromiseProxy->GetWorkerPrivate(), this);
  Unused << r->Dispatch();
}

void
WorkerFetchResolver::OnResponseEnd(FetchDriverObserver::EndReason aReason)
{
  AssertIsOnMainThread();
  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  FlushConsoleReport();

  RefPtr<WorkerFetchResponseEndRunnable> r =
    new WorkerFetchResponseEndRunnable(mPromiseProxy->GetWorkerPrivate(),
                                       this, aReason);

  if (!r->Dispatch()) {
    RefPtr<WorkerFetchResponseEndControlRunnable> cr =
      new WorkerFetchResponseEndControlRunnable(mPromiseProxy->GetWorkerPrivate(),
                                                this);
    // This can fail if the worker thread is canceled or killed causing
    // the PromiseWorkerProxy to give up its WorkerHolder immediately,
    // allowing the worker thread to become Dead.
    if (!cr->Dispatch()) {
      NS_WARNING("Failed to dispatch WorkerFetchResponseEndControlRunnable");
    }
  }
}

void
WorkerFetchResolver::FlushConsoleReport()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mPromiseProxy);

  if(!mReporter) {
    return;
  }

  workers::WorkerPrivate* worker = mPromiseProxy->GetWorkerPrivate();
  if (!worker) {
    mReporter->FlushConsoleReports((nsIDocument*)nullptr);
    return;
  }

  if (worker->IsServiceWorker()) {
    // Flush to service worker
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mReporter->FlushConsoleReports((nsIDocument*)nullptr);
      return;
    }

    swm->FlushReportsToAllClients(worker->WorkerName(), mReporter);
    return;
  }

  if (worker->IsSharedWorker()) {
    // Flush to shared worker
    worker->FlushReportsToSharedWorkers(mReporter);
    return;
  }

  // Flush to dedicated worker
  mReporter->FlushConsoleReports(worker->GetDocument());
}

namespace {

nsresult
ExtractFromArrayBuffer(const ArrayBuffer& aBuffer,
                       nsIInputStream** aStream,
                       uint64_t& aContentLength)
{
  aBuffer.ComputeLengthAndData();
  aContentLength = aBuffer.Length();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromArrayBufferView(const ArrayBufferView& aBuffer,
                           nsIInputStream** aStream,
                           uint64_t& aContentLength)
{
  aBuffer.ComputeLengthAndData();
  aContentLength = aBuffer.Length();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromBlob(const Blob& aBlob,
                nsIInputStream** aStream,
                nsCString& aContentType,
                uint64_t& aContentLength)
{
  RefPtr<BlobImpl> impl = aBlob.Impl();
  ErrorResult rv;
  aContentLength = impl->GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  impl->GetInternalStream(aStream, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString type;
  impl->GetType(type);
  aContentType = NS_ConvertUTF16toUTF8(type);
  return NS_OK;
}

nsresult
ExtractFromFormData(FormData& aFormData,
                    nsIInputStream** aStream,
                    nsCString& aContentType,
                    uint64_t& aContentLength)
{
  nsAutoCString unusedCharset;
  return aFormData.GetSendInfo(aStream, &aContentLength,
                               aContentType, unusedCharset);
}

nsresult
ExtractFromUSVString(const nsString& aStr,
                     nsIInputStream** aStream,
                     nsCString& aContentType,
                     uint64_t& aContentLength)
{
  nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
  if (!encoder) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t destBufferLen;
  nsresult rv = encoder->GetMaxLength(aStr.get(), aStr.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString encoded;
  if (!encoded.SetCapacity(destBufferLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* destBuffer = encoded.BeginWriting();
  int32_t srcLen = (int32_t) aStr.Length();
  int32_t outLen = destBufferLen;
  rv = encoder->Convert(aStr.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  encoded.SetLength(outLen);

  aContentType = NS_LITERAL_CSTRING("text/plain;charset=UTF-8");
  aContentLength = outLen;

  return NS_NewCStringInputStream(aStream, encoded);
}

nsresult
ExtractFromURLSearchParams(const URLSearchParams& aParams,
                           nsIInputStream** aStream,
                           nsCString& aContentType,
                           uint64_t& aContentLength)
{
  nsAutoString serialized;
  aParams.Stringify(serialized);
  aContentType = NS_LITERAL_CSTRING("application/x-www-form-urlencoded;charset=UTF-8");
  aContentLength = serialized.Length();
  return NS_NewCStringInputStream(aStream, NS_ConvertUTF16toUTF8(serialized));
}
} // namespace

nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType,
                          uint64_t& aContentLength)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream, aContentLength);
  }
  if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream, aContentLength);
  }
  if (aBodyInit.IsBlob()) {
    const Blob& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsFormData()) {
    FormData& form = aBodyInit.GetAsFormData();
    return ExtractFromFormData(form, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsUSVString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsUSVString());
    return ExtractFromUSVString(str, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType, aContentLength);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult
ExtractByteStreamFromBody(const ArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType,
                          uint64_t& aContentLength)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!*aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream, aContentLength);
  }
  if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream, aContentLength);
  }
  if (aBodyInit.IsBlob()) {
    const Blob& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsFormData()) {
    FormData& form = aBodyInit.GetAsFormData();
    return ExtractFromFormData(form, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsUSVString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsUSVString());
    return ExtractFromUSVString(str, aStream, aContentType, aContentLength);
  }
  if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType, aContentLength);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

template <class Derived>
FetchBody<Derived>::FetchBody()
  : mWorkerPrivate(nullptr)
  , mBodyUsed(false)
{
  if (!NS_IsMainThread()) {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
  }
}

template
FetchBody<Request>::FetchBody();

template
FetchBody<Response>::FetchBody();

template <class Derived>
FetchBody<Derived>::~FetchBody()
{
}

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv)
{
  RefPtr<AbortSignal> signal = DerivedClass()->GetSignal();
  if (signal && signal->Aborted()) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  SetBodyUsed();

  RefPtr<Promise> promise =
    FetchBodyConsumer<Derived>::Create(DerivedClass()->GetParentObject(),
                                       this, signal, aType, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv);

template <class Derived>
void
FetchBody<Derived>::SetMimeType()
{
  // Extract mime type.
  ErrorResult result;
  nsCString contentTypeValues;
  MOZ_ASSERT(DerivedClass()->GetInternalHeaders());
  DerivedClass()->GetInternalHeaders()->Get(NS_LITERAL_CSTRING("Content-Type"),
                                            contentTypeValues, result);
  MOZ_ALWAYS_TRUE(!result.Failed());

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (!contentTypeValues.IsVoid() && contentTypeValues.Find(",") == -1) {
    mMimeType = contentTypeValues;
    ToLowerCase(mMimeType);
  }
}

template
void
FetchBody<Request>::SetMimeType();

template
void
FetchBody<Response>::SetMimeType();

} // namespace dom
} // namespace mozilla
