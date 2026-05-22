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
#include "vm/Symbol.h"

#include "jswrapper.h"
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
GetPrototypeFromWeakRefConstructor(JSContext* cx, HandleObject newTarget,
                                   MutableHandleObject proto)
{
    if (!GetPrototypeFromConstructor(cx, newTarget, proto))
        return false;
    if (proto)
        return true;

    RootedObject realmObject(cx, CheckedUnwrap(newTarget, /* stopAtWindowProxy = */ false));
    if (!realmObject)
        return false;

    {
        JSAutoCompartment ac(cx, realmObject);
        Rooted<GlobalObject*> global(cx, &realmObject->global());
        if (!GlobalObject::ensureConstructor(cx, global, JSProto_WeakRef))
            return false;
        proto.set(&global->getPrototype(JSProto_WeakRef).toObject());
    }

    return cx->compartment()->wrap(cx, proto);
}

static bool
WeakRef_deref_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakRef(args.thisv()));

    WeakRefObject::Referent* data = GetReferent(&args.thisv().toObject());
    if (!data) {
        args.rval().setUndefined();
        return true;
    }

    if (data->isObject()) {
        JSObject* target = data->objectTarget.get();
        if (target) {
            RootedValue kept(cx, ObjectValue(*target));
            if (!cx->runtime()->addWeakRefKeptObject(cx, kept))
                return false;
            args.rval().set(kept);
        } else {
            args.rval().setUndefined();
        }
    } else {
        MOZ_ASSERT(data->isSymbol());
        JS::Symbol* target = data->symbolTarget.get();
        if (target) {
            RootedValue kept(cx, SymbolValue(target));
            if (!cx->runtime()->addWeakRefKeptObject(cx, kept))
                return false;
            args.rval().set(kept);
        } else {
            args.rval().setUndefined();
        }
    }

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
WeakRefObject::create(JSContext* cx, HandleValue target, HandleObject proto /* = nullptr */)
{
    Rooted<WeakRefObject*> obj(cx, NewObjectWithClassProto<WeakRefObject>(cx, proto));
    if (!obj)
        return nullptr;

    Referent* data;
    if (target.isObject())
        data = cx->new_<Referent>(&target.toObject());
    else
        data = cx->new_<Referent>(target.toSymbol());

    if (!data)
        return nullptr;

    obj->setPrivate(data);
    return obj;
}

bool
js::CanBeHeldWeakly(HandleValue target)
{
    if (target.isObject())
        return true;

    if (target.isSymbol())
        return target.toSymbol()->code() != JS::SymbolCode::InSymbolRegistry;

    return false;
}

/* static */ bool
WeakRefObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "WeakRef"))
        return false;

    if (!CanBeHeldWeakly(args.get(0))) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_WEAKREF_TARGET);
        return false;
    }

    RootedValue target(cx, args[0]);

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromWeakRefConstructor(cx, newTarget, &proto))
        return false;

    Rooted<WeakRefObject*> obj(cx, WeakRefObject::create(cx, target, proto));
    if (!obj)
        return false;

    if (!cx->runtime()->addWeakRefKeptObject(cx, target))
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
        if (data->isObject()) {
            JSObject* target = data->objectTarget.unbarrieredGet();
            if (!target)
                return;

            if (IsInsideNursery(target)) {
                // Weak edges must be tenured; trace strongly while referent is in the nursery.
                TraceManuallyBarrieredEdge(trc, data->objectTarget.unsafeGet(),
                                           "WeakRef nursery referent");
            } else {
                TraceWeakEdge(trc, &data->objectTarget, "WeakRef referent");
            }
        } else {
            MOZ_ASSERT(data->isSymbol());
            JS::Symbol* target = data->symbolTarget.unbarrieredGet();
            if (target)
                TraceWeakEdge(trc, &data->symbolTarget, "WeakRef symbol referent");
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
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakRef),
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
