/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ErrorObject-inl.h"

#include "mozilla/Range.h"

#include "jsexn.h"

#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "vm/StringBuffer.h"
#include "vm/GlobalObject.h"
#include "vm/String.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"
#include "vm/SavedStacks-inl.h"
#include "vm/Shape-inl.h"

using namespace js;

#define IMPLEMENT_ERROR_PROTO_CLASS(name) \
    { \
        js_Object_str, \
        JSCLASS_HAS_CACHED_PROTO(JSProto_##name), \
        JS_NULL_CLASS_OPS, \
        &ErrorObject::classSpecs[JSProto_##name - JSProto_Error] \
    }

const Class
ErrorObject::protoClasses[JSEXN_ERROR_LIMIT] = {
    IMPLEMENT_ERROR_PROTO_CLASS(Error),

    IMPLEMENT_ERROR_PROTO_CLASS(InternalError),
    IMPLEMENT_ERROR_PROTO_CLASS(EvalError),
    IMPLEMENT_ERROR_PROTO_CLASS(RangeError),
    IMPLEMENT_ERROR_PROTO_CLASS(ReferenceError),
    IMPLEMENT_ERROR_PROTO_CLASS(SyntaxError),
    IMPLEMENT_ERROR_PROTO_CLASS(TypeError),
    IMPLEMENT_ERROR_PROTO_CLASS(URIError),

    IMPLEMENT_ERROR_PROTO_CLASS(DebuggeeWouldRun),
    IMPLEMENT_ERROR_PROTO_CLASS(CompileError),
    IMPLEMENT_ERROR_PROTO_CLASS(RuntimeError)
};

static bool
exn_toSource(JSContext* cx, unsigned argc, Value* vp);

static const JSFunctionSpec error_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, exn_toSource, 0, 0),
#endif
    JS_SELF_HOSTED_FN(js_toString_str, "ErrorToString", 0,0),
    JS_FS_END
};

static const JSPropertySpec error_properties[] = {
    JS_STRING_PS("message", "", 0),
    JS_STRING_PS("name", "Error", 0),
    // Only Error.prototype has .stack!
    JS_PSGS("stack", ErrorObject::getStack, ErrorObject::setStack, 0),
    JS_PS_END
};

