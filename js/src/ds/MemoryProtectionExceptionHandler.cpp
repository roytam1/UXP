/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ds/MemoryProtectionExceptionHandler.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"

#if defined(XP_WIN)
# include "jswin.h"
#elif defined(XP_UNIX)
# include <signal.h>
# include <sys/types.h>
# include <unistd.h>
#endif

#ifdef ANDROID
# include <android/log.h>
#endif

#include "ds/SplayTree.h"

#include "threading/LockGuard.h"
#include "threading/Thread.h"
#include "vm/MutexIDs.h"

namespace js {

/*
 * A class to store the addresses of the regions recognized as protected
 * by this exception handler. We use a splay tree to store these addresses.
 */
class ProtectedRegionTree
{
    struct Region
    {
        uintptr_t first;
        uintptr_t last;

        Region(uintptr_t addr, size_t size) : first(addr),
                                              last(addr + (size - 1)) {}

        static int compare(const Region& A, const Region& B) {
            if (A.last < B.first)
                return -1;
            if (A.first > B.last)
                return 1;
            return 0;
        }
    };

    Mutex lock;
    LifoAlloc alloc;
    SplayTree<Region, Region> tree;

  public:
    ProtectedRegionTree() : lock(mutexid::ProtectedRegionTree),
                            alloc(4096),
                            tree(&alloc) {}

    ~ProtectedRegionTree() { MOZ_ASSERT(tree.empty()); }

    void insert(uintptr_t addr, size_t size) {
        MOZ_ASSERT(addr && size);
        LockGuard<Mutex> guard(lock);
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!tree.insert(Region(addr, size)))
            oomUnsafe.crash("Failed to store allocation ID.");
    }

    void remove(uintptr_t addr) {
        MOZ_ASSERT(addr);
        LockGuard<Mutex> guard(lock);
        tree.remove(Region(addr, 1));
    }

    bool isProtected(uintptr_t addr) {
        if (!addr)
            return false;
        LockGuard<Mutex> guard(lock);
        return tree.maybeLookup(Region(addr, 1));
    }
};

static bool sExceptionHandlerInstalled = false;

static ProtectedRegionTree sProtectedRegions;

bool
MemoryProtectionExceptionHandler::isDisabled()
{
    // Disabled everywhere for this release.
    return true;
}

void
MemoryProtectionExceptionHandler::addRegion(void* addr, size_t size)
{
    if (sExceptionHandlerInstalled)
        sProtectedRegions.insert(uintptr_t(addr), size);
}

void
MemoryProtectionExceptionHandler::removeRegion(void* addr)
{
    if (sExceptionHandlerInstalled)
        sProtectedRegions.remove(uintptr_t(addr));
}

/* -------------------------------------------------------------------------- */

/*
 * This helper function attempts to replicate the functionality of
 * mozilla::MOZ_ReportCrash() in an async-signal-safe way.
 */
static MOZ_COLD MOZ_ALWAYS_INLINE void
ReportCrashIfDebug(const char* aStr)
    MOZ_PRETEND_NORETURN_FOR_STATIC_ANALYSIS
{
#ifdef DEBUG
# if defined(XP_WIN)
    DWORD bytesWritten;
    BOOL ret = WriteFile(GetStdHandle(STD_ERROR_HANDLE), aStr,
                         strlen(aStr) + 1, &bytesWritten, nullptr);
    ::__debugbreak();
# elif defined(ANDROID)
    int ret = __android_log_write(ANDROID_LOG_FATAL, "MOZ_CRASH", aStr);
# else
    ssize_t ret = write(STDERR_FILENO, aStr, strlen(aStr) + 1);
# endif
    (void)ret; // Ignore failures; we're already crashing anyway.
#endif
}

/* -------------------------------------------------------------------------- */

#if defined(XP_WIN)

static void* sVectoredExceptionHandler = nullptr;

/*
 * We can only handle one exception. To guard against races and reentrancy,
 * we set this value the first time we enter the exception handler and never
 * touch it again.
 */
static mozilla::Atomic<bool> sHandlingException(false);

static long __stdcall
VectoredExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
    EXCEPTION_RECORD* ExceptionRecord = ExceptionInfo->ExceptionRecord;

    // We only handle one kind of exception; ignore all others.
    if (ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        // Make absolutely sure we can only get here once.
        if (sHandlingException.compareExchange(false, true)) {
            // Restore the previous handler. We're going to forward to it
            // anyway, and if we crash while doing so we don't want to hang.
            MOZ_ALWAYS_TRUE(RemoveVectoredExceptionHandler(sVectoredExceptionHandler));

            // Get the address that the offending code tried to access.
            uintptr_t address = uintptr_t(ExceptionRecord->ExceptionInformation[1]);

            // If the faulting address is in one of our protected regions, we
            // want to annotate the crash to make it stand out from the crowd.
            if (sProtectedRegions.isProtected(address)) {
                ReportCrashIfDebug("Hit MOZ_CRASH(Tried to access a protected region!)\n");
                MOZ_CRASH_ANNOTATE("MOZ_CRASH(Tried to access a protected region!)");
            }
        }
    }

    // Forward to the previous handler which may be a debugger,
    // the crash reporter or something else entirely.
    return EXCEPTION_CONTINUE_SEARCH;
}

