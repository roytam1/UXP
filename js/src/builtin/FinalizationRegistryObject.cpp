/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/FinalizationRegistryObject.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "builtin/WeakRefObject.h"
#include "gc/Nursery.h"
#include "gc/Tracer.h"
#include "vm/EqualityOperations.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Symbol.h"

#include "jswrapper.h"
#include "jsobjinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

enum class WeakCellKind {
    Empty,
    Object,
    Symbol
};

static JSObject*
NormalizeWeakObject(JSObject* obj)
{
    if (JSObject* unwrapped = CheckedUnwrap(obj, /* stopAtWindowProxy = */ false))
        return unwrapped;
    return obj;
}

static bool
GetPrototypeFromFinalizationRegistryConstructor(JSContext* cx, HandleObject newTarget,
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
        if (!GlobalObject::ensureConstructor(cx, global, JSProto_FinalizationRegistry))
            return false;
        proto.set(&global->getPrototype(JSProto_FinalizationRegistry).toObject());
    }

    return cx->compartment()->wrap(cx, proto);
}

struct WeakCell {
    WeakCellKind kind;
    WeakRef<JSObject*> object;
    WeakRef<JS::Symbol*> symbol;

    WeakCell() : kind(WeakCellKind::Empty), object(nullptr), symbol(nullptr) {}
    explicit WeakCell(JSObject* obj) : kind(WeakCellKind::Object), object(obj), symbol(nullptr) {}
    explicit WeakCell(JS::Symbol* sym) : kind(WeakCellKind::Symbol), object(nullptr), symbol(sym) {}

    void clear() {
        kind = WeakCellKind::Empty;
        object = nullptr;
        symbol = nullptr;
    }

    bool isEmpty() const { return kind == WeakCellKind::Empty; }

    bool isDead() const {
        if (kind == WeakCellKind::Object)
            return !object.unbarrieredGet();
        if (kind == WeakCellKind::Symbol)
            return !symbol.unbarrieredGet();
        return false;
    }

    bool sameValue(HandleValue value) const {
        if (kind == WeakCellKind::Object)
            return value.isObject() && object.get() == NormalizeWeakObject(&value.toObject());
        if (kind == WeakCellKind::Symbol)
            return value.isSymbol() && symbol.get() == value.toSymbol();
        return false;
    }

    void trace(JSTracer* trc, const char* name) {
        if (kind == WeakCellKind::Object) {
            JSObject* target = object.unbarrieredGet();
            if (!target)
                return;

            if (IsInsideNursery(target)) {
                TraceManuallyBarrieredEdge(trc, object.unsafeGet(), name);
            } else {
                if (trc->isMarkingTracer() && !target->asTenured().zone()->isCollecting())
                    return;
                TraceWeakEdge(trc, &object, name);
            }
        } else if (kind == WeakCellKind::Symbol) {
            JS::Symbol* target = symbol.unbarrieredGet();
            if (target) {
                if (trc->isMarkingTracer() && !target->asTenured().zone()->isCollecting())
                    return;
                TraceWeakEdge(trc, &symbol, name);
            }
        }
    }

    void traceIfZoneIsCollecting(JSTracer* trc, const char* name) {
        if (kind == WeakCellKind::Object) {
            JSObject* target = object.unbarrieredGet();
            if (!target)
                return;

            if (IsInsideNursery(target)) {
                TraceManuallyBarrieredEdge(trc, object.unsafeGet(), name);
            } else if (target->asTenured().zone()->isCollecting()) {
                TraceWeakEdge(trc, &object, name);
            }
        } else if (kind == WeakCellKind::Symbol) {
            JS::Symbol* target = symbol.unbarrieredGet();
            if (target && target->asTenured().zone()->isCollecting())
                TraceWeakEdge(trc, &symbol, name);
        }
    }
};

struct FinalizationRecord {
    WeakCell target;
    JS::Heap<Value> heldValue;
    WeakCell unregisterToken;
    bool active;
    bool queued;

