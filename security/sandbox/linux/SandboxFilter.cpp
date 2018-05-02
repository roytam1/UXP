/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilter.h"
#include "SandboxFilterUtil.h"

#include "SandboxBrokerClient.h"
#include "SandboxInfo.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"

#include "mozilla/UniquePtr.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/ipc.h>
#include <linux/net.h>
#include <linux/prctl.h>
#include <linux/sched.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using namespace sandbox::bpf_dsl;
#define CASES SANDBOX_BPF_DSL_CASES

// Fill in defines in case of old headers.
// (Warning: these are wrong on PA-RISC.)
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif
#ifndef MADV_DONTDUMP
#define MADV_DONTDUMP 16
#endif

// Added in Linux 4.5; see bug 1303813.
#ifndef MADV_FREE
#define MADV_FREE 8
#endif

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

// To avoid visual confusion between "ifdef ANDROID" and "ifndef ANDROID":
#ifndef ANDROID
#define DESKTOP
#endif

// This file defines the seccomp-bpf system call filter policies.
// See also SandboxFilterUtil.h, for the CASES_FOR_* macros and
// SandboxFilterBase::Evaluate{Socket,Ipc}Call.
//
// One important difference from how Chromium bpf_dsl filters are
// normally interpreted: returning -ENOSYS from a Trap() handler
// indicates an unexpected system call; SigSysHandler() in Sandbox.cpp
// will detect this, request a crash dump, and terminate the process.
// This does not apply to using Error(ENOSYS) in the policy, so that
// can be used if returning an actual ENOSYS is needed.

namespace mozilla {

// This class whitelists everything used by the sandbox itself, by the
// core IPC code, by the crash reporter, or other core code.
class SandboxPolicyCommon : public SandboxPolicyBase
{
protected:
  typedef const sandbox::arch_seccomp_data& ArgsRef;

  static intptr_t BlockedSyscallTrap(ArgsRef aArgs, void *aux) {
    MOZ_ASSERT(!aux);
    return -ENOSYS;
  }

private:
#if defined(ANDROID) && ANDROID_VERSION < 16
  // Bug 1093893: Translate tkill to tgkill for pthread_kill; fixed in
  // bionic commit 10c8ce59a (in JB and up; API level 16 = Android 4.1).
  static intptr_t TKillCompatTrap(const sandbox::arch_seccomp_data& aArgs,
                                  void *aux)
  {
    return syscall(__NR_tgkill, getpid(), aArgs.args[0], aArgs.args[1]);
  }
#endif

