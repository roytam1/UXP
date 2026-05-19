/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/SharedArrayObject.h"

#include "mozilla/Atomics.h"

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jsprf.h"

#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"
#ifndef XP_WIN
# include <sys/mman.h>
#endif
#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "vm/SharedMem.h"
#include "vm/TypedArrayCommon.h"
#include "wasm/AsmJS.h"
#include "wasm/WasmTypes.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

static inline void*
MapMemory(size_t length, bool commit)
{
#ifdef XP_WIN
    int prot = (commit ? MEM_COMMIT : MEM_RESERVE);
    int flags = (commit ? PAGE_READWRITE : PAGE_NOACCESS);
    return VirtualAlloc(nullptr, length, prot, flags);
#else
    int prot = (commit ? (PROT_READ | PROT_WRITE) : PROT_NONE);
    void* p = mmap(nullptr, length, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED)
        return nullptr;
    return p;
#endif
}

static inline void
UnmapMemory(void* addr, size_t len)
{
#ifdef XP_WIN
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, len);
#endif
}

static inline bool
MarkValidRegion(void* addr, size_t len)
{
#ifdef XP_WIN
    if (!VirtualAlloc(addr, len, MEM_COMMIT, PAGE_READWRITE))
        return false;
    return true;
#else
    if (mprotect(addr, len, PROT_READ | PROT_WRITE))
        return false;
    return true;
#endif
}

// Since this SharedArrayBuffer will likely be used for asm.js code, prepare it
// for asm.js by mapping the 4gb protected zone described in WasmTypes.h.
// Since we want to put the SharedArrayBuffer header immediately before the
// heap but keep the heap page-aligned, allocate an extra page before the heap.
static uint64_t
SharedArrayMappedSize(uint32_t allocSize)
{
    MOZ_RELEASE_ASSERT(sizeof(SharedArrayRawBuffer) < gc::SystemPageSize());
#ifdef WASM_HUGE_MEMORY
    return wasm::HugeMappedSize + gc::SystemPageSize();
#else
    return allocSize + wasm::GuardSize;
#endif
}

// If there are too many 4GB buffers live we run up against system resource
// exhaustion (address space or number of memory map descriptors), see
// bug 1068684, bug 1073934 for details.  The limiting case seems to be
// Windows Vista Home 64-bit, where the per-process address space is limited
// to 8TB.  Thus we track the number of live objects, and set a limit of
// 1000 live objects per process; we run synchronous GC if necessary; and
// we throw an OOM error if the per-process limit is exceeded.
static mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> numLive;
static const uint32_t maxLive = 1000;

static uint32_t
SharedArrayAllocSize(uint32_t length)
{
    return AlignBytes(length + gc::SystemPageSize(), gc::SystemPageSize());
}

SharedArrayRawBuffer*
SharedArrayRawBuffer::New(JSContext* cx, uint32_t length, uint32_t maxLength, bool growable)
{
    // The value (uint32_t)-1 is used as a signal in various places,
    // so guard against it on principle.
    MOZ_ASSERT(length != (uint32_t)-1);
    MOZ_ASSERT(maxLength != (uint32_t)-1);
    MOZ_ASSERT(maxLength >= length);

    // Add a page for the header and round to a page boundary.
    uint32_t allocationLength = growable ? maxLength : length;
    uint32_t allocSize = SharedArrayAllocSize(allocationLength);
    if (allocSize <= allocationLength)
        return nullptr;

    // Test >= to guard against the case where multiple extant runtimes
    // race to allocate.
    if (++numLive >= maxLive) {
        JSRuntime* rt = cx->runtime();
        if (rt->largeAllocationFailureCallback)
            rt->largeAllocationFailureCallback(rt->largeAllocationFailureCallbackData);
        if (numLive >= maxLive) {
            numLive--;
            return nullptr;
        }
    }

    bool preparedForAsmJS =
        !growable && jit::JitOptions.asmJSAtomicsEnable && IsValidAsmJSHeapLength(length);

    void* p = nullptr;
    if (preparedForAsmJS) {
        uint32_t mappedSize = SharedArrayMappedSize(allocSize);

        // Get the entire reserved region (with all pages inaccessible)
        p = MapMemory(mappedSize, false);
        if (!p) {
            numLive--;
            return nullptr;
        }

        if (!MarkValidRegion(p, allocSize)) {
            UnmapMemory(p, mappedSize);
            numLive--;
            return nullptr;
        }

# if defined(MOZ_VALGRIND) && defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
        // Tell Valgrind/Memcheck to not report accesses in the inaccessible region.
        VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)p + allocSize,
                                                       mappedSize - allocSize);
