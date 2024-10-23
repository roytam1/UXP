/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ScriptLoadHandler.h"
#include "ModuleLoadRequest.h"
#include "ModuleScript.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SRILogHelper.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsGlobalWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsContentPolicyUtils.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsITimedChannel.h"
#include "nsIScriptElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/Logging.h"
#include "nsCRT.h"
#include "nsContentCreatorFunctions.h"
#include "nsProxyRelease.h"
#include "nsSandboxFlags.h"
#include "nsContentTypeParser.h"
#include "nsINetworkPredictor.h"
#include "ImportManager.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/CycleCollectedJSContext.h"

#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "nsIScriptError.h"

using JS::SourceBufferHolder;

namespace mozilla {
namespace dom {

void
ImplCycleCollectionUnlink(ScriptLoadRequestList& aField);

void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            ScriptLoadRequestList& aField,
                            const char* aName,
                            uint32_t aFlags = 0);

//////////////////////////////////////////////////////////////
// ScriptFetchOptions
//////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION(ScriptFetchOptions,
                         mElement,
                         mTriggeringPrincipal)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ScriptFetchOptions, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ScriptFetchOptions, Release)

ScriptFetchOptions::ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                                       mozilla::net::ReferrerPolicy aReferrerPolicy,
                                       nsIScriptElement* aElement,
                                       nsIPrincipal* aTriggeringPrincipal)
  : mCORSMode(aCORSMode)
  , mReferrerPolicy(aReferrerPolicy)
  , mIsPreload(false)
  , mElement(aElement)
  , mTriggeringPrincipal(aTriggeringPrincipal)
{
}

ScriptFetchOptions::~ScriptFetchOptions()
{}

//////////////////////////////////////////////////////////////
// nsScriptLoadRequest
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoadRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoadRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions)
  tmp->mScript = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


ScriptLoadRequest::ScriptLoadRequest(ScriptKind aKind,
                                     nsIURI* aURI,
                                     ScriptFetchOptions* aFetchOptions,
                                     const SRIMetadata& aIntegrity,
                                     nsIURI* aReferrer)
  : mKind(aKind),
    mScriptMode(ScriptMode::eBlocking),
    mProgress(Progress::Loading),
    mIsInline(true),
    mHasSourceMapURL(false),
    mInDeferList(false),
    mInAsyncList(false),
    mIsNonAsyncScriptInserted(false),
    mIsXSLT(false),
    mIsCanceled(false),
    mWasCompiledOMT(false),
    mOffThreadToken(nullptr),
    mScriptTextBuf(nullptr),
    mScriptTextLength(0),
    mURI(aURI),
    mLineNo(1),
    mIntegrity(aIntegrity),
    mReferrer(aReferrer),
    mFetchOptions(aFetchOptions)
{
  MOZ_ASSERT(mFetchOptions);
}

ScriptLoadRequest::~ScriptLoadRequest()
{
  js_free(mScriptTextBuf);

  // We should always clean up any off-thread script parsing resources.
  MOZ_ASSERT(!mOffThreadToken);

  // But play it safe in release builds and try to clean them up here
  // as a fail safe.
  MaybeCancelOffThreadScript();

  DropJSObjects(this);
}

void
ScriptLoadRequest::SetReady()
{
  MOZ_ASSERT(mProgress != Progress::Ready);
  mProgress = Progress::Ready;
}

void
ScriptLoadRequest::Cancel()
{
  MaybeCancelOffThreadScript();
  mIsCanceled = true;
}

void
ScriptLoadRequest::MaybeCancelOffThreadScript()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOffThreadToken) {
    return;
  }

  JSContext* cx = danger::GetJSContext();
  if (IsModuleRequest()) {
    JS::CancelOffThreadModule(cx, mOffThreadToken);
  } else {
    JS::CancelOffThreadScript(cx, mOffThreadToken);
  }
  mOffThreadToken = nullptr;
}

inline ModuleLoadRequest*
ScriptLoadRequest::AsModuleRequest()
{
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

void
ScriptLoadRequest::SetScriptMode(bool aDeferAttr, bool aAsyncAttr)
{
  if (aAsyncAttr) {
    mScriptMode = ScriptMode::eAsync;
  } else if (aDeferAttr || IsModuleRequest()) {
    mScriptMode = ScriptMode::eDeferred;
  } else {
    mScriptMode = ScriptMode::eBlocking;
  }
}

void ScriptLoadRequest::SetScript(JSScript* aScript) {
  MOZ_ASSERT(!mScript);
  mScript = aScript;
  HoldJSObjects(this);
}

//////////////////////////////////////////////////////////////

// ScriptLoadRequestList
//////////////////////////////////////////////////////////////

ScriptLoadRequestList::~ScriptLoadRequestList()
{
  Clear();
}

void
ScriptLoadRequestList::Clear()
{
  while (!isEmpty()) {
    RefPtr<ScriptLoadRequest> first = StealFirst();
    first->Cancel();
    // And just let it go out of scope and die.
  }
}

#ifdef DEBUG
bool
ScriptLoadRequestList::Contains(ScriptLoadRequest* aElem) const
{
  for (const ScriptLoadRequest* req = getFirst();
       req; req = req->getNext()) {
    if (req == aElem) {
      return true;
    }
  }

  return false;
}
#endif // DEBUG

inline void
ImplCycleCollectionUnlink(ScriptLoadRequestList& aField)
{
  while (!aField.isEmpty()) {
    RefPtr<ScriptLoadRequest> first = aField.StealFirst();
  }
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            ScriptLoadRequestList& aField,
                            const char* aName,
                            uint32_t aFlags)
{
  for (ScriptLoadRequest* request = aField.getFirst();
       request; request = request->getNext())
  {
    CycleCollectionNoteChild(aCallback, request, aName, aFlags);
  }
}

//////////////////////////////////////////////////////////////
// ScriptLoader::PreloadInfo
//////////////////////////////////////////////////////////////

inline void
ImplCycleCollectionUnlink(ScriptLoader::PreloadInfo& aField)
{
  ImplCycleCollectionUnlink(aField.mRequest);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            ScriptLoader::PreloadInfo& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mRequest, aName, aFlags);
}

//////////////////////////////////////////////////////////////
// ScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ScriptLoader,
                         mNonAsyncExternalScriptInsertedRequests,
                         mLoadingAsyncRequests,
                         mLoadedAsyncRequests,
                         mDeferRequests, 
                         mXSLTRequests, 
                         mDynamicImportRequests,
                         mParserBlockingRequest, 
                         mPreloads, 
                         mPendingChildLoaders, 
                         mFetchedModules)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoader)

ScriptLoader::ScriptLoader(nsIDocument *aDocument)
  : mDocument(aDocument),
    mParserBlockingBlockerCount(0),
    mBlockerCount(0),
    mNumberOfProcessors(0),
    mEnabled(true),
    mDeferEnabled(false),
    mDocumentParsingDone(false),
    mBlockingDOMContentLoaded(false),
    mReporter(new ConsoleReportCollector())
{
    EnsureModuleHooksInitialized();
}

ScriptLoader::~ScriptLoader()
{
  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    FinishDynamicImport(req->AsModuleRequest(), NS_ERROR_ABORT);
  }

  for(ScriptLoadRequest* req = mNonAsyncExternalScriptInsertedRequests.getFirst();
      req;
      req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  // Unblock the kids, in case any of them moved to a different document
  // subtree in the meantime and therefore aren't actually going away.
  for (uint32_t j = 0; j < mPendingChildLoaders.Length(); ++j) {
    mPendingChildLoaders[j]->RemoveParserBlockingScriptExecutionBlocker();
  }
}

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

static bool
IsScriptEventHandler(ScriptKind kind, nsIContent* aScriptElement)
{
  if (kind != ScriptKind::Classic) {
    return false;
  }

  if (!aScriptElement->IsHTMLElement()) {
    return false;
  }

  nsAutoString forAttr, eventAttr;
  if (!aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_for, forAttr) ||
      !aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, eventAttr)) {
    return false;
  }

  const nsAString& for_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return true;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(eventAttr, false);
  if (!StringBeginsWith(event_str, NS_LITERAL_STRING("onload"),
                        nsCaseInsensitiveStringComparator())) {
    // It ain't "onload.*".

    return true;
  }

  nsAutoString::const_iterator start, end;
  event_str.BeginReading(start);
  event_str.EndReading(end);

  start.advance(6); // advance past "onload"

  if (start != end && *start != '(' && *start != ' ') {
    // We got onload followed by something other than space or
    // '('. Not good enough.

    return true;
  }

  return false;
}

nsresult
ScriptLoader::CheckContentPolicy(nsIDocument* aDocument,
                                 nsISupports *aContext,
                                 nsIURI *aURI,
                                 const nsAString &aType,
                                 bool aIsPreLoad)
{
  nsContentPolicyType contentPolicyType = aIsPreLoad
                                          ? nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD
                                          : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(contentPolicyType,
                                          aURI,
                                          aDocument->NodePrincipal(),
                                          aContext,
                                          NS_LossyConvertUTF16toASCII(aType),
                                          nullptr,    //extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
      return NS_ERROR_CONTENT_BLOCKED;
    }
    return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  return NS_OK;
}

bool
ScriptLoader::ModuleMapContainsURL(nsIURI* aURL) const
{
  // Returns whether we have fetched, or are currently fetching, a module script
  // for a URL.
  return mFetchingModules.Contains(aURL) ||
         mFetchedModules.Contains(aURL);
}

bool
ScriptLoader::IsFetchingModule(ModuleLoadRequest *aRequest) const
{
  bool fetching = mFetchingModules.Contains(aRequest->mURI);
  MOZ_ASSERT_IF(fetching, !mFetchedModules.Contains(aRequest->mURI));
  return fetching;
}

void
ScriptLoader::SetModuleFetchStarted(ModuleLoadRequest *aRequest)
{
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsLoading());
  MOZ_ASSERT(!ModuleMapContainsURL(aRequest->mURI));
  mFetchingModules.Put(aRequest->mURI, nullptr);
}

void
ScriptLoader::SetModuleFetchFinishedAndResumeWaitingRequests(ModuleLoadRequest *aRequest,
                                                             nsresult aResult)
{
  // Update module map with the result of fetching a single module script.
  //
  // If any requests for the same URL are waiting on this one to complete, they
  // will have ModuleLoaded or LoadFailed on them when the promise is
  // resolved/rejected. This is set up in StartLoad.

  RefPtr<GenericPromise::Private> promise;
  MOZ_ALWAYS_TRUE(mFetchingModules.Get(aRequest->mURI, getter_AddRefs(promise)));
  mFetchingModules.Remove(aRequest->mURI);

  RefPtr<ModuleScript> moduleScript(aRequest->mModuleScript);
  MOZ_ASSERT(NS_FAILED(aResult) == !moduleScript);

  mFetchedModules.Put(aRequest->mURI, moduleScript);

  if (promise) {
    if (moduleScript) {
      promise->Resolve(true, __func__);
    } else {
      promise->Reject(aResult, __func__);
    }
  }
}