  static intptr_t SetNoNewPrivsTrap(ArgsRef& aArgs, void* aux) {
    if (gSetSandboxFilter == nullptr) {
      // Called after BroadcastSetThreadSandbox finished, therefore
      // not our doing and not expected.
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    // Signal that the filter is already in place.
    return -ETXTBSY;
  }

public:
  virtual ResultExpr InvalidSyscall() const override {
    return Trap(BlockedSyscallTrap, nullptr);
  }

  virtual ResultExpr ClonePolicy(ResultExpr failPolicy) const {
    // Allow use for simple thread creation (pthread_create) only.

    // WARNING: s390 and cris pass the flags in the second arg -- see
    // CLONE_BACKWARDS2 in arch/Kconfig in the kernel source -- but we
    // don't support seccomp-bpf on those archs yet.
    Arg<int> flags(0);

    // The glibc source hasn't changed the thread creation clone flags
    // since 2004, so this *should* be safe to hard-code.  Bionic's
    // value has changed a few times, and has converged on the same one
    // as glibc; allow any of them.
    static const int flags_common = CLONE_VM | CLONE_FS | CLONE_FILES |
      CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;
    static const int flags_modern = flags_common | CLONE_SETTLS |
      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    // Can't use CASES here because its decltype magic infers const
    // int instead of regular int and bizarre voluminous errors issue
    // forth from the depths of the standard library implementation.
    return Switch(flags)
#ifdef ANDROID
      .Case(flags_common | CLONE_DETACHED, Allow()) // <= JB 4.2
      .Case(flags_common, Allow()) // JB 4.3 or KK 4.4
#endif
      .Case(flags_modern, Allow()) // Android L or glibc
      .Default(failPolicy);
  }

  virtual ResultExpr PrctlPolicy() const {
    // Note: this will probably need PR_SET_VMA if/when it's used on
    // Android without being overridden by an allow-all policy, and
    // the constant will need to be defined locally.
    Arg<int> op(0);
    return Switch(op)
      .CASES((PR_GET_SECCOMP, // BroadcastSetThreadSandbox, etc.
              PR_SET_NAME,    // Thread creation
              PR_SET_DUMPABLE, // Crash reporting
              PR_SET_PTRACER), // Debug-mode crash handling
             Allow())
      .Default(InvalidSyscall());
  }

  virtual Maybe<ResultExpr> EvaluateSocketCall(int aCall) const override {
    switch (aCall) {
    case SYS_RECVMSG:
    case SYS_SENDMSG:
      return Some(Allow());
    default:
      return Nothing();
    }
  }

  virtual ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      // Timekeeping
    case __NR_clock_gettime: {
      Arg<clockid_t> clk_id(0);
      return If(clk_id == CLOCK_MONOTONIC, Allow())
#ifdef CLOCK_MONOTONIC_COARSE
        .ElseIf(clk_id == CLOCK_MONOTONIC_COARSE, Allow())
#endif
        .ElseIf(clk_id == CLOCK_PROCESS_CPUTIME_ID, Allow())
        .ElseIf(clk_id == CLOCK_REALTIME, Allow())
#ifdef CLOCK_REALTIME_COARSE
        .ElseIf(clk_id == CLOCK_REALTIME_COARSE, Allow())
#endif
        .ElseIf(clk_id == CLOCK_THREAD_CPUTIME_ID, Allow())
        .Else(InvalidSyscall());
    }
    case __NR_gettimeofday:
#ifdef __NR_time
    case __NR_time:
#endif
    case __NR_nanosleep:
      return Allow();

      // Thread synchronization
    case __NR_futex:
      // FIXME: This could be more restrictive....
      return Allow();

      // Asynchronous I/O
    case __NR_epoll_wait:
    case __NR_epoll_pwait:
    case __NR_epoll_ctl:
    case __NR_ppoll:
    case __NR_poll:
      return Allow();

      // Used when requesting a crash dump.
    case __NR_pipe:
      return Allow();

      // Metadata of opened files
    CASES_FOR_fstat:
      return Allow();

      // Simple I/O
    case __NR_write:
    case __NR_read:
    case __NR_readv:
    case __NR_writev: // see SandboxLogging.cpp
    CASES_FOR_lseek:
      return Allow();

      // Memory mapping
    CASES_FOR_mmap:
    case __NR_munmap:
      return Allow();

      // Signal handling
#if defined(ANDROID) || defined(MOZ_ASAN)
    case __NR_sigaltstack:
#endif
    CASES_FOR_sigreturn:
    CASES_FOR_sigprocmask:
    CASES_FOR_sigaction:
      return Allow();

      // Send signals within the process (raise(), profiling, etc.)
    case __NR_tgkill: {
      Arg<pid_t> tgid(0);
      return If(tgid == getpid(), Allow())
        .Else(InvalidSyscall());
    }

#if defined(ANDROID) && ANDROID_VERSION < 16
      // Polyfill with tgkill; see above.
    case __NR_tkill:
      return Trap(TKillCompatTrap, nullptr);
#endif

      // Yield
    case __NR_sched_yield:
      return Allow();

      // Thread creation.
    case __NR_clone:
      return ClonePolicy(InvalidSyscall());

      // More thread creation.
#ifdef __NR_set_robust_list
    case __NR_set_robust_list:
      return Allow();
#endif
#ifdef ANDROID
    case __NR_set_tid_address:
      return Allow();
#endif

      // prctl
    case __NR_prctl: {
      if (SandboxInfo::Get().Test(SandboxInfo::kHasSeccompTSync)) {
        return PrctlPolicy();
      }

      Arg<int> option(0);
      return If(option == PR_SET_NO_NEW_PRIVS,
                Trap(SetNoNewPrivsTrap, nullptr))
        .Else(PrctlPolicy());
    }

      // NSPR can call this when creating a thread, but it will accept a
      // polite "no".
    case __NR_getpriority:
      // But if thread creation races with sandbox startup, that call
      // could succeed, and then we get one of these:
    case __NR_setpriority:
      return Error(EACCES);

      // Stack bounds are obtained via pthread_getattr_np, which calls
      // this but doesn't actually need it:
    case __NR_sched_getaffinity:
      return Error(ENOSYS);

      // Read own pid/tid.
    case __NR_getpid:
    case __NR_gettid:
      return Allow();

      // Discard capabilities
    case __NR_close:
      return Allow();

      // Machine-dependent stuff
#ifdef __arm__
    case __ARM_NR_breakpoint:
    case __ARM_NR_cacheflush:
    case __ARM_NR_usr26: // FIXME: do we actually need this?
    case __ARM_NR_usr32:
    case __ARM_NR_set_tls:
      return Allow();
#endif

      // Needed when being debugged:
    case __NR_restart_syscall:
      return Allow();

      // Terminate threads or the process
    case __NR_exit:
    case __NR_exit_group:
      return Allow();

#ifdef MOZ_ASAN
      // ASAN's error reporter wants to know if stderr is a tty.
    case __NR_ioctl: {
      Arg<int> fd(0);
      return If(fd == STDERR_FILENO, Allow())
        .Else(InvalidSyscall());
    }

      // ...and before compiler-rt r209773, it will call readlink on
      // /proc/self/exe and use the cached value only if that fails:
    case __NR_readlink:
    case __NR_readlinkat:
      return Error(ENOENT);

      // ...and if it found an external symbolizer, it will try to run it:
      // (See also bug 1081242 comment #7.)
    CASES_FOR_stat:
      return Error(ENOENT);
#endif

    default:
      return SandboxPolicyBase::EvaluateSyscall(sysno);
    }
  }
};

// The process-type-specific syscall rules start here:

}
