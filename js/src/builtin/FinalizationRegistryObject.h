/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_FinalizationRegistryObject_h
#define builtin_FinalizationRegistryObject_h

#include "gc/Barrier.h"
#include "vm/NativeObject.h"

namespace js {

class FinalizationRegistryObject : public NativeObject
{
  public:
    struct Data;

    static const Class class_;

    static JSObject* initClass(JSContext* cx, HandleObject obj);
    static FinalizationRegistryObject* create(JSContext* cx, HandleObject cleanupCallback,
                                              HandleObject proto = nullptr);

    static void trace(JSTracer* trc, JSObject* obj);
    static void finalize(FreeOp* fop, JSObject* obj);
    void traceWeakEdgesForCollectedZones(JSTracer* trc);
    [[nodiscard]] static bool construct(JSContext* cx, unsigned argc, Value* vp);
    [[nodiscard]] static bool register_(JSContext* cx, unsigned argc, Value* vp);
    [[nodiscard]] static bool unregister(JSContext* cx, unsigned argc, Value* vp);
    [[nodiscard]] static bool cleanupJob(JSContext* cx, unsigned argc, Value* vp);

    void sweepAfterGC(JSRuntime* rt);

    Data* getData() const {
        return static_cast<Data*>(getPrivate());
    }

    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
};

extern JSObject*
InitFinalizationRegistryClass(JSContext* cx, HandleObject obj);

extern JSObject*
InitBareFinalizationRegistryCtor(JSContext* cx, HandleObject obj);

} // namespace js

#endif /* builtin_FinalizationRegistryObject_h */