    FinalizationRecord(HandleValue targetValue, HandleValue heldValue,
                       HandleValue unregisterTokenValue)
      : heldValue(heldValue),
        active(true),
        queued(false)
    {
        if (targetValue.isObject())
            target = WeakCell(NormalizeWeakObject(&targetValue.toObject()));
        else
            target = WeakCell(targetValue.toSymbol());

        if (unregisterTokenValue.isObject())
            unregisterToken = WeakCell(NormalizeWeakObject(&unregisterTokenValue.toObject()));
        else if (unregisterTokenValue.isSymbol())
            unregisterToken = WeakCell(unregisterTokenValue.toSymbol());
    }

    void trace(JSTracer* trc) {
        if (active || queued)
            JS::TraceEdge(trc, &heldValue, "FinalizationRegistry held value");

        if (active) {
            target.trace(trc, "FinalizationRegistry target");
            if (!unregisterToken.isEmpty())
                unregisterToken.trace(trc, "FinalizationRegistry unregister token");
        }
    }

    void traceWeakEdgesForCollectedZones(JSTracer* trc) {
        if (!active)
            return;

        target.traceIfZoneIsCollecting(trc, "FinalizationRegistry target");
        if (!unregisterToken.isEmpty())
            unregisterToken.traceIfZoneIsCollecting(trc,
                                                    "FinalizationRegistry unregister token");
    }

    bool targetIsDead() const {
        return active && target.isDead();
    }

    bool matchesToken(HandleValue token) const {
        return active && !queued && !unregisterToken.isEmpty() &&
               unregisterToken.sameValue(token);
    }

    void queueForCleanup() {
        MOZ_ASSERT(active);
        MOZ_ASSERT(!queued);
        active = false;
        queued = true;
        target.clear();
        unregisterToken.clear();
    }

    void clear() {
        active = false;
        queued = false;
        target.clear();
        unregisterToken.clear();
        heldValue.setUndefined();
    }
};

using FinalizationRecordVector = Vector<FinalizationRecord*, 0, SystemAllocPolicy>;

struct FinalizationRegistryObject::Data {
    JS::Heap<JSObject*> cleanupCallback;
    JS::Heap<JSObject*> cleanupJob;
    FinalizationRecordVector records;
    FinalizationRecordVector cleanupQueue;
    bool queuedForCleanup;

    explicit Data(JSObject* cleanupCallback)
      : cleanupCallback(cleanupCallback),
        cleanupJob(nullptr),
        records(SystemAllocPolicy()),
        cleanupQueue(SystemAllocPolicy()),
        queuedForCleanup(false)
    {}

    ~Data() {
        for (size_t i = 0; i < records.length(); i++)
            js_delete(records[i]);
    }

    void trace(JSTracer* trc) {
        JS::TraceEdge(trc, &cleanupCallback, "FinalizationRegistry cleanup callback");
        if (cleanupJob.unbarrieredGet())
            JS::TraceEdge(trc, &cleanupJob, "FinalizationRegistry cleanup job");
        for (size_t i = 0; i < records.length(); i++)
            records[i]->trace(trc);
    }

    bool appendRecord(FinalizationRecord* record) {
        return records.append(record);
    }

    bool appendCleanupRecord(FinalizationRecord* record) {
        return cleanupQueue.append(record);
    }

    void compactRecords() {
        for (size_t i = 0; i < records.length();) {
            FinalizationRecord* record = records[i];
            if (!record->active && !record->queued) {
                js_delete(record);
                records.erase(records.begin() + i);
                continue;
            }
            i++;
        }
    }

    void unregister(HandleValue token, bool* removed) {
        *removed = false;
        for (size_t i = 0; i < records.length(); i++) {
            FinalizationRecord* record = records[i];
            if (record->matchesToken(token)) {
                record->clear();
                *removed = true;
            }
        }
        compactRecords();
    }
};

static FinalizationRegistryObject::Data*
GetData(JSObject* obj)
{
    return obj->as<FinalizationRegistryObject>().getData();
}

static MOZ_ALWAYS_INLINE bool
IsFinalizationRegistry(HandleValue v)
{
    return v.isObject() && v.toObject().is<FinalizationRegistryObject>();
}