# endif
    } else {
        p = MapMemory(allocSize, true);
        if (!p) {
            numLive--;
            return nullptr;
        }
    }

    uint8_t* buffer = reinterpret_cast<uint8_t*>(p) + gc::SystemPageSize();
    uint8_t* base = buffer - sizeof(SharedArrayRawBuffer);
    SharedArrayRawBuffer* rawbuf =
        new (base) SharedArrayRawBuffer(buffer, length, maxLength, growable, preparedForAsmJS);
    MOZ_ASSERT(rawbuf->allocatedByteLength() == allocationLength); // Deallocation needs this.
    return rawbuf;
}

bool
SharedArrayRawBuffer::addReference()
{
    MOZ_ASSERT(this->refcount_ > 0);

    // Be careful never to overflow the refcount field.
    for (;;) {
        uint32_t old_refcount = this->refcount_;
        uint32_t new_refcount = old_refcount+1;
        if (new_refcount == 0)
            return false;
        if (this->refcount_.compareExchange(old_refcount, new_refcount))
            return true;
    }
}

void
SharedArrayRawBuffer::dropReference()
{
    // Normally if the refcount is zero then the memory will have been unmapped
    // and this test may just crash, but if the memory has been retained for any
    // reason we will catch the underflow here.
    MOZ_RELEASE_ASSERT(this->refcount_ > 0);

    // Drop the reference to the buffer.
    uint32_t refcount = --this->refcount_; // Atomic.
    if (refcount)
        return;

    // If this was the final reference, release the buffer.

    SharedMem<uint8_t*> p = this->dataPointerShared() - gc::SystemPageSize();
    MOZ_ASSERT(p.asValue() % gc::SystemPageSize() == 0);

    uint8_t* address = p.unwrap(/*safe - only reference*/);
    uint32_t allocSize = SharedArrayAllocSize(this->allocatedByteLength());

    if (this->preparedForAsmJS) {
        uint32_t mappedSize = SharedArrayMappedSize(allocSize);
        UnmapMemory(address, mappedSize);

# if defined(MOZ_VALGRIND) && defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
        // Tell Valgrind/Memcheck to recommence reporting accesses in the
        // previously-inaccessible region.
        VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(address, mappedSize);
# endif
    } else {
        UnmapMemory(address, allocSize);
    }

    // Decrement the buffer counter at the end -- otherwise, a race condition
    // could enable the creation of unlimited buffers.
    numLive--;
}

bool
SharedArrayRawBuffer::growTo(uint32_t newLength)
{
    MOZ_ASSERT(growable);
    MOZ_ASSERT(newLength <= maxLength);

    for (;;) {
        uint32_t oldLength = length;
        if (newLength < oldLength)
            return false;
        if (newLength == oldLength)
            return true;
        if (length.compareExchange(oldLength, newLength))
            return true;
    }
}


MOZ_ALWAYS_INLINE bool
SharedArrayBufferObject::byteLengthGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsSharedArrayBuffer(args.thisv()));
    args.rval().setInt32(args.thisv().toObject().as<SharedArrayBufferObject>().byteLength());
    return true;
}

bool
SharedArrayBufferObject::byteLengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsSharedArrayBuffer, byteLengthGetterImpl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
SharedArrayBufferObject::maxByteLengthGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsSharedArrayBuffer(args.thisv()));
    args.rval().setInt32(args.thisv().toObject().as<SharedArrayBufferObject>().maxByteLength());
    return true;
}