RefPtr<GenericPromise>
ScriptLoader::WaitForModuleFetch(nsIURI* aURL)
{
  MOZ_ASSERT(ModuleMapContainsURL(aURL));

  RefPtr<GenericPromise::Private> promise;
  if (mFetchingModules.Get(aURL, getter_AddRefs(promise))) {
    if (!promise) {
      promise = new GenericPromise::Private(__func__);
      mFetchingModules.Put(aURL, promise);
    }
    return promise;
  }

  RefPtr<ModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(aURL, getter_AddRefs(ms)));
  if (!ms) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

ModuleScript*
ScriptLoader::GetFetchedModule(nsIURI* aURL) const
{
  bool found;
  ModuleScript* ms = mFetchedModules.GetWeak(aURL, &found);
  MOZ_ASSERT(found);
  return ms;
}

void ScriptLoader::ClearModuleMap() {
  MOZ_ASSERT(mFetchingModules.IsEmpty());
  mFetchedModules.Clear();
}

nsresult
ScriptLoader::ProcessFetchedModuleSource(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = CreateModuleScript(aRequest);
  MOZ_ASSERT(NS_FAILED(rv) == !aRequest->mModuleScript);

  free(aRequest->mScriptTextBuf);
  aRequest->mScriptTextBuf = nullptr;
  aRequest->mScriptTextLength = 0;

  if (NS_FAILED(rv)) {
    aRequest->LoadFailed();
    return rv;
  }

  if (!aRequest->mIsInline) {
    SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);
  }

  if (!aRequest->mModuleScript->HasParseError()) {
    StartFetchingModuleDependencies(aRequest);
  }

  return NS_OK;
}

static nsresult
ResolveRequestedModules(ModuleLoadRequest* aRequest, nsCOMArray<nsIURI>* aUrlsOut);

nsresult
ScriptLoader::CreateModuleScript(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(!aRequest->mModuleScript);
  MOZ_ASSERT(aRequest->mBaseURL);

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  nsAutoMicroTask mt;
  AutoEntryScript aes(globalObject, "CompileModule", true);

  bool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(true);

  nsresult rv;
  {
    // Update our current script.
    Maybe<AutoCurrentScriptUpdater> masterScriptUpdater;
    nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
    if (master != mDocument) {
      masterScriptUpdater.emplace(master->ScriptLoader(),
                                  aRequest->Element());
    }

    JSContext* cx = aes.cx();
    JS::Rooted<JSObject*> module(cx);

    if (aRequest->mWasCompiledOMT) {
      module = JS::FinishOffThreadModule(cx, aRequest->mOffThreadToken);
      aRequest->mOffThreadToken = nullptr;
      rv = module ? NS_OK : NS_ERROR_FAILURE;
    } else {
      JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

      JS::CompileOptions options(cx);
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        nsAutoString inlineData;
        SourceBufferHolder srcBuf = GetScriptSource(aRequest, inlineData);
        rv = nsJSUtils::CompileModule(cx, srcBuf, global, options, &module);
      }
    }

    MOZ_ASSERT(NS_SUCCEEDED(rv) == (module != nullptr));

    RefPtr<ModuleScript> moduleScript =
        new ModuleScript(aRequest->mFetchOptions, aRequest->mBaseURL);
    aRequest->mModuleScript = moduleScript;

    if (!module) {
      // Compilation failed
      MOZ_ASSERT(aes.HasException());
      JS::Rooted<JS::Value> error(cx);
      if (!aes.StealException(&error)) {
        aRequest->mModuleScript = nullptr;
        return NS_ERROR_FAILURE;
      }

      moduleScript->SetParseError(error);
      aRequest->ModuleErrored();
      return NS_OK;
    }

    moduleScript->SetModuleRecord(module);

    // Validate requested modules and treat failure to resolve module specifiers
    // the same as a parse error.
    rv = ResolveRequestedModules(aRequest, nullptr);
    if (NS_FAILED(rv)) {
      aRequest->ModuleErrored();
      return NS_OK;
    }
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  return rv;
}

static bool
CreateTypeError(JSContext* aCx, ModuleScript* aScript, const nsString& aMessage,
                JS::MutableHandle<JS::Value> aError)
{
  JS::Rooted<JSObject*> module(aCx, aScript->ModuleRecord());
  JS::Rooted<JSScript*> script(aCx, JS::GetModuleScript(aCx, module));
  JS::Rooted<JSString*> filename(aCx);
  filename = JS_NewStringCopyZ(aCx, JS_GetScriptFilename(script));
  if (!filename) {
    return false;
  }

  JS::Rooted<JSString*> message(aCx, JS_NewUCStringCopyZ(aCx, aMessage.get()));
  if (!message) {
    return false;
  }

  return JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, filename, 0, 0, nullptr,
                         message, aError);
}

static nsresult
HandleResolveFailure(JSContext* aCx, ModuleScript* aScript,
                     const nsAString& aSpecifier)
{
  // TODO: How can we get the line number of the failed import?

  nsAutoString message(NS_LITERAL_STRING("Error resolving module specifier: "));
  message.Append(aSpecifier);

  JS::Rooted<JS::Value> error(aCx);
  if (!CreateTypeError(aCx, aScript, message, &error)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aScript->SetParseError(error);
  return NS_OK;
}

static already_AddRefed<nsIURI>
ResolveModuleSpecifier(ScriptLoader* aLoader, LoadedScript* aScript, const nsAString& aSpecifier)
{
  // The following module specifiers are allowed by the spec:
  //  - a valid absolute URL
  //  - a valid relative URL that starts with "/", "./" or "../"
  //
  // Bareword module specifiers are currently disallowed as these may be given
  // special meanings in the future.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpecifier);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  if (rv != NS_ERROR_MALFORMED_URI) {
    return nullptr;
  }

  if (!StringBeginsWith(aSpecifier, NS_LITERAL_STRING("/")) &&
      !StringBeginsWith(aSpecifier, NS_LITERAL_STRING("./")) &&
      !StringBeginsWith(aSpecifier, NS_LITERAL_STRING("../"))) {
    return nullptr;
  }

  // Get the document's base URL if we don't have a referencing script here.
  nsCOMPtr<nsIURI> baseURL;
  if (aScript) {
    baseURL = aScript->BaseURL();
  } else {
    baseURL = aLoader->GetDocument()->GetDocBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, baseURL);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  return nullptr;
}

static nsresult
ResolveRequestedModules(ModuleLoadRequest* aRequest, nsCOMArray<nsIURI>* aUrlsOut)
{
  ModuleScript* ms = aRequest->mModuleScript;

  AutoJSAPI jsapi;
  if (!jsapi.Init(ms->ModuleRecord())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> moduleRecord(cx, ms->ModuleRecord());
  JS::Rooted<JSObject*> specifiers(cx, JS::GetRequestedModules(cx, moduleRecord));

  uint32_t length;
  if (!JS_GetArrayLength(cx, specifiers, &length)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JS::Value> val(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, specifiers, i, &val)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoJSString specifier;
    if (!specifier.init(cx, val)) {
      return NS_ERROR_FAILURE;
    }

    // Let url be the result of resolving a module specifier given module script and requested.
    nsCOMPtr<nsIURI> uri =
        ResolveModuleSpecifier(aRequest->mLoader, ms, specifier);
    if (!uri) {
      nsresult rv = HandleResolveFailure(cx, ms, specifier);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_ERROR_FAILURE;
    }

    if (aUrlsOut) {
      aUrlsOut->AppendElement(uri.forget());
    }
  }

  return NS_OK;
}

void
ScriptLoader::StartFetchingModuleDependencies(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->mModuleScript->HasParseError());
  MOZ_ASSERT(!aRequest->IsReadyToRun());

  auto visitedSet = aRequest->mVisitedSet;
  MOZ_ASSERT(visitedSet->Contains(aRequest->mURI));

  aRequest->mProgress = ModuleLoadRequest::Progress::FetchingImports;

  nsCOMArray<nsIURI> urls;
  nsresult rv = ResolveRequestedModules(aRequest, &urls);
  if (NS_FAILED(rv)) {
    aRequest->ModuleErrored();
    return;
  }

  // Remove already visited URLs from the list. Put unvisited URLs into the
  // visited set.
  int32_t i = 0;
  while (i < urls.Count()) {
    nsIURI* url = urls[i];
    if (visitedSet->Contains(url)) {
      urls.RemoveObjectAt(i);
    } else {
      visitedSet->PutEntry(url);
      i++;
    }
  }

  if (urls.Count() == 0) {
    // There are no descendants to load so this request is ready.
    aRequest->DependenciesLoaded();
    return;
  }

  // For each url in urls, fetch a module script tree given url, module script's
  // CORS setting, and module script's settings object.
  nsTArray<RefPtr<GenericPromise>> importsReady;
  for (size_t i = 0; i < urls.Length(); i++) {
    RefPtr<GenericPromise> childReady =
      StartFetchingModuleAndDependencies(aRequest, urls[i]);
    importsReady.AppendElement(childReady);
  }

  // Wait for all imports to become ready.
  RefPtr<GenericPromise::AllPromiseType> allReady =
    GenericPromise::All(AbstractThread::GetCurrent(), importsReady);
  allReady->Then(AbstractThread::GetCurrent(), __func__, aRequest,
                 &ModuleLoadRequest::DependenciesLoaded,
                 &ModuleLoadRequest::ModuleErrored);
}

RefPtr<GenericPromise>
ScriptLoader::StartFetchingModuleAndDependencies(ModuleLoadRequest* aParent,
                                                 nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  RefPtr<ModuleLoadRequest> childRequest =
      ModuleLoadRequest::CreateStaticImport(aURI, aParent);

  aParent->mImports.AppendElement(childRequest);

  RefPtr<GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartLoad(childRequest, NS_LITERAL_STRING("module"), false);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(!childRequest->mModuleScript);
    childRequest->mReady.Reject(rv, __func__);
    return ready;
  }

  return ready;
}

static ScriptLoader* GetCurrentScriptLoader(JSContext* aCx) {
  JSObject* object = JS::CurrentGlobalOrNull(aCx);
  if (!object) {
    return nullptr;
  }

  nsIGlobalObject* global = xpc::NativeGlobal(object);
  if (!global) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
  nsGlobalWindow* window = nsGlobalWindow::Cast(win);
  if (!window) {
    return nullptr;
  }

  nsIDocument* document = window->GetDocument();
  if (!document) {
    return nullptr;
  }

  ScriptLoader* loader = document->ScriptLoader();
  if (!loader) {
    return nullptr;
  }

  return loader;
}

static LoadedScript* GetLoadedScriptOrNull(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate) {
  if (aReferencingPrivate.isUndefined()) {
    return nullptr;
  }

  auto script = static_cast<LoadedScript*>(aReferencingPrivate.toPrivate());
  MOZ_ASSERT_IF(
      script->IsModuleScript(),
      JS::GetModulePrivate(script->AsModuleScript()->ModuleRecord()) ==
          aReferencingPrivate);

  return script;
}

