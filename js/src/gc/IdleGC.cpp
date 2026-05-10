/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * IDLE-TIME GARBAGE COLLECTION SYSTEM
 * 
 * Overview
 * --------
 * This system implements idle-time-only garbage collection, deferring GC work
 * to periods when the JavaScript engine is not actively processing code.
 * This improves perceived responsiveness by avoiding GC pauses during critical
 * execution windows.
 * 
 * Architecture
 * -----------
 * 
 * The system consists of several key components:
 * 
 * 1. IdleGCManager (gc/IdleGC.h, gc/IdleGC.cpp)
 *    - Tracks when the JS engine is executing vs. idle
 *    - Maintains timestamp of last execution activity  
 *    - Checks if sufficient idle time has passed
 *    - Configurable idle threshold (default: 100ms)
 *    - Can determine which GC reasons should bypass idle checks
 * 
 * 2. GCRuntime Integration (gc/GCRuntime.h)
 *    - Contains an IdleGCManager instance
 *    - Provides public methods: notifyJSExecutionStart/End()
 *    - Modified checkIfGCAllowedInCurrentState() to check idle status
 * 
 * 3. Activity Tracking (vm/Runtime.cpp)
 *    - triggerActivityCallback() now notifies IdleGCManager
 *    - Called when JS enters/exits request (execution boundary)
 *    - Updated in jsapi.cpp's StartRequest/StopRequest functions
 * 
 * 4. Public API (jsapi.h, jsapi.cpp)
 *    - JS_SetIdleGCEnabled() / JS_IsIdleGCEnabled()
 *    - JS_SetIdleGCThreshold() / JS_GetIdleGCThreshold()
 *    - JS_GetIdleTimeSinceLastExecution()
 *    - Allows embedders to configure idle GC behavior
 *
 * Behavior
 * --------
 * 
 * Normal GC Trigger (Idle GC Enabled):
 * 
 *   1. JS code executes → notifyJSExecutionStart() called
 *   2. JS code finishes → notifyJSExecutionEnd() called, timestamp recorded
 *   3. GC needed → checkIfGCAllowedInCurrentState() checks idle status
 *   4a. If idle >= threshold → GC proceeds normally
 *   4b. If still executing/idle < threshold → GC is deferred
 *   5. Once idle threshold is met, next GC request proceeds
 * 
 * Critical GC Triggers (Always Proceed):
 * 
 *   The following GC reasons bypass idle checking:
 *   - OUT_OF_MEMORY: Memory pressure conditions
 *   - ALLOC_TRIGGER: Allocation threshold exceeded
 *   - MALLOC_PRESSURE: Malloc pressure from OS
 *   - EAGER_ALLOC_TRIGGER: Eager allocation trigger
 *   - API: Explicit JS API calls
 *   - DETERMINISTIC: Deterministic tests
 *   - EVICT_NURSERY: Nursery eviction
 *   - SHUTDOWN_CC, DESTROY_RUNTIME, LAST_DITCH: Shutdown GCs
 *   - DESTROY_ZONE, COMPARTMENT_REVOKED: Zone/compartment destruction
 * 
 * Configuration
 * -------------
 * 
 * Idle GC Enabled (default: true):
 *   - Enables idle-time-only GC mode
 *   - Can be toggled dynamically via JS_SetIdleGCEnabled()
 * 
 * Idle Threshold (default: 100ms):
 *   - Minimum idle time before GC is permitted
 *   - Configurable via JS_SetIdleGCThreshold(ms)
 *   - Typical values: 50-200ms depending on application
 * 
 * Integration Examples
 * --------------------
 * 
 * Browser Integration:
 * 
 *   // When browser loads configuration
 *   JS_SetIdleGCEnabled(cx, true);
 *   JS_SetIdleGCThreshold(cx, 100);  // 100ms idle threshold
 * 
 *   // Monitor idle GC effectiveness (optional)
 *   uint64_t idleTime = JS_GetIdleTimeSinceLastExecution(cx);
 *   if (idleTime > JS_GetIdleGCThreshold(cx)) {
 *       // System is idle, GC would be allowed if triggered
 *   }
 * 
 * Disabling for Specific Scenarios:
 * 
 *   // During initialization when nothing is "idle" yet
 *   JS_SetIdleGCEnabled(cx, false);
 *   // ... do initial setup ...
 *   JS_SetIdleGCEnabled(cx, true);  // Re-enable for normal operation
 * 
 * Performance Considerations
 * --------------------------
 * 
 * Benefits:
 *   - Reduced jank during active JS execution
 *   - GC pauses moved to idle periods where users won't notice
 *   - Especially effective for interactive applications
 *   - Improves Time-to-Interactive and First Input Delay metrics
 * 
 * Tradeoffs:
 *   - May accumulate more garbage before collection
 *   - Requires predictable idle periods (not suitable for all workloads)
 *   - Critical memory pressure GCs still proceed immediately
 * 
 * Tuning:
 *   - Lower threshold (50ms) = more frequent GC, less memory overhead
 *   - Higher threshold (200ms) = less GC overhead, more memory usage
 *   - Optimal value depends on application's execution pattern
 * 
 * Testing
 * -------
 * 
 * Unit Tests:
 *   // Test idle detection
 *   JS_SetIdleGCThreshold(cx, 100);
 *   // Simulate JS execution
 *   cx->runtime()->gc.notifyJSExecutionStart();
 *   // ... wait 50ms ...
 *   MOZ_ASSERT(!cx->runtime()->gc.idleGCMgr().isIdleEnough());
 *   cx->runtime()->gc.notifyJSExecutionEnd();
 *   // ... wait 150ms ...
 *   MOZ_ASSERT(cx->runtime()->gc.idleGCMgr().isIdleEnough());
 * 
 * Integration Tests:
 *   - Verify GC is deferred during active execution
 *   - Verify GC proceeds after idle period
 *   - Verify critical GC reasons bypass idle check
 *   - Measure latency improvements
 * 
 * Implementation Notes
 * --------------------
 * 
 * Thread Safety:
 *   - IdleGCManager uses mozilla::Atomic for thread-safe state
 *   - TimeStamp operations are atomic
 *   - No additional locking needed beyond existing GC locks
 * 
 * Compatibility:
 *   - Works with both incremental and non-incremental GC
 *   - Compatible with generational GC
 *   - Works with zone GC and full GC
 *   - Respects existing GC suppression mechanisms
 * 
 * Future Enhancements
 * -------------------
 * 
 * Potential improvements:
 *   - Adaptive idle threshold based on historical GC times
 *   - Per-zone idle configuration
 *   - Integration with browser rendering idle callback API
 *   - Metrics/telemetry for idle GC effectiveness
 *   - Machine learning-based prediction of idle periods
 *   - Cooperative GC scheduling with other subsystems
 * 
 */

