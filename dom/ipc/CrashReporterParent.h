/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CrashReporterParent_h
#define mozilla_dom_CrashReporterParent_h

#include "mozilla/dom/PCrashReporterParent.h"
#include "mozilla/dom/TabMessageUtils.h"
#include "nsIFile.h"

namespace mozilla {
namespace dom {

class CrashReporterParent : public PCrashReporterParent
{
public:
  CrashReporterParent();
  virtual ~CrashReporterParent();

  /*
   * Initialize this reporter with data from the child process.
   */
  void
  SetChildData(const NativeThreadId& id, const uint32_t& processType);

  /*
   * Returns the ID of the child minidump.
   * GeneratePairedMinidump or GenerateCrashReport must be called first.
   */
  const nsString& ChildDumpID() const {
    return mChildDumpID;
  }

  /*
   * Add an annotation to our internally tracked list of annotations.
   * Callers must apply these notes using GenerateChildData otherwise
   * the notes will get dropped.
   */
  void
  AnnotateCrashReport(const nsCString& aKey, const nsCString& aData);

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvAnnotateCrashReport(const nsCString& aKey,
                                       const nsCString& aData) override
  {
    AnnotateCrashReport(aKey, aData);
    return true;
  }

  virtual bool RecvAppendAppNotes(const nsCString& aData) override;

  nsCString mAppNotes;
  nsString mChildDumpID;
  // stores the child main thread id
  NativeThreadId mMainThread;
  time_t mStartTime;
  // stores the child process type
  GeckoProcessType mProcessType;
  bool mInitialized;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CrashReporterParent_h