// 8.1.3.8.1 HostResolveImportedModule(referencingModule, specifier)
JSObject*
HostResolveImportedModule(JSContext* aCx,
                          JS::Handle<JS::Value> aReferencingPrivate,
                          JS::Handle<JSString*> aSpecifier)
{
  JS::Rooted<JSObject*> module(aCx);
  ScriptLoader::ResolveImportedModule(aCx, aReferencingPrivate, aSpecifier,
                                      &module);
  return module;
}

/* static */ void ScriptLoader::ResolveImportedModule(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSString*> aSpecifier, JS::MutableHandle<JSObject*> aModuleOut) {
  MOZ_ASSERT(!aModuleOut);

  RefPtr<LoadedScript> script(GetLoadedScriptOrNull(aCx, aReferencingPrivate));

  // Let url be the result of resolving a module specifier given referencing
  // module script and specifier.
  nsAutoJSString string;
  if (!string.init(aCx, aSpecifier)) {
    return;
  }

  RefPtr<ScriptLoader> loader = GetCurrentScriptLoader(aCx);
  if (!loader) {
    return;
  }

  nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(loader, script, string);

  // This cannot fail because resolving a module specifier must have been
  // previously successful with these same two arguments.
  MOZ_ASSERT(uri, "Failed to resolve previously-resolved module specifier");

  // Let resolved module script be moduleMap[url]. (This entry must exist for us
  // to have gotten to this point.)

  ModuleScript* ms = loader->GetFetchedModule(uri);
  MOZ_ASSERT(ms, "Resolved module not found in module map");

  MOZ_ASSERT(!ms->HasParseError());
  MOZ_ASSERT(ms->ModuleRecord());

  aModuleOut.set(ms->ModuleRecord());
}

bool
HostPopulateImportMeta(JSContext* aCx, JS::Handle<JSObject*> aModule,
                       JS::Handle<JSObject*> aMetaObject)
{
  MOZ_DIAGNOSTIC_ASSERT(aModule);

  JS::Value value = JS::GetModulePrivate(aModule);
  if (value.isUndefined()) {
    JS_ReportErrorASCII(aCx, "Module script not found");
    return false;
  }

  auto script = static_cast<ModuleScript*>(value.toPrivate());
  MOZ_DIAGNOSTIC_ASSERT(script->ModuleRecord() == aModule);

  nsAutoCString url;
  MOZ_DIAGNOSTIC_ASSERT(script->BaseURL());
  MOZ_ALWAYS_SUCCEEDS(script->BaseURL()->GetAsciiSpec(url));

  JS::Rooted<JSString*> urlString(aCx, JS_NewStringCopyZ(aCx, url.get()));
  if (!urlString) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  return JS_DefineProperty(aCx, aMetaObject, "url", urlString, JSPROP_ENUMERATE);
}

bool HostImportModuleDynamically(JSContext* aCx,
                                 JS::Handle<JS::Value> aReferencingPrivate,
                                 JS::Handle<JSString*> aSpecifier,
                                 JS::Handle<JSObject*> aPromise) {
  RefPtr<LoadedScript> script(GetLoadedScriptOrNull(aCx, aReferencingPrivate));

  // Attempt to resolve the module specifier.
  nsAutoJSString string;
  if (!string.init(aCx, aSpecifier)) {
    return false;
  }

  RefPtr<ScriptLoader> loader = GetCurrentScriptLoader(aCx);
  if (!loader) {
    return false;
  }

  nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(loader, script, string);
  if (!uri) {
    JS_ReportErrorNumberUC(aCx, js::GetErrorMessage, nullptr,
                           JSMSG_BAD_MODULE_SPECIFIER, string.get());
    return false;
  }

  // Create a new top-level load request.
  ScriptFetchOptions* options;
  nsIURI* baseURL;
  if (script) {
    options = script->FetchOptions();
    baseURL = script->BaseURL();
  } else {
    // We don't have a referencing script so fall back on using
    // options from the document. This can happen when the user
    // triggers an inline event handler, as there is no active script
    // there.
    nsIDocument* document = loader->GetDocument();
    options = new ScriptFetchOptions(mozilla::CORS_NONE,
                                     document->GetReferrerPolicy(), nullptr,
                                     document->NodePrincipal());
    baseURL = document->GetDocBaseURI();
  }

  RefPtr<ModuleLoadRequest> request = ModuleLoadRequest::CreateDynamicImport(
      uri, options, baseURL, loader, aReferencingPrivate, aSpecifier, aPromise);

  loader->StartDynamicImport(request);
  return true;
}

void ScriptLoader::StartDynamicImport(ModuleLoadRequest* aRequest) {
  mDynamicImportRequests.AppendElement(aRequest);

  nsresult rv = StartLoad(aRequest, NS_LITERAL_STRING("script"), false);
  if (NS_FAILED(rv)) {
    FinishDynamicImport(aRequest, rv);
  }
}

void ScriptLoader::FinishDynamicImport(ModuleLoadRequest* aRequest,
                                       nsresult aResult) {
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(aRequest->mDynamicPromise));
  FinishDynamicImport(jsapi.cx(), aRequest, aResult);
}

void ScriptLoader::FinishDynamicImport(JSContext* aCx,
                                       ModuleLoadRequest* aRequest,
                                       nsresult aResult) {
  // Complete the dynamic import, report failures indicated by aResult or as a
  // pending exception on the context.

  if (NS_FAILED(aResult)) {
    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    JS_ReportErrorNumberUC(aCx, js::GetErrorMessage, nullptr,
                           JSMSG_DYNAMIC_IMPORT_FAILED);
  }

  JS::Rooted<JS::Value> referencingScript(aCx,
                                          aRequest->mDynamicReferencingPrivate);
  JS::Rooted<JSString*> specifier(aCx, aRequest->mDynamicSpecifier);
  JS::Rooted<JSObject*> promise(aCx, aRequest->mDynamicPromise);

  JS::FinishDynamicModuleImport(aCx, referencingScript, specifier, promise);

  // FinishDynamicModuleImport clears any pending exception.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aRequest->ClearDynamicImport();
}

static void DynamicImportPrefChangedCallback(const char* aPrefName,
                                             void* aClosure) {
  bool enabled = Preferences::GetBool(aPrefName);
  JS::ModuleDynamicImportHook hook =
      enabled ? HostImportModuleDynamically : nullptr;

  AutoJSAPI jsapi;
  jsapi.Init();
  JS::SetModuleDynamicImportHook(jsapi.cx(), hook);
}

void ScriptLoader::EnsureModuleHooksInitialized() {
  AutoJSAPI jsapi;
  jsapi.Init();
  JSRuntime* rt = JS_GetRuntime(jsapi.cx());
  if (JS::GetModuleResolveHook(rt)) {
    return;
  }

  JS::SetModuleResolveHook(rt, HostResolveImportedModule);
  JS::SetModuleMetadataHook(jsapi.cx(), HostPopulateImportMeta);
  JS::SetScriptPrivateReferenceHooks(jsapi.cx(), HostAddRefTopLevelScript,
                                     HostReleaseTopLevelScript);

  Preferences::RegisterCallbackAndCall(DynamicImportPrefChangedCallback,
                                       "javascript.options.dynamicImport",
                                       (void*)nullptr);
}

void
ScriptLoader::CheckModuleDependenciesLoaded(ModuleLoadRequest* aRequest)
{
  RefPtr<ModuleScript> moduleScript = aRequest->mModuleScript;
  if (!moduleScript || moduleScript->HasParseError()) {
    return;
  }

  for (auto childRequest : aRequest->mImports) {
    ModuleScript* childScript = childRequest->mModuleScript;
    if (!childScript) {
      aRequest->mModuleScript = nullptr;
      // Load error on script load request; bail.
      return;
    }
  }
}

class ScriptRequestProcessor : public Runnable
{
private:
  RefPtr<ScriptLoader> mLoader;
  RefPtr<ScriptLoadRequest> mRequest;
public:
  ScriptRequestProcessor(ScriptLoader* aLoader,
                         ScriptLoadRequest* aRequest)
    : mLoader(aLoader)
    , mRequest(aRequest)
  {}
  NS_IMETHOD Run() override {
    if (mRequest->IsModuleRequest() &&
        mRequest->AsModuleRequest()->IsDynamicImport()) {
      mLoader->ProcessDynamicImport(mRequest->AsModuleRequest());
      return NS_OK;
    }

    return mLoader->ProcessRequest(mRequest);
  }
};

void ScriptLoader::RunScriptWhenSafe(ScriptLoadRequest* aRequest) {
  auto runnable = new ScriptRequestProcessor(this, aRequest);
  nsContentUtils::AddScriptRunner(runnable);
}

void
ScriptLoader::ProcessLoadedModuleTree(ModuleLoadRequest* aRequest)
{
  if (aRequest->IsTopLevel()) {
    ModuleScript* moduleScript = aRequest->mModuleScript;
    if (moduleScript && !moduleScript->HasErrorToRethrow()) {
      if (!InstantiateModuleTree(aRequest)) {
        aRequest->mModuleScript = nullptr;
      }
    }

    if (aRequest->IsDynamicImport()) {
      MOZ_ASSERT(aRequest->isInList());
      RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
      RunScriptWhenSafe(req);
    } else if (aRequest->mIsInline &&
               aRequest->GetParserCreated() == NOT_FROM_PARSER) {
        RunScriptWhenSafe(aRequest);
    } else {
      MaybeMoveToLoadedList(aRequest);
      ProcessPendingRequests();
    }
  }

  if (aRequest->mWasCompiledOMT) {
    mDocument->UnblockOnload(false);
  }
}

JS::Value
ScriptLoader::FindFirstParseError(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  if (moduleScript->HasParseError()) {
    return moduleScript->ParseError();
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    JS::Value error = FindFirstParseError(childRequest);
    if (!error.isUndefined()) {
      return error;
    }
  }

  return JS::UndefinedValue();
}

bool
ScriptLoader::InstantiateModuleTree(ModuleLoadRequest* aRequest)
{
  // Instantiate a top-level module and record any error.

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aRequest->IsTopLevel());

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  JS::Value parseError = FindFirstParseError(aRequest);
  if (!parseError.isUndefined()) {
    // Parse error found in the requested script
    moduleScript->SetErrorToRethrow(parseError);
    return true;
  }

  MOZ_ASSERT(moduleScript->ModuleRecord());

  nsAutoMicroTask mt;
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(moduleScript->ModuleRecord()))) {
    return false;
  }

  JS::Rooted<JSObject*> module(jsapi.cx(), moduleScript->ModuleRecord());
  bool ok = NS_SUCCEEDED(nsJSUtils::ModuleInstantiate(jsapi.cx(), module));

  if (!ok) {
    MOZ_ASSERT(jsapi.HasException());
    JS::RootedValue exception(jsapi.cx());
    if (!jsapi.StealException(&exception)) {
      return false;
    }
    MOZ_ASSERT(!exception.isUndefined());
    moduleScript->SetErrorToRethrow(exception);
  }

  return true;
}

