/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.DateTimeFormat implementation. */

#include "builtin/intl/DateTimeFormat.h"

#include "mozilla/Assertions.h"
#include "mozilla/Range.h"

#include "jscntxt.h"
#include "jsfriendapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/SharedIntlData.h"
#include "builtin/intl/TimeZoneDataGenerated.h"
#include "vm/GlobalObject.h"
#include "vm/Runtime.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::IsFinite;

using JS::ClippedTime;
using JS::TimeClip;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::SharedIntlData;
using js::intl::StringsAreEqual;

/******************** DateTimeFormat ********************/

const ClassOps DateTimeFormatObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    DateTimeFormatObject::finalize
};

const Class DateTimeFormatObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DateTimeFormatObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &DateTimeFormatObject::classOps_
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

        obj = NewObjectWithGivenProto<DateTimeFormatObject>(cx, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(DateTimeFormatObject::INTERNALS_SLOT, NullValue());
        obj->as<NativeObject>().setReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT, PrivateValue(nullptr));
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

void
js::DateTimeFormatObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<DateTimeFormatObject>().getReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT);
    if (!slot.isUndefined()) {
        if (UDateFormat* df = static_cast<UDateFormat*>(slot.toPrivate()))
            udat_close(df);
    }
}

JSObject*
js::CreateDateTimeFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = GlobalObject::createConstructor(cx, &DateTimeFormat, cx->names().DateTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, GlobalObject::createBlankPrototype(cx, global,
                                                                    &DateTimeFormatObject::class_));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT, PrivateValue(nullptr));

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
    bool isDateTimeFormatInstance = dateTimeFormat->getClass() == &DateTimeFormatObject::class_;
    UDateFormat* df;
    if (isDateTimeFormatInstance) {
        void* priv =
            dateTimeFormat->as<NativeObject>().getReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT).toPrivate();
        df = static_cast<UDateFormat*>(priv);
        if (!df) {
            df = NewUDateFormat(cx, dateTimeFormat);
            if (!df)
                return false;
            dateTimeFormat->as<NativeObject>().setReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT, PrivateValue(df));
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

