/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleLoadRequest.h"

#include "mozilla/HoldDropJSObjects.h"

#include "ModuleScript.h"
#include "ScriptLoader.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleLoadRequest)
NS_INTERFACE_MAP_END_INHERITING(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ModuleLoadRequest,
                                                ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoader, mModuleScript, mImports)
  tmp->ClearDynamicImport();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ModuleLoadRequest,
                                                  ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader, mModuleScript, mImports)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ModuleLoadRequest,
                                               ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicReferencingPrivate)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicSpecifier)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicPromise)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
                                                   
NS_IMPL_ADDREF_INHERITED(ModuleLoadRequest, ScriptLoadRequest)
NS_IMPL_RELEASE_INHERITED(ModuleLoadRequest, ScriptLoadRequest)

static VisitedURLSet* NewVisitedSetForTopLevelImport(nsIURI* aURI) {
  auto set = new VisitedURLSet();
  set->PutEntry(aURI);
  return set;
}

/* static */ ModuleLoadRequest* ModuleLoadRequest::CreateTopLevel(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions,
    const SRIMetadata& aIntegrity, nsIURI* aReferrer, ScriptLoader* aLoader) {
  return new ModuleLoadRequest(aURI, aFetchOptions, aIntegrity, aReferrer,
                               true,  /* is top level */
                               false, /* is dynamic import */
                               aLoader, NewVisitedSetForTopLevelImport(aURI));
}

/* static */ ModuleLoadRequest* ModuleLoadRequest::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  auto request =
      new ModuleLoadRequest(aURI, aParent->mFetchOptions, SRIMetadata(),
                            aParent->mURI, false, /* is top level */
                            false,                /* is dynamic import */
                            aParent->mLoader, aParent->mVisitedSet);

  request->mIsInline = false;
  request->mScriptMode = aParent->mScriptMode;

  return request;
}

/* static */ ModuleLoadRequest* ModuleLoadRequest::CreateDynamicImport(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL,
    ScriptLoader* aLoader, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSString*> aSpecifier, JS::Handle<JSObject*> aPromise) 
{
  MOZ_ASSERT(aSpecifier);
  MOZ_ASSERT(aPromise);

  auto request = new ModuleLoadRequest(
      aURI, aFetchOptions, SRIMetadata(), aBaseURL,
      true, /* is top level */
      true, /* is dynamic import */
      aLoader, NewVisitedSetForTopLevelImport(aURI));

  request->mIsInline = false;
  request->mScriptMode = ScriptMode::eAsync;
  request->mDynamicReferencingPrivate = aReferencingPrivate;
  request->mDynamicSpecifier = aSpecifier;
  request->mDynamicPromise = aPromise;

  HoldJSObjects(request);

  return request;
}
 
ModuleLoadRequest::ModuleLoadRequest(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions,
    const SRIMetadata& aIntegrity, nsIURI* aReferrer, bool aIsTopLevel,
    bool aIsDynamicImport, ScriptLoader* aLoader, VisitedURLSet* aVisitedSet)
    : ScriptLoadRequest(ScriptKind::Module, aURI, aFetchOptions, aIntegrity,
                        aReferrer),
      mIsTopLevel(aIsTopLevel),
      mIsDynamicImport(aIsDynamicImport),
      mLoader(aLoader),
      mVisitedSet(aVisitedSet) {}

void ModuleLoadRequest::Cancel()
{
  ScriptLoadRequest::Cancel();
  mModuleScript = nullptr;
  mProgress = ScriptLoadRequest::Progress::Ready;
  CancelImports();
  mReady.RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
}

void
ModuleLoadRequest::CancelImports()
{
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
}

void
ModuleLoadRequest::SetReady()
{
  // Mark a module as ready to execute. This means that this module and all it
  // dependencies have had their source loaded, parsed as a module and the
  // modules instantiated.
  //
  // The mReady promise is used to ensure that when all dependencies of a module
  // have become ready, DependenciesLoaded is called on that module
  // request. This is set up in StartFetchingModuleDependencies.

#ifdef DEBUG
  for (size_t i = 0; i < mImports.Length(); i++) {
    MOZ_ASSERT(mImports[i]->IsReadyToRun());
  }
#endif

  ScriptLoadRequest::SetReady();
  mReady.ResolveIfExists(true, __func__);
}

void
ModuleLoadRequest::ModuleLoaded()
{
  // A module that was found to be marked as fetching in the module map has now
  // been loaded.

  mModuleScript = mLoader->GetFetchedModule(mURI);
  if (!mModuleScript || mModuleScript->HasParseError()) {
    ModuleErrored();
    return;
  }

  mLoader->StartFetchingModuleDependencies(this);
}

void
ModuleLoadRequest::ModuleErrored()
{
  mLoader->CheckModuleDependenciesLoaded(this);
  MOZ_ASSERT(!mModuleScript || mModuleScript->HasParseError());

  CancelImports();
  SetReady();
  LoadFinished();
}

void
ModuleLoadRequest::DependenciesLoaded()
{
  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  MOZ_ASSERT(mModuleScript);

  mLoader->CheckModuleDependenciesLoaded(this);
  SetReady();
  LoadFinished();
}

void
ModuleLoadRequest::LoadFailed()
{
  // We failed to load the source text or an error occurred unrelated to the
  // content of the module (e.g. OOM).

  if (IsCanceled()) {
    return;
  }

  MOZ_ASSERT(!IsReadyToRun());
  MOZ_ASSERT(!mModuleScript);

  Cancel();
  LoadFinished();
}

void
ModuleLoadRequest::LoadFinished()
{
  mLoader->ProcessLoadedModuleTree(this);
  mLoader = nullptr;
}

void ModuleLoadRequest::ClearDynamicImport() {
  mDynamicReferencingPrivate = JS::UndefinedValue();
  mDynamicSpecifier = nullptr;
  mDynamicPromise = nullptr;
}

} // dom namespace
} // mozilla namespace