nsresult
ScriptLoader::StartLoad(ScriptLoadRequest *aRequest, const nsAString &aType,
                        bool aScriptFromHead)
{
  MOZ_ASSERT(aRequest->IsLoading());
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (aRequest->IsModuleRequest()) {
    // Check whether the module has been fetched or is currently being fetched,
    // and if so wait for it rather than starting a new fetch.
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (ModuleMapContainsURL(request->mURI)) {
      WaitForModuleFetch(request->mURI)
        ->Then(AbstractThread::GetCurrent(), __func__, request,
               &ModuleLoadRequest::ModuleLoaded,
               &ModuleLoadRequest::LoadFailed);
      return NS_OK;
    }
  }

  nsContentPolicyType contentPolicyType = aRequest->IsPreload()
                                          ? nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD
                                          : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;
  nsCOMPtr<nsINode> context;
  if (aRequest->Element()) {
    context = do_QueryInterface(aRequest->Element());
  }
  else {
    context = mDocument;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->MasterDocument()->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  nsIDocShell *docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  nsSecurityFlags securityFlags;
  if (aRequest->IsModuleRequest()) {
    // According to the spec, module scripts have different behaviour to classic
    // scripts and always use CORS.
    securityFlags = nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
    if (aRequest->CORSMode() == CORS_NONE ||
        aRequest->CORSMode() == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else {
      MOZ_ASSERT(aRequest->CORSMode() == CORS_USE_CREDENTIALS);
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  } else {
    securityFlags = aRequest->CORSMode() == CORS_NONE
      ? nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL
      : nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
    if (aRequest->CORSMode() == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else if (aRequest->CORSMode() == CORS_USE_CREDENTIALS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  }
  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                              aRequest->mURI,
                              context,
                              securityFlags,
                              contentPolicyType,
                              loadGroup,
                              prompter,
                              nsIRequest::LOAD_NORMAL |
                              nsIChannel::LOAD_CLASSIFY_URI);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));

  if (cos) {
    if (aScriptFromHead && aRequest->IsBlockingScript()) {
      // synchronous head scripts block lading of most other non js/css
      // content such as images
      cos->AddClassFlags(nsIClassOfService::Leader);
    } else if (!aRequest->IsDeferredScript()) {
      // other scripts are neither blocked nor prioritized unless marked deferred
      cos->AddClassFlags(nsIClassOfService::Unblocked);
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // HTTP content negotation has little value in this context.
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("*/*"),
                                  false);
    httpChannel->SetReferrerWithPolicy(aRequest->mReferrer,
                                       aRequest->ReferrerPolicy());

    nsCOMPtr<nsIHttpChannelInternal> internalChannel(do_QueryInterface(httpChannel));
    if (internalChannel) {
      internalChannel->SetIntegrityMetadata(aRequest->mIntegrity.GetIntegrityString());
    }
  }

  nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(docshell));
  mozilla::net::PredictorLearn(aRequest->mURI, mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, loadContext);

  // Set the initiator type
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
  if (timedChannel) {
    timedChannel->SetInitiatorType(NS_LITERAL_STRING("script"));
  }

  nsAutoPtr<mozilla::dom::SRICheckDataVerifier> sriDataVerifier;
  if (!aRequest->mIntegrity.IsEmpty()) {
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    sriDataVerifier = new SRICheckDataVerifier(aRequest->mIntegrity, sourceUri,
                                               mReporter);
  }

  RefPtr<ScriptLoadHandler> handler =
      new ScriptLoadHandler(this, aRequest, sriDataVerifier.forget());

  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), handler);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen2(loader);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRequest->IsModuleRequest()) {
    // We successfully started fetching a module so put its URL in the module
    // map and mark it as fetching.
    SetModuleFetchStarted(aRequest->AsModuleRequest());
  }

  return NS_OK;
}

bool
ScriptLoader::PreloadURIComparator::Equals(const PreloadInfo &aPi,
                                           nsIURI * const &aURI) const
{
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) &&
         same;
}

static inline bool
ParseTypeAttribute(const nsAString& aType, JSVersion* aVersion)
{
  MOZ_ASSERT(!aType.IsEmpty());
  MOZ_ASSERT(aVersion);
  MOZ_ASSERT(*aVersion == JSVERSION_DEFAULT);

  nsContentTypeParser parser(aType);

  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  NS_ENSURE_SUCCESS(rv, false);

  if (!nsContentUtils::IsJavascriptMIMEType(mimeType)) {
    return false;
  }

  // Get the version string, and ensure the language supports it.
  nsAutoString versionName;
  rv = parser.GetParameter("version", versionName);

  if (NS_SUCCEEDED(rv)) {
    *aVersion = nsContentUtils::ParseJavascriptVersion(versionName);
  } else if (rv != NS_ERROR_INVALID_ARG) {
    return false;
  }

  return true;
}

static bool
CSPAllowsInlineScript(nsIScriptElement *aElement, nsIDocument *aDocument)
{
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  // Note: For imports NodePrincipal and the principal of the master are
  // the same.
  nsresult rv = aDocument->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, false);

  if (!csp) {
    // no CSP --> allow
    return true;
  }

  // query the nonce
  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);
  nsAutoString nonce;
  scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::nonce, nonce);
  bool parserCreated = aElement->GetParserCreated() != mozilla::dom::NOT_FROM_PARSER;

  // query the scripttext
  nsAutoString scriptText;
  aElement->GetScriptText(scriptText);

  bool allowInlineScript = false;
  rv = csp->GetAllowsInline(nsIContentSecurityPolicy::SCRIPT_SRC_ELEM_DIRECTIVE,
                            nonce, parserCreated, scriptText,
                            aElement->GetScriptLineNumber(),
                            aElement->GetScriptColumnNumber(),
                            &allowInlineScript);
  return allowInlineScript;
}

ScriptLoadRequest*
ScriptLoader::CreateLoadRequest(ScriptKind aKind,
                                nsIURI* aURI,
                                nsIScriptElement* aElement,
                                nsIPrincipal* aTriggeringPrincipal,
                                CORSMode aCORSMode,
                                const SRIMetadata& aIntegrity,
                                mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  nsIURI* referrer = mDocument->GetDocumentURI();
  ScriptFetchOptions* fetchOptions =
    new ScriptFetchOptions(aCORSMode, aReferrerPolicy, aElement, aTriggeringPrincipal);

  if (aKind == ScriptKind::Classic) {
    return new ScriptLoadRequest(aKind, aURI, fetchOptions, aIntegrity, referrer);
  }

  MOZ_ASSERT(aKind == ScriptKind::Module);
  return ModuleLoadRequest::CreateTopLevel(aURI, fetchOptions, aIntegrity, referrer, this);
}