static bool
FinalizationRegistry_register_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFinalizationRegistry(args.thisv()));

    if (!CanBeHeldWeakly(args.get(0))) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_BAD_FINALIZATION_REGISTRY_TARGET);
        return false;
    }

    RootedValue target(cx, args[0]);
    RootedValue heldValue(cx, args.get(1));

    bool same;
    if (!SameValue(cx, target, heldValue, &same))
        return false;
    if (same) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_BAD_FINALIZATION_REGISTRY_HELD_VALUE);
        return false;
    }

    RootedValue unregisterToken(cx, args.get(2));
    if (!CanBeHeldWeakly(unregisterToken)) {
        if (!unregisterToken.isUndefined()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_BAD_FINALIZATION_REGISTRY_TOKEN);
            return false;
        }
        unregisterToken.setUndefined();
    }

    FinalizationRecord* record = cx->new_<FinalizationRecord>(target, heldValue, unregisterToken);
    if (!record)
        return false;

    auto* data = GetData(&args.thisv().toObject());
    if (!data->appendRecord(record)) {
        js_delete(record);
        ReportOutOfMemory(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
FinalizationRegistry_unregister_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFinalizationRegistry(args.thisv()));

    RootedValue unregisterToken(cx, args.get(0));
    if (!CanBeHeldWeakly(unregisterToken)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_BAD_FINALIZATION_REGISTRY_TOKEN);
        return false;
    }

    bool removed = false;
    auto* data = GetData(&args.thisv().toObject());
    data->unregister(unregisterToken, &removed);

    args.rval().setBoolean(removed);
    return true;
}

const JSPropertySpec FinalizationRegistryObject::properties[] = {
    JS_PS_END
};

const JSFunctionSpec FinalizationRegistryObject::methods[] = {
    JS_FN("register", FinalizationRegistryObject::register_, 2, 0),
    JS_FN("unregister", FinalizationRegistryObject::unregister, 1, 0),
    JS_FS_END
};

static JSObject*
InitFinalizationRegistryClass(JSContext* cx, HandleObject obj, bool defineMembers)
{
    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    RootedPlainObject proto(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, GlobalObject::createConstructor(cx, FinalizationRegistryObject::construct,
                                                            ClassName(JSProto_FinalizationRegistry, cx), 1));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (defineMembers) {
        if (!DefinePropertiesAndFunctions(cx, proto, FinalizationRegistryObject::properties,
                                          FinalizationRegistryObject::methods)) {
            return nullptr;
        }
        if (!DefineToStringTag(cx, proto, cx->names().FinalizationRegistry))
            return nullptr;
    }

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_FinalizationRegistry, ctor, proto))
        return nullptr;
    return proto;
}

/* static */ FinalizationRegistryObject*
FinalizationRegistryObject::create(JSContext* cx, HandleObject cleanupCallback,
                                   HandleObject proto /* = nullptr */)
{
    Rooted<FinalizationRegistryObject*> obj(cx,
        NewObjectWithClassProto<FinalizationRegistryObject>(cx, proto));
    if (!obj)
        return nullptr;

    Data* data = cx->new_<Data>(cleanupCallback);
    if (!data)
        return nullptr;

    obj->setPrivate(data);

    RootedAtom funName(cx, cx->names().empty);
    RootedFunction cleanupJob(cx, NewNativeFunction(cx, FinalizationRegistryObject::cleanupJob, 0,
                                                    funName, gc::AllocKind::FUNCTION_EXTENDED,
                                                    GenericObject));
    if (!cleanupJob)
        return nullptr;

    cleanupJob->setExtendedSlot(0, ObjectValue(*obj));
    data->cleanupJob = cleanupJob;

    if (!cx->zone()->finalizationRegistries.append(WeakRef<JSObject*>(obj))) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    return obj;
}

/* static */ bool
FinalizationRegistryObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "FinalizationRegistry"))
        return false;

    RootedValue cleanupCallbackValue(cx, args.get(0));
    RootedObject cleanupCallback(cx, ValueToCallable(cx, cleanupCallbackValue, 0, NO_CONSTRUCT));
    if (!cleanupCallback)
        return false;

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromFinalizationRegistryConstructor(cx, newTarget, &proto))
        return false;

    Rooted<FinalizationRegistryObject*> obj(cx,
        FinalizationRegistryObject::create(cx, cleanupCallback, proto));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* static */ bool
