/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

#include "builtin/Intl.h"

#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "builtin/intl/Collator.h"
#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/NumberFormat.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/SharedIntlData.h"
#include "ds/Sort.h"
#include "vm/DateTime.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Stack.h"
#include "vm/StringBuffer.h"
#include "vm/Unicode.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::AssertedCast;
using mozilla::IsFinite;
using mozilla::IsNegativeZero;
using mozilla::PodCopy;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::SharedIntlData;
using js::intl::StringsAreEqual;

/******************** DateTimeFormat ********************/

static void dateTimeFormat_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t UDATE_FORMAT_SLOT = 0;
static const uint32_t DATE_TIME_FORMAT_SLOTS_COUNT = 1;

static const ClassOps DateTimeFormatClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    dateTimeFormat_finalize
};

static const Class DateTimeFormatClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DATE_TIME_FORMAT_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &DateTimeFormatClassOps
};

#if JS_HAS_TOSOURCE
static bool
dateTimeFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().DateTimeFormat);
    return true;
}
#endif

static const JSFunctionSpec dateTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_DateTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec dateTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_DateTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("formatToParts", "Intl_DateTimeFormat_formatToParts", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, dateTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 12.2.1 Intl.DateTimeFormat([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
DateTimeFormat(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    // We're following ECMA-402 1st Edition when DateTimeFormat is called
    // because of backward compatibility issues.
    // See https://github.com/tc39/ecma402/issues/57
    if (!construct) {
        // ES Intl 1st ed., 12.1.2.1 step 3
        JSObject* intl = GlobalObject::getOrCreateIntlObject(cx, cx->global());
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // ES Intl 1st ed., 12.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // ES Intl 1st ed., 12.1.2.1 step 5
            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // ES Intl 1st ed., 12.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
        RootedObject proto(cx);
        if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        if (!proto) {
            proto = GlobalObject::getOrCreateDateTimeFormatPrototype(cx, cx->global());
            if (!proto)
                return false;
        }

        obj = NewObjectWithGivenProto(cx, &DateTimeFormatClass, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // Step 3.
    if (!intl::InitializeObject(cx, obj, cx->names().InitializeDateTimeFormat, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return DateTimeFormat(cx, args, args.isConstructing());
}

bool
js::intl_DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_DateTimeFormat is an intrinsic for self-hosted JavaScript, so it
    // cannot be used with "new", but it still has to be treated as a
    // constructor.
    return DateTimeFormat(cx, args, true);
}

static void
dateTimeFormat_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(UDATE_FORMAT_SLOT);
    if (!slot.isUndefined()) {
        if (UDateFormat* df = static_cast<UDateFormat*>(slot.toPrivate()))
            udat_close(df);
    }
}

static JSObject*
CreateDateTimeFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = GlobalObject::createConstructor(cx, &DateTimeFormat, cx->names().DateTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global,
                                                                    &DateTimeFormatClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 12.2.2
    if (!JS_DefineFunctions(cx, ctor, dateTimeFormat_static_methods))
        return nullptr;

    // 12.3.2 and 12.3.3
    if (!JS_DefineFunctions(cx, proto, dateTimeFormat_methods))
        return nullptr;

    // Install a getter for DateTimeFormat.prototype.format that returns a
    // formatting function bound to a specified DateTimeFormat object (suitable
    // for passing to methods like Array.prototype.map).
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().DateTimeFormatFormatGet,
                                         &getter))
    {
        return nullptr;
    }
    if (!DefineProperty(cx, proto, cx->names().format, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    RootedValue options(cx);
    if (!intl::CreateDefaultOptions(cx, &options))
        return nullptr;

    // 12.2.1 and 12.3
    if (!intl::InitializeObject(cx, proto, cx->names().InitializeDateTimeFormat, UndefinedHandleValue,
                        options))
    {
        return nullptr;
    }

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().DateTimeFormat, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_DateTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!GetAvailableLocales(cx, udat_countAvailable, udat_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

// ICU returns old-style keyword values; map them to BCP 47 equivalents
// (see http://bugs.icu-project.org/trac/ticket/9620).
static const char*
bcp47CalendarName(const char* icuName)
{
    if (StringsAreEqual(icuName, "ethiopic-amete-alem"))
        return "ethioaa";
    if (StringsAreEqual(icuName, "gregorian"))
        return "gregory";
    if (StringsAreEqual(icuName, "islamic-civil"))
        return "islamicc";
    return icuName;
}

bool
js::intl_availableCalendars(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    RootedObject calendars(cx, NewDenseEmptyArray(cx));
    if (!calendars)
        return false;
    uint32_t index = 0;

    // We need the default calendar for the locale as the first result.
    UErrorCode status = U_ZERO_ERROR;
    RootedString jscalendar(cx);
    {
        UCalendar* cal = ucal_open(nullptr, 0, locale.ptr(), UCAL_DEFAULT, &status);

        // This correctly handles nullptr |cal| when opening failed.
        ScopedICUObject<UCalendar, ucal_close> closeCalendar(cal);

        const char* calendar = ucal_getType(cal, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        jscalendar = JS_NewStringCopyZ(cx, bcp47CalendarName(calendar));
        if (!jscalendar)
            return false;
    }

    RootedValue element(cx, StringValue(jscalendar));
    if (!DefineElement(cx, calendars, index++, element))
        return false;

    // Now get the calendars that "would make a difference", i.e., not the default.
    UEnumeration* values = ucal_getKeywordValuesForLocale("ca", locale.ptr(), false, &status);
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

    for (; count > 0; count--) {
        const char* calendar = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        jscalendar = JS_NewStringCopyZ(cx, bcp47CalendarName(calendar));
        if (!jscalendar)
            return false;
        element = StringValue(jscalendar);
        if (!DefineElement(cx, calendars, index++, element))
            return false;
    }

    args.rval().setObject(*calendars);
    return true;
}

bool
js::intl_IsValidTimeZoneName(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    RootedString timeZone(cx, args[0].toString());
    RootedString validatedTimeZone(cx);
    if (!sharedIntlData.validateTimeZoneName(cx, timeZone, &validatedTimeZone))
        return false;

    if (validatedTimeZone)
        args.rval().setString(validatedTimeZone);
    else
        args.rval().setNull();

    return true;
}

bool
js::intl_canonicalizeTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    // Some time zone names are canonicalized differently by ICU -- handle
    // those first:
    RootedString timeZone(cx, args[0].toString());
    RootedString ianaTimeZone(cx);
    if (!sharedIntlData.tryCanonicalizeTimeZoneConsistentWithIANA(cx, timeZone, &ianaTimeZone))
        return false;

    if (ianaTimeZone) {
        args.rval().setString(ianaTimeZone);
        return true;
    }

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, timeZone))
        return false;

    mozilla::Range<const char16_t> tzchars = stableChars.twoByteRange();

    JSString* str = CallICU(cx, [&tzchars](UChar* chars, uint32_t size, UErrorCode* status) {
        return ucal_getCanonicalTimeZoneID(tzchars.begin().get(), tzchars.length(),
                                           chars, size, nullptr, status);
    });
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    // The current default might be stale, because JS::ResetTimeZone() doesn't
    // immediately update ICU's default time zone. So perform an update if
    // needed.
    js::ResyncICUDefaultTimeZone();

    JSString* str = CallICU(cx, ucal_getDefaultTimeZone);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZoneOffset(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    UErrorCode status = U_ZERO_ERROR;
    const UChar* uTimeZone = nullptr;
    int32_t uTimeZoneLength = 0;
    const char* rootLocale = "";
    UCalendar* cal = ucal_open(uTimeZone, uTimeZoneLength, rootLocale, UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UCalendar, ucal_close> toClose(cal);

    int32_t offset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    args.rval().setInt32(offset);
    return true;
}

bool
js::intl_patternForSkeleton(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    JSFlatString* skeletonFlat = args[1].toString()->ensureFlat(cx);
    if (!skeletonFlat)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, skeletonFlat))
        return false;

    mozilla::Range<const char16_t> skeletonChars = stableChars.twoByteRange();
    uint32_t skeletonLen = u_strlen(Char16ToUChar(skeletonChars.begin().get()));

    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* gen = udatpg_open(IcuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UDateTimePatternGenerator, udatpg_close> toClose(gen);

    JSString* str =
        CallICU(cx, [gen, &skeletonChars, skeletonLen](UChar* chars, uint32_t size, UErrorCode* status) {
            return udatpg_getBestPattern(gen, skeletonChars.begin().get(), skeletonLen,
                                         chars, size, status);
        });
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/**
 * Returns a new UDateFormat with the locale and date-time formatting options
 * of the given DateTimeFormat.
 */
static UDateFormat*
NewUDateFormat(JSContext* cx, HandleObject dateTimeFormat)
{
    RootedValue value(cx);

    RootedObject internals(cx, intl::GetInternalsObject(cx, dateTimeFormat));
    if (!internals)
       return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // We don't need to look at calendar and numberingSystem - they can only be
    // set via the Unicode locale extension and are therefore already set on
    // locale.

    if (!GetProperty(cx, internals, internals, cx->names().timeZone, &value))
        return nullptr;

    AutoStableStringChars timeZoneChars(cx);
    Rooted<JSFlatString*> timeZoneFlat(cx, value.toString()->ensureFlat(cx));
    if (!timeZoneFlat || !timeZoneChars.initTwoByte(cx, timeZoneFlat))
        return nullptr;

    const UChar* uTimeZone = Char16ToUChar(timeZoneChars.twoByteRange().begin().get());
    uint32_t uTimeZoneLength = u_strlen(uTimeZone);

    if (!GetProperty(cx, internals, internals, cx->names().pattern, &value))
        return nullptr;

    AutoStableStringChars patternChars(cx);
    Rooted<JSFlatString*> patternFlat(cx, value.toString()->ensureFlat(cx));
    if (!patternFlat || !patternChars.initTwoByte(cx, patternFlat))
        return nullptr;

    const UChar* uPattern = Char16ToUChar(patternChars.twoByteRange().begin().get());
    uint32_t uPatternLength = u_strlen(uPattern);

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat* df =
        udat_open(UDAT_PATTERN, UDAT_PATTERN, IcuLocale(locale.ptr()), uTimeZone, uTimeZoneLength,
                  uPattern, uPatternLength, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return nullptr;
    }

    // ECMAScript requires the Gregorian calendar to be used from the beginning
    // of ECMAScript time.
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(df));
    ucal_setGregorianChange(cal, StartOfTime, &status);

    // An error here means the calendar is not Gregorian, so we don't care.

    return df;
}

static bool
intl_FormatDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE);
        return false;
    }

    JSString* str = CallICU(cx, [df, x](UChar* chars, int32_t size, UErrorCode* status) {
        return udat_format(df, x, chars, size, nullptr, status);
    });
    if (!str)
        return false;

    result.setString(str);
    return true;
}

using FieldType = ImmutablePropertyNamePtr JSAtomState::*;

static FieldType
GetFieldTypeForFormatField(UDateFormatField fieldName)
{
    // See intl/icu/source/i18n/unicode/udat.h for a detailed field list.  This
    // switch is deliberately exhaustive: cases might have to be added/removed
    // if this code is compiled with a different ICU with more
    // UDateFormatField enum initializers.  Please guard such cases with
    // appropriate ICU version-testing #ifdefs, should cross-version divergence
    // occur.
    switch (fieldName) {
      case UDAT_ERA_FIELD:
        return &JSAtomState::era;
      case UDAT_YEAR_FIELD:
      case UDAT_YEAR_WOY_FIELD:
      case UDAT_EXTENDED_YEAR_FIELD:
      case UDAT_YEAR_NAME_FIELD:
        return &JSAtomState::year;

      case UDAT_MONTH_FIELD:
      case UDAT_STANDALONE_MONTH_FIELD:
        return &JSAtomState::month;

      case UDAT_DATE_FIELD:
      case UDAT_JULIAN_DAY_FIELD:
        return &JSAtomState::day;

      case UDAT_HOUR_OF_DAY1_FIELD:
      case UDAT_HOUR_OF_DAY0_FIELD:
      case UDAT_HOUR1_FIELD:
      case UDAT_HOUR0_FIELD:
        return &JSAtomState::hour;

      case UDAT_MINUTE_FIELD:
        return &JSAtomState::minute;

      case UDAT_SECOND_FIELD:
        return &JSAtomState::second;

      case UDAT_DAY_OF_WEEK_FIELD:
      case UDAT_STANDALONE_DAY_FIELD:
      case UDAT_DOW_LOCAL_FIELD:
      case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
        return &JSAtomState::weekday;

      case UDAT_AM_PM_FIELD:
        return &JSAtomState::dayPeriod;

      case UDAT_TIMEZONE_FIELD:
        return &JSAtomState::timeZoneName;

      case UDAT_FRACTIONAL_SECOND_FIELD:
      case UDAT_DAY_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_MONTH_FIELD:
      case UDAT_MILLISECONDS_IN_DAY_FIELD:
      case UDAT_TIMEZONE_RFC_FIELD:
      case UDAT_TIMEZONE_GENERIC_FIELD:
      case UDAT_QUARTER_FIELD:
      case UDAT_STANDALONE_QUARTER_FIELD:
      case UDAT_TIMEZONE_SPECIAL_FIELD:
      case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
      case UDAT_TIMEZONE_ISO_FIELD:
      case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
#ifndef U_HIDE_INTERNAL_API
      case UDAT_RELATED_YEAR_FIELD:
#endif
#ifndef U_HIDE_DRAFT_API
      case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
      case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
#endif
#ifndef U_HIDE_INTERNAL_API
      case UDAT_TIME_SEPARATOR_FIELD:
#endif
        // These fields are all unsupported.
        return nullptr;

      case UDAT_FIELD_COUNT:
        MOZ_ASSERT_UNREACHABLE("format field sentinel value returned by "
                               "iterator!");
    }

    MOZ_ASSERT_UNREACHABLE("unenumerated, undocumented format field returned "
                           "by iterator");
    return nullptr;
}

static bool
intl_FormatToPartsDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE);
        return false;
    }

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    UErrorCode status = U_ZERO_ERROR;
    UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UFieldPositionIterator, ufieldpositer_close> toClose(fpositer);

    RootedString overallResult(cx);
    overallResult = CallICU(cx, [df, x, fpositer](UChar* chars, int32_t size, UErrorCode* status) {
        return udat_formatForFields(df, x, chars, size, fpositer, status);
    });
    if (!overallResult)
        return false;

    RootedArrayObject partsArray(cx, NewDenseEmptyArray(cx));
    if (!partsArray)
        return false;
    if (overallResult->length() == 0) {
        // An empty string contains no parts, so avoid extra work below.
        result.setObject(*partsArray);
        return true;
    }

    size_t lastEndIndex = 0;

    uint32_t partIndex = 0;
    RootedObject singlePart(cx);
    RootedValue partType(cx);
    RootedValue val(cx);

    auto AppendPart = [&](FieldType type, size_t beginIndex, size_t endIndex) {
        singlePart = NewBuiltinClassInstance<PlainObject>(cx);
        if (!singlePart)
            return false;

        partType = StringValue(cx->names().*type);
        if (!DefineProperty(cx, singlePart, cx->names().type, partType))
            return false;

        JSLinearString* partSubstr =
            NewDependentString(cx, overallResult, beginIndex, endIndex - beginIndex);
        if (!partSubstr)
            return false;

        val = StringValue(partSubstr);
        if (!DefineProperty(cx, singlePart, cx->names().value, val))
            return false;

        val = ObjectValue(*singlePart);
        if (!DefineElement(cx, partsArray, partIndex, val))
            return false;

        lastEndIndex = endIndex;
        partIndex++;
        return true;
    };

    int32_t fieldInt, beginIndexInt, endIndexInt;
    while ((fieldInt = ufieldpositer_next(fpositer, &beginIndexInt, &endIndexInt)) >= 0) {
        MOZ_ASSERT(beginIndexInt >= 0);
        MOZ_ASSERT(endIndexInt >= 0);
        MOZ_ASSERT(beginIndexInt <= endIndexInt,
                   "field iterator returning invalid range");

        size_t beginIndex(beginIndexInt);
        size_t endIndex(endIndexInt);

        // Technically this isn't guaranteed.  But it appears true in pratice,
        // and http://bugs.icu-project.org/trac/ticket/12024 is expected to
        // correct the documentation lapse.
        MOZ_ASSERT(lastEndIndex <= beginIndex,
                   "field iteration didn't return fields in order start to "
                   "finish as expected");

        if (FieldType type = GetFieldTypeForFormatField(static_cast<UDateFormatField>(fieldInt))) {
            if (lastEndIndex < beginIndex) {
                if (!AppendPart(&JSAtomState::literal, lastEndIndex, beginIndex))
                    return false;
            }

            if (!AppendPart(type, beginIndex, endIndex))
                return false;
        }
    }

    // Append any final literal.
    if (lastEndIndex < overallResult->length()) {
        if (!AppendPart(&JSAtomState::literal, lastEndIndex, overallResult->length()))
            return false;
    }

    result.setObject(*partsArray);
    return true;
}

