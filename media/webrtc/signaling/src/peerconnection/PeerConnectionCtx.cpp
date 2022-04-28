/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"
#include "runnable_utils.h"
#include "prcvar.h"

#include "browser_logging/WebRtcLog.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/Preferences.h"
#include <mozilla/Types.h>
#endif

#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"

#include "gmp-video-decode.h" // GMP_API_VIDEO_DECODER
#include "gmp-video-encode.h" // GMP_API_VIDEO_ENCODER

static const char* logTag = "PeerConnectionCtx";

namespace mozilla {

using namespace dom;

class PeerConnectionCtxShutdown : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxShutdown() {}

  void Init()
    {
      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (!observerService)
        return;

      nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
      rv = observerService->AddObserver(this,
                                        NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                        false);
      MOZ_ALWAYS_SUCCEEDS(rv);
#endif
      (void) rv;
    }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(logTag, "Shutting down PeerConnectionCtx");
      PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (!observerService)
        return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(this,
                                                    NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      MOZ_ALWAYS_SUCCEEDS(rv);

      // Make sure we're not deleted while still inside ::Observe()
      RefPtr<PeerConnectionCtxShutdown> kungFuDeathGrip(this);
      PeerConnectionCtx::gPeerConnectionCtxShutdown = nullptr;
    }
    return NS_OK;
  }

private:
  virtual ~PeerConnectionCtxShutdown()
    {
      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (observerService)
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
};

NS_IMPL_ISUPPORTS(PeerConnectionCtxShutdown, nsIObserver);
}

namespace mozilla {

PeerConnectionCtx* PeerConnectionCtx::gInstance;
nsIThread* PeerConnectionCtx::gMainThread;
StaticRefPtr<PeerConnectionCtxShutdown> PeerConnectionCtx::gPeerConnectionCtxShutdown;

const std::map<const std::string, PeerConnectionImpl *>&
PeerConnectionCtx::mGetPeerConnections()
{
  return mPeerConnections;
}

nsresult PeerConnectionCtx::InitializeGlobal(nsIThread *mainThread,
  nsIEventTarget* stsThread) {
  if (!gMainThread) {
    gMainThread = mainThread;
  } else {
    MOZ_ASSERT(gMainThread == mainThread);
  }

  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance) {
    CSFLogDebug(logTag, "Creating PeerConnectionCtx");
    PeerConnectionCtx *ctx = new PeerConnectionCtx();

    res = ctx->Initialize();
    PR_ASSERT(NS_SUCCEEDED(res));
    if (!NS_SUCCEEDED(res))
      return res;

    gInstance = ctx;

    if (!PeerConnectionCtx::gPeerConnectionCtxShutdown) {
      PeerConnectionCtx::gPeerConnectionCtxShutdown = new PeerConnectionCtxShutdown();
      PeerConnectionCtx::gPeerConnectionCtxShutdown->Init();
    }
  }

  EnableWebRtcLog();
  return NS_OK;
}

PeerConnectionCtx* PeerConnectionCtx::GetInstance() {
  MOZ_ASSERT(gInstance);
  return gInstance;
}

bool PeerConnectionCtx::isActive() {
  return gInstance;
}

void PeerConnectionCtx::Destroy() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  if (gInstance) {
    gInstance->Cleanup();
    delete gInstance;
    gInstance = nullptr;
  }

  StopWebRtcLog();
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;

// The threading model around the media pipelines is weird:
// - The pipelines are containers,
// - containers that are only safe on main thread, with members only safe on STS,
// - hence the there and back again approach.

static auto
FindId(const Sequence<RTCInboundRTPStreamStats>& aArray,
       const nsString &aId) -> decltype(aArray.Length()) {
  for (decltype(aArray.Length()) i = 0; i < aArray.Length(); i++) {
    if (aArray[i].mId.Value() == aId) {
      return i;
    }
  }
  return aArray.NoIndex;
}

static auto
FindId(const nsTArray<nsAutoPtr<RTCStatsReportInternal>>& aArray,
       const nsString &aId) -> decltype(aArray.Length()) {
  for (decltype(aArray.Length()) i = 0; i < aArray.Length(); i++) {
    if (aArray[i]->mPcid == aId) {
      return i;
    }
  }
  return aArray.NoIndex;
}

static void
FreeOnMain_m(nsAutoPtr<RTCStatsQueries> aQueryList) {
  MOZ_ASSERT(NS_IsMainThread());
}

#endif

nsresult PeerConnectionCtx::Initialize() {
  initGMP();

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (XRE_IsContentProcess()) {
    WebrtcGlobalChild::Create();
  }
#endif // MOZILLA_INTERNAL_API

  return NS_OK;
}

static void GMPReady_m() {
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->onGMPReady();
  }
};

static void GMPReady() {
  PeerConnectionCtx::gMainThread->Dispatch(WrapRunnableNM(&GMPReady_m),
                                           NS_DISPATCH_NORMAL);
};

void PeerConnectionCtx::initGMP()
{
  mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");

  if (!mGMPService) {
    CSFLogError(logTag, "%s failed to get the gecko-media-plugin-service",
                __FUNCTION__);
    return;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = mGMPService->GetThread(getter_AddRefs(thread));

  if (NS_FAILED(rv)) {
    mGMPService = nullptr;
    CSFLogError(logTag,
                "%s failed to get the gecko-media-plugin thread, err=%u",
                __FUNCTION__,
                static_cast<unsigned>(rv));
    return;
  }

  // presumes that all GMP dir scans have been queued for the GMPThread
  thread->Dispatch(WrapRunnableNM(&GMPReady), NS_DISPATCH_NORMAL);
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mQueuedJSEPOperations.Clear();
  mGMPService = nullptr;
  return NS_OK;
}

PeerConnectionCtx::~PeerConnectionCtx() {
};

void PeerConnectionCtx::queueJSEPOperation(nsIRunnable* aOperation) {
  mQueuedJSEPOperations.AppendElement(aOperation);
}

void PeerConnectionCtx::onGMPReady() {
  mGMPReady = true;
  for (size_t i = 0; i < mQueuedJSEPOperations.Length(); ++i) {
    mQueuedJSEPOperations[i]->Run();
  }
  mQueuedJSEPOperations.Clear();
}

bool PeerConnectionCtx::gmpHasH264() {
  if (!mGMPService) {
    return false;
  }

  // XXX I'd prefer if this was all known ahead of time...

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));

  bool has_gmp;
  nsresult rv;
  rv = mGMPService->HasPluginForAPI(NS_LITERAL_CSTRING(GMP_API_VIDEO_ENCODER),
                                    &tags,
                                    &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  rv = mGMPService->HasPluginForAPI(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                                    &tags,
                                    &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  return true;
}

} // namespace mozilla