FinalizationRegistryObject::register_(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFinalizationRegistry, FinalizationRegistry_register_impl>(cx, args);
}

/* static */ bool
FinalizationRegistryObject::unregister(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFinalizationRegistry, FinalizationRegistry_unregister_impl>(cx, args);
}

/* static */ bool
FinalizationRegistryObject::cleanupJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction job(cx, &args.callee().as<JSFunction>());
    Rooted<FinalizationRegistryObject*> registry(cx,
        &job->getExtendedSlot(0).toObject().as<FinalizationRegistryObject>());
    Data* data = registry->getData();
    if (!data) {
        args.rval().setUndefined();
        return true;
    }

    data->queuedForCleanup = false;

    RootedValue callback(cx, ObjectValue(*data->cleanupCallback.get()));
    RootedValue heldValue(cx);
    RootedValue ignored(cx);
    RootedValue undefined(cx, UndefinedValue());

    while (data->cleanupQueue.length() > 0) {
        FinalizationRecord* record = data->cleanupQueue[0];
        data->cleanupQueue.erase(data->cleanupQueue.begin());

        if (!record->queued) {
            data->compactRecords();
            continue;
        }

        heldValue.set(record->heldValue.get());
        record->clear();
        data->compactRecords();

        if (!Call(cx, callback, undefined, heldValue, &ignored))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

/* static */ void
FinalizationRegistryObject::trace(JSTracer* trc, JSObject* obj)
{
    if (Data* data = GetData(obj))
        data->trace(trc);
}

/* static */ void
FinalizationRegistryObject::finalize(FreeOp* fop, JSObject* obj)
{
    if (Data* data = GetData(obj))
        fop->delete_(data);
}

void
FinalizationRegistryObject::traceWeakEdgesForCollectedZones(JSTracer* trc)
{
    Data* data = getData();
    if (!data)
        return;

    for (size_t i = 0; i < data->records.length(); i++)
        data->records[i]->traceWeakEdgesForCollectedZones(trc);
}

void
FinalizationRegistryObject::sweepAfterGC(JSRuntime* rt)
{
    Data* data = getData();
    if (!data)
        return;

    bool needsCleanupJob = false;
    for (size_t i = 0; i < data->records.length(); i++) {
        FinalizationRecord* record = data->records[i];
        if (!record->targetIsDead())
            continue;

        record->queueForCleanup();
        if (!data->appendCleanupRecord(record)) {
            AutoEnterOOMUnsafeRegion oomUnsafe;
            oomUnsafe.crash("queueing FinalizationRegistry cleanup record");
        }
        needsCleanupJob = true;
    }

    if (needsCleanupJob && !data->queuedForCleanup) {
        JSContext* cx = rt->contextFromMainThread();
        RootedObject cleanupJob(cx, data->cleanupJob.unbarrieredGet());
        if (!rt->enqueueFinalizationRegistryCleanupJob(cx, cleanupJob)) {
            AutoEnterOOMUnsafeRegion oomUnsafe;
            oomUnsafe.crash("queueing FinalizationRegistry cleanup job");
        }
        data->queuedForCleanup = true;
    }

    data->compactRecords();
}

static const ClassOps FinalizationRegistryObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    FinalizationRegistryObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    FinalizationRegistryObject::trace
};

const Class FinalizationRegistryObject::class_ = {
    "FinalizationRegistry",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_FinalizationRegistry) |
    JSCLASS_FOREGROUND_FINALIZE,
    &FinalizationRegistryObjectClassOps
};

/* static */ JSObject*
FinalizationRegistryObject::initClass(JSContext* cx, HandleObject obj)
{
    return ::InitFinalizationRegistryClass(cx, obj, true);
}

JSObject*
js::InitFinalizationRegistryClass(JSContext* cx, HandleObject obj)
{
    return FinalizationRegistryObject::initClass(cx, obj);
}

JSObject*
js::InitBareFinalizationRegistryCtor(JSContext* cx, HandleObject obj)
{
    return ::InitFinalizationRegistryClass(cx, obj, false);
}
