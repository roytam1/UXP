/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_WeakRefObject_h
#define builtin_WeakRefObject_h

#include "gc/Barrier.h"
#include "vm/NativeObject.h"

namespace js {

class WeakRefObject : public NativeObject
{
  public:
    struct Referent {
        explicit Referent(JSObject* obj, bool enabled)
          : target(obj), enabled(enabled) {}
        WeakRef<JSObject*> target;
        bool enabled;
    };

    static const Class class_;

    static JSObject* initClass(JSContext* cx, HandleObject obj);
    static WeakRefObject* create(JSContext* cx, HandleObject target, HandleObject proto = nullptr);

    static void trace(JSTracer* trc, JSObject* obj);
    static void finalize(FreeOp* fop, JSObject* obj);
    [[nodiscard]] static bool construct(JSContext* cx, unsigned argc, Value* vp);
    static bool deref(JSContext* cx, unsigned argc, Value* vp);

    Referent* getData() const {
        return static_cast<Referent*>(getPrivate());
    }

    WeakRef<JSObject*>& target() {
        MOZ_ASSERT(getData());
        return getData()->target;
    }

    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];

  private:
};

extern JSObject*
InitWeakRefClass(JSContext* cx, HandleObject obj);

extern JSObject*
InitBareWeakRefCtor(JSContext* cx, HandleObject obj);

} // namespace js

#endif /* builtin_WeakRefObject_h */
