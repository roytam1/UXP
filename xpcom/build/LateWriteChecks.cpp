/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/IOInterposer.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/SHA1.h"
#include "mozilla/Scoped.h"
#include "mozilla/StaticPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/StackWalk.h"
#include "plstr.h"
#include "prio.h"

#ifdef XP_WIN
#define NS_T(str) L ## str
#define NS_SLASH "\\"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#else
#define NS_SLASH "/"
#endif

#include "LateWriteChecks.h"

using namespace mozilla;

/*************************** Auxiliary Declarations ***************************/

// This a wrapper over a file descriptor that provides a Printf method and
// computes the sha1 of the data that passes through it.
class SHA1Stream
{
public:
  explicit SHA1Stream(FILE* aStream)
    : mFile(aStream)
  {
    MozillaRegisterDebugFILE(mFile);
  }

  void Printf(const char* aFormat, ...)
  {
    MOZ_ASSERT(mFile);
    va_list list;
    va_start(list, aFormat);
    nsAutoCString str;
    str.AppendPrintf(aFormat, list);
    va_end(list);
    mSHA1.update(str.get(), str.Length());
    fwrite(str.get(), 1, str.Length(), mFile);
  }
  void Finish(SHA1Sum::Hash& aHash)
  {
    int fd = fileno(mFile);
    fflush(mFile);
    MozillaUnRegisterDebugFD(fd);
    fclose(mFile);
    mSHA1.finish(aHash);
    mFile = nullptr;
  }
private:
  FILE* mFile;
  SHA1Sum mSHA1;
};

static void
RecordStackWalker(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  std::vector<uintptr_t>* stack =
    static_cast<std::vector<uintptr_t>*>(aClosure);
  stack->push_back(reinterpret_cast<uintptr_t>(aPC));
}

/**************************** Late-Write Observer  ****************************/

/**
 * An implementation of IOInterposeObserver to be registered with IOInterposer.
 * This observer logs all writes as late writes.
 */
class LateWriteObserver final : public IOInterposeObserver
{
public:
  explicit LateWriteObserver(const char* aProfileDirectory)
    : mProfileDirectory(PL_strdup(aProfileDirectory))
  {
  }
  ~LateWriteObserver()
  {
    PL_strfree(mProfileDirectory);
    mProfileDirectory = nullptr;
  }

  void Observe(IOInterposeObserver::Observation& aObservation);
private:
  char* mProfileDirectory;
};

void
LateWriteObserver::Observe(IOInterposeObserver::Observation& aOb)
{
/* *** STUB *** */
}

/******************************* Setup/Teardown *******************************/

static StaticAutoPtr<LateWriteObserver> sLateWriteObserver;

namespace mozilla {

void
InitLateWriteChecks()
{
  nsCOMPtr<nsIFile> mozFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mozFile));
  if (mozFile) {
    nsAutoCString nativePath;
    nsresult rv = mozFile->GetNativePath(nativePath);
    if (NS_SUCCEEDED(rv) && nativePath.get()) {
      sLateWriteObserver = new LateWriteObserver(nativePath.get());
    }
  }
}

void
BeginLateWriteChecks()
{
  if (sLateWriteObserver) {
    IOInterposer::Register(
      IOInterposeObserver::OpWriteFSync,
      sLateWriteObserver
    );
  }
}

void
StopLateWriteChecks()
{
  if (sLateWriteObserver) {
    IOInterposer::Unregister(
      IOInterposeObserver::OpAll,
      sLateWriteObserver
    );
    // Deallocation would not be thread-safe, and StopLateWriteChecks() is
    // called at shutdown and only in special cases.
    // sLateWriteObserver = nullptr;
  }
}

} // namespace mozilla
