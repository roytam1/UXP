/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadStackHelper.h"
#include "MainThreadUtils.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"
#include "jsfriendapi.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Sprintf.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#if defined(MOZ_VALGRIND)
# include <valgrind/valgrind.h>
#endif

#include <string.h>
#include <vector>
#include <cstdlib>

#ifdef XP_LINUX
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef __GNUC__
# pragma GCC diagnostic pop // -Wshadow
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX)
#include <pthread.h>
#endif

#ifdef ANDROID
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif
#if defined(__arm__) && !defined(__NR_rt_tgsigqueueinfo)
// Some NDKs don't define this constant even though the kernel supports it.
#define __NR_rt_tgsigqueueinfo (__NR_SYSCALL_BASE+363)
#endif
#ifndef SYS_rt_tgsigqueueinfo
#define SYS_rt_tgsigqueueinfo __NR_rt_tgsigqueueinfo
#endif
#endif

namespace mozilla {

void
ThreadStackHelper::Startup()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInitialized) {
    // TODO: centralize signal number allocation
    sFillStackSignum = SIGRTMIN + 4;
    if (sFillStackSignum > SIGRTMAX) {
      // Leave uninitialized
      MOZ_ASSERT(false);
      return;
    }
    struct sigaction sigact = {};
    sigact.sa_sigaction = FillStackHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO | SA_RESTART;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
  }
  sInitialized++;
#endif
}

void
ThreadStackHelper::Shutdown()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitialized == 1) {
    struct sigaction sigact = {};
    sigact.sa_handler = SIG_DFL;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
  }
  sInitialized--;
#endif
}

ThreadStackHelper::ThreadStackHelper()
  : mStackToFill(nullptr)
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_init(&mSem, 0, 0));
  mThreadID = ::syscall(SYS_gettid);
#elif defined(XP_WIN)
  mInitialized = !!::DuplicateHandle(
    ::GetCurrentProcess(), ::GetCurrentThread(),
    ::GetCurrentProcess(), &mThreadID,
    THREAD_SUSPEND_RESUME
    , FALSE, 0);
  MOZ_ASSERT(mInitialized);
#elif defined(XP_MACOSX)
  mThreadID = mach_thread_self();
#endif
}

ThreadStackHelper::~ThreadStackHelper()
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_destroy(&mSem));
#elif defined(XP_WIN)
  if (mInitialized) {
    MOZ_ALWAYS_TRUE(!!::CloseHandle(mThreadID));
  }
#endif
}

namespace {
template<typename T>
class ScopedSetPtr
{
private:
  T*& mPtr;
public:
  ScopedSetPtr(T*& p, T* val) : mPtr(p) { mPtr = val; }
  ~ScopedSetPtr() { mPtr = nullptr; }
};
} // namespace

void
ThreadStackHelper::GetStack(Stack& aStack)
{
  // Always run PrepareStackBuffer first to clear aStack
  if (!PrepareStackBuffer(aStack)) {
    // Skip and return empty aStack
    return;
  }

  ScopedSetPtr<Stack> stackPtr(mStackToFill, &aStack);

#if defined(XP_LINUX)
  if (!sInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  siginfo_t uinfo = {};
  uinfo.si_signo = sFillStackSignum;
  uinfo.si_code = SI_QUEUE;
  uinfo.si_pid = getpid();
  uinfo.si_uid = getuid();
  uinfo.si_value.sival_ptr = this;
  if (::syscall(SYS_rt_tgsigqueueinfo, uinfo.si_pid,
                mThreadID, sFillStackSignum, &uinfo)) {
    // rt_tgsigqueueinfo was added in Linux 2.6.31.
    // Could have failed because the syscall did not exist.
    return;
  }
  MOZ_ALWAYS_TRUE(!::sem_wait(&mSem));

#elif defined(XP_WIN)
  if (!mInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  if (::SuspendThread(mThreadID) == DWORD(-1)) {
    MOZ_ASSERT(false);
    return;
  }

  // SuspendThread is asynchronous, so the thread may still be running. Use
  // GetThreadContext to ensure it's really suspended.
  // See https://blogs.msdn.microsoft.com/oldnewthing/20150205-00/?p=44743.
  CONTEXT context;
  context.ContextFlags = CONTEXT_CONTROL;
  if (::GetThreadContext(mThreadID, &context)) {
    FillStackBuffer();
    FillThreadContext();
  }

  MOZ_ALWAYS_TRUE(::ResumeThread(mThreadID) != DWORD(-1));

#elif defined(XP_MACOSX)
# if defined(MOZ_VALGRIND) && defined(RUNNING_ON_VALGRIND)
  if (RUNNING_ON_VALGRIND) {
    /* thread_suspend and thread_resume sometimes hang runs on Valgrind,
       for unknown reasons.  So, just avoid them.  See bug 1100911. */
    return;
  }
# endif

  if (::thread_suspend(mThreadID) != KERN_SUCCESS) {
    MOZ_ASSERT(false);
    return;
  }

  FillStackBuffer();
  FillThreadContext();

  MOZ_ALWAYS_TRUE(::thread_resume(mThreadID) == KERN_SUCCESS);

#endif
}

void
ThreadStackHelper::GetNativeStack(Stack& aStack)
{
  /*** STUB ***/
}

#ifdef XP_LINUX

int ThreadStackHelper::sInitialized;
int ThreadStackHelper::sFillStackSignum;

void
ThreadStackHelper::FillStackHandler(int aSignal, siginfo_t* aInfo,
                                    void* aContext)
{
  ThreadStackHelper* const helper =
    reinterpret_cast<ThreadStackHelper*>(aInfo->si_value.sival_ptr);
  helper->FillStackBuffer();
  helper->FillThreadContext(aContext);
  ::sem_post(&helper->mSem);
}

#endif // XP_LINUX

bool
ThreadStackHelper::PrepareStackBuffer(Stack& aStack)
{
  // Return false to skip getting the stack and return an empty stack
  aStack.clear();
  return false;
}

void
ThreadStackHelper::FillStackBuffer()
{
  MOZ_ASSERT(mStackToFill->empty());
  /*** STUB ***/
}

MOZ_ASAN_BLACKLIST void
ThreadStackHelper::FillThreadContext(void* aContext)
{
  /*** STUB ***/
}

} // namespace mozilla