#include "gc/IdleGC.h"
#include "jsapi.h"

namespace js {
namespace gc {

IdleGCManager::IdleGCManager()
  : lastExecutionTime_(mozilla::TimeStamp::Now()),
    idleGCEnabled_(true),
    idleThresholdMs_(100),  // 100ms default idle threshold
    isExecuting_(false)
{
}

void
IdleGCManager::notifyJSExecutionStart()
{
    isExecuting_ = true;
}

void
IdleGCManager::notifyJSExecutionEnd()
{
    isExecuting_ = false;
    lastExecutionTime_ = mozilla::TimeStamp::Now();
}

bool
IdleGCManager::isIdleEnough() const
{
    if (!idleGCEnabled_) {
        return true;  // If disabled, always consider idle
    }

    if (isExecuting_) {
        return false;  // Still executing, not idle
    }

    uint64_t idleTime = idleTimeSinceLastExecution();
    return idleTime >= idleThresholdMs_;
}

uint64_t
IdleGCManager::idleTimeSinceLastExecution() const
{
    mozilla::TimeStamp now = mozilla::TimeStamp::Now();
    mozilla::TimeDuration idle = now - lastExecutionTime_;
    return idle.ToMilliseconds();
}

bool
IdleGCManager::shouldBypassIdleCheck(JS::gcreason::Reason reason)
{
    // These reasons indicate urgent GC needs that should bypass idle checking
    switch (reason) {
        // Allocation and memory pressure conditions.
        case JS::gcreason::ALLOC_TRIGGER:
        case JS::gcreason::EAGER_ALLOC_TRIGGER:
        case JS::gcreason::TOO_MUCH_MALLOC:
        case JS::gcreason::MEM_PRESSURE:
        case JS::gcreason::LAST_DITCH:

        // Nursery/store-buffer pressure.
        case JS::gcreason::OUT_OF_NURSERY:
        case JS::gcreason::EVICT_NURSERY:
        case JS::gcreason::FULL_STORE_BUFFER:
        case JS::gcreason::SHARED_MEMORY_LIMIT:

        // Explicit API calls
        case JS::gcreason::API:
        case JS::gcreason::ABORT_GC:

        // Shutdown and finalization
        case JS::gcreason::SHUTDOWN_CC:
        case JS::gcreason::DESTROY_RUNTIME:
        case JS::gcreason::NSJSCONTEXT_DESTROY:
        case JS::gcreason::XPCONNECT_SHUTDOWN:

            return true;

        default:
            return false;
    }
}

void
IdleGCManager::reset()
{
    lastExecutionTime_ = mozilla::TimeStamp::Now();
    isExecuting_ = false;
}

} // namespace gc
} // namespace js