bool
js::intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isNumber());
    MOZ_ASSERT(args[2].isBoolean());

    RootedObject dateTimeFormat(cx, &args[0].toObject());

    // Obtain a UDateFormat object, cached if possible.
    bool isDateTimeFormatInstance = dateTimeFormat->getClass() == &DateTimeFormatClass;
    UDateFormat* df;
    if (isDateTimeFormatInstance) {
        void* priv =
            dateTimeFormat->as<NativeObject>().getReservedSlot(UDATE_FORMAT_SLOT).toPrivate();
        df = static_cast<UDateFormat*>(priv);
        if (!df) {
            df = NewUDateFormat(cx, dateTimeFormat);
            if (!df)
                return false;
            dateTimeFormat->as<NativeObject>().setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(df));
        }
    } else {
        // There's no good place to cache the ICU date-time format for an object
        // that has been initialized as a DateTimeFormat but is not a
        // DateTimeFormat instance. One possibility might be to add a
        // DateTimeFormat instance as an internal property to each such object.
        df = NewUDateFormat(cx, dateTimeFormat);
        if (!df)
            return false;
    }

    // Use the UDateFormat to actually format the time stamp.
    RootedValue result(cx);
    bool success = args[2].toBoolean()
                   ? intl_FormatToPartsDateTime(cx, df, args[1].toNumber(), &result)
                   : intl_FormatDateTime(cx, df, args[1].toNumber(), &result);

    if (!isDateTimeFormatInstance)
        udat_close(df);
    if (!success)
        return false;
    args.rval().set(result);
    return true;
}