#define IMPLEMENT_ERROR_PROPERTIES(name) \
    { \
        JS_STRING_PS("message", "", 0), \
        JS_STRING_PS("name", #name, 0), \
        JS_PS_END \
    }

static const JSPropertySpec other_error_properties[JSEXN_ERROR_LIMIT - 1][3] = {
    IMPLEMENT_ERROR_PROPERTIES(InternalError),
    IMPLEMENT_ERROR_PROPERTIES(EvalError),
    IMPLEMENT_ERROR_PROPERTIES(RangeError),
    IMPLEMENT_ERROR_PROPERTIES(ReferenceError),
    IMPLEMENT_ERROR_PROPERTIES(SyntaxError),
    IMPLEMENT_ERROR_PROPERTIES(TypeError),
    IMPLEMENT_ERROR_PROPERTIES(URIError),
    IMPLEMENT_ERROR_PROPERTIES(DebuggeeWouldRun),
    IMPLEMENT_ERROR_PROPERTIES(CompileError),
    IMPLEMENT_ERROR_PROPERTIES(RuntimeError)
};

#define IMPLEMENT_NATIVE_ERROR_SPEC(name) \
    { \
        ErrorObject::createConstructor, \
        ErrorObject::createProto, \
        nullptr, \
        nullptr, \
        nullptr, \
        other_error_properties[JSProto_##name - JSProto_Error - 1], \
        nullptr, \
        JSProto_Error \
    }

#define IMPLEMENT_NONGLOBAL_ERROR_SPEC(name) \
    { \
        ErrorObject::createConstructor, \
        ErrorObject::createProto, \
        nullptr, \
        nullptr, \
        nullptr, \
        other_error_properties[JSProto_##name - JSProto_Error - 1], \
        nullptr, \
        JSProto_Error | ClassSpec::DontDefineConstructor \
    }

const ClassSpec
ErrorObject::classSpecs[JSEXN_ERROR_LIMIT] = {
    {
        ErrorObject::createConstructor,
        ErrorObject::createProto,
        nullptr,
        nullptr,
        error_methods,
        error_properties
    },

    IMPLEMENT_NATIVE_ERROR_SPEC(InternalError),
    IMPLEMENT_NATIVE_ERROR_SPEC(EvalError),
    IMPLEMENT_NATIVE_ERROR_SPEC(RangeError),
    IMPLEMENT_NATIVE_ERROR_SPEC(ReferenceError),
    IMPLEMENT_NATIVE_ERROR_SPEC(SyntaxError),
    IMPLEMENT_NATIVE_ERROR_SPEC(TypeError),
    IMPLEMENT_NATIVE_ERROR_SPEC(URIError),

    IMPLEMENT_NONGLOBAL_ERROR_SPEC(DebuggeeWouldRun),
    IMPLEMENT_NONGLOBAL_ERROR_SPEC(CompileError),
    IMPLEMENT_NONGLOBAL_ERROR_SPEC(RuntimeError)
};

#define IMPLEMENT_ERROR_CLASS(name) \
    { \
        js_Error_str, /* yes, really */ \
        JSCLASS_HAS_CACHED_PROTO(JSProto_##name) | \
        JSCLASS_HAS_RESERVED_SLOTS(ErrorObject::RESERVED_SLOTS) | \
        JSCLASS_BACKGROUND_FINALIZE, \
        &ErrorObjectClassOps, \
        &ErrorObject::classSpecs[JSProto_##name - JSProto_Error ] \
    }

static void
exn_finalize(FreeOp* fop, JSObject* obj);

static const ClassOps ErrorObjectClassOps = {
    nullptr,                 /* addProperty */
    nullptr,                 /* delProperty */
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    nullptr,                 /* enumerate */
    nullptr,                 /* resolve */
    nullptr,                 /* mayResolve */
    exn_finalize,
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    nullptr,                 /* trace       */
};

const Class
ErrorObject::classes[JSEXN_ERROR_LIMIT] = {
    IMPLEMENT_ERROR_CLASS(Error),
    IMPLEMENT_ERROR_CLASS(InternalError),
    IMPLEMENT_ERROR_CLASS(EvalError),
    IMPLEMENT_ERROR_CLASS(RangeError),
    IMPLEMENT_ERROR_CLASS(ReferenceError),
    IMPLEMENT_ERROR_CLASS(SyntaxError),
    IMPLEMENT_ERROR_CLASS(TypeError),
    IMPLEMENT_ERROR_CLASS(URIError),
    // These Error subclasses are not accessible via the global object:
    IMPLEMENT_ERROR_CLASS(DebuggeeWouldRun),
    IMPLEMENT_ERROR_CLASS(CompileError),
    IMPLEMENT_ERROR_CLASS(RuntimeError)
};

static void
exn_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->maybeOffMainThread());
    if (JSErrorReport* report = obj->as<ErrorObject>().getErrorReport())
        fop->delete_(report);
}

bool
Error(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // ES6 19.5.1.1 mandates the .prototype lookup happens before the toString
    RootedObject proto(cx);
    if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    /* Compute the error message, if any. */
    RootedString message(cx, nullptr);
    if (args.hasDefined(0)) {
        message = ToString<CanGC>(cx, args[0]);
        if (!message)
            return false;
    }

    /* Find the scripted caller, but only ones we're allowed to know about. */
    NonBuiltinFrameIter iter(cx, cx->compartment()->principals());

    /* Set the 'fileName' property. */
    RootedString fileName(cx);
    if (args.length() > 1) {
        fileName = ToString<CanGC>(cx, args[1]);
    } else {
        fileName = cx->runtime()->emptyString;
        if (!iter.done()) {
            if (const char* cfilename = iter.filename())
                fileName = JS_NewStringCopyZ(cx, cfilename);
        }
    }
    if (!fileName)
        return false;

    /* Set the 'lineNumber' property. */
    uint32_t lineNumber, columnNumber = 0;
    if (args.length() > 2) {
        if (!ToUint32(cx, args[2], &lineNumber))
            return false;
    } else {
        lineNumber = iter.done() ? 0 : iter.computeLine(&columnNumber);
        // XXX: Make the column 1-based as in other browsers, instead of 0-based
        // which is how SpiderMonkey stores it internally. This will be
        // unnecessary once bug 1144340 is fixed.
        ++columnNumber;
    }

    RootedObject stack(cx);
    if (!CaptureStack(cx, &stack))
        return false;

    /*
     * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
     * called as functions, without operator new.  But as we do not give
     * each constructor a distinct JSClass, we must get the exception type
     * ourselves.
     */
    JSExnType exnType = JSExnType(args.callee().as<JSFunction>().getExtendedSlot(0).toInt32());

    RootedObject obj(cx, ErrorObject::create(cx, exnType, stack, fileName,
                                             lineNumber, columnNumber, nullptr, message, proto));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* static */ JSObject*
ErrorObject::createProto(JSContext* cx, JSProtoKey key)
{
    JSExnType type = ExnTypeFromProtoKey(key);

    if (type == JSEXN_ERR) {
        return GlobalObject::createBlankPrototype(cx, cx->global(),
                                                  &ErrorObject::protoClasses[JSEXN_ERR]);
    }

    RootedObject protoProto(cx, GlobalObject::getOrCreateErrorPrototype(cx, cx->global()));
    if (!protoProto)
        return nullptr;

    return GlobalObject::createBlankPrototypeInheriting(cx, cx->global(),
                                                        &ErrorObject::protoClasses[type],
                                                        protoProto);
}

/* static */ JSObject*
ErrorObject::createConstructor(JSContext* cx, JSProtoKey key)
{
    JSExnType type = ExnTypeFromProtoKey(key);
    RootedObject ctor(cx);

    if (type == JSEXN_ERR) {
        ctor = GenericCreateConstructor<Error, 1, gc::AllocKind::FUNCTION_EXTENDED>(cx, key);
    } else {
        RootedFunction proto(cx, GlobalObject::getOrCreateErrorConstructor(cx, cx->global()));
        if (!proto)
            return nullptr;

        ctor = NewFunctionWithProto(cx, Error, 1, JSFunction::NATIVE_CTOR, nullptr,
                                    ClassName(key, cx), proto, gc::AllocKind::FUNCTION_EXTENDED,
                                    SingletonObject);
    }

    if (!ctor)
        return nullptr;

    ctor->as<JSFunction>().setExtendedSlot(0, Int32Value(type));
    return ctor;
}

/* static */ Shape*
js::ErrorObject::assignInitialShape(ExclusiveContext* cx, Handle<ErrorObject*> obj)
{
    MOZ_ASSERT(obj->empty());

    if (!obj->addDataProperty(cx, cx->names().fileName, FILENAME_SLOT, 0))
        return nullptr;
    if (!obj->addDataProperty(cx, cx->names().lineNumber, LINENUMBER_SLOT, 0))
        return nullptr;
    return obj->addDataProperty(cx, cx->names().columnNumber, COLUMNNUMBER_SLOT, 0);
}

/* static */ bool
js::ErrorObject::init(JSContext* cx, Handle<ErrorObject*> obj, JSExnType type,
                      ScopedJSFreePtr<JSErrorReport>* errorReport, HandleString fileName,
                      HandleObject stack, uint32_t lineNumber, uint32_t columnNumber,
                      HandleString message)
{
    AssertObjectIsSavedFrameOrWrapper(cx, stack);
    assertSameCompartment(cx, obj, stack);

    // Null out early in case of error, for exn_finalize's sake.
    obj->initReservedSlot(ERROR_REPORT_SLOT, PrivateValue(nullptr));

    if (!EmptyShape::ensureInitialCustomShape<ErrorObject>(cx, obj))
        return false;

    // The .message property isn't part of the initial shape because it's
    // present in some error objects -- |Error.prototype|, |new Error("f")|,
    // |new Error("")| -- but not in others -- |new Error(undefined)|,
    // |new Error()|.
    RootedShape messageShape(cx);
    if (message) {
        messageShape = obj->addDataProperty(cx, cx->names().message, MESSAGE_SLOT, 0);
        if (!messageShape)
            return false;
        MOZ_ASSERT(messageShape->slot() == MESSAGE_SLOT);
    }

    MOZ_ASSERT(obj->lookupPure(NameToId(cx->names().fileName))->slot() == FILENAME_SLOT);
    MOZ_ASSERT(obj->lookupPure(NameToId(cx->names().lineNumber))->slot() == LINENUMBER_SLOT);
    MOZ_ASSERT(obj->lookupPure(NameToId(cx->names().columnNumber))->slot() ==
               COLUMNNUMBER_SLOT);
    MOZ_ASSERT_IF(message,
                  obj->lookupPure(NameToId(cx->names().message))->slot() == MESSAGE_SLOT);

    MOZ_ASSERT(JSEXN_ERR <= type && type < JSEXN_LIMIT);

    JSErrorReport* report = errorReport ? errorReport->forget() : nullptr;
    obj->initReservedSlot(EXNTYPE_SLOT, Int32Value(type));
    obj->initReservedSlot(STACK_SLOT, ObjectOrNullValue(stack));
    obj->setReservedSlot(ERROR_REPORT_SLOT, PrivateValue(report));
    obj->initReservedSlot(FILENAME_SLOT, StringValue(fileName));
    obj->initReservedSlot(LINENUMBER_SLOT, Int32Value(lineNumber));
    obj->initReservedSlot(COLUMNNUMBER_SLOT, Int32Value(columnNumber));
    if (message)
        obj->setSlotWithType(cx, messageShape, StringValue(message));

    return true;
}

/* static */ ErrorObject*
js::ErrorObject::create(JSContext* cx, JSExnType errorType, HandleObject stack,
                        HandleString fileName, uint32_t lineNumber, uint32_t columnNumber,
                        ScopedJSFreePtr<JSErrorReport>* report, HandleString message,
                        HandleObject protoArg /* = nullptr */)
{
    AssertObjectIsSavedFrameOrWrapper(cx, stack);

    RootedObject proto(cx, protoArg);
    if (!proto) {
        proto = GlobalObject::getOrCreateCustomErrorPrototype(cx, cx->global(), errorType);
        if (!proto)
            return nullptr;
    }

    Rooted<ErrorObject*> errObject(cx);
    {
        const Class* clasp = ErrorObject::classForType(errorType);
        JSObject* obj = NewObjectWithGivenProto(cx, clasp, proto);
        if (!obj)
            return nullptr;
        errObject = &obj->as<ErrorObject>();
    }

    if (!ErrorObject::init(cx, errObject, errorType, report, fileName, stack,
                           lineNumber, columnNumber, message))
    {
        return nullptr;
    }

    return errObject;
}

JSErrorReport*
js::ErrorObject::getOrCreateErrorReport(JSContext* cx)
{
    if (JSErrorReport* r = getErrorReport())
        return r;

    // We build an error report on the stack and then use CopyErrorReport to do
    // the nitty-gritty malloc stuff.
    JSErrorReport report;

    // Type.
    JSExnType type_ = type();
    report.exnType = type_;

    // Filename.
    JSAutoByteString filenameStr;
    if (!filenameStr.encodeLatin1(cx, fileName(cx)))
        return nullptr;
    report.filename = filenameStr.ptr();

    // Coordinates.
    report.lineno = lineNumber();
    report.column = columnNumber();

    // Message. Note that |new Error()| will result in an undefined |message|
    // slot, so we need to explicitly substitute the empty string in that case.
    RootedString message(cx, getMessage());
    if (!message)
        message = cx->runtime()->emptyString;
    if (!message->ensureFlat(cx))
        return nullptr;

    UniquePtr<char[], JS::FreePolicy> utf8 = StringToNewUTF8CharsZ(cx, *message);
    if (!utf8)
        return nullptr;
    report.initOwnedMessage(utf8.release());

    // Cache and return.
    JSErrorReport* copy = CopyErrorReport(cx, &report);
    if (!copy)
        return nullptr;
    setReservedSlot(ERROR_REPORT_SLOT, PrivateValue(copy));
    return copy;
}

static bool
FindErrorInstanceOrPrototype(JSContext* cx, HandleObject obj, MutableHandleObject result)
{
    // Walk up the prototype chain until we find an error object instance or
    // prototype object. This allows code like:
    //  Object.create(Error.prototype).stack
    // or
    //   function NYI() { }
    //   NYI.prototype = new Error;
    //   (new NYI).stack
    // to continue returning stacks that are useless, but at least don't throw.

    RootedObject target(cx, CheckedUnwrap(obj));
    if (!target) {
        JS_ReportErrorASCII(cx, "Permission denied to access object");
        return false;
    }

    RootedObject proto(cx);
    while (!IsErrorProtoKey(StandardProtoKeyOrNull(target))) {
        if (!GetPrototype(cx, target, &proto))
            return false;

        if (!proto) {
            // We walked the whole prototype chain and did not find an Error
            // object.
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                      js_Error_str, "(get stack)", obj->getClass()->name);
            return false;
        }

        target = CheckedUnwrap(proto);
        if (!target) {
            JS_ReportErrorASCII(cx, "Permission denied to access object");
            return false;
        }
    }

    result.set(target);
    return true;
}


static MOZ_ALWAYS_INLINE bool
IsObject(HandleValue v)
{
    return v.isObject();
}

/* static */ bool
js::ErrorObject::getStack(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    // We accept any object here, because of poor-man's subclassing of Error.
    return CallNonGenericMethod<IsObject, getStack_impl>(cx, args);
}

/* static */ bool
js::ErrorObject::getStack_impl(JSContext* cx, const CallArgs& args)
{
    RootedObject thisObj(cx, &args.thisv().toObject());

    RootedObject obj(cx);
    if (!FindErrorInstanceOrPrototype(cx, thisObj, &obj))
        return false;

    if (!obj->is<ErrorObject>()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    RootedObject savedFrameObj(cx, obj->as<ErrorObject>().stack());
    RootedString stackString(cx);
    if (!BuildStackString(cx, savedFrameObj, &stackString))
        return false;

    if (cx->stackFormat() == js::StackFormat::V8) {
        // When emulating V8 stack frames, we also need to prepend the
        // stringified Error to the stack string.
        HandlePropertyName name = cx->names().ErrorToStringWithTrailingNewline;
        RootedValue val(cx);
        if (!GlobalObject::getSelfHostedFunction(cx, cx->global(), name, name, 0, &val))
            return false;

        RootedValue rval(cx);
        if (!js::Call(cx, val, args.thisv(), &rval))
            return false;

        if (!rval.isString())
            return false;

        RootedString stringified(cx, rval.toString());
        stackString = ConcatStrings<CanGC>(cx, stringified, stackString);
    }

    args.rval().setString(stackString);
    return true;
}

/* static */ bool
js::ErrorObject::setStack(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    // We accept any object here, because of poor-man's subclassing of Error.
    return CallNonGenericMethod<IsObject, setStack_impl>(cx, args);
}

/* static */ bool
js::ErrorObject::setStack_impl(JSContext* cx, const CallArgs& args)
{
    RootedObject thisObj(cx, &args.thisv().toObject());

    if (!args.requireAtLeast(cx, "(set stack)", 1))
        return false;
    RootedValue val(cx, args[0]);

    return DefineProperty(cx, thisObj, cx->names().stack, val);
}

/*
 * Return a string that may eval to something similar to the original object.
 */
static bool
exn_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedValue nameVal(cx);
    RootedString name(cx);
    if (!GetProperty(cx, obj, obj, cx->names().name, &nameVal) ||
        !(name = ToString<CanGC>(cx, nameVal)))
    {
        return false;
    }

    RootedValue messageVal(cx);
    RootedString message(cx);
    if (!GetProperty(cx, obj, obj, cx->names().message, &messageVal) ||
        !(message = ValueToSource(cx, messageVal)))
    {
        return false;
    }

    RootedValue filenameVal(cx);
    RootedString filename(cx);
    if (!GetProperty(cx, obj, obj, cx->names().fileName, &filenameVal) ||
        !(filename = ValueToSource(cx, filenameVal)))
    {
        return false;
    }

    RootedValue linenoVal(cx);
    uint32_t lineno;
    if (!GetProperty(cx, obj, obj, cx->names().lineNumber, &linenoVal) ||
        !ToUint32(cx, linenoVal, &lineno))
    {
        return false;
    }

    StringBuffer sb(cx);
    if (!sb.append("(new ") || !sb.append(name) || !sb.append("("))
        return false;

    if (!sb.append(message))
        return false;

    if (!filename->empty()) {
        if (!sb.append(", ") || !sb.append(filename))
            return false;
    }
    if (lineno != 0) {
        /* We have a line, but no filename, add empty string */
        if (filename->empty() && !sb.append(", \"\""))
                return false;

        JSString* linenumber = ToString<CanGC>(cx, linenoVal);
        if (!linenumber)
            return false;
        if (!sb.append(", ") || !sb.append(linenumber))
            return false;
    }

    if (!sb.append("))"))
        return false;

    JSString* str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}