bool
ScriptLoader::ProcessScriptElement(nsIScriptElement *aElement)
{
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, false);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return false;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);

  nsAutoString type;
  bool hasType = aElement->GetScriptType(type);

  ScriptKind scriptKind = aElement->GetScriptIsModule() ?
                          ScriptKind::Module :
                          ScriptKind::Classic;

  // Step 13. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptKind, scriptContent)) {
    return false;
  }

  JSVersion version = JSVERSION_DEFAULT;

  // For classic scripts, check the type attribute to determine language and
  // version. If type exists, it trumps the deprecated 'language='
  if (scriptKind == ScriptKind::Classic) {
    if (!type.IsEmpty()) {
      NS_ENSURE_TRUE(ParseTypeAttribute(type, &version), false);
    } else if (!hasType) {
      // no 'type=' element
      // "language" is a deprecated attribute of HTML, so we check it only for
      // HTML script elements.
      if (scriptContent->IsHTMLElement()) {
        nsAutoString language;
        scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::language, language);
        if (!language.IsEmpty()) {
          if (!nsContentUtils::IsJavaScriptLanguage(language)) {
            return false;
          }
        }
      }
    }
  }

  // "In modern user agents that support module scripts, the script element with
  // the nomodule attribute will be ignored".
  // "The nomodule attribute must not be specified on module scripts (and will
  // be ignored if it is)."
  if (mDocument->ModuleScriptsEnabled() &&
      scriptKind == ScriptKind::Classic &&
      scriptContent->IsHTMLElement() &&
      scriptContent->HasAttr(kNameSpaceID_None, nsGkAtoms::nomodule)) {
    return false;
  }

  // Step 15. and later in the HTML5 spec
  nsresult rv = NS_OK;
  RefPtr<ScriptLoadRequest> request;
  mozilla::net::ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);
  if (aElement->GetScriptExternal()) {
    // external script
    nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
    if (!scriptURI) {
      // Asynchronously report the failure to create a URI object
      NS_DispatchToCurrentThread(
        NewRunnableMethod(aElement,
                          &nsIScriptElement::FireErrorEvent));
      return false;
    }

    // Double-check that the preload matches what we're asked to load now.
    CORSMode ourCORSMode = aElement->GetCORSMode();
    nsTArray<PreloadInfo>::index_type i =
      mPreloads.IndexOf(scriptURI.get(), 0, PreloadURIComparator());
    if (i != nsTArray<PreloadInfo>::NoIndex) {
      // preloaded
      // note that a script-inserted script can steal a preload!
      request = mPreloads[i].mRequest;
      request->SetIsLoadRequest(aElement);
      nsString preloadCharset(mPreloads[i].mCharset);
      mPreloads.RemoveElementAt(i);

      // Double-check that the charset the preload used is the same as
      // the charset we have now.
      nsAutoString elementCharset;
      aElement->GetScriptCharset(elementCharset);
      if (elementCharset.Equals(preloadCharset) &&
          ourCORSMode == request->CORSMode() &&
          referrerPolicy == request->ReferrerPolicy() &&
          scriptKind == request->mKind) {
        rv = CheckContentPolicy(mDocument, aElement, request->mURI, type, false);
        if (NS_FAILED(rv)) {
          // probably plans have changed; even though the preload was allowed seems
          // like the actual load is not; let's cancel the preload request.
          request->Cancel();
          return false;
        }
      } else {
        // Drop the preload
        request = nullptr;
      }
    }

    if (request) {
      // Use a preload request.

      // It's possible these attributes changed since we started the preload, so
      // update them here.
      request->SetScriptMode(aElement->GetScriptDeferred(),
                             aElement->GetScriptAsync());
    } else {    
      // No usable preload found.

      SRIMetadata sriMetadata;
      {
        nsAutoString integrity;
        scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::integrity,
                               integrity);
        if (!integrity.IsEmpty()) {
          MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
                 ("ScriptLoader::ProcessScriptElement, integrity=%s",
                  NS_ConvertUTF16toUTF8(integrity).get()));
          nsAutoCString sourceUri;
          if (mDocument->GetDocumentURI()) {
            mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
          }
          SRICheck::IntegrityMetadata(integrity, sourceUri, mReporter,
                                      &sriMetadata);
        }
      }

      nsCOMPtr<nsIPrincipal> principal = scriptContent->NodePrincipal();

      request = CreateLoadRequest(scriptKind, scriptURI, aElement, principal,
                                  ourCORSMode, sriMetadata, referrerPolicy);
      request->mIsInline = false;
      request->SetScriptMode(aElement->GetScriptDeferred(),
                             aElement->GetScriptAsync());

      // Set aScriptFromHead to false so we don't treat non-preloaded scripts as
      // blockers for full page load. See bug 792438.
      rv = StartLoad(request, type, false);
      if (NS_FAILED(rv)) {
        // Asynchronously report the load failure
        NS_DispatchToCurrentThread(
          NewRunnableMethod(aElement,
                            &nsIScriptElement::FireErrorEvent));
        return false;
      }
    }

    // Should still be in loading stage of script unless we're loading a module.
    NS_ASSERTION(!request->InCompilingStage() || request->IsModuleRequest(),
                 "Request should not yet be in compiling stage.");

    if (request->IsAsyncScript()) {
      AddAsyncRequest(request);
      if (request->IsReadyToRun()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.

        // KVKV TODO: Instead of processing immediately, try off-thread-parsing
        // it and only schedule a pending ProcessRequest if that fails.
        ProcessPendingRequestsAsync();
      }
      return false;
    }
    if (!aElement->GetParserCreated()) {
      // Violate the HTML5 spec in order to make LABjs and the "order" plug-in
      // for RequireJS work with their Gecko-sniffed code path. See
      // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
      request->mIsNonAsyncScriptInserted = true;
      mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
      if (request->IsReadyToRun()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return false;
    }
    // we now have a parser-inserted request that may or may not be still
    // loading
    if (request->IsDeferredScript()) {
      // We don't want to run this yet.
      // If we come here, the script is a parser-created script and it has
      // the defer attribute but not the async attribute. Since a
      // a parser-inserted script is being run, we came here by the parser
      // running the script, which means the parser is still alive and the
      // parse is ongoing.
      NS_ASSERTION(mDocument->GetCurrentContentSink() ||
                   aElement->GetParserCreated() == FROM_PARSER_XSLT,
          "Non-XSLT Defer script on a document without an active parser; bug 592366.");
      AddDeferRequest(request);
      return false;
    }

    if (aElement->GetParserCreated() == FROM_PARSER_XSLT) {
      // Need to maintain order for XSLT-inserted scripts
      NS_ASSERTION(!mParserBlockingRequest,
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      request->mIsXSLT = true;
      mXSLTRequests.AppendElement(request);
      if (request->IsReadyToRun()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return true;
    }

    if (request->IsReadyToRun() && ReadyToExecuteParserBlockingScripts()) {
      // The request has already been loaded and there are no pending style
      // sheets. If the script comes from the network stream, cheat for
      // performance reasons and avoid a trip through the event loop.
      if (aElement->GetParserCreated() == FROM_PARSER_NETWORK) {
        return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
      }
      // Otherwise, we've got a document.written script, make a trip through
      // the event loop to hide the preload effects from the scripts on the
      // Web page.
      NS_ASSERTION(!mParserBlockingRequest,
          "There can be only one parser-blocking script at a time");
      NS_ASSERTION(mXSLTRequests.isEmpty(),
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      mParserBlockingRequest = request;
      ProcessPendingRequestsAsync();
      return true;
    }

    // The script hasn't loaded yet or there's a style sheet blocking it.
    // The script will be run when it loads or the style sheet loads.
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mParserBlockingRequest = request;
    return true;
  }

  // inline script
  // Is this document sandboxed without 'allow-scripts'?
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return false;
  }

  // Does CSP allow this inline script to run?
  if (!CSPAllowsInlineScript(aElement, mDocument)) {
    return false;
  }

  // Inline scripts ignore their CORS mode and are always CORS_NONE.
  request = CreateLoadRequest(scriptKind, mDocument->GetDocumentURI(), aElement,
                              mDocument->NodePrincipal(),
                              CORS_NONE,
                              SRIMetadata(), // SRI doesn't apply
                              referrerPolicy);
  request->mIsInline = true;
  request->mLineNo = aElement->GetScriptLineNumber();

  // Only the 'async' attribute is heeded on an inline module script and
  // inline classic scripts ignore both these attributes.
  MOZ_ASSERT(!aElement->GetScriptDeferred());
  MOZ_ASSERT_IF(!request->IsModuleRequest(), !aElement->GetScriptAsync());
  request->SetScriptMode(false, aElement->GetScriptAsync());

  request->mBaseURL = mDocument->GetDocBaseURI();

  if (request->IsModuleRequest()) {
    ModuleLoadRequest* modReq = request->AsModuleRequest();

    if (aElement->GetScriptAsync()) {
      AddAsyncRequest(modReq);
    } else {
      AddDeferRequest(modReq);
    }

    nsresult rv = ProcessFetchedModuleSource(modReq);
    if (NS_FAILED(rv)) {
      HandleLoadError(modReq, rv);
    }

    return false;
  }
  request->mProgress = ScriptLoadRequest::Progress::Ready;
  if (aElement->GetParserCreated() == FROM_PARSER_XSLT &&
      (!ReadyToExecuteParserBlockingScripts() || !mXSLTRequests.isEmpty())) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mXSLTRequests.AppendElement(request);
    return true;
  }
  if (aElement->GetParserCreated() == NOT_FROM_PARSER) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
        "A script-inserted script is inserted without an update batch?");
    nsContentUtils::AddScriptRunner(new ScriptRequestProcessor(this,
                                                                 request));
    return false;
  }
  if (aElement->GetParserCreated() == FROM_PARSER_NETWORK &&
      !ReadyToExecuteParserBlockingScripts()) {
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    mParserBlockingRequest = request;
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    return true;
  }
  // We now have a document.written inline script or we have an inline script
  // from the network but there is no style sheet that is blocking scripts.
  // Don't check for style sheets blocking scripts in the document.write
  // case to avoid style sheet network activity affecting when
  // document.write returns. It's not really necessary to do this if
  // there's no document.write currently on the call stack. However,
  // this way matches IE more closely than checking if document.write
  // is on the call stack.
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
      "Not safe to run a parser-inserted script?");
  return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
}

mozilla::net::ReferrerPolicy
ScriptLoader::GetReferrerPolicy(nsIScriptElement* aElement)
{
  mozilla::net::ReferrerPolicy scriptReferrerPolicy =
    aElement->GetReferrerPolicy();
  if (scriptReferrerPolicy != mozilla::net::RP_Unset) {
    return scriptReferrerPolicy;
  }
  return mDocument->GetReferrerPolicy();
}

namespace {

class NotifyOffThreadScriptLoadCompletedRunnable : public Runnable
{
  RefPtr<ScriptLoadRequest> mRequest;
  RefPtr<ScriptLoader> mLoader;
  void *mToken;

public:
  NotifyOffThreadScriptLoadCompletedRunnable(ScriptLoadRequest* aRequest,
                                             ScriptLoader* aLoader)
    : mRequest(aRequest), mLoader(aLoader), mToken(nullptr)
  {}

  virtual ~NotifyOffThreadScriptLoadCompletedRunnable();

  void SetToken(void* aToken) {
    MOZ_ASSERT(aToken && !mToken);
    mToken = aToken;
  }

  NS_DECL_NSIRUNNABLE
};

} /* anonymous namespace */

nsresult
ScriptLoader::ProcessOffThreadRequest(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::Compiling);
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  aRequest->mWasCompiledOMT = true;

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->mOffThreadToken);
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    return ProcessFetchedModuleSource(request);
  }

  aRequest->SetReady();

  if (aRequest == mParserBlockingRequest) {
    if (!ReadyToExecuteParserBlockingScripts()) {
      // If not ready to execute scripts, schedule an async call to
      // ProcessPendingRequests to handle it.
      ProcessPendingRequestsAsync();
      return NS_OK;
    }

    // Same logic as in top of ProcessPendingRequests.
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);
    ProcessRequest(aRequest);
    mDocument->UnblockOnload(false);
    ContinueParserAsync(aRequest);
    return NS_OK;
  }

  nsresult rv = ProcessRequest(aRequest);
  mDocument->UnblockOnload(false);
  return rv;
}

NotifyOffThreadScriptLoadCompletedRunnable::~NotifyOffThreadScriptLoadCompletedRunnable()
{
  if (MOZ_UNLIKELY(mRequest || mLoader) && !NS_IsMainThread()) {
    NS_ReleaseOnMainThread(mRequest.forget());
    NS_ReleaseOnMainThread(mLoader.forget());
  }
}

NS_IMETHODIMP
NotifyOffThreadScriptLoadCompletedRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We want these to be dropped on the main thread, once we return from this
  // function.
  RefPtr<ScriptLoadRequest> request = mRequest.forget();
  RefPtr<ScriptLoader> loader = mLoader.forget();

  request->mOffThreadToken = mToken;
  nsresult rv = loader->ProcessOffThreadRequest(request);

  return rv;
}