/**************** PluralRules *****************/

static void pluralRules_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t UPLURAL_RULES_SLOT = 0;
static const uint32_t PLURAL_RULES_SLOTS_COUNT = 1;

static const ClassOps PluralRulesClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    pluralRules_finalize
};

static const Class PluralRulesClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(PLURAL_RULES_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &PluralRulesClassOps
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
        obj = NewObjectWithGivenProto(cx, &PluralRulesClass, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(UPLURAL_RULES_SLOT, PrivateValue(nullptr));
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

static void
pluralRules_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(UPLURAL_RULES_SLOT);
    if (!slot.isUndefined()) {
        if (UPluralRules* pr = static_cast<UPluralRules*>(slot.toPrivate()))
            uplrules_close(pr);
    }
}

static JSObject*
CreatePluralRulesPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &PluralRules, cx->names().PluralRules, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global, &PluralRulesClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(UPLURAL_RULES_SLOT, PrivateValue(nullptr));

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

/**************** RelativeTimeFormat *****************/

static void relativeTimeFormat_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t URELATIVE_TIME_FORMAT_SLOT = 0;
static const uint32_t RELATIVE_TIME_FORMAT_SLOTS_COUNT = 1;

static const ClassOps RelativeTimeFormatClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    relativeTimeFormat_finalize
};