bool
SharedArrayBufferObject::maxByteLengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsSharedArrayBuffer, maxByteLengthGetterImpl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
SharedArrayBufferObject::growableGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsSharedArrayBuffer(args.thisv()));
    args.rval().setBoolean(args.thisv().toObject().as<SharedArrayBufferObject>().isGrowable());
    return true;
}

bool
SharedArrayBufferObject::growableGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsSharedArrayBuffer, growableGetterImpl>(cx, args);
}

static bool
ReportSharedArrayBufferNotGrowable(JSContext* cx)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SHARED_ARRAY_NOT_GROWABLE);
    return false;
}

static bool
ReportSharedArrayBufferLengthOutOfRange(JSContext* cx)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SHARED_ARRAY_BAD_LENGTH);
    return false;
}

static bool
GetSharedArrayBufferMaxByteLengthOption(JSContext* cx, HandleValue options,
                                        uint32_t byteLength, uint32_t* maxByteLength,
                                        bool* growable)
{
    *maxByteLength = byteLength;
    *growable = false;

    if (!options.isObject())
        return true;

    RootedObject opts(cx, &options.toObject());
    RootedValue maxByteLengthValue(cx);
    if (!GetProperty(cx, opts, opts, cx->names().maxByteLength, &maxByteLengthValue))
        return false;

    if (maxByteLengthValue.isUndefined())
        return true;

    uint64_t max;
    if (!ToIndex(cx, maxByteLengthValue, &max))
        return false;

    if (max > INT32_MAX || max < byteLength)
        return ReportSharedArrayBufferLengthOutOfRange(cx);

    *maxByteLength = uint32_t(max);
    *growable = true;
    return true;
}

MOZ_ALWAYS_INLINE bool
SharedArrayBufferObject::fun_grow_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsSharedArrayBuffer(args.thisv()));

    Rooted<SharedArrayBufferObject*> buffer(cx,
        &args.thisv().toObject().as<SharedArrayBufferObject>());
    if (!buffer->isGrowable())
        return ReportSharedArrayBufferNotGrowable(cx);

    uint64_t newByteLength;
    if (!ToIndex(cx, args.get(0), &newByteLength))
        return false;
    if (newByteLength > INT32_MAX)
        return ReportSharedArrayBufferLengthOutOfRange(cx);

    uint32_t newLength = uint32_t(newByteLength);
    if (newLength > buffer->maxByteLength() || !buffer->growTo(newLength))
        return ReportSharedArrayBufferLengthOutOfRange(cx);

    args.rval().setUndefined();
    return true;
}

bool
SharedArrayBufferObject::fun_grow(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsSharedArrayBuffer, fun_grow_impl>(cx, args);
}

bool
SharedArrayBufferObject::class_constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "SharedArrayBuffer"))
        return false;

    uint64_t length64;
    if (!ToIndex(cx, args.get(0), &length64))
        return false;
    // Bugs 1068458, 1161298: Limit length to 2^31-1.
    if (length64 > INT32_MAX)
        return ReportSharedArrayBufferLengthOutOfRange(cx);
    uint32_t length = uint32_t(length64);

    uint32_t maxByteLength;
    bool growable;
    if (!GetSharedArrayBufferMaxByteLengthOption(cx, args.get(1), length,
                                                 &maxByteLength, &growable))
    {
        return false;
    }

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    JSObject* bufobj = New(cx, length, maxByteLength, growable, proto);
    if (!bufobj)
        return false;
    args.rval().setObject(*bufobj);
    return true;
}

SharedArrayBufferObject*
SharedArrayBufferObject::New(JSContext* cx, uint32_t length, uint32_t maxLength, bool growable,
                             HandleObject proto)
{
    SharedArrayRawBuffer* buffer = SharedArrayRawBuffer::New(cx, length, maxLength, growable);
    if (!buffer)
        return nullptr;

    return New(cx, buffer, proto);
}

