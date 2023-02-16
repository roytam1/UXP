/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Runtime-wide Intl data shared across compartments. */

#include "builtin/intl/SharedIntlData.h"

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"

#include <stdint.h>

#include "jsatom.h"
#include "jsstr.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/TimeZoneDataGenerated.h"
#include "js/Utility.h"

using js::HashNumber;
using js::intl::StringsAreEqual;

template<typename Char>
static constexpr Char
ToUpperASCII(Char c)
{
    return ('a' <= c && c <= 'z')
           ? (c & ~0x20)
           : c;
}

static_assert(ToUpperASCII('a') == 'A', "verifying 'a' uppercases correctly");
static_assert(ToUpperASCII('m') == 'M', "verifying 'm' uppercases correctly");
static_assert(ToUpperASCII('z') == 'Z', "verifying 'z' uppercases correctly");
static_assert(ToUpperASCII(u'a') == u'A', "verifying u'a' uppercases correctly");
static_assert(ToUpperASCII(u'k') == u'K', "verifying u'k' uppercases correctly");
static_assert(ToUpperASCII(u'z') == u'Z', "verifying u'z' uppercases correctly");

template<typename Char>
static HashNumber
HashStringIgnoreCaseASCII(const Char* s, size_t length)
{
    uint32_t hash = 0;
    for (size_t i = 0; i < length; i++)
        hash = mozilla::AddToHash(hash, ToUpperASCII(s[i]));
    return hash;
}

template<typename Char1, typename Char2>
static bool
EqualCharsIgnoreCaseASCII(const Char1* s1, const Char2* s2, size_t len)
{
    for (const Char1* s1end = s1 + len; s1 < s1end; s1++, s2++) {
        if (ToUpperASCII(*s1) != ToUpperASCII(*s2))
            return false;
    }
    return true;
}

js::intl::SharedIntlData::TimeZoneHasher::Lookup::Lookup(JSFlatString* timeZone)
  : isLatin1(timeZone->hasLatin1Chars()), length(timeZone->length())
{
    if (isLatin1) {
        latin1Chars = timeZone->latin1Chars(nogc);
        hash = HashStringIgnoreCaseASCII(latin1Chars, length);
    } else {
        twoByteChars = timeZone->twoByteChars(nogc);
        hash = HashStringIgnoreCaseASCII(twoByteChars, length);
    }
}

bool
js::intl::SharedIntlData::TimeZoneHasher::match(TimeZoneName key, const Lookup& lookup)
{
    if (key->length() != lookup.length)
        return false;

    // Compare time zone names ignoring ASCII case differences.
    if (key->hasLatin1Chars()) {
        const Latin1Char* keyChars = key->latin1Chars(lookup.nogc);
        if (lookup.isLatin1)
            return EqualCharsIgnoreCaseASCII(keyChars, lookup.latin1Chars, lookup.length);
        return EqualCharsIgnoreCaseASCII(keyChars, lookup.twoByteChars, lookup.length);
    }

    const char16_t* keyChars = key->twoByteChars(lookup.nogc);
    if (lookup.isLatin1)
        return EqualCharsIgnoreCaseASCII(lookup.latin1Chars, keyChars, lookup.length);
    return EqualCharsIgnoreCaseASCII(keyChars, lookup.twoByteChars, lookup.length);
}

static bool
IsLegacyICUTimeZone(const char* timeZone)
{
    for (const auto& legacyTimeZone : js::timezone::legacyICUTimeZones) {
        if (StringsAreEqual(timeZone, legacyTimeZone))
            return true;
    }
    return false;
}