bool
MemoryProtectionExceptionHandler::install()
{
    MOZ_ASSERT(!sExceptionHandlerInstalled);

    // If the exception handler is disabled, report success anyway.
    if (MemoryProtectionExceptionHandler::isDisabled())
        return true;

    // Install our new exception handler.
    sVectoredExceptionHandler = AddVectoredExceptionHandler(/* FirstHandler = */ true,
                                                            VectoredExceptionHandler);

    sExceptionHandlerInstalled = sVectoredExceptionHandler != nullptr;
    return sExceptionHandlerInstalled;
}

void
MemoryProtectionExceptionHandler::uninstall()
{
    if (sExceptionHandlerInstalled) {
        MOZ_ASSERT(!sHandlingException);

        // Restore the previous exception handler.
        MOZ_ALWAYS_TRUE(RemoveVectoredExceptionHandler(sVectoredExceptionHandler));

        sExceptionHandlerInstalled = false;
    }
}

#elif defined(XP_UNIX)

static struct sigaction sPrevSEGVHandler = {};

/*
 * We can only handle one exception. To guard against races and reentrancy,
 * we set this value the first time we enter the exception handler and never
 * touch it again.
 */
static mozilla::Atomic<bool> sHandlingException(false);

static void
UnixExceptionHandler(int signum, siginfo_t* info, void* context)
{
    // Make absolutely sure we can only get here once.
    if (sHandlingException.compareExchange(false, true)) {
        // Restore the previous handler. We're going to forward to it
        // anyway, and if we crash while doing so we don't want to hang.
        MOZ_ALWAYS_FALSE(sigaction(SIGSEGV, &sPrevSEGVHandler, nullptr));

        MOZ_ASSERT(signum == SIGSEGV && info->si_signo == SIGSEGV);

        if (info->si_code == SEGV_ACCERR) {
            // Get the address that the offending code tried to access.
            uintptr_t address = uintptr_t(info->si_addr);

            // If the faulting address is in one of our protected regions, we
            // want to annotate the crash to make it stand out from the crowd.
            if (sProtectedRegions.isProtected(address)) {
                ReportCrashIfDebug("Hit MOZ_CRASH(Tried to access a protected region!)\n");
                MOZ_CRASH_ANNOTATE("MOZ_CRASH(Tried to access a protected region!)");
            }
        }
    }

    // Forward to the previous handler which may be a debugger,
    // the crash reporter or something else entirely.
    if (sPrevSEGVHandler.sa_flags & SA_SIGINFO)
        sPrevSEGVHandler.sa_sigaction(signum, info, context);
    else if (sPrevSEGVHandler.sa_handler == SIG_DFL || sPrevSEGVHandler.sa_handler == SIG_IGN)
        sigaction(SIGSEGV, &sPrevSEGVHandler, nullptr);
    else
        sPrevSEGVHandler.sa_handler(signum);

    // If we reach here, we're returning to let the default signal handler deal
    // with the exception. This is technically undefined behavior, but
    // everything seems to do it, and it removes us from the crash stack.
}

bool
MemoryProtectionExceptionHandler::install()
{
    MOZ_ASSERT(!sExceptionHandlerInstalled);

    // If the exception handler is disabled, report success anyway.
    if (MemoryProtectionExceptionHandler::isDisabled())
        return true;

    // Install our new exception handler and save the previous one.
    struct sigaction faultHandler = {};
    faultHandler.sa_flags = SA_SIGINFO | SA_NODEFER;
    faultHandler.sa_sigaction = UnixExceptionHandler;
    sigemptyset(&faultHandler.sa_mask);
    sExceptionHandlerInstalled = !sigaction(SIGSEGV, &faultHandler, &sPrevSEGVHandler);

    return sExceptionHandlerInstalled;
}

void
MemoryProtectionExceptionHandler::uninstall()
{
    if (sExceptionHandlerInstalled) {
        MOZ_ASSERT(!sHandlingException);

        // Restore the previous exception handler.
        MOZ_ALWAYS_FALSE(sigaction(SIGSEGV, &sPrevSEGVHandler, nullptr));

        sExceptionHandlerInstalled = false;
    }
}

#else

#error "This platform is not supported!"

#endif

} /* namespace js */
