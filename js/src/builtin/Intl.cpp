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
#include "builtin/intl/DateTimeFormat.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/NumberFormat.h"
#include "builtin/intl/PluralRules.h"
#include "builtin/intl/ScopedICUObject.h"
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

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::StringsAreEqual;

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
