/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BigIntType_h
#define vm_BigIntType_h

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Heap.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "vm/String.h"

namespace JS {

class BigInt final : public js::gc::TenuredCell
{
  private:
    // The minimum allocation size is currently 16 bytes (see
    // SortedArenaList in gc/ArenaList.h).
    uint8_t unused_[16 + (16 % js::gc::CellSize)];

  public:
    // Allocate and initialize a BigInt value
    static BigInt* create(js::ExclusiveContext* cx);

    static const JS::TraceKind TraceKind = JS::TraceKind::BigInt;

    void traceChildren(JSTracer* trc);

    void finalize(js::FreeOp* fop);

    js::HashNumber hash();

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    static JSLinearString* toString(js::ExclusiveContext* cx, BigInt* x);
    bool toBoolean();

    static BigInt* copy(js::ExclusiveContext* cx, Handle<BigInt*> x);
};

static_assert(sizeof(BigInt) >= js::gc::CellSize,
              "sizeof(BigInt) must be greater than the minimum allocation size");

} // namespace JS

namespace js {

extern JSAtom*
BigIntToAtom(ExclusiveContext* cx, JS::BigInt* bi);

} // namespace js

#endif