SharedArrayBufferObject*
SharedArrayBufferObject::New(JSContext* cx, SharedArrayRawBuffer* buffer, HandleObject proto)
{
    MOZ_ASSERT(cx->compartment()->creationOptions().getSharedMemoryAndAtomicsEnabled());

    AutoSetNewObjectMetadata metadata(cx);
    Rooted<SharedArrayBufferObject*> obj(cx,
        NewObjectWithClassProto<SharedArrayBufferObject>(cx, proto));
    if (!obj)
        return nullptr;

    MOZ_ASSERT(obj->getClass() == &class_);

    obj->acceptRawBuffer(buffer);

    return obj;
}

void
SharedArrayBufferObject::acceptRawBuffer(SharedArrayRawBuffer* buffer)
{
    setReservedSlot(RAWBUF_SLOT, PrivateValue(buffer));
}

void
SharedArrayBufferObject::dropRawBuffer()
{
    setReservedSlot(RAWBUF_SLOT, UndefinedValue());
}

SharedArrayRawBuffer*
SharedArrayBufferObject::rawBufferObject() const
{
    Value v = getReservedSlot(RAWBUF_SLOT);
    MOZ_ASSERT(!v.isUndefined());
    return reinterpret_cast<SharedArrayRawBuffer*>(v.toPrivate());
}

void
SharedArrayBufferObject::Finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->maybeOffMainThread());

    SharedArrayBufferObject& buf = obj->as<SharedArrayBufferObject>();

    // Detect the case of failure during SharedArrayBufferObject creation,
    // which causes a SharedArrayRawBuffer to never be attached.
    Value v = buf.getReservedSlot(RAWBUF_SLOT);
    if (!v.isUndefined()) {
        buf.rawBufferObject()->dropReference();
        buf.dropRawBuffer();
    }
}

/* static */ void
SharedArrayBufferObject::addSizeOfExcludingThis(JSObject* obj, mozilla::MallocSizeOf mallocSizeOf,
                                                JS::ClassInfo* info)
{
    // Divide the buffer size by the refcount to get the fraction of the buffer
    // owned by this thread. It's conceivable that the refcount might change in
    // the middle of memory reporting, in which case the amount reported for
    // some threads might be to high (if the refcount goes up) or too low (if
    // the refcount goes down). But that's unlikely and hard to avoid, so we
    // just live with the risk.
    const SharedArrayBufferObject& buf = obj->as<SharedArrayBufferObject>();
    info->objectsNonHeapElementsShared +=
        buf.rawBufferObject()->allocatedByteLength() / buf.rawBufferObject()->refcount();
}

/* static */ void
SharedArrayBufferObject::copyData(Handle<SharedArrayBufferObject*> toBuffer, uint32_t toIndex,
                                  Handle<SharedArrayBufferObject*> fromBuffer, uint32_t fromIndex,
                                  uint32_t count)
{
    MOZ_ASSERT(toBuffer->byteLength() >= count);
    MOZ_ASSERT(toBuffer->byteLength() >= toIndex + count);
    MOZ_ASSERT(fromBuffer->byteLength() >= fromIndex);
    MOZ_ASSERT(fromBuffer->byteLength() >= fromIndex + count);

    jit::AtomicOperations::memcpySafeWhenRacy(toBuffer->dataPointerShared() + toIndex,
                                              fromBuffer->dataPointerShared() + fromIndex,
                                              count);
}

static const ClassSpec SharedArrayBufferObjectProtoClassSpec = {
    DELEGATED_CLASSSPEC(SharedArrayBufferObject::class_.spec),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ClassSpec::IsDelegated
};

static const Class SharedArrayBufferObjectProtoClass = {
    "SharedArrayBufferPrototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_SharedArrayBuffer),
    JS_NULL_CLASS_OPS,
    &SharedArrayBufferObjectProtoClassSpec
};

