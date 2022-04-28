/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A watchdog designed to terminate shutdown if it lasts too long.
 *
 * This watchdog is designed as a worst-case problem container for the
 * common case in which Firefox just won't shutdown.
 *
 * We spawn a thread during quit-application. If any of the shutdown
 * steps takes more than n milliseconds (63000 by default), kill the
 * process as fast as possible, without any cleanup.
 */

#include "nsTerminator.h"

#include "prthread.h"
#include "prmon.h"
#include "plstr.h"
#include "prio.h"

#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIObserverService.h"
#include "nsIPrefService.h"

#ifdef XP_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

// Normally, the number of milliseconds that AsyncShutdown waits until
// it decides to crash is specified as a preference. We use the
// following value as a fallback if for some reason the preference is
// absent.
#define FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS 60000

// Additional number of milliseconds to wait until we decide to exit
// forcefully.
#define ADDITIONAL_WAIT_BEFORE_CRASH_MS 3000

namespace mozilla {

namespace {

// Utility function: create a thread that is non-joinable,
// does not prevent the process from terminating, is never
// cooperatively scheduled, and uses a default stack size.
PRThread* CreateSystemThread(void (*start)(void* arg),
                             void* arg)
{
  PRThread* thread = PR_CreateThread(
    PR_SYSTEM_THREAD, /* This thread will not prevent the process from terminating */
    start,
    arg,
    PR_PRIORITY_LOW,
    PR_GLOBAL_THREAD /* Make sure that the thread is never cooperatively scheduled */,
    PR_UNJOINABLE_THREAD,
    0 /* Use default stack size */
  );
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(thread); // This pointer will never be deallocated.
  return thread;
}


////////////////////////////////////////////
//
// The watchdog
//
// This nspr thread is in charge of crashing the process if any stage of shutdown
// lasts more than some predefined duration. As a side-effect, it measures the
// duration of each stage of shutdown.
//

// The heartbeat of the operation.
//
// Main thread:
//
// * Whenever a shutdown step has been completed, the main thread
// swaps gHeartbeat to 0 to mark that the shutdown process is still
// progressing. The value swapped away indicates the number of ticks
// it took for the shutdown step to advance.
//
// Watchdog thread:
//
// * Every tick, the watchdog thread increments gHearbeat atomically.
//
// A note about precision:
// Since gHeartbeat is generally reset to 0 between two ticks, this means
// that gHeartbeat stays at 0 less than one tick. Consequently, values
// extracted from gHeartbeat must be considered rounded up.
Atomic<uint32_t> gHeartbeat(0);

struct Options {
  /**
   * How many ticks before we should crash the process.
   */
  uint32_t crashAfterTicks;
};

/**
 * Entry point for the watchdog thread
 */
void
RunWatchdog(void* arg)
{
  PR_SetCurrentThreadName("Shutdown Hang Terminator");

  // Let's copy and deallocate options, that's one less leak to worry
  // about.
  UniquePtr<Options> options((Options*)arg);
  uint32_t crashAfterTicks = options->crashAfterTicks;
  options = nullptr;

  const uint32_t timeToLive = crashAfterTicks;
  while (true) {
    //
    // We do not want to sleep for the entire duration,
    // as putting the computer to sleep would suddenly
    // cause us to timeout on wakeup.
    //
    // Rather, we prefer sleeping for at most 1 second
    // at a time. If the computer sleeps then wakes up,
    // we have lost at most one second, which is much
    // more reasonable.
    //
#if defined(XP_WIN)
    Sleep(1000 /* ms */);
#else
    usleep(1000000 /* usec */);
#endif

    if (gHeartbeat++ < timeToLive) {
      continue;
    }

    // Shutdown is apparently dead. Crash the process.
    MOZ_CRASH("Shutdown too long, probably frozen, causing a crash.");
  }
}

/**
 * A step during shutdown.
 *
 * Shutdown is divided in steps, which all map to an observer
 * notification. The duration of a step is defined as the number of
 * ticks between the time we receive a notification and the next one.
 */
struct ShutdownStep
{
  char const* const mTopic;
  int mTicks;

