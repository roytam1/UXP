/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of the Intl.PluralRules proposal. */

#include "builtin/intl/NumberFormat.h"
#include "builtin/intl/PluralRules.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"

#include "jscntxt.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/ScopedICUObject.h"
#include "vm/GlobalObject.h"
#include "vm/String.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::AssertedCast;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::StringsAreEqual;

/**************** PluralRules *****************/

const ClassOps PluralRulesObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    PluralRulesObject::finalize
};

const Class PluralRulesObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(PluralRulesObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &PluralRulesObject::classOps_
};

#if JS_HAS_TOSOURCE
static bool
pluralRules_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().PluralRules);
    return true;
}
#endif

static const JSFunctionSpec pluralRules_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_PluralRules_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec pluralRules_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_PluralRules_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("select", "Intl_PluralRules_select", 1, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, pluralRules_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * PluralRules constructor.
 * Spec: ECMAScript 402 API, PluralRules, 1.1
 */
static bool
PluralRules(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    if (!construct) {
        JSObject* intl = GlobalObject::getOrCreateIntlObject(cx, cx->global());
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            construct = true;
        }
    }
    if (construct) {
        RootedObject proto(cx, GlobalObject::getOrCreatePluralRulesPrototype(cx, cx->global()));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto<PluralRulesObject>(cx, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(PluralRulesObject::INTERNALS_SLOT, NullValue());
        obj->as<NativeObject>().setReservedSlot(PluralRulesObject::UPLURAL_RULES_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    if (!intl::InitializeObject(cx, obj, cx->names().InitializePluralRules, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
PluralRules(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PluralRules(cx, args, args.isConstructing());
}

bool
js::intl_PluralRules(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    return PluralRules(cx, args, true);
}

void
js::PluralRulesObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<PluralRulesObject>().getReservedSlot(PluralRulesObject::UPLURAL_RULES_SLOT);
    if (!slot.isUndefined()) {
        if (UPluralRules* pr = static_cast<UPluralRules*>(slot.toPrivate()))
            uplrules_close(pr);
    }
}

JSObject*
js::CreatePluralRulesPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &PluralRules, cx->names().PluralRules, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global, &PluralRulesObject::class_));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(PluralRulesObject::UPLURAL_RULES_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, pluralRules_static_methods))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, pluralRules_methods))
        return nullptr;

    RootedValue options(cx);
    if (!intl::CreateDefaultOptions(cx, &options))
        return nullptr;

    if (!intl::InitializeObject(cx, proto, cx->names().InitializePluralRules, UndefinedHandleValue,
                        options))
    {
        return nullptr;
    }

    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().PluralRules, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_PluralRules_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    // We're going to use ULocale availableLocales as per ICU recommendation:
    // https://ssl.icu-project.org/trac/ticket/12756
    if (!GetAvailableLocales(cx, uloc_countAvailable, uloc_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

/**
 *
 * This creates new UNumberFormat with calculated digit formatting
 * properties for PluralRules.
 *
 * This is similar to NewUNumberFormat but doesn't allow for currency or
 * percent types.
 *
 */
static UNumberFormat*
NewUNumberFormatForPluralRules(JSContext* cx, HandleObject pluralRules)
{
    RootedObject internals(cx, intl::GetInternalsObject(cx, pluralRules));
    if (!internals)
       return nullptr;

    RootedValue value(cx);

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    uint32_t uMinimumIntegerDigits = 1;
    uint32_t uMinimumFractionDigits = 0;
    uint32_t uMaximumFractionDigits = 3;
    int32_t uMinimumSignificantDigits = -1;
    int32_t uMaximumSignificantDigits = -1;

    RootedId id(cx, NameToId(cx->names().minimumSignificantDigits));
    bool hasP;
    if (!HasProperty(cx, internals, id, &hasP))
        return nullptr;
    if (hasP) {
        if (!GetProperty(cx, internals, internals, cx->names().minimumSignificantDigits,
                         &value))
            return nullptr;
        uMinimumSignificantDigits = value.toInt32();

        if (!GetProperty(cx, internals, internals, cx->names().maximumSignificantDigits,
                         &value))
            return nullptr;
        uMaximumSignificantDigits = value.toInt32();
    } else {
        if (!GetProperty(cx, internals, internals, cx->names().minimumIntegerDigits,
                         &value))
            return nullptr;
        uMinimumIntegerDigits = AssertedCast<uint32_t>(value.toInt32());

        if (!GetProperty(cx, internals, internals, cx->names().minimumFractionDigits,
                         &value))
            return nullptr;
        uMinimumFractionDigits = AssertedCast<uint32_t>(value.toInt32());

        if (!GetProperty(cx, internals, internals, cx->names().maximumFractionDigits,
                         &value))
            return nullptr;
        uMaximumFractionDigits = AssertedCast<uint32_t>(value.toInt32());
    }

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(UNUM_DECIMAL, nullptr, 0, IcuLocale(locale.ptr()), nullptr, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return nullptr;
    }
    ScopedICUObject<UNumberFormat, unum_close> toClose(nf);

    if (uMinimumSignificantDigits != -1) {
        unum_setAttribute(nf, UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(nf, UNUM_MIN_SIGNIFICANT_DIGITS, uMinimumSignificantDigits);
        unum_setAttribute(nf, UNUM_MAX_SIGNIFICANT_DIGITS, uMaximumSignificantDigits);
    } else {
        unum_setAttribute(nf, UNUM_MIN_INTEGER_DIGITS, uMinimumIntegerDigits);
        unum_setAttribute(nf, UNUM_MIN_FRACTION_DIGITS, uMinimumFractionDigits);
        unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, uMaximumFractionDigits);
    }

    return toClose.forget();
}

bool
js::intl_SelectPluralRule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject pluralRules(cx, &args[0].toObject());

    UNumberFormat* nf = NewUNumberFormatForPluralRules(cx, pluralRules);
    if (!nf)
        return false;

    ScopedICUObject<UNumberFormat, unum_close> closeNumberFormat(nf);

    RootedObject internals(cx, intl::GetInternalsObject(cx, pluralRules));
    if (!internals)
        return false;

    RootedValue value(cx);

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return false;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return false;

    if (!GetProperty(cx, internals, internals, cx->names().type, &value))
        return false;
    JSAutoByteString type(cx, value.toString());
    if (!type)
        return false;

    double x = args[1].toNumber();

    // We need a NumberFormat in order to format the number
    // using the number formatting options (minimum/maximum*Digits)
    // before we push the result to PluralRules
    //
    // This should be fixed in ICU 59 and we'll be able to switch to that
    // API: http://bugs.icu-project.org/trac/ticket/12763
    //
    RootedValue fmtNumValue(cx);
    if (!intl_FormatNumber(cx, nf, x, &fmtNumValue))
        return false;
    RootedString fmtNumValueString(cx, fmtNumValue.toString());
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, fmtNumValueString))
        return false;

    const UChar* uFmtNumValue = Char16ToUChar(stableChars.twoByteRange().begin().get());

    UErrorCode status = U_ZERO_ERROR;

    UFormattable* fmt = unum_parseToUFormattable(nf, nullptr, uFmtNumValue,
                                                 stableChars.twoByteRange().length(), 0, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    ScopedICUObject<UFormattable, ufmt_close> closeUFormattable(fmt);

    double y = ufmt_getDouble(fmt, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    UPluralType category;

    if (StringsAreEqual(type, "cardinal")) {
        category = UPLURAL_TYPE_CARDINAL;
    } else {
        MOZ_ASSERT(StringsAreEqual(type, "ordinal"));
        category = UPLURAL_TYPE_ORDINAL;
    }

    UPluralRules* pr = uplrules_openForType(IcuLocale(locale.ptr()), category, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    ScopedICUObject<UPluralRules, uplrules_close> closePluralRules(pr);

    JSString* str = CallICU(cx, [pr, y](UChar* chars, int32_t size, UErrorCode* status) {
        return uplrules_select(pr, y, chars, size, status);
    });
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

bool
js::intl_GetPluralCategories(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    JSAutoByteString type(cx, args[1].toString());
    if (!type)
        return false;

    UErrorCode status = U_ZERO_ERROR;

    UPluralType category;

    if (StringsAreEqual(type, "cardinal")) {
        category = UPLURAL_TYPE_CARDINAL;
    } else {
        MOZ_ASSERT(StringsAreEqual(type, "ordinal"));
        category = UPLURAL_TYPE_ORDINAL;
    }

    UPluralRules* pr = uplrules_openForType(
        IcuLocale(locale.ptr()),
        category,
        &status
    );
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    ScopedICUObject<UPluralRules, uplrules_close> closePluralRules(pr);

    // We should get a C API for that in ICU 59 and switch to it
    // https://ssl.icu-project.org/trac/ticket/12772
    //
    icu::StringEnumeration* kwenum =
        reinterpret_cast<icu::PluralRules*>(pr)->getKeywords(status);
    UEnumeration* ue = uenum_openFromStringEnumeration(kwenum, &status);

    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    ScopedICUObject<UEnumeration, uenum_close> closeEnum(ue);

    RootedObject res(cx, NewDenseEmptyArray(cx));
    if (!res)
        return false;

    RootedValue element(cx);
    uint32_t i = 0;
    int32_t catSize;
    const char* cat;

    do {
        cat = uenum_next(ue, &catSize, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        if (!cat)
            break;

        JSString* str = NewStringCopyN<CanGC>(cx, cat, catSize);
        if (!str)
            return false;

        element.setString(str);
        if (!DefineElement(cx, res, i, element))
            return false;
        i++;
    } while (true);

    args.rval().setObject(*res);
    return true;
}