static const Class RelativeTimeFormatClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(RELATIVE_TIME_FORMAT_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &RelativeTimeFormatClassOps
};

#if JS_HAS_TOSOURCE
static bool
relativeTimeFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().RelativeTimeFormat);
    return true;
}
#endif

static const JSFunctionSpec relativeTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_RelativeTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec relativeTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_RelativeTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("format", "Intl_RelativeTimeFormat_format", 2, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, relativeTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

static const JSPropertySpec relativeTimeFormat_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Intl.RelativeTimeFormat", JSPROP_READONLY),
    JS_PS_END};

/**
 * RelativeTimeFormat constructor.
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.1
 */
static bool
RelativeTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    if (!ThrowIfNotConstructing(cx, args, "Intl.RelativeTimeFormat"))
        return false;

    // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
    RootedObject proto(cx);
    if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    if (!proto) {
        proto = GlobalObject::getOrCreateRelativeTimeFormatPrototype(cx, cx->global());
        if (!proto)
            return false;
    }

    RootedObject relativeTimeFormat(cx);
    relativeTimeFormat = NewObjectWithGivenProto(cx, &RelativeTimeFormatClass, proto);
    if (!relativeTimeFormat)
        return false;

    relativeTimeFormat->as<NativeObject>().setReservedSlot(URELATIVE_TIME_FORMAT_SLOT, PrivateValue(nullptr));

    RootedValue locales(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    // Step 3.
    if (!intl::InitializeObject(cx, relativeTimeFormat, cx->names().InitializeRelativeTimeFormat, locales, options))
        return false;

    args.rval().setObject(*relativeTimeFormat);
    return true;
}