  constexpr explicit ShutdownStep(const char *const topic)
    : mTopic(topic)
    , mTicks(-1)
  {}

};

static ShutdownStep sShutdownSteps[] = {
  ShutdownStep("quit-application"),
  ShutdownStep("profile-change-teardown"),
  ShutdownStep("profile-before-change"),
  ShutdownStep("xpcom-will-shutdown"),
  ShutdownStep("xpcom-shutdown"),
};

} // namespace

NS_IMPL_ISUPPORTS(nsTerminator, nsIObserver)

nsTerminator::nsTerminator()
  : mInitialized(false)
  , mCurrentStep(-1)
{
}

// During startup, register as an observer for all interesting topics.
nsresult
nsTerminator::SelfInit()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_UNEXPECTED;
  }

  for (size_t i = 0; i < ArrayLength(sShutdownSteps); ++i) {
    DebugOnly<nsresult> rv = os->AddObserver(this, sShutdownSteps[i].mTopic, false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddObserver failed");
  }

  return NS_OK;
}

// Actually launch the thread. This takes place at the first sign of shutdown.
void
nsTerminator::Start()
{
  MOZ_ASSERT(!mInitialized);
  StartWatchdog();
  mInitialized = true;
}

// Prepare, allocate and start the watchdog thread.
// By design, it will never finish, nor be deallocated.
void
nsTerminator::StartWatchdog()
{
  int32_t crashAfterMS =
    Preferences::GetInt("toolkit.asyncshutdown.crash_timeout",
                        FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS);
  // Ignore negative values
  if (crashAfterMS <= 0) {
    crashAfterMS = FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS;
  }

  // Add a little padding, to ensure that we do not crash before
  // AsyncShutdown.
  if (crashAfterMS > INT32_MAX - ADDITIONAL_WAIT_BEFORE_CRASH_MS) {
    // Defend against overflow
    crashAfterMS = INT32_MAX;
  } else {
    crashAfterMS += ADDITIONAL_WAIT_BEFORE_CRASH_MS;
  }

  UniquePtr<Options> options(new Options());
#ifdef XP_SOLARIS
  const PRIntervalTime ticksDuration = PR_MillisecondsToInterval(1000);
#else
  const PRIntervalTime ticksDuration = 1000;
#endif  
  options->crashAfterTicks = crashAfterMS / ticksDuration;

  DebugOnly<PRThread*> watchdogThread = CreateSystemThread(RunWatchdog,
                                                options.release());
  MOZ_ASSERT(watchdogThread);
}

NS_IMETHODIMP
nsTerminator::Observe(nsISupports *, const char *aTopic, const char16_t *)
{
  if (strcmp(aTopic, "profile-after-change") == 0) {
    return SelfInit();
  }

  // Other notifications are shutdown-related.

  // As we have seen examples in the wild of shutdown notifications
  // not being sent (or not being sent in the expected order), we do
  // not assume a specific order.
  if (!mInitialized) {
    Start();
  }

  UpdateHeartbeat(aTopic);

  // Perform a little cleanup
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_RELEASE_ASSERT(os);
  (void)os->RemoveObserver(this, aTopic);

  return NS_OK;
}

void
nsTerminator::UpdateHeartbeat(const char* aTopic)
{
  // Reset the clock, find out how long the current phase has lasted.
  uint32_t ticks = gHeartbeat.exchange(0);
  if (mCurrentStep > 0) {
    sShutdownSteps[mCurrentStep].mTicks = ticks;
  }

  // Find out where we now are in the current shutdown.
  // Don't assume that shutdown takes place in the expected order.
  int nextStep = -1;
  for (size_t i = 0; i < ArrayLength(sShutdownSteps); ++i) {
    if (strcmp(sShutdownSteps[i].mTopic, aTopic) == 0) {
      nextStep = i;
      break;
    }
  }
  MOZ_ASSERT(nextStep != -1);
  mCurrentStep = nextStep;
}

} // namespace mozilla
