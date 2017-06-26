/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"

#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"

#include "gc/GCInternals.h"
#include "gc/Zone.h"
#include "js/GCAPI.h"
#include "js/HashTable.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

#ifdef JSGC_HASH_TABLE_CHECKS

class CheckHeapTracer : public JS::CallbackTracer
{
  public:
    explicit CheckHeapTracer(JSRuntime* rt);
    bool init();
    void check(AutoLockForExclusiveAccess& lock);

  private:
    void onChild(const JS::GCCellPtr& thing) override;

    struct WorkItem {
        WorkItem(JS::GCCellPtr thing, const char* name, int parentIndex)
          : thing(thing), name(name), parentIndex(parentIndex), processed(false)
        {}

        JS::GCCellPtr thing;
        const char* name;
        int parentIndex;
        bool processed;
    };

    JSRuntime* rt;
    bool oom;
    size_t failures;
    HashSet<Cell*, DefaultHasher<Cell*>, SystemAllocPolicy> visited;
    Vector<WorkItem, 0, SystemAllocPolicy> stack;
    int parentIndex;
};

CheckHeapTracer::CheckHeapTracer(JSRuntime* rt)
  : CallbackTracer(rt, TraceWeakMapKeysValues),
    rt(rt),
    oom(false),
    failures(0),
    parentIndex(-1)
{
#ifdef DEBUG
    setCheckEdges(false);
#endif
}

bool
CheckHeapTracer::init()
{
    return visited.init();
}

inline static bool
IsValidGCThingPointer(Cell* cell)
{
    return (uintptr_t(cell) & CellMask) == 0;
}

void
CheckHeapTracer::onChild(const JS::GCCellPtr& thing)
{
    Cell* cell = thing.asCell();
    if (visited.lookup(cell))
        return;

    if (!visited.put(cell)) {
        oom = true;
        return;
    }

    if (!IsValidGCThingPointer(cell) || !IsGCThingValidAfterMovingGC(cell))
    {
        failures++;
        fprintf(stderr, "Bad pointer %p\n", cell);
        const char* name = contextName();
        for (int index = parentIndex; index != -1; index = stack[index].parentIndex) {
            const WorkItem& parent = stack[index];
            cell = parent.thing.asCell();
            fprintf(stderr, "  from %s %p %s edge\n",
                    GCTraceKindToAscii(cell->getTraceKind()), cell, name);
            name = parent.name;
        }
        fprintf(stderr, "  from root %s\n", name);
        return;
    }

    WorkItem item(thing, contextName(), parentIndex);
    if (!stack.append(item))
        oom = true;
}

void
CheckHeapTracer::check(AutoLockForExclusiveAccess& lock)
{
    // The analysis thinks that traceRuntime might GC by calling a GC callback.
    JS::AutoSuppressGCAnalysis nogc;
    if (!rt->isBeingDestroyed())
        rt->gc.traceRuntime(this, lock);

    while (!stack.empty()) {
        WorkItem item = stack.back();
        if (item.processed) {
            stack.popBack();
        } else {
            parentIndex = stack.length() - 1;
            TraceChildren(this, item.thing);
            stack.back().processed = true;
        }
    }

    if (oom)
        return;

    if (failures) {
        fprintf(stderr, "Heap check: %" PRIuSIZE " failure(s) out of %" PRIu32 " pointers checked\n",
                failures, visited.count());
    }
    MOZ_RELEASE_ASSERT(failures == 0);
}

void
js::gc::CheckHeapAfterGC(JSRuntime* rt)
{
    AutoTraceSession session(rt, JS::HeapState::Tracing);
    CheckHeapTracer tracer(rt);
    if (tracer.init())
        tracer.check(session.lock);
}

#endif /* JSGC_HASH_TABLE_CHECKS */