static void
OffThreadScriptLoaderCallback(void *aToken, void *aCallbackData)
{
  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable =
    dont_AddRef(static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  aRunnable->SetToken(aToken);
  NS_DispatchToMainThread(aRunnable);
}

nsresult
ScriptLoader::AttemptAsyncScriptCompile(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT_IF(!aRequest->IsModuleRequest(), aRequest->IsReadyToRun());
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  // Don't off-thread compile inline scripts.
  if (aRequest->mIsInline) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());
  JS::CompileOptions options(cx);

  nsresult rv = FillCompileOptionsForRequest(jsapi, aRequest, global, &options);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!JS::CanCompileOffThread(cx, options, aRequest->mScriptTextLength)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
    new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  if (aRequest->IsModuleRequest()) {
    if (!JS::CompileOffThreadModule(cx, options,
                                    aRequest->mScriptTextBuf, aRequest->mScriptTextLength,
                                    OffThreadScriptLoaderCallback,
                                    static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    if (!JS::CompileOffThread(cx, options,
                              aRequest->mScriptTextBuf, aRequest->mScriptTextLength,
                              OffThreadScriptLoaderCallback,
                              static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mDocument->BlockOnload();
  aRequest->mProgress = ScriptLoadRequest::Progress::Compiling;

  Unused << runnable.forget();
  return NS_OK;
}

nsresult
ScriptLoader::CompileOffThreadOrProcessRequest(ScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(!aRequest->mOffThreadToken,
               "Candidate for off-thread compile is already parsed off-thread");
  NS_ASSERTION(!aRequest->InCompilingStage(),
               "Candidate for off-thread compile is already in compiling stage.");

  nsresult rv = AttemptAsyncScriptCompile(aRequest);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  return ProcessRequest(aRequest);
}

SourceBufferHolder
ScriptLoader::GetScriptSource(ScriptLoadRequest* aRequest, nsAutoString& inlineData)
{
  // Return a SourceBufferHolder object holding the script's source text.
  // |inlineData| is used to hold the text for inline objects.

  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    // XXX This is inefficient - GetText makes multiple
    // copies.
    aRequest->Element()->GetScriptText(inlineData);
    return SourceBufferHolder(inlineData.get(),
                              inlineData.Length(),
                              SourceBufferHolder::NoOwnership);
  }

  return SourceBufferHolder(aRequest->mScriptTextBuf,
                            aRequest->mScriptTextLength,
                            SourceBufferHolder::NoOwnership);
}

nsresult
ScriptLoader::ProcessRequest(ScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsReadyToRun(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);

  if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (request->mModuleScript) {
      if (!InstantiateModuleTree(request)) {
        request->mModuleScript = nullptr;
      }
    }

    if (!request->mModuleScript) {
      // There was an error fetching a module script.  Nothing to do here.
      FireScriptAvailable(NS_ERROR_FAILURE, aRequest);
      return NS_OK;
    }
  }

  nsCOMPtr<nsINode> scriptElem = do_QueryInterface(aRequest->Element());

  nsCOMPtr<nsIDocument> doc;
  if (!aRequest->mIsInline) {
    doc = scriptElem->OwnerDoc();
  }

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  uint32_t parserCreated = aRequest->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->Element();
  }

  aRequest->Element()->BeginEvaluating();

  FireScriptAvailable(NS_OK, aRequest);

  // The window may have gone away by this point, in which case there's no point
  // in trying to run the script.
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  {
    // Try to perform a microtask checkpoint
    nsAutoMicroTask mt;
  }

  nsPIDOMWindowInner *pwin = master->GetInnerWindow();
  bool runScript = !!pwin;
  if (runScript) {
    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("beforescriptexecute"),
                                         true, true, &runScript);
  }

  // Inner window could have gone away after firing beforescriptexecute
  pwin = master->GetInnerWindow();
  if (!pwin) {
    runScript = false;
  }

  nsresult rv = NS_OK;
  if (runScript) {
    if (doc) {
      doc->BeginEvaluatingExternalScript();
    }
    rv = EvaluateScript(aRequest);
    if (doc) {
      doc->EndEvaluatingExternalScript();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("afterscriptexecute"),
                                         true, false);
  }

  FireScriptEvaluated(rv, aRequest);

  aRequest->Element()->EndEvaluating();

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  if (aRequest->mOffThreadToken) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks in the JS engine.
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    aRequest->MaybeCancelOffThreadScript();
  }

  // Free any source data.
  free(aRequest->mScriptTextBuf);
  aRequest->mScriptTextBuf = nullptr;
  aRequest->mScriptTextLength = 0;

  return rv;
}

void ScriptLoader::ProcessDynamicImport(ModuleLoadRequest* aRequest) {
  if (aRequest->mModuleScript) {
    if (!InstantiateModuleTree(aRequest)) {
      aRequest->mModuleScript = nullptr;
    }
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aRequest->mModuleScript) {
    rv = EvaluateScript(aRequest);
  }

  if (NS_FAILED(rv)) {
    FinishDynamicImport(aRequest, rv);
  }

  return;
}

void
ScriptLoader::FireScriptAvailable(nsresult aResult,
                                  ScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptAvailable(aResult, aRequest->Element(),
                         aRequest->mIsInline, aRequest->mURI,
                         aRequest->mLineNo);
  }

  aRequest->FireScriptAvailable(aResult);
}

void
ScriptLoader::FireScriptEvaluated(nsresult aResult,
                                  ScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->Element(),
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

already_AddRefed<nsIScriptGlobalObject>
ScriptLoader::GetScriptGlobalObject()
{
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  nsPIDOMWindowInner *pwin = master->GetInnerWindow();
  if (!pwin) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(pwin);
  NS_ASSERTION(globalObject, "windows must be global objects");

  // and make sure we are setup for this type of script.
  nsresult rv = globalObject->EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return globalObject.forget();
}

nsresult
ScriptLoader::FillCompileOptionsForRequest(
    const mozilla::dom::AutoJSAPI& jsapi, ScriptLoadRequest* aRequest,
    JS::Handle<JSObject*> aScopeChain, JS::CompileOptions* aOptions)
{
  // It's very important to use aRequest->mURI, not the final URI of the channel
  // aRequest ended up getting script data from, as the script filename.
  nsresult rv;
  nsContentUtils::GetWrapperSafeScriptFilename(mDocument, aRequest->mURI,
                                               aRequest->mURL, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isScriptElement = !aRequest->IsModuleRequest() ||
                         aRequest->AsModuleRequest()->IsTopLevel();
  aOptions->setIntroductionType(isScriptElement ? "scriptElement"
                                                : "importedModule");
  aOptions->setFileAndLine(aRequest->mURL.get(), aRequest->mLineNo);
  aOptions->setIsRunOnce(true);
  // We only need the setNoScriptRval bit when compiling off-thread here, since
  // otherwise nsJSUtils::EvaluateString will set it up for us.
  aOptions->setNoScriptRval(true);
  if (aRequest->mHasSourceMapURL) {
    aOptions->setSourceMapURL(aRequest->mSourceMapURL.get());
  }
  if (aRequest->mOriginPrincipal) {
    nsIPrincipal* scriptPrin = nsContentUtils::ObjectPrincipal(aScopeChain);
    bool subsumes = scriptPrin->Subsumes(aRequest->mOriginPrincipal);
    aOptions->setMutedErrors(!subsumes);
  }

  if (!aRequest->IsModuleRequest()) {
    // Only do this for classic scripts.
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> elementVal(cx);
    MOZ_ASSERT(aRequest->Element());
    if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, aRequest->Element(),
                                                &elementVal,
                                                /* aAllowWrapping = */ true))) {
      MOZ_ASSERT(elementVal.isObject());
      aOptions->setElement(&elementVal.toObject());
    }
  }

  return NS_OK;
}

class MOZ_RAII AutoSetProcessingScriptTag {
  nsCOMPtr<nsIScriptContext> mContext;
  bool mOldTag;

 public:
  explicit AutoSetProcessingScriptTag(nsIScriptContext* aContext)
      : mContext(aContext), mOldTag(mContext->GetProcessingScriptTag()) {
    mContext->SetProcessingScriptTag(true);
  }

  ~AutoSetProcessingScriptTag() { mContext->SetProcessingScriptTag(mOldTag); }
};

nsresult
ScriptLoader::EvaluateScript(ScriptLoadRequest* aRequest)
{
  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  bool isDynamicImport = aRequest->IsModuleRequest() &&
                         aRequest->AsModuleRequest()->IsDynamicImport();
  if (!isDynamicImport) {
    nsCOMPtr<nsIContent> scriptContent(do_QueryInterface(aRequest->Element()));
    MOZ_ASSERT(scriptContent);
    nsIDocument* ownerDoc = scriptContent->OwnerDoc();
    if (ownerDoc != mDocument) {
      // Willful violation of HTML5 as of 2010-12-01
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  // Make sure context is a strong reference since we access it after
  // we've executed a script, which may cause all other references to
  // the context to go away.
  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  // New script entry point required, due to the "Create a script" sub-step of
  // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block
  nsAutoMicroTask mt;
  AutoEntryScript aes(globalObject, "<script> element", true);
  JSContext* cx = aes.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

  AutoSetProcessingScriptTag setProcessingScriptTag(context);
  nsresult rv;
  {
    Maybe<AutoCurrentScriptUpdater> masterScriptUpdater;
    nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
    if (master != mDocument) {
      // If this script belongs to an import document, it will be
      // executed in the context of the master document. During the
      // execution currentScript of the master should refer to this
      // script. So let's update the mCurrentScript of the ScriptLoader
      // of the master document too.
      masterScriptUpdater.emplace(master->ScriptLoader(),
                                  aRequest->Element());
    }

    if (aRequest->IsModuleRequest()) {
      // For modules, currentScript is set to null.
      AutoCurrentScriptUpdater scriptUpdater(this, nullptr);

      ModuleLoadRequest* request = aRequest->AsModuleRequest();
      MOZ_ASSERT(request->mModuleScript);
      MOZ_ASSERT(!request->mOffThreadToken);

      ModuleScript* moduleScript = request->mModuleScript;
      if (moduleScript->HasErrorToRethrow()) {
        // Module has an error status to be rethrown
        JS::Rooted<JS::Value> error(cx, moduleScript->ErrorToRethrow());
        JS_SetPendingException(cx, error);

        // For a dynamic import, the promise is rejected.  Otherwise an error is
        // either reported by AutoEntryScript.
        if (request->IsDynamicImport()) {
          FinishDynamicImport(cx, request, NS_OK);
        }
        return NS_OK;
      }

      JS::Rooted<JSObject*> module(cx, moduleScript->ModuleRecord());
      MOZ_ASSERT(module);

      rv = nsJSUtils::ModuleEvaluate(cx, module);
      MOZ_ASSERT(NS_FAILED(rv) == aes.HasException());
      if (NS_FAILED(rv)) {
        // For a dynamic import, the promise is rejected.  Otherwise an error is
        // either reported by AutoEntryScript.
        rv = NS_OK;
      }

      if (request->IsDynamicImport()) {
        FinishDynamicImport(cx, request, rv);
      }
    } else {
      // Update our current script.
      AutoCurrentScriptUpdater scriptUpdater(this, aRequest->Element());

      JS::CompileOptions options(cx);
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        {
          nsJSUtils::ExecutionContext exec(cx, global);
          if (aRequest->mOffThreadToken) {
            rv = exec.JoinCompile(&aRequest->mOffThreadToken);
          } else {
            nsAutoString inlineData;
            SourceBufferHolder srcBuf = GetScriptSource(aRequest, inlineData);
            rv = exec.Compile(options, srcBuf);
          }
          if (rv == NS_OK) {
             JS::Rooted<JSScript*> script(cx);
             script = exec.GetScript();

             // With scripts disabled GetScript() will return nullptr
             if (script) {
                 // Create a ClassicScript object and associate it with the
                 // JSScript.
                 RefPtr<ClassicScript> classicScript = new ClassicScript(
                     aRequest->mFetchOptions, aRequest->mBaseURL);
                 classicScript->AssociateWithScript(script);
             }

             rv = exec.ExecScript();
          }
        }
      }
    }
  }

  return rv;
}

/* static */ LoadedScript* ScriptLoader::GetActiveScript(JSContext* aCx) {
  JS::Value value = JS::GetScriptedCallerPrivate(aCx);
  if (value.isUndefined()) {
    return nullptr;
  }

  return static_cast<LoadedScript*>(value.toPrivate());
}

void
ScriptLoader::ProcessPendingRequestsAsync()
{
  if (mParserBlockingRequest ||
      !mXSLTRequests.isEmpty() ||
      !mLoadedAsyncRequests.isEmpty() ||
      !mNonAsyncExternalScriptInsertedRequests.isEmpty() ||
      !mDeferRequests.isEmpty() ||
      !mDynamicImportRequests.isEmpty() ||
      !mPendingChildLoaders.IsEmpty()) {
    NS_DispatchToCurrentThread(NewRunnableMethod(this,
                                                 &ScriptLoader::ProcessPendingRequests));
  }
}

void
ScriptLoader::ProcessPendingRequests()
{
  RefPtr<ScriptLoadRequest> request;

  if (mParserBlockingRequest &&
      mParserBlockingRequest->IsReadyToRun() &&
      ReadyToExecuteParserBlockingScripts()) {
    request.swap(mParserBlockingRequest);
    UnblockParser(request);
    ProcessRequest(request);
    if (request->mWasCompiledOMT) {
      mDocument->UnblockOnload(false);
    }
    ContinueParserAsync(request);
  }

  while (ReadyToExecuteParserBlockingScripts() &&
         !mXSLTRequests.isEmpty() &&
         mXSLTRequests.getFirst()->IsReadyToRun()) {
    request = mXSLTRequests.StealFirst();
    ProcessRequest(request);
  }

  while (ReadyToExecuteScripts() && !mLoadedAsyncRequests.isEmpty()) {
    request = mLoadedAsyncRequests.StealFirst();
    if (request->IsModuleRequest()) {
      ProcessRequest(request);
    } else {
      CompileOffThreadOrProcessRequest(request);
    }
  }

  while (ReadyToExecuteScripts() &&
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
         mNonAsyncExternalScriptInsertedRequests.getFirst()->IsReadyToRun()) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request = mNonAsyncExternalScriptInsertedRequests.StealFirst();
    ProcessRequest(request);
  }

  if (mDocumentParsingDone && mXSLTRequests.isEmpty()) {
    while (ReadyToExecuteScripts() &&
           !mDeferRequests.isEmpty() &&
           mDeferRequests.getFirst()->IsReadyToRun()) {
      request = mDeferRequests.StealFirst();
      ProcessRequest(request);
    }
  }

  while (!mPendingChildLoaders.IsEmpty() &&
         ReadyToExecuteParserBlockingScripts()) {
    RefPtr<ScriptLoader> child = mPendingChildLoaders[0];
    mPendingChildLoaders.RemoveElementAt(0);
    child->RemoveParserBlockingScriptExecutionBlocker();
  }

  if (mDocumentParsingDone && mDocument && !mParserBlockingRequest &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty() &&
      MaybeRemovedDeferRequests()) {
    return ProcessPendingRequests();
  }

  if (mDocumentParsingDone && mDocument &&
      !mParserBlockingRequest && mLoadingAsyncRequests.isEmpty() &&
      mLoadedAsyncRequests.isEmpty() &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty()) {
    // No more pending scripts; time to unblock onload.
    // OK to unblock onload synchronously here, since callers must be
    // prepared for the world changing anyway.
    mDocumentParsingDone = false;
    mDocument->UnblockOnload(true);
  }
}

