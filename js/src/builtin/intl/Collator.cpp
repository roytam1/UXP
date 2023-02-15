/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Collator implementation. */

#include "builtin/intl/Collator.h"

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/SharedIntlData.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using namespace js;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::ReportInternalError;
using js::intl::SharedIntlData;
using js::intl::StringsAreEqual;

/******************** Collator ********************/

const ClassOps CollatorObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    CollatorObject::finalize
};

const Class CollatorObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(CollatorObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &CollatorObject::classOps_
};

#if JS_HAS_TOSOURCE
static bool
collator_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Collator);
    return true;
}
#endif

static const JSFunctionSpec collator_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_Collator_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec collator_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_Collator_resolvedOptions", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, collator_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 10.1.2 Intl.Collator([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
Collator(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    // We're following ECMA-402 1st Edition when Collator is called because of
    // backward compatibility issues.
    // See https://github.com/tc39/ecma402/issues/57
    if (!construct) {
        // ES Intl 1st ed., 10.1.2.1 step 3
        JSObject* intl = GlobalObject::getOrCreateIntlObject(cx, cx->global());
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // ES Intl 1st ed., 10.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // ES Intl 1st ed., 10.1.2.1 step 5
            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // ES Intl 1st ed., 10.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // Steps 2-5 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
        RootedObject proto(cx);
        if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        if (!proto) {
            proto = GlobalObject::getOrCreateCollatorPrototype(cx, cx->global());
            if (!proto)
                return false;
        }

        obj = NewObjectWithGivenProto<CollatorObject>(cx, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(CollatorObject::INTERNALS_SLOT, NullValue());
        obj->as<NativeObject>().setReservedSlot(CollatorObject::UCOLLATOR_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // Step 6.
    if (!intl::InitializeObject(cx, obj, cx->names().InitializeCollator, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return Collator(cx, args, args.isConstructing());
}

bool
js::intl_Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_Collator is an intrinsic for self-hosted JavaScript, so it cannot
    // be used with "new", but it still has to be treated as a constructor.
    return Collator(cx, args, true);
}

void
js::CollatorObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(CollatorObject::UCOLLATOR_SLOT);
    if (!slot.isUndefined()) {
        if (UCollator* coll = static_cast<UCollator*>(slot.toPrivate()))
            ucol_close(coll);
    }
}

JSObject*
js::CreateCollatorPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, GlobalObject::createConstructor(cx, &Collator, cx->names().Collator,
                                                            0));
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global, &CollatorObject::class_));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(CollatorObject::UCOLLATOR_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 10.2.2
    if (!JS_DefineFunctions(cx, ctor, collator_static_methods))
        return nullptr;

    // 10.3.2 and 10.3.3
    if (!JS_DefineFunctions(cx, proto, collator_methods))
        return nullptr;

    /*
     * Install the getter for Collator.prototype.compare, which returns a bound
     * comparison function for the specified Collator object (suitable for
     * passing to methods like Array.prototype.sort).
     */
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().CollatorCompareGet, &getter))
        return nullptr;
    if (!DefineProperty(cx, proto, cx->names().compare, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    RootedValue options(cx);
    if (!intl::CreateDefaultOptions(cx, &options))
        return nullptr;

    // 10.2.1 and 10.3
    if (!intl::InitializeObject(cx, proto, cx->names().InitializeCollator, UndefinedHandleValue, options))
        return nullptr;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().Collator, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_Collator_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!GetAvailableLocales(cx, ucol_countAvailable, ucol_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

bool
js::intl_availableCollations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucol_getKeywordValuesForLocale("co", locale.ptr(), false, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    uint32_t count = uenum_count(values, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    RootedObject collations(cx, NewDenseEmptyArray(cx));
    if (!collations)
        return false;

    uint32_t index = 0;
    for (uint32_t i = 0; i < count; i++) {
        const char* collation = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        // Per ECMA-402, 10.2.3, we don't include standard and search:
        // "The values 'standard' and 'search' must not be used as elements in
        // any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co
        // array."
        if (StringsAreEqual(collation, "standard") || StringsAreEqual(collation, "search"))
            continue;

        // ICU returns old-style keyword values; map them to BCP 47 equivalents
        // (see http://bugs.icu-project.org/trac/ticket/9620).
        if (StringsAreEqual(collation, "dictionary"))
            collation = "dict";
        else if (StringsAreEqual(collation, "gb2312han"))
            collation = "gb2312";
        else if (StringsAreEqual(collation, "phonebook"))
            collation = "phonebk";
        else if (StringsAreEqual(collation, "traditional"))
            collation = "trad";

        RootedString jscollation(cx, JS_NewStringCopyZ(cx, collation));
        if (!jscollation)
            return false;
        RootedValue element(cx, StringValue(jscollation));
        if (!DefineElement(cx, collations, index++, element))
            return false;
    }

    args.rval().setObject(*collations);
    return true;
}

/**
 * Returns a new UCollator with the locale and collation options
 * of the given Collator.
 */
static UCollator*
NewUCollator(JSContext* cx, HandleObject collator)
{
    RootedValue value(cx);

    RootedObject internals(cx, intl::GetInternalsObject(cx, collator));
    if (!internals)
        return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // UCollator options with default values.
    UColAttributeValue uStrength = UCOL_DEFAULT;
    UColAttributeValue uCaseLevel = UCOL_OFF;
    UColAttributeValue uAlternate = UCOL_DEFAULT;
    UColAttributeValue uNumeric = UCOL_OFF;
    // Normalization is always on to meet the canonical equivalence requirement.
    UColAttributeValue uNormalization = UCOL_ON;
    UColAttributeValue uCaseFirst = UCOL_DEFAULT;

    if (!GetProperty(cx, internals, internals, cx->names().usage, &value))
        return nullptr;
    JSAutoByteString usage(cx, value.toString());
    if (!usage)
        return nullptr;
    if (StringsAreEqual(usage, "search")) {
        // ICU expects search as a Unicode locale extension on locale.
        // Unicode locale extensions must occur before private use extensions.
        const char* oldLocale = locale.ptr();
        const char* p;
        size_t index;
        size_t localeLen = strlen(oldLocale);
        if ((p = strstr(oldLocale, "-x-")))
            index = p - oldLocale;
        else
            index = localeLen;

        const char* insert;
        if ((p = strstr(oldLocale, "-u-")) && static_cast<size_t>(p - oldLocale) < index) {
            index = p - oldLocale + 2;
            insert = "-co-search";
        } else {
            insert = "-u-co-search";
        }
        size_t insertLen = strlen(insert);
        char* newLocale = cx->pod_malloc<char>(localeLen + insertLen + 1);
        if (!newLocale)
            return nullptr;
        memcpy(newLocale, oldLocale, index);
        memcpy(newLocale + index, insert, insertLen);
        memcpy(newLocale + index + insertLen, oldLocale + index, localeLen - index + 1); // '\0'
        locale.clear();
        locale.initBytes(newLocale);
    }

    // We don't need to look at the collation property - it can only be set
    // via the Unicode locale extension and is therefore already set on
    // locale.

    if (!GetProperty(cx, internals, internals, cx->names().sensitivity, &value))
        return nullptr;
    JSAutoByteString sensitivity(cx, value.toString());
    if (!sensitivity)
        return nullptr;
    if (StringsAreEqual(sensitivity, "base")) {
        uStrength = UCOL_PRIMARY;
    } else if (StringsAreEqual(sensitivity, "accent")) {
        uStrength = UCOL_SECONDARY;
    } else if (StringsAreEqual(sensitivity, "case")) {
        uStrength = UCOL_PRIMARY;
        uCaseLevel = UCOL_ON;
    } else {
        MOZ_ASSERT(StringsAreEqual(sensitivity, "variant"));
        uStrength = UCOL_TERTIARY;
    }

    if (!GetProperty(cx, internals, internals, cx->names().ignorePunctuation, &value))
        return nullptr;
    // According to the ICU team, UCOL_SHIFTED causes punctuation to be
    // ignored. Looking at Unicode Technical Report 35, Unicode Locale Data
    // Markup Language, "shifted" causes whitespace and punctuation to be
    // ignored - that's a bit more than asked for, but there's no way to get
    // less.
    if (value.toBoolean())
        uAlternate = UCOL_SHIFTED;

    if (!GetProperty(cx, internals, internals, cx->names().numeric, &value))
        return nullptr;
    if (!value.isUndefined() && value.toBoolean())
        uNumeric = UCOL_ON;

    if (!GetProperty(cx, internals, internals, cx->names().caseFirst, &value))
        return nullptr;
    if (!value.isUndefined()) {
        JSAutoByteString caseFirst(cx, value.toString());
        if (!caseFirst)
            return nullptr;
        if (StringsAreEqual(caseFirst, "upper"))
            uCaseFirst = UCOL_UPPER_FIRST;
        else if (StringsAreEqual(caseFirst, "lower"))
            uCaseFirst = UCOL_LOWER_FIRST;
        else
            MOZ_ASSERT(StringsAreEqual(caseFirst, "false"));
    }

    UErrorCode status = U_ZERO_ERROR;
    UCollator* coll = ucol_open(IcuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return nullptr;
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, uStrength, &status);
    ucol_setAttribute(coll, UCOL_CASE_LEVEL, uCaseLevel, &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, uAlternate, &status);
    ucol_setAttribute(coll, UCOL_NUMERIC_COLLATION, uNumeric, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, uNormalization, &status);
    ucol_setAttribute(coll, UCOL_CASE_FIRST, uCaseFirst, &status);
    if (U_FAILURE(status)) {
        ucol_close(coll);
        intl::ReportInternalError(cx);
        return nullptr;
    }

    return coll;
}

static bool
intl_CompareStrings(JSContext* cx, UCollator* coll, HandleString str1, HandleString str2,
                    MutableHandleValue result)
{
    MOZ_ASSERT(str1);
    MOZ_ASSERT(str2);

    if (str1 == str2) {
        result.setInt32(0);
        return true;
    }

    AutoStableStringChars stableChars1(cx);
    if (!stableChars1.initTwoByte(cx, str1))
        return false;

    AutoStableStringChars stableChars2(cx);
    if (!stableChars2.initTwoByte(cx, str2))
        return false;

    mozilla::Range<const char16_t> chars1 = stableChars1.twoByteRange();
    mozilla::Range<const char16_t> chars2 = stableChars2.twoByteRange();

    UCollationResult uresult = ucol_strcoll(coll,
                                            Char16ToUChar(chars1.begin().get()), chars1.length(),
                                            Char16ToUChar(chars2.begin().get()), chars2.length());
    int32_t res;
    switch (uresult) {
        case UCOL_LESS: res = -1; break;
        case UCOL_EQUAL: res = 0; break;
        case UCOL_GREATER: res = 1; break;
        default: MOZ_CRASH("ucol_strcoll returned bad UCollationResult");
    }
    result.setInt32(res);
    return true;
}

bool
js::intl_CompareStrings(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isString());

    Rooted<CollatorObject*> collator(cx, &args[0].toObject().as<CollatorObject>());
        
    // Obtain a UCollator object, cached if possible.
    // XXX Does this handle Collator instances from other globals correctly?
    bool isCollatorInstance = collator->getClass() == &CollatorObject::class_;
    UCollator* coll;
    if (isCollatorInstance) {
        void* priv = collator->getReservedSlot(CollatorObject::UCOLLATOR_SLOT).toPrivate();
        coll = static_cast<UCollator*>(priv);
        if (!coll) {
            coll = NewUCollator(cx, collator);
            if (!coll)
                return false;
            collator->setReservedSlot(CollatorObject::UCOLLATOR_SLOT, PrivateValue(coll));
        }
    } else {
        // There's no good place to cache the ICU collator for an object
        // that has been initialized as a Collator but is not a Collator
        // instance. One possibility might be to add a Collator instance as an
        // internal property to each such object.
        coll = NewUCollator(cx, collator);
        if (!coll)
            return false;
    }

    // Use the UCollator to actually compare the strings.
    RootedString str1(cx, args[1].toString());
    RootedString str2(cx, args[2].toString());
    RootedValue result(cx);
    bool success = intl_CompareStrings(cx, coll, str1, str2, &result);

    if (!isCollatorInstance)
        ucol_close(coll);
    if (!success)
        return false;
    args.rval().set(result);
    return true;
}