static JSObject*
CreateSharedArrayBufferPrototype(JSContext* cx, JSProtoKey key)
{
    return GlobalObject::createBlankPrototype(cx, cx->global(),
                                              &SharedArrayBufferObjectProtoClass);
}

static const ClassOps SharedArrayBufferObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    SharedArrayBufferObject::Finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* trace */
};

static const JSFunctionSpec static_functions[] = {
    JS_FS_END
};

static const JSPropertySpec static_properties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "SharedArrayBufferSpecies", 0),
    JS_PS_END
};

static const JSFunctionSpec prototype_functions[] = {
    JS_FN("grow", SharedArrayBufferObject::fun_grow, 1, 0),
    JS_SELF_HOSTED_FN("slice", "SharedArrayBufferSlice", 2, 0),
    JS_FS_END
};

static const JSPropertySpec prototype_properties[] = {
    JS_PSG("byteLength", SharedArrayBufferObject::byteLengthGetter, 0),
    JS_PSG("growable", SharedArrayBufferObject::growableGetter, 0),
    JS_PSG("maxByteLength", SharedArrayBufferObject::maxByteLengthGetter, 0),
    JS_STRING_SYM_PS(toStringTag, "SharedArrayBuffer", JSPROP_READONLY),
    JS_PS_END
};

static const ClassSpec ArrayBufferObjectClassSpec = {
    GenericCreateConstructor<SharedArrayBufferObject::class_constructor, 1, gc::AllocKind::FUNCTION>,
    CreateSharedArrayBufferPrototype,
    static_functions,
    static_properties,
    prototype_functions,
    prototype_properties
};

const Class SharedArrayBufferObject::class_ = {
    "SharedArrayBuffer",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(SharedArrayBufferObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_SharedArrayBuffer) |
    JSCLASS_BACKGROUND_FINALIZE,
    &SharedArrayBufferObjectClassOps,
    &ArrayBufferObjectClassSpec,
    JS_NULL_CLASS_EXT
};

bool
js::IsSharedArrayBuffer(HandleValue v)
{
    return v.isObject() && v.toObject().is<SharedArrayBufferObject>();
}

bool
js::IsSharedArrayBuffer(HandleObject o)
{
    return o->is<SharedArrayBufferObject>();
}

bool
js::IsSharedArrayBuffer(JSObject* o)
{
    return o->is<SharedArrayBufferObject>();
}

SharedArrayBufferObject&
js::AsSharedArrayBuffer(HandleObject obj)
{
    MOZ_ASSERT(IsSharedArrayBuffer(obj));
    return obj->as<SharedArrayBufferObject>();
}

JS_FRIEND_API(uint32_t)
JS_GetSharedArrayBufferByteLength(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->as<SharedArrayBufferObject>().byteLength() : 0;
}

JS_FRIEND_API(void)
js::GetSharedArrayBufferLengthAndData(JSObject* obj, uint32_t* length, bool* isSharedMemory, uint8_t** data)
{
    MOZ_ASSERT(obj->is<SharedArrayBufferObject>());
    *length = obj->as<SharedArrayBufferObject>().byteLength();
    *data = obj->as<SharedArrayBufferObject>().dataPointerShared().unwrap(/*safe - caller knows*/);
    *isSharedMemory = true;
}

JS_FRIEND_API(JSObject*)
JS_NewSharedArrayBuffer(JSContext* cx, uint32_t nbytes)
{
    MOZ_ASSERT(cx->compartment()->creationOptions().getSharedMemoryAndAtomicsEnabled());

    MOZ_ASSERT(nbytes <= INT32_MAX);
    return SharedArrayBufferObject::New(cx, nbytes, /* proto = */ nullptr);
}

JS_FRIEND_API(bool)
JS_IsSharedArrayBufferObject(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<SharedArrayBufferObject>() : false;
}

JS_FRIEND_API(uint8_t*)
JS_GetSharedArrayBufferData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    *isSharedMemory = true;
    return obj->as<SharedArrayBufferObject>().dataPointerShared().unwrap(/*safe - caller knows*/);
}
