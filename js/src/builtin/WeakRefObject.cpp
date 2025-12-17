/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/WeakRefObject.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "gc/Nursery.h"
#include "gc/Tracer.h"
#include "vm/GlobalObject.h"

#include "jsobjinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

static WeakRefObject::Referent*
GetReferent(JSObject* obj)
{
    return obj->as<WeakRefObject>().getData();
}

static MOZ_ALWAYS_INLINE bool
IsWeakRef(HandleValue v)
{
    return v.isObject() && v.toObject().is<WeakRefObject>();
}

static bool
WeakRef_deref_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakRef(args.thisv()));

    WeakRefObject::Referent* data = GetReferent(&args.thisv().toObject());
    JSObject* target = data ? data->target.get() : nullptr;
    if (target)
        args.rval().setObject(*target);
    else
        args.rval().setUndefined();
    return true;
}

const JSPropertySpec WeakRefObject::properties[] = {
    JS_PS_END
};

const JSFunctionSpec WeakRefObject::methods[] = {
    JS_FN("deref", WeakRefObject::deref, 0, 0),
    JS_FS_END
};

static JSObject*
InitWeakRefClass(JSContext* cx, HandleObject obj, bool defineMembers)
{
    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    RootedPlainObject proto(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, GlobalObject::createConstructor(cx, WeakRefObject::construct,
                                                            ClassName(JSProto_WeakRef, cx), 1));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (defineMembers) {
        if (!DefinePropertiesAndFunctions(cx, proto, WeakRefObject::properties, WeakRefObject::methods))
            return nullptr;
        if (!DefineToStringTag(cx, proto, cx->names().WeakRef))
            return nullptr;
    }

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_WeakRef, ctor, proto))
        return nullptr;
    return proto;
}

/* static */ WeakRefObject*
WeakRefObject::create(JSContext* cx, HandleObject target, HandleObject proto /* = nullptr */)
{
    Rooted<WeakRefObject*> obj(cx, NewObjectWithClassProto<WeakRefObject>(cx, proto));
    if (!obj)
        return nullptr;

    Referent* data = cx->new_<Referent>(target, cx->options().weakRefs());
    if (!data)
        return nullptr;

    obj->setPrivate(data);
    return obj;
}

/* static */ bool
WeakRefObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "WeakRef"))
        return false;

    if (!args.get(0).isObject()) {
        UniqueChars bytes =
            DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, args.get(0), nullptr);
        if (!bytes)
            return false;

        JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT,
                                   bytes.get());
        return false;
    }

    RootedObject target(cx, &args[0].toObject());

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    Rooted<WeakRefObject*> obj(cx, WeakRefObject::create(cx, target, proto));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* static */ bool
WeakRefObject::deref(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakRef, WeakRef_deref_impl>(cx, args);
}

/* static */ void
WeakRefObject::trace(JSTracer* trc, JSObject* obj)
{
    if (Referent* data = GetReferent(obj)) {
        JSObject* target = data->target.unbarrieredGet();
        if (!target)
            return;

        // When pref-disabled, keep referent alive via strong trace so deref()
        // stays usable as a stub without touching GC internals.
        if (!data->enabled) {
            TraceManuallyBarrieredEdge(trc, data->target.unsafeGet(), "WeakRef stub referent");
        } else if (IsInsideNursery(target)) {
            // Weak edges must be tenured; trace strongly while referent is in the nursery.
            TraceManuallyBarrieredEdge(trc, data->target.unsafeGet(), "WeakRef nursery referent");
        } else {
            TraceWeakEdge(trc, &data->target, "WeakRef referent");
        }
    }
}

/* static */ void
WeakRefObject::finalize(FreeOp* fop, JSObject* obj)
{
    if (Referent* data = GetReferent(obj))
        fop->delete_(data);
}

static const ClassOps WeakRefObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WeakRefObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WeakRefObject::trace
};

const Class WeakRefObject::class_ = {
    "WeakRef",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakRef) |
    JSCLASS_BACKGROUND_FINALIZE,
    &WeakRefObjectClassOps
};

/* static */ JSObject*
WeakRefObject::initClass(JSContext* cx, HandleObject obj)
{
    return ::InitWeakRefClass(cx, obj, true);
}

JSObject*
js::InitWeakRefClass(JSContext* cx, HandleObject obj)
{
    return WeakRefObject::initClass(cx, obj);
}

JSObject*
js::InitBareWeakRefCtor(JSContext* cx, HandleObject obj)
{
    return ::InitWeakRefClass(cx, obj, false);
}
