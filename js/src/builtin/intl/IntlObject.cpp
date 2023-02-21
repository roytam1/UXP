/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of the Intl object and its non-constructor properties. */

#include "builtin/intl/IntlObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"
#include "mozilla/Range.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "builtin/intl/Collator.h"
#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/DateTimeFormat.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/NumberFormat.h"
#include "builtin/intl/PluralRules.h"
#include "builtin/intl/RelativeTimeFormat.h"
#include "builtin/intl/ScopedICUObject.h"
#include "vm/GlobalObject.h"

#include "jsobjinlines.h"

using namespace js;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::StringsAreEqual;

/******************** Intl ********************/

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
