/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCInternals_h
#define gc_GCInternals_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Maybe.h"

#include "jscntxt.h"

#include "gc/Zone.h"
#include "vm/HelperThreads.h"
#include "vm/Runtime.h"
#include "vm/BigIntType.h"

namespace js {
namespace gc {

void FinishGC(JSContext* cx);

/*
 * This class should be used by any code that needs to exclusive access to the
 * heap in order to trace through it...
 */
class MOZ_RAII AutoTraceSession
{
  public:
    explicit AutoTraceSession(JSRuntime* rt, JS::HeapState state = JS::HeapState::Tracing);
    ~AutoTraceSession();

    // Threads with an exclusive context can hit refillFreeList while holding
    // the exclusive access lock. To avoid deadlocking when we try to acquire
    // this lock during GC and the other thread is waiting, make sure we hold
    // the exclusive access lock during GC sessions.
    AutoLockForExclusiveAccess lock;

  protected:
    JSRuntime* runtime;

  private:
    AutoTraceSession(const AutoTraceSession&) = delete;
    void operator=(const AutoTraceSession&) = delete;

    JS::HeapState prevState;
    AutoSPSEntry pseudoFrame;
};

class MOZ_RAII AutoPrepareForTracing
{
    mozilla::Maybe<AutoTraceSession> session_;

  public:
    AutoPrepareForTracing(JSContext* cx, ZoneSelector selector);
    AutoTraceSession& session() { return session_.ref(); }
};

AbortReason
IsIncrementalGCUnsafe(JSRuntime* rt);

#ifdef JSGC_HASH_TABLE_CHECKS
void CheckHashTablesAfterMovingGC(JSRuntime* rt);
void CheckHeapAfterGC(JSRuntime* rt);
#endif

struct MovingTracer : JS::CallbackTracer
{
    explicit MovingTracer(JSRuntime* rt) : CallbackTracer(rt, TraceWeakMapKeysValues) {}

    void onObjectEdge(JSObject** objp) override;
    void onShapeEdge(Shape** shapep) override;
    void onStringEdge(JSString** stringp) override;
    void onBigIntEdge(JS::BigInt** bip) override;
    void onScriptEdge(JSScript** scriptp) override;
    void onLazyScriptEdge(LazyScript** lazyp) override;
    void onBaseShapeEdge(BaseShape** basep) override;
    void onScopeEdge(Scope** basep) override;
    void onRegExpSharedEdge(RegExpShared** sharedp) override;
    void onChild(const JS::GCCellPtr& thing) override {
        MOZ_ASSERT(!RelocationOverlay::isCellForwarded(thing.asCell()));
    }

#ifdef DEBUG
    TracerKind getTracerKind() const override { return TracerKind::Moving; }
#endif

  private:
    template <typename T>
    void updateEdge(T** thingp);
};

// Structure for counting how many times objects in a particular group have
// been tenured during a minor collection.
struct TenureCount
{
    ObjectGroup* group;
    int count;
};

// Keep rough track of how many times we tenure objects in particular groups
// during minor collections, using a fixed size hash for efficiency at the cost
// of potential collisions.
struct TenureCountCache
{
    static const size_t EntryShift = 4;
    static const size_t EntryCount = 1 << EntryShift;

    TenureCount entries[EntryCount] = {}; // zeroes

    TenureCountCache() = default;

    HashNumber hash(ObjectGroup* group) {
#if JS_BITS_PER_WORD == 32
        static const size_t ZeroBits = 3;
#else
        static const size_t ZeroBits = 4;
#endif

        uintptr_t word = uintptr_t(group);
        MOZ_ASSERT((word & ((1 << ZeroBits) - 1)) == 0);
        word >>= ZeroBits;
        return HashNumber((word >> EntryShift) ^ word);
    }

    TenureCount& findEntry(ObjectGroup* group) {
        return entries[hash(group) % EntryCount];
    }
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_GCInternals_h */
