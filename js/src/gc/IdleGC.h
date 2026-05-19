/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_IdleGC_h
#define gc_IdleGC_h

#include <stdint.h>
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "js/GCAPI.h"

namespace js {
namespace gc {

/*
 * Idle-Time Garbage Collection System
 * ====================================
 *
 * This system defers garbage collection to occur only during periods when
 * the browser is not actively processing JavaScript. This helps maintain
 * responsiveness by avoiding GC pauses during critical execution windows.
 *
 * When JavaScript execution is active, GC triggers are deferred. Once the
 * JS engine has been idle for a configurable threshold period, pending GC
 * work is performed immediately or incrementally as appropriate.
 *
 * Key characteristics:
 *  - Tracks JavaScript activity via hooks in the execution engine
 *  - Configurable idle time threshold (default: 100ms)
 *  - Can be disabled per-zone or globally
 *  - Works with both incremental and non-incremental GC modes
 *  - Respects critical GC reasons that override idle checking
 */

class IdleGCManager
{
  public:
    // Initialize the idle GC manager
    IdleGCManager();

    /*
     * Called when JavaScript execution begins. Marks the engine as active.
     */
    void notifyJSExecutionStart();

    /*
     * Called when JavaScript execution ends. Records the end time for
     * idle detection purposes.
     */
    void notifyJSExecutionEnd();

    /*
     * Check if the system has been idle for long enough to permit GC.
     * Returns true if sufficient idle time has passed since last JS execution.
     */
    bool isIdleEnough() const;

    /*
     * Get the amount of idle time since the last JS execution.
     * Returns time in milliseconds.
     */
    uint64_t idleTimeSinceLastExecution() const;

    /*
     * Set the idle threshold - minimum idle time before GC is permitted.
     * Time is in milliseconds. Default is 100ms.
     */
    void setIdleThresholdMs(uint64_t thresholdMs) {
        idleThresholdMs_ = thresholdMs;
    }

    uint64_t idleThresholdMs() const {
        return idleThresholdMs_;
    }

    /*
     * Enable or disable idle-time-only GC mode.
     */
    void setIdleGCEnabled(bool enabled) {
        idleGCEnabled_ = enabled;
    }

    bool isIdleGCEnabled() const {
        return idleGCEnabled_;
    }

    /*
     * Check if a GC reason should bypass idle checking.
     * Critical reasons (OOM-like pressure, nursery pressure, explicit requests)
     * always proceed.
     */
    static bool shouldBypassIdleCheck(JS::gcreason::Reason reason);

    /*
     * Reset idle tracking state (used during GC or at shutdown).
     */
    void reset();

  private:
    // Timestamp of the last JavaScript execution activity
    mozilla::TimeStamp lastExecutionTime_;

    // Whether idle-time-only GC mode is enabled
    mozilla::Atomic<bool, mozilla::ReleaseAcquire> idleGCEnabled_;

    // Minimum idle time (in milliseconds) before GC is permitted
    mozilla::Atomic<uint64_t, mozilla::ReleaseAcquire> idleThresholdMs_;

    // Whether the JS engine is currently executing
    mozilla::Atomic<bool, mozilla::ReleaseAcquire> isExecuting_;
};

} // namespace gc
} // namespace js

#endif // gc_IdleGC_h