static void
relativeTimeFormat_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(URELATIVE_TIME_FORMAT_SLOT);
    if (!slot.isUndefined()) {
        if (URelativeDateTimeFormatter* rtf = static_cast<URelativeDateTimeFormatter*>(slot.toPrivate()))
            ureldatefmt_close(rtf);
    }
}

static JSObject*
CreateRelativeTimeFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &RelativeTimeFormat, cx->names().RelativeTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global, &RelativeTimeFormatClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(URELATIVE_TIME_FORMAT_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, relativeTimeFormat_static_methods))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, relativeTimeFormat_methods))
        return nullptr;

    if (!JS_DefineProperties(cx, proto, relativeTimeFormat_properties))
        return nullptr;

    RootedValue options(cx);
    if (!intl::CreateDefaultOptions(cx, &options))
        return nullptr;

    if (!intl::InitializeObject(cx, proto, cx->names().InitializeRelativeTimeFormat, UndefinedHandleValue,
                        options))
    {
        return nullptr;
    }

    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().RelativeTimeFormat, ctorValue, nullptr, nullptr, 0)) {
        return nullptr;
    }

    return proto;
}

bool
js::intl_RelativeTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp)
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


enum class RelativeTimeNumeric
{
    /**
     * Only strings with numeric components like `1 day ago`.
     */
    Always,
    /**
     * Natural-language strings like `yesterday` when possible,
     * otherwise strings with numeric components as in `7 months ago`.
     */
    Auto,
};

