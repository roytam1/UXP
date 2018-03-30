/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterHost_h
#define mozilla_ipc_CrashReporterHost_h

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/Shmem.h"
#include "base/process.h"
#include "nsExceptionHandler.h"

namespace mozilla {
namespace ipc {

// This is the newer replacement for CrashReporterParent. It is created in
// response to a InitCrashReporter message on a top-level actor, and simply
// holds the metadata shmem alive until the process ends. When the process
// terminates abnormally, the top-level should call GenerateCrashReport to
// automatically integrate metadata.
class CrashReporterHost
{
  typedef mozilla::ipc::Shmem Shmem;
  typedef CrashReporter::AnnotationTable AnnotationTable;

public:
  CrashReporterHost(GeckoProcessType aProcessType, const Shmem& aShmem);

private:
  void GenerateCrashReport(RefPtr<nsIFile> aCrashDump);

private:
  GeckoProcessType mProcessType;
  Shmem mShmem;
  time_t mStartTime;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_CrashReporterHost_h