bool
ScriptLoader::ReadyToExecuteParserBlockingScripts()
{
  // Make sure the SelfReadyToExecuteParserBlockingScripts check is first, so
  // that we don't block twice on an ancestor.
  if (!SelfReadyToExecuteParserBlockingScripts()) {
    return false;
  }

  for (nsIDocument* doc = mDocument; doc; doc = doc->GetParentDocument()) {
    ScriptLoader* ancestor = doc->ScriptLoader();
    if (!ancestor->SelfReadyToExecuteParserBlockingScripts() &&
        ancestor->AddPendingChildLoader(this)) {
      AddParserBlockingScriptExecutionBlocker();
      return false;
    }
  }

  if (mDocument && !mDocument->IsMasterDocument()) {
    RefPtr<ImportManager> im = mDocument->ImportManager();
    RefPtr<ImportLoader> loader = im->Find(mDocument);
    MOZ_ASSERT(loader, "How can we have an import document without a loader?");

    // The referring link that counts in the execution order calculation
    // (in spec: flagged as branch)
    nsCOMPtr<nsINode> referrer = loader->GetMainReferrer();
    MOZ_ASSERT(referrer, "There has to be a main referring link for each imports");

    // Import documents are blocked by their import predecessors. We need to
    // wait with script execution until all the predecessors are done.
    // Technically it means we have to wait for the last one to finish,
    // which is the neares one to us in the order.
    RefPtr<ImportLoader> lastPred = im->GetNearestPredecessor(referrer);
    if (!lastPred) {
      // If there is no predecessor we can run.
      return true;
    }

    nsCOMPtr<nsIDocument> doc = lastPred->GetDocument();
    if (lastPred->IsBlocking() || !doc ||
        !doc->ScriptLoader()->SelfReadyToExecuteParserBlockingScripts()) {
      // Document has not been created yet or it was created but not ready.
      // Either case we are blocked by it. The ImportLoader will take care
      // of blocking us, and adding the pending child loader to the blocking
      // ScriptLoader when it's possible (at this point the blocking loader
      // might not have created the document/ScriptLoader)
      lastPred->AddBlockedScriptLoader(this);
      // As more imports are parsed, this can change, let's cache what we
      // blocked, so it can be later updated if needed (see: ImportLoader::Updater).
      loader->SetBlockingPredecessor(lastPred);
      return false;
    }
  }

  return true;
}

/* static */ nsresult
ScriptLoader::ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                             uint32_t aLength, const nsAString& aHintCharset,
                             nsIDocument* aDocument,
                             char16_t*& aBufOut, size_t& aLengthOut)
{
  if (!aLength) {
    aBufOut = nullptr;
    aLengthOut = 0;
    return NS_OK;
  }

  // The encoding info precedence is as follows from high to low:
  // The BOM
  // HTTP Content-Type (if name recognized)
  // charset attribute (if name recognized)
  // The encoding of the document

  nsAutoCString charset;

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;

  if (nsContentUtils::CheckForBOM(aData, aLength, charset)) {
    // charset is now one of "UTF-16BE", "UTF-16BE" or "UTF-8".  Those decoder
    // will take care of swallowing the BOM.
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      aChannel &&
      NS_SUCCEEDED(aChannel->GetContentCharset(charset)) &&
      EncodingUtils::FindEncodingForLabel(charset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      EncodingUtils::FindEncodingForLabel(aHintCharset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder && aDocument) {
    charset = aDocument->GetDocumentCharacterSet();
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder) {
    // Curiously, there are various callers that don't pass aDocument. The
    // fallback in the old code was ISO-8859-1, which behaved like
    // windows-1252. Saying windows-1252 for clarity and for compliance
    // with the Encoding Standard.
    unicodeDecoder = EncodingUtils::DecoderForEncoding("windows-1252");
  }

  int32_t unicodeLength = 0;

  nsresult rv =
    unicodeDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                                 aLength, &unicodeLength);
  NS_ENSURE_SUCCESS(rv, rv);

  aBufOut = static_cast<char16_t*>(js_malloc(unicodeLength * sizeof(char16_t)));
  if (!aBufOut) {
    aLengthOut = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aLengthOut = unicodeLength;

  rv = unicodeDecoder->Convert(reinterpret_cast<const char*>(aData),
                               (int32_t *) &aLength, aBufOut,
                               &unicodeLength);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  aLengthOut = unicodeLength;
  if (NS_FAILED(rv)) {
    js_free(aBufOut);
    aBufOut = nullptr;
    aLengthOut = 0;
  }
  return rv;
}

nsresult
ScriptLoader::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                               nsISupports* aContext,
                               nsresult aChannelStatus,
                               nsresult aSRIStatus,
                               mozilla::Vector<char16_t> &aString,
                               SRICheckDataVerifier* aSRIDataVerifier)
{
  ScriptLoadRequest* request = static_cast<ScriptLoadRequest*>(aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsresult rv = VerifySRI(request, aLoader, aSRIStatus, aSRIDataVerifier);

  if (NS_SUCCEEDED(rv)) {
    rv = PrepareLoadedRequest(request, aLoader, aChannelStatus, aString);
  }

  if (NS_FAILED(rv)) {
    HandleLoadError(request, rv);
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return rv;
}

nsresult
ScriptLoader::VerifySRI(ScriptLoadRequest* aRequest,
                        nsIIncrementalStreamLoader* aLoader,
                        nsresult aSRIStatus,
                        SRICheckDataVerifier* aSRIDataVerifier) const
{
  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(channelRequest);

  nsresult rv = NS_OK;
  if (!aRequest->mIntegrity.IsEmpty() &&
      NS_SUCCEEDED((rv = aSRIStatus))) {
    MOZ_ASSERT(aSRIDataVerifier);
    MOZ_ASSERT(mReporter);

    nsAutoCString sourceUri;
    if (mDocument && mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    rv = aSRIDataVerifier->Verify(aRequest->mIntegrity, channel, sourceUri,
                                  mReporter);
    mReporter->FlushConsoleReports(mDocument);
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_SRI_CORRUPT;
    }
  } else {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

    if (loadInfo->GetEnforceSRI()) {
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
             ("ScriptLoader::OnStreamComplete, required SRI not found"));
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      loadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(csp));
      nsAutoCString violationURISpec;
      mDocument->GetDocumentURI()->GetAsciiSpec(violationURISpec);
      uint32_t lineNo = aRequest->Element() ? aRequest->Element()->GetScriptLineNumber() : 0;
      uint32_t columnNo = aRequest->Element() ? aRequest->Element()->GetScriptColumnNumber() : 0;
      csp->LogViolationDetails(
        nsIContentSecurityPolicy::VIOLATION_TYPE_REQUIRE_SRI_FOR_SCRIPT,
        NS_ConvertUTF8toUTF16(violationURISpec),
        EmptyString(), lineNo, columnNo, EmptyString(), EmptyString());
      rv = NS_ERROR_SRI_CORRUPT;
    }
  }
  
  return rv;
}

void
ScriptLoader::HandleLoadError(ScriptLoadRequest *aRequest, nsresult aResult) {
  /*
   * Handle script not loading error because source was a tracking URL.
   * We make a note of this script node by including it in a dedicated
   * array of blocked tracking nodes under its parent document.
   */
  if (aResult == NS_ERROR_TRACKING_URI) {
    nsCOMPtr<nsIContent> cont = do_QueryInterface(aRequest->mElement);
    mDocument->AddBlockedTrackingNode(cont);
  }

  if (aRequest->IsModuleRequest() && !aRequest->mIsInline) {
    auto request = aRequest->AsModuleRequest();
    SetModuleFetchFinishedAndResumeWaitingRequests(request, aResult);
  }

  if (aRequest->mInDeferList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mDeferRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mInAsyncList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mIsNonAsyncScriptInserted) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req =
        mNonAsyncExternalScriptInsertedRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mIsXSLT) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mXSLTRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->IsPreload()) {
    if (aRequest->IsModuleRequest()) {
      aRequest->Cancel();
    }
    if (aRequest->IsTopLevel()) {
      MOZ_ALWAYS_TRUE(
          mPreloads.RemoveElement(aRequest, PreloadRequestComparator()));
    }
    MOZ_ASSERT(!aRequest->isInList());
  } else if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* modReq = aRequest->AsModuleRequest();
    if (modReq->IsDynamicImport()) {
      MOZ_ASSERT(modReq->IsTopLevel());
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
        modReq->Cancel();
        // FinishDynamicImport must happen exactly once for each dynamic import
        // request. If the load is aborted we do it when we remove the request
        // from mDynamicImportRequests.
        FinishDynamicImport(modReq, aResult);
      }
    } else {
      MOZ_ASSERT(!modReq->IsTopLevel());
      MOZ_ASSERT(!modReq->isInList());
      modReq->Cancel();
      // The error is handled for the top level module.
    }
  } else if (mParserBlockingRequest == aRequest) {
    MOZ_ASSERT(!aRequest->isInList());
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);

    // Ensure that we treat request->mElement as our current parser-inserted
    // script while firing onerror on it.
    MOZ_ASSERT(aRequest->Element()->GetParserCreated());
    nsCOMPtr<nsIScriptElement> oldParserInsertedScript =
      mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->Element();
    FireScriptAvailable(aResult, aRequest);
    ContinueParserAsync(aRequest);
    mCurrentParserInsertedScript = oldParserInsertedScript;
  } else {
    // This happens for blocking requests cancelled by ParsingComplete().
    MOZ_ASSERT(aRequest->IsCanceled());
    MOZ_ASSERT(!aRequest->isInList());
  }
}