bool
js::intl_FormatRelativeTime(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    RootedObject relativeTimeFormat(cx, &args[0].toObject());

    RootedObject internals(cx, intl::GetInternalsObject(cx, relativeTimeFormat));
    if (!internals)
        return false;

    RootedValue value(cx);

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return false;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return false;

    if (!GetProperty(cx, internals, internals, cx->names().style, &value))
        return false;
    RootedLinearString style(cx, value.toString()->ensureLinear(cx));
    if (!style)
        return false;

    double t = args[1].toNumber();

    UDateRelativeDateTimeFormatterStyle relDateTimeStyle;

    if (StringEqualsAscii(style, "short")) {
        relDateTimeStyle = UDAT_STYLE_SHORT;
    } else if (StringEqualsAscii(style, "narrow")) {
        relDateTimeStyle = UDAT_STYLE_NARROW;
    } else {
        MOZ_ASSERT(StringEqualsAscii(style, "long"));
        relDateTimeStyle = UDAT_STYLE_LONG;
    }

    URelativeDateTimeUnit relDateTimeUnit;
    {
        JSLinearString* unit = args[2].toString()->ensureLinear(cx);
        if (!unit) {
            return false;
        }

        if (StringEqualsAscii(unit, "second") || StringEqualsAscii(unit, "seconds")) {
            relDateTimeUnit = UDAT_REL_UNIT_SECOND;
        } else if (StringEqualsAscii(unit, "minute") || StringEqualsAscii(unit, "minutes")) {
            relDateTimeUnit = UDAT_REL_UNIT_MINUTE;
        } else if (StringEqualsAscii(unit, "hour") || StringEqualsAscii(unit, "hours")) {
            relDateTimeUnit = UDAT_REL_UNIT_HOUR;
        } else if (StringEqualsAscii(unit, "day") || StringEqualsAscii(unit, "days")) {
            relDateTimeUnit = UDAT_REL_UNIT_DAY;
        } else if (StringEqualsAscii(unit, "week") || StringEqualsAscii(unit, "weeks")) {
            relDateTimeUnit = UDAT_REL_UNIT_WEEK;
        } else if (StringEqualsAscii(unit, "month") || StringEqualsAscii(unit, "months")) {
            relDateTimeUnit = UDAT_REL_UNIT_MONTH;
        } else if (StringEqualsAscii(unit, "quarter") || StringEqualsAscii(unit, "quarters")) {
            relDateTimeUnit = UDAT_REL_UNIT_QUARTER;
        } else {
            MOZ_ASSERT(StringEqualsAscii(unit, "year") || StringEqualsAscii(unit, "years"));
            relDateTimeUnit = UDAT_REL_UNIT_YEAR;
        }
    }

    if (!GetProperty(cx, internals, internals, cx->names().numeric, &value))
        return false;
    RootedLinearString numeric(cx, value.toString()->ensureLinear(cx));
    if (!numeric)
        return false;

    RelativeTimeNumeric relDateTimeNumeric;

    if (StringEqualsAscii(numeric, "auto")) {
        relDateTimeNumeric = RelativeTimeNumeric::Auto;
    } else {
        MOZ_ASSERT(StringEqualsAscii(numeric, "always"));
        relDateTimeNumeric = RelativeTimeNumeric::Always;
    }

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;
    UErrorCode status = U_ZERO_ERROR;
    URelativeDateTimeFormatter* rtf =
        ureldatefmt_open(IcuLocale(locale.ptr()), nullptr, relDateTimeStyle,
                         UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<URelativeDateTimeFormatter, ureldatefmt_close> closeRelativeTimeFormat(rtf);

    JSString* str =
        CallICU(cx, [rtf, t, relDateTimeUnit, relDateTimeNumeric](UChar* chars, int32_t size,
                                                               UErrorCode* status)
        {
            auto fmt = relDateTimeNumeric == RelativeTimeNumeric::Auto
                       ? ureldatefmt_format
                       : ureldatefmt_formatNumeric;
            return fmt(rtf, t, relDateTimeUnit, chars, size, status);
        });
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

bool
js::intl_GetCalendarInfo(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    UErrorCode status = U_ZERO_ERROR;
    const UChar* uTimeZone = nullptr;
    int32_t uTimeZoneLength = 0;
    UCalendar* cal = ucal_open(uTimeZone, uTimeZoneLength, locale.ptr(), UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UCalendar, ucal_close> toClose(cal);

    RootedObject info(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!info)
        return false;

    RootedValue v(cx);
    int32_t firstDayOfWeek = ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK);
    v.setInt32(firstDayOfWeek);

    if (!DefineProperty(cx, info, cx->names().firstDayOfWeek, v))
        return false;

    int32_t minDays = ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
    v.setInt32(minDays);
    if (!DefineProperty(cx, info, cx->names().minDays, v))
        return false;

    UCalendarWeekdayType prevDayType = ucal_getDayOfWeekType(cal, UCAL_SATURDAY, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    RootedValue weekendStart(cx), weekendEnd(cx);

    for (int i = UCAL_SUNDAY; i <= UCAL_SATURDAY; i++) {
        UCalendarDaysOfWeek dayOfWeek = static_cast<UCalendarDaysOfWeek>(i);
        UCalendarWeekdayType type = ucal_getDayOfWeekType(cal, dayOfWeek, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        if (prevDayType != type) {
            switch (type) {
              case UCAL_WEEKDAY:
                // If the first Weekday after Weekend is Sunday (1),
                // then the last Weekend day is Saturday (7).
                // Otherwise we'll just take the previous days number.
                weekendEnd.setInt32(i == 1 ? 7 : i - 1);
                break;
              case UCAL_WEEKEND:
                weekendStart.setInt32(i);
                break;
              case UCAL_WEEKEND_ONSET:
              case UCAL_WEEKEND_CEASE:
                // At the time this code was added, ICU apparently never behaves this way,
                // so just throw, so that users will report a bug and we can decide what to
                // do.
                intl::ReportInternalError(cx);
                return false;
              default:
                break;
            }
        }

        prevDayType = type;
    }

    MOZ_ASSERT(weekendStart.isInt32());
    MOZ_ASSERT(weekendEnd.isInt32());

    if (!DefineProperty(cx, info, cx->names().weekendStart, weekendStart))
        return false;

    if (!DefineProperty(cx, info, cx->names().weekendEnd, weekendEnd))
        return false;

    args.rval().setObject(*info);
    return true;
}

template<size_t N>
inline bool
MatchPart(const char** pattern, const char (&part)[N])
{
    if (strncmp(*pattern, part, N - 1))
        return false;

    *pattern += N - 1;
    return true;
}

bool
js::intl_ComputeDisplayNames(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    // 1. Assert: locale is a string.
    MOZ_ASSERT(args[0].isString());
    // 2. Assert: style is a string.
    MOZ_ASSERT(args[1].isString());
    // 3. Assert: keys is an Array.
    MOZ_ASSERT(args[2].isObject());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    JSAutoByteString style(cx, args[1].toString());
    if (!style)
        return false;

    RootedArrayObject keys(cx, &args[2].toObject().as<ArrayObject>());
    if (!keys)
        return false;

    // 4. Let result be ArrayCreate(0).
    RootedArrayObject result(cx, NewDenseUnallocatedArray(cx, keys->length()));
    if (!result)
        return false;

    UErrorCode status = U_ZERO_ERROR;

    UDateFormat* fmt =
        udat_open(UDAT_DEFAULT, UDAT_DEFAULT, IcuLocale(locale.ptr()),
        nullptr, 0, nullptr, 0, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UDateFormat, udat_close> datToClose(fmt);

    // UDateTimePatternGenerator will be needed for translations of date and
    // time fields like "month", "week", "day" etc.
    UDateTimePatternGenerator* dtpg = udatpg_open(IcuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UDateTimePatternGenerator, udatpg_close> datPgToClose(dtpg);

    RootedValue keyValue(cx);
    RootedString keyValStr(cx);
    RootedValue wordVal(cx);
    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    // 5. For each element of keys,
    for (uint32_t i = 0; i < keys->length(); i++) {
        /**
         * We iterate over keys array looking for paths that we have code
         * branches for.
         *
         * For any unknown path branch, the wordVal will keep NullValue and
         * we'll throw at the end.
         */

        if (!GetElement(cx, keys, keys, i, &keyValue))
            return false;

        JSAutoByteString pattern;
        keyValStr = keyValue.toString();
        if (!pattern.encodeUtf8(cx, keyValStr))
            return false;

        wordVal.setNull();

        // 5.a. Perform an implementation dependent algorithm to map a key to a
        //      corresponding display name.
        const char* pat = pattern.ptr();

        if (!MatchPart(&pat, "dates")) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
            return false;
        }

        if (!MatchPart(&pat, "/")) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
            return false;
        }

        if (MatchPart(&pat, "fields")) {
            if (!MatchPart(&pat, "/")) {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            UDateTimePatternField fieldType;

            if (MatchPart(&pat, "year")) {
                fieldType = UDATPG_YEAR_FIELD;
            } else if (MatchPart(&pat, "month")) {
                fieldType = UDATPG_MONTH_FIELD;
            } else if (MatchPart(&pat, "week")) {
                fieldType = UDATPG_WEEK_OF_YEAR_FIELD;
            } else if (MatchPart(&pat, "day")) {
                fieldType = UDATPG_DAY_FIELD;
            } else {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            // This part must be the final part with no trailing data.
            if (*pat != '\0') {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            int32_t resultSize;

            const UChar* value = udatpg_getAppendItemName(dtpg, fieldType, &resultSize);
            if (U_FAILURE(status)) {
                intl::ReportInternalError(cx);
                return false;
            }

            JSString* word = NewStringCopyN<CanGC>(cx, UCharToChar16(value), resultSize);
            if (!word)
                return false;

            wordVal.setString(word);
        } else if (MatchPart(&pat, "gregorian")) {
            if (!MatchPart(&pat, "/")) {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            UDateFormatSymbolType symbolType;
            int32_t index;

            if (MatchPart(&pat, "months")) {
                if (!MatchPart(&pat, "/")) {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }

                if (StringsAreEqual(style, "narrow")) {
                    symbolType = UDAT_STANDALONE_NARROW_MONTHS;
                } else if (StringsAreEqual(style, "short")) {
                    symbolType = UDAT_STANDALONE_SHORT_MONTHS;
                } else {
                    MOZ_ASSERT(StringsAreEqual(style, "long"));
                    symbolType = UDAT_STANDALONE_MONTHS;
                }

                if (MatchPart(&pat, "january")) {
                    index = UCAL_JANUARY;
                } else if (MatchPart(&pat, "february")) {
                    index = UCAL_FEBRUARY;
                } else if (MatchPart(&pat, "march")) {
                    index = UCAL_MARCH;
                } else if (MatchPart(&pat, "april")) {
                    index = UCAL_APRIL;
                } else if (MatchPart(&pat, "may")) {
                    index = UCAL_MAY;
                } else if (MatchPart(&pat, "june")) {
                    index = UCAL_JUNE;
                } else if (MatchPart(&pat, "july")) {
                    index = UCAL_JULY;
                } else if (MatchPart(&pat, "august")) {
                    index = UCAL_AUGUST;
                } else if (MatchPart(&pat, "september")) {
                    index = UCAL_SEPTEMBER;
                } else if (MatchPart(&pat, "october")) {
                    index = UCAL_OCTOBER;
                } else if (MatchPart(&pat, "november")) {
                    index = UCAL_NOVEMBER;
                } else if (MatchPart(&pat, "december")) {
                    index = UCAL_DECEMBER;
                } else {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }
            } else if (MatchPart(&pat, "weekdays")) {
                if (!MatchPart(&pat, "/")) {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }

                if (StringsAreEqual(style, "narrow")) {
                    symbolType = UDAT_STANDALONE_NARROW_WEEKDAYS;
                } else if (StringsAreEqual(style, "short")) {
                    symbolType = UDAT_STANDALONE_SHORT_WEEKDAYS;
                } else {
                    MOZ_ASSERT(StringsAreEqual(style, "long"));
                    symbolType = UDAT_STANDALONE_WEEKDAYS;
                }

                if (MatchPart(&pat, "monday")) {
                    index = UCAL_MONDAY;
                } else if (MatchPart(&pat, "tuesday")) {
                    index = UCAL_TUESDAY;
                } else if (MatchPart(&pat, "wednesday")) {
                    index = UCAL_WEDNESDAY;
                } else if (MatchPart(&pat, "thursday")) {
                    index = UCAL_THURSDAY;
                } else if (MatchPart(&pat, "friday")) {
                    index = UCAL_FRIDAY;
                } else if (MatchPart(&pat, "saturday")) {
                    index = UCAL_SATURDAY;
                } else if (MatchPart(&pat, "sunday")) {
                    index = UCAL_SUNDAY;
                } else {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }
            } else if (MatchPart(&pat, "dayperiods")) {
                if (!MatchPart(&pat, "/")) {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }

                symbolType = UDAT_AM_PMS;

                if (MatchPart(&pat, "am")) {
                    index = UCAL_AM;
                } else if (MatchPart(&pat, "pm")) {
                    index = UCAL_PM;
                } else {
                    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                    return false;
                }
            } else {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            // This part must be the final part with no trailing data.
            if (*pat != '\0') {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
                return false;
            }

            JSString* word = CallICU(cx, [fmt, symbolType, index](UChar* chars, int32_t size, UErrorCode* status) {
                return udat_getSymbols(fmt, symbolType, index, chars, size, status);
            });
            if (!word)
                return false;

            wordVal.setString(word);
        } else {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_KEY, pattern.ptr());
            return false;
        }

        MOZ_ASSERT(wordVal.isString());

        // 5.b. Append the result string to result.
        if (!DefineElement(cx, result, i, wordVal))
            return false;
    }

    // 6. Return result.
    args.rval().setObject(*result);
    return true;
}

/******************** Intl ********************/

const Class js::IntlClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Intl)
};

#if JS_HAS_TOSOURCE
static bool
intl_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Intl);
    return true;
}
#endif

static const JSFunctionSpec intl_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  intl_toSource,        0, 0),
#endif
    JS_SELF_HOSTED_FN("getCanonicalLocales", "Intl_getCanonicalLocales", 1, 0),
    JS_FS_END
};

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
/* static */ bool
GlobalObject::initIntlObject(JSContext* cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, GlobalObject::getOrCreateObjectPrototype(cx, global));
    if (!proto)
        return false;

    // The |Intl| object is just a plain object with some "static" function
    // properties and some constructor properties.
    RootedObject intl(cx, NewObjectWithGivenProto(cx, &IntlClass, proto, SingletonObject));
    if (!intl)
        return false;

    // Add the static functions.
    if (!JS_DefineFunctions(cx, intl, intl_static_methods))
        return false;

    // Add the constructor properties, computing and returning the relevant
    // prototype objects needed below.
    RootedObject collatorProto(cx, CreateCollatorPrototype(cx, intl, global));
    if (!collatorProto)
        return false;
    RootedObject dateTimeFormatProto(cx, CreateDateTimeFormatPrototype(cx, intl, global));
    if (!dateTimeFormatProto)
        return false;
    RootedObject numberFormatProto(cx, CreateNumberFormatPrototype(cx, intl, global));
    if (!numberFormatProto)
        return false;
    RootedObject pluralRulesProto(cx, CreatePluralRulesPrototype(cx, intl, global));
    if (!pluralRulesProto)
        return false;

    RootedObject relativeTimeFmtProto(cx, CreateRelativeTimeFormatPrototype(cx, intl, global));
    if (!relativeTimeFmtProto) {
        return false;
    }

    // The |Intl| object is fully set up now, so define the global property.
    RootedValue intlValue(cx, ObjectValue(*intl));
    if (!DefineProperty(cx, global, cx->names().Intl, intlValue, nullptr, nullptr,
                        JSPROP_RESOLVING))
    {
        return false;
    }

    // Now that the |Intl| object is successfully added, we can OOM-safely fill
    // in all relevant reserved global slots.

    // Cache the various prototypes, for use in creating instances of these
    // objects with the proper [[Prototype]] as "the original value of
    // |Intl.Collator.prototype|" and similar.  For builtin classes like
    // |String.prototype| we have |JSProto_*| that enables
    // |getPrototype(JSProto_*)|, but that has global-object-property-related
    // baggage we don't need or want, so we use one-off reserved slots.
    global->setReservedSlot(COLLATOR_PROTO, ObjectValue(*collatorProto));
    global->setReservedSlot(DATE_TIME_FORMAT_PROTO, ObjectValue(*dateTimeFormatProto));
    global->setReservedSlot(NUMBER_FORMAT_PROTO, ObjectValue(*numberFormatProto));
    global->setReservedSlot(PLURAL_RULES_PROTO, ObjectValue(*pluralRulesProto));
    global->setReservedSlot(RELATIVE_TIME_FORMAT_PROTO, ObjectValue(*relativeTimeFmtProto));

    // Also cache |Intl| to implement spec language that conditions behavior
    // based on values being equal to "the standard built-in |Intl| object".
    // Use |setConstructor| to correspond with |JSProto_Intl|.
    //
    // XXX We should possibly do a one-off reserved slot like above.
    global->setConstructor(JSProto_Intl, ObjectValue(*intl));
    return true;
}

JSObject*
js::InitIntlClass(JSContext* cx, HandleObject obj)
{
    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    if (!GlobalObject::initIntlObject(cx, global))
        return nullptr;

    return &global->getConstructor(JSProto_Intl).toObject();
}