bool
js::intl::SharedIntlData::ensureTimeZones(JSContext* cx)
{
    if (timeZoneDataInitialized)
        return true;

    // If initTimeZones() was called previously, but didn't complete due to
    // OOM, clear all sets/maps and start from scratch.
    if (availableTimeZones.initialized())
        availableTimeZones.finish();
    if (!availableTimeZones.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucal_openTimeZones(&status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    RootedAtom timeZone(cx);
    while (true) {
        int32_t size;
        const char* rawTimeZone = uenum_next(values, &size, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        if (rawTimeZone == nullptr)
            break;

        // Skip legacy ICU time zone names.
        if (IsLegacyICUTimeZone(rawTimeZone))
            continue;

        MOZ_ASSERT(size >= 0);
        timeZone = Atomize(cx, rawTimeZone, size_t(size));
        if (!timeZone)
            return false;

        TimeZoneHasher::Lookup lookup(timeZone);
        TimeZoneSet::AddPtr p = availableTimeZones.lookupForAdd(lookup);

        // ICU shouldn't report any duplicate time zone names, but if it does,
        // just ignore the duplicate name.
        if (!p && !availableTimeZones.add(p, timeZone)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    if (ianaZonesTreatedAsLinksByICU.initialized())
        ianaZonesTreatedAsLinksByICU.finish();
    if (!ianaZonesTreatedAsLinksByICU.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    for (const char* rawTimeZone : timezone::ianaZonesTreatedAsLinksByICU) {
        MOZ_ASSERT(rawTimeZone != nullptr);
        timeZone = Atomize(cx, rawTimeZone, strlen(rawTimeZone));
        if (!timeZone)
            return false;

        TimeZoneHasher::Lookup lookup(timeZone);
        TimeZoneSet::AddPtr p = ianaZonesTreatedAsLinksByICU.lookupForAdd(lookup);
        MOZ_ASSERT(!p, "Duplicate entry in timezone::ianaZonesTreatedAsLinksByICU");

        if (!ianaZonesTreatedAsLinksByICU.add(p, timeZone)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    if (ianaLinksCanonicalizedDifferentlyByICU.initialized())
        ianaLinksCanonicalizedDifferentlyByICU.finish();
    if (!ianaLinksCanonicalizedDifferentlyByICU.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    RootedAtom linkName(cx);
    RootedAtom& target = timeZone;
    for (const auto& linkAndTarget : timezone::ianaLinksCanonicalizedDifferentlyByICU) {
        const char* rawLinkName = linkAndTarget.link;
        const char* rawTarget = linkAndTarget.target;

        MOZ_ASSERT(rawLinkName != nullptr);
        linkName = Atomize(cx, rawLinkName, strlen(rawLinkName));
        if (!linkName)
            return false;

        MOZ_ASSERT(rawTarget != nullptr);
        target = Atomize(cx, rawTarget, strlen(rawTarget));
        if (!target)
            return false;

        TimeZoneHasher::Lookup lookup(linkName);
        TimeZoneMap::AddPtr p = ianaLinksCanonicalizedDifferentlyByICU.lookupForAdd(lookup);
        MOZ_ASSERT(!p, "Duplicate entry in timezone::ianaLinksCanonicalizedDifferentlyByICU");

        if (!ianaLinksCanonicalizedDifferentlyByICU.add(p, linkName, target)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    MOZ_ASSERT(!timeZoneDataInitialized, "ensureTimeZones is neither reentrant nor thread-safe");
    timeZoneDataInitialized = true;

    return true;
}

bool
js::intl::SharedIntlData::validateTimeZoneName(JSContext* cx, HandleString timeZone,
                                               MutableHandleString result)
{
    if (!ensureTimeZones(cx))
        return false;

    Rooted<JSFlatString*> timeZoneFlat(cx, timeZone->ensureFlat(cx));
    if (!timeZoneFlat)
        return false;

    TimeZoneHasher::Lookup lookup(timeZoneFlat);
    if (TimeZoneSet::Ptr p = availableTimeZones.lookup(lookup))
        result.set(*p);

    return true;
}

bool
js::intl::SharedIntlData::tryCanonicalizeTimeZoneConsistentWithIANA(JSContext* cx, HandleString timeZone,
                                                                    MutableHandleString result)
{
    if (!ensureTimeZones(cx))
        return false;

    Rooted<JSFlatString*> timeZoneFlat(cx, timeZone->ensureFlat(cx));
    if (!timeZoneFlat)
        return false;

    TimeZoneHasher::Lookup lookup(timeZoneFlat);
    MOZ_ASSERT(availableTimeZones.has(lookup), "Invalid time zone name");

    if (TimeZoneMap::Ptr p = ianaLinksCanonicalizedDifferentlyByICU.lookup(lookup)) {
        // The effectively supported time zones aren't known at compile time,
        // when
        // 1. SpiderMonkey was compiled with "--with-system-icu".
        // 2. ICU's dynamic time zone data loading feature was used.
        //    (ICU supports loading time zone files at runtime through the
        //    ICU_TIMEZONE_FILES_DIR environment variable.)
        // Ensure ICU supports the new target zone before applying the update.
        TimeZoneName targetTimeZone = p->value();
        TimeZoneHasher::Lookup targetLookup(targetTimeZone);
        if (availableTimeZones.has(targetLookup))
            result.set(targetTimeZone);
    } else if (TimeZoneSet::Ptr p = ianaZonesTreatedAsLinksByICU.lookup(lookup)) {
        result.set(*p);
    }

    return true;
}

void
js::intl::SharedIntlData::destroyInstance()
{
    availableTimeZones.finish();
    ianaZonesTreatedAsLinksByICU.finish();
    ianaLinksCanonicalizedDifferentlyByICU.finish();
}

void
js::intl::SharedIntlData::trace(JSTracer* trc)
{
    // Atoms are always tenured.
    if (!trc->runtime()->isHeapMinorCollecting()) {
        availableTimeZones.trace(trc);
        ianaZonesTreatedAsLinksByICU.trace(trc);
        ianaLinksCanonicalizedDifferentlyByICU.trace(trc);
    }
}

size_t
js::intl::SharedIntlData::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return availableTimeZones.sizeOfExcludingThis(mallocSizeOf) +
           ianaZonesTreatedAsLinksByICU.sizeOfExcludingThis(mallocSizeOf) +
           ianaLinksCanonicalizedDifferentlyByICU.sizeOfExcludingThis(mallocSizeOf);
}