void
ScriptLoader::UnblockParser(ScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->Element()->UnblockParser();
}

void
ScriptLoader::ContinueParserAsync(ScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->Element()->ContinueParserAsync();
}

uint32_t
ScriptLoader::NumberOfProcessors()
{
  if (mNumberOfProcessors > 0)
    return mNumberOfProcessors;

  int32_t numProcs = PR_GetNumberOfProcessors();
  if (numProcs > 0)
    mNumberOfProcessors = numProcs;
  return mNumberOfProcessors;
}

static bool
IsInternalURIScheme(nsIURI* uri)
{
  // Note: Extend this if other schemes need to be included.
  bool isResource;
  if (NS_SUCCEEDED(uri->SchemeIs("resource", &isResource)) && isResource) {
    return true;
  }

  return false;
}

nsresult
ScriptLoader::PrepareLoadedRequest(ScriptLoadRequest* aRequest,
                                   nsIIncrementalStreamLoader* aLoader,
                                   nsresult aStatus,
                                   mozilla::Vector<char16_t> &aString)
{
  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  if (aRequest->IsCanceled()) {
    return NS_BINDING_ABORTED;
  }

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the load returned an error page, then we need to abort
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(req);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsAutoCString sourceMapURL;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("SourceMap"), sourceMapURL);
    if (NS_FAILED(rv)) {
      rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-SourceMap"), sourceMapURL);
    }
    if (NS_SUCCEEDED(rv)) {
      aRequest->mHasSourceMapURL = true;
      aRequest->mSourceMapURL = NS_ConvertUTF8toUTF16(sourceMapURL);
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  // If this load was subject to a CORS check, don't flag it with a separate
  // origin principal, so that it will treat our document's principal as the
  // origin principal.  Module loads always use CORS.
  if (!aRequest->IsModuleRequest() && aRequest->CORSMode() == CORS_NONE) {
    rv = nsContentUtils::GetSecurityManager()->
      GetChannelResultPrincipal(channel, getter_AddRefs(aRequest->mOriginPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aString.empty()) {
    aRequest->mScriptTextLength = aString.length();
    aRequest->mScriptTextBuf = aString.extractOrCopyRawBuffer();
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mDeferRequests.Contains(aRequest) ||
               mLoadingAsyncRequests.Contains(aRequest) ||
               mNonAsyncExternalScriptInsertedRequests.Contains(aRequest) ||
               mXSLTRequests.Contains(aRequest)  ||
               mDynamicImportRequests.Contains(aRequest) ||
               (aRequest->IsModuleRequest() &&
                !aRequest->AsModuleRequest()->IsTopLevel() &&
                !aRequest->isInList()) ||
               mPreloads.Contains(aRequest, PreloadRequestComparator()) ||
               mParserBlockingRequest == aRequest,
               "aRequest should be pending!");

  nsCOMPtr<nsIURI> uri;
  rv = channel->GetOriginalURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Fixup moz-extension: and resource: URIs, because the channel URI will
  // point to file:, which won't be allowed to load.
  if (uri && IsInternalURIScheme(uri)) {
    aRequest->mBaseURL = uri;
  } else {
    channel->GetURI(getter_AddRefs(aRequest->mBaseURL));
  }

  if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* request = aRequest->AsModuleRequest();

    // When loading a module, only responses with a JavaScript MIME type are
    // acceptable.
    nsAutoCString mimeType;
    channel->GetContentType(mimeType);
    NS_ConvertUTF8toUTF16 typeString(mimeType);
    if (!nsContentUtils::IsJavascriptMIMEType(typeString)) {
      return NS_ERROR_FAILURE;
    }

    // Attempt to compile off main thread.
    rv = AttemptAsyncScriptCompile(request);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }

    // Otherwise compile it right away and start fetching descendents.
    return ProcessFetchedModuleSource(request);
  }

  // The script is now loaded and ready to run.
  aRequest->SetReady();

  // If this is currently blocking the parser, attempt to compile it off-main-thread.
  if (aRequest == mParserBlockingRequest && (NumberOfProcessors() > 1)) {
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    nsresult rv = AttemptAsyncScriptCompile(aRequest);
    if (rv == NS_OK) {
      MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::Compiling,
                 "Request should be off-thread compiling now.");
      return NS_OK;
    }

    // If off-thread compile errored, return the error.
    if (rv != NS_ERROR_FAILURE) {
      return rv;
    }

    // If off-thread compile was rejected, continue with regular processing.
  }

  MaybeMoveToLoadedList(aRequest);

  return NS_OK;
}

void
ScriptLoader::ParsingComplete(bool aTerminated)
{
  if (mDeferEnabled) {
    // Have to check because we apparently get ParsingComplete
    // without BeginDeferringScripts in some cases
    mDocumentParsingDone = true;
  }
  mDeferEnabled = false;
  if (aTerminated) {
    mDeferRequests.Clear();
    mLoadingAsyncRequests.Clear();
    mLoadedAsyncRequests.Clear();
    mNonAsyncExternalScriptInsertedRequests.Clear();
    mXSLTRequests.Clear();

    for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
         req = req->getNext()) {
      req->Cancel();
      // FinishDynamicImport must happen exactly once for each dynamic import
      // request. If the load is aborted we do it when we remove the request
      // from mDynamicImportRequests.
      FinishDynamicImport(req->AsModuleRequest(), NS_ERROR_ABORT);
    }
    mDynamicImportRequests.Clear();

    if (mParserBlockingRequest) {
      mParserBlockingRequest->Cancel();
      mParserBlockingRequest = nullptr;
    }
  }

  // Have to call this even if aTerminated so we'll correctly unblock
  // onload and all.
  ProcessPendingRequests();
}

void
ScriptLoader::PreloadURI(nsIURI *aURI,
                         const nsAString &aCharset,
                         const nsAString &aType,
                         const nsAString &aCrossOrigin,
                         const nsAString& aIntegrity,
                         bool aScriptFromHead,
                         bool aAsync,
                         bool aDefer,
                         bool aNoModule,
                         const mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  NS_ENSURE_TRUE_VOID(mDocument);
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  ScriptKind scriptKind = ScriptKind::Classic;
  
  if (mDocument->ModuleScriptsEnabled()) {
    // Don't load nomodule scripts.
    if (aNoModule) {
      return;
    }

    // Preload module scripts.
    static const char kASCIIWhitespace[] = "\t\n\f\r ";

    nsAutoString type(aType);
    type.Trim(kASCIIWhitespace);
    if (type.LowerCaseEqualsASCII("module")) {
      scriptKind = ScriptKind::Module;
    }
  }

  if (scriptKind == ScriptKind::Classic &&
      !aType.IsEmpty() && !nsContentUtils::IsJavascriptMIMEType(aType)) {
    // Unknown type (not type = module and not type = JS MIME type).
    // Don't load it.
    return;
  }

  SRIMetadata sriMetadata;
  if (!aIntegrity.IsEmpty()) {
    MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
           ("ScriptLoader::PreloadURI, integrity=%s",
            NS_ConvertUTF16toUTF8(aIntegrity).get()));
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    SRICheck::IntegrityMetadata(aIntegrity, sourceUri, mReporter, &sriMetadata);
  }

  RefPtr<ScriptLoadRequest> request =
    CreateLoadRequest(scriptKind, aURI, nullptr,
                      mDocument->NodePrincipal(),
                      Element::StringToCORSMode(aCrossOrigin), sriMetadata,
                      aReferrerPolicy);
  request->mIsInline = false;
  request->SetScriptMode(aDefer, aAsync);
  request->SetIsPreloadRequest();

  nsresult rv = StartLoad(request, aType, aScriptFromHead);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo *pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void
ScriptLoader::AddDeferRequest(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsDeferredScript());
  MOZ_ASSERT(!aRequest->mInDeferList && !aRequest->mInAsyncList);

  aRequest->mInDeferList = true;
  mDeferRequests.AppendElement(aRequest);
  if (mDeferEnabled && aRequest == mDeferRequests.getFirst() &&
      mDocument && !mBlockingDOMContentLoaded) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == nsIDocument::READYSTATE_LOADING);
    mBlockingDOMContentLoaded = true;
    mDocument->BlockDOMContentLoaded();
  }
}

void
ScriptLoader::AddAsyncRequest(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsAsyncScript());
  MOZ_ASSERT(!aRequest->mInDeferList && !aRequest->mInAsyncList);

  aRequest->mInAsyncList = true;
  if (aRequest->IsReadyToRun()) {
    mLoadedAsyncRequests.AppendElement(aRequest);
  } else {
    mLoadingAsyncRequests.AppendElement(aRequest);
  }
}

void
ScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // If it's async, move it to the loaded list.  aRequest->mInAsyncList really
  // _should_ be in a list, but the consequences if it's not are bad enough we
  // want to avoid trying to move it if it's not.
  if (aRequest->mInAsyncList) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  }
}

bool
ScriptLoader::MaybeRemovedDeferRequests()
{
  if (mDeferRequests.isEmpty() && mDocument &&
      mBlockingDOMContentLoaded) {
    mBlockingDOMContentLoaded = false;
    mDocument->UnblockDOMContentLoaded();
    return true;
  }
  return false;
}

} // dom namespace
} // mozilla namespace
