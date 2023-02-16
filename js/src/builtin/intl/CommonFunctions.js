/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Portions Copyright Norbert Lindenberg 2011-2012. */


/**
 * Holder object for encapsulating regexp instances.
 *
 * Regular expression instances should be created after the initialization of
 * self-hosted global.
 */
var internalIntlRegExps = std_Object_create(null);
internalIntlRegExps.unicodeLocaleExtensionSequenceRE = null;
internalIntlRegExps.languageTagRE = null;
internalIntlRegExps.duplicateVariantRE = null;
internalIntlRegExps.duplicateSingletonRE = null;
internalIntlRegExps.isWellFormedCurrencyCodeRE = null;
internalIntlRegExps.currencyDigitsRE = null;

/**
 * Regular expression matching a "Unicode locale extension sequence", which the
 * specification defines as: "any substring of a language tag that starts with
 * a separator '-' and the singleton 'u' and includes the maximum sequence of
 * following non-singleton subtags and their preceding '-' separators."
 *
 * Alternatively, this may be defined as: the components of a language tag that
 * match the extension production in RFC 5646, where the singleton component is
 * "u".
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.1.
 */
function getUnicodeLocaleExtensionSequenceRE() {
    return internalIntlRegExps.unicodeLocaleExtensionSequenceRE ||
           (internalIntlRegExps.unicodeLocaleExtensionSequenceRE =
            RegExpCreate("-u(?:-[a-z0-9]{2,8})+"));
}


/**
 * Removes Unicode locale extension sequences from the given language tag.
 */
function removeUnicodeExtensions(locale) {
    // A wholly-privateuse locale has no extension sequences.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return locale;

    // Otherwise, split on "-x-" marking the start of any privateuse component.
    // Replace Unicode locale extension sequences in the left half, and return
    // the concatenation.
    var pos = callFunction(std_String_indexOf, locale, "-x-");
    if (pos < 0)
        pos = locale.length;

    var left = callFunction(String_substring, locale, 0, pos);
    var right = callFunction(String_substring, locale, pos);

    var extensions;
    var unicodeLocaleExtensionSequenceRE = getUnicodeLocaleExtensionSequenceRE();
    while ((extensions = regexp_exec_no_statics(unicodeLocaleExtensionSequenceRE, left)) !== null) {
        left = StringReplaceString(left, extensions[0], "");
        unicodeLocaleExtensionSequenceRE.lastIndex = 0;
    }

    var combined = left + right;
    assert(IsStructurallyValidLanguageTag(combined), "recombination produced an invalid language tag");
    assert(function() {
        var uindex = callFunction(std_String_indexOf, combined, "-u-");
        if (uindex < 0)
            return true;
        var xindex = callFunction(std_String_indexOf, combined, "-x-");
        return xindex > 0 && xindex < uindex;
    }(), "recombination failed to remove all Unicode locale extension sequences");

    return combined;
}


/**
 * Regular expression defining BCP 47 language tags.
 *
 * Spec: RFC 5646 section 2.1.
 */
function getLanguageTagRE() {
    if (internalIntlRegExps.languageTagRE)
        return internalIntlRegExps.languageTagRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // regular       = "art-lojban"        ; these tags match the 'langtag'
    //               / "cel-gaulish"       ; production, but their subtags
    //               / "no-bok"            ; are not extended language
    //               / "no-nyn"            ; or variant subtags: their meaning
    //               / "zh-guoyu"          ; is defined by their registration
    //               / "zh-hakka"          ; and all of these are deprecated
    //               / "zh-min"            ; in favor of a more modern
    //               / "zh-min-nan"        ; subtag or sequence of subtags
    //               / "zh-xiang"
    var regular = "(?:art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|zh-min|zh-min-nan|zh-xiang)";
    // irregular     = "en-GB-oed"         ; irregular tags do not match
    //                / "i-ami"             ; the 'langtag' production and
    //                / "i-bnn"             ; would not otherwise be
    //                / "i-default"         ; considered 'well-formed'
    //                / "i-enochian"        ; These tags are all valid,
    //                / "i-hak"             ; but most are deprecated
    //                / "i-klingon"         ; in favor of more modern
    //                / "i-lux"             ; subtags or subtag
    //                / "i-mingo"           ; combination
    //                / "i-navajo"
    //                / "i-pwn"
    //                / "i-tao"
    //                / "i-tay"
    //                / "i-tsu"
    //                / "sgn-BE-FR"
    //                / "sgn-BE-NL"
    //                / "sgn-CH-DE"
    var irregular = "(?:en-GB-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|i-tsu|sgn-BE-FR|sgn-BE-NL|sgn-CH-DE)";
    // grandfathered = irregular           ; non-redundant tags registered
    //               / regular             ; during the RFC 3066 era
    var grandfathered = "(?:" + irregular + "|" + regular + ")";
    // privateuse    = "x" 1*("-" (1*8alphanum))
    var privateuse = "(?:x(?:-[a-z0-9]{1,8})+)";
    // singleton     = DIGIT               ; 0 - 9
    //               / %x41-57             ; A - W
    //               / %x59-5A             ; Y - Z
    //               / %x61-77             ; a - w
    //               / %x79-7A             ; y - z
    var singleton = "(?:" + DIGIT + "|[A-WY-Za-wy-z])";
    // extension     = singleton 1*("-" (2*8alphanum))
    var extension = "(?:" + singleton + "(?:-" + alphanum + "{2,8})+)";
    // variant       = 5*8alphanum         ; registered variants
    //               / (DIGIT 3alphanum)
    var variant = "(?:" + alphanum + "{5,8}|(?:" + DIGIT + alphanum + "{3}))";
    // region        = 2ALPHA              ; ISO 3166-1 code
    //               / 3DIGIT              ; UN M.49 code
    var region = "(?:" + ALPHA + "{2}|" + DIGIT + "{3})";
    // script        = 4ALPHA              ; ISO 15924 code
    var script = "(?:" + ALPHA + "{4})";
    // extlang       = 3ALPHA              ; selected ISO 639 codes
    //                 *2("-" 3ALPHA)      ; permanently reserved
    var extlang = "(?:" + ALPHA + "{3}(?:-" + ALPHA + "{3}){0,2})";
    // language      = 2*3ALPHA            ; shortest ISO 639 code
    //                 ["-" extlang]       ; sometimes followed by
    //                                     ; extended language subtags
    //               / 4ALPHA              ; or reserved for future use
    //               / 5*8ALPHA            ; or registered language subtag
    var language = "(?:" + ALPHA + "{2,3}(?:-" + extlang + ")?|" + ALPHA + "{4}|" + ALPHA + "{5,8})";
    // langtag       = language
    //                 ["-" script]
    //                 ["-" region]
    //                 *("-" variant)
    //                 *("-" extension)
    //                 ["-" privateuse]
    var langtag = language + "(?:-" + script + ")?(?:-" + region + ")?(?:-" +
                  variant + ")*(?:-" + extension + ")*(?:-" + privateuse + ")?";
    // Language-Tag  = langtag             ; normal language tags
    //               / privateuse          ; private use tag
    //               / grandfathered       ; grandfathered tags
    var languageTag = "^(?:" + langtag + "|" + privateuse + "|" + grandfathered + ")$";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).
    return (internalIntlRegExps.languageTagRE = RegExpCreate(languageTag, "i"));
}


function getDuplicateVariantRE() {
    if (internalIntlRegExps.duplicateVariantRE)
        return internalIntlRegExps.duplicateVariantRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // variant       = 5*8alphanum         ; registered variants
    //               / (DIGIT 3alphanum)
    var variant = "(?:" + alphanum + "{5,8}|(?:" + DIGIT + alphanum + "{3}))";

    // Match a langtag that contains a duplicate variant.
    var duplicateVariant =
        // Match everything in a langtag prior to any variants, and maybe some
        // of the variants as well (which makes this pattern inefficient but
        // not wrong, for our purposes);
        "(?:" + alphanum + "{2,8}-)+" +
        // a variant, parenthesised so that we can refer back to it later;
        "(" + variant + ")-" +
        // zero or more subtags at least two characters long (thus stopping
        // before extension and privateuse components);
        "(?:" + alphanum + "{2,8}-)*" +
        // and the same variant again
        "\\1" +
        // ...but not followed by any characters that would turn it into a
        // different subtag.
        "(?!" + alphanum + ")";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).  Using
    // character classes covering both upper- and lower-case characters nearly
    // addresses this -- but for the possibility of variant repetition with
    // differing case, e.g. "en-variant-Variant".  Use a case-insensitive
    // regular expression to address this.  (Note that there's no worry about
    // case transformation accepting invalid characters here: users have
    // already verified the string is alphanumeric Latin plus "-".)
    return (internalIntlRegExps.duplicateVariantRE = RegExpCreate(duplicateVariant, "i"));
}


function getDuplicateSingletonRE() {
    if (internalIntlRegExps.duplicateSingletonRE)
        return internalIntlRegExps.duplicateSingletonRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // singleton     = DIGIT               ; 0 - 9
    //               / %x41-57             ; A - W
    //               / %x59-5A             ; Y - Z
    //               / %x61-77             ; a - w
    //               / %x79-7A             ; y - z
    var singleton = "(?:" + DIGIT + "|[A-WY-Za-wy-z])";

    // Match a langtag that contains a duplicate singleton.
    var duplicateSingleton =
        // Match a singleton subtag, parenthesised so that we can refer back to
        // it later;
        "-(" + singleton + ")-" +
        // then zero or more subtags;
        "(?:" + alphanum + "+-)*" +
        // and the same singleton again
        "\\1" +
        // ...but not followed by any characters that would turn it into a
        // different subtag.
        "(?!" + alphanum + ")";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).  Using
    // character classes covering both upper- and lower-case characters nearly
    // addresses this -- but for the possibility of singleton repetition with
    // differing case, e.g. "en-u-foo-U-foo".  Use a case-insensitive regular
    // expression to address this.  (Note that there's no worry about case
    // transformation accepting invalid characters here: users have already
    // verified the string is alphanumeric Latin plus "-".)
    return (internalIntlRegExps.duplicateSingletonRE = RegExpCreate(duplicateSingleton, "i"));
}


/**
 * Verifies that the given string is a well-formed BCP 47 language tag
 * with no duplicate variant or singleton subtags.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.2.
 */
function IsStructurallyValidLanguageTag(locale) {
    assert(typeof locale === "string", "IsStructurallyValidLanguageTag");
    var languageTagRE = getLanguageTagRE();
    if (!regexp_test_no_statics(languageTagRE, locale))
        return false;

    // Before checking for duplicate variant or singleton subtags with
    // regular expressions, we have to get private use subtag sequences
    // out of the picture.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return true;
    var pos = callFunction(std_String_indexOf, locale, "-x-");
    if (pos !== -1)
        locale = callFunction(String_substring, locale, 0, pos);

    // Check for duplicate variant or singleton subtags.
    var duplicateVariantRE = getDuplicateVariantRE();
    var duplicateSingletonRE = getDuplicateSingletonRE();
    return !regexp_test_no_statics(duplicateVariantRE, locale) &&
           !regexp_test_no_statics(duplicateSingletonRE, locale);
}

/**
 * Joins the array elements in the given range with the supplied separator.
 */
function ArrayJoinRange(array, separator, from, to = array.length) {
    assert(typeof separator === "string", "|separator| is a string value");
    assert(typeof from === "number", "|from| is a number value");
    assert(typeof to === "number", "|to| is a number value");
    assert(0 <= from && from <= to && to <= array.length, "|from| and |to| form a valid range");

    if (from === to)
        return "";

    var result = array[from];
    for (var i = from + 1; i < to; i++) {
        result += separator + array[i];
    }
    return result;
}

/**
 * Canonicalizes the given structurally valid BCP 47 language tag, including
 * regularized case of subtags. For example, the language tag
 * Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE, where
 *
 *     Zh             ; 2*3ALPHA
 *     -NAN           ; ["-" extlang]
 *     -haNS          ; ["-" script]
 *     -bu            ; ["-" region]
 *     -variant2      ; *("-" variant)
 *     -Variant1
 *     -u-ca-chinese  ; *("-" extension)
 *     -t-Zh-laTN
 *     -x-PRIVATE     ; ["-" privateuse]
 *
 * becomes nan-Hans-mm-variant2-variant1-t-zh-latn-u-ca-chinese-x-private
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.3.
 * Spec: RFC 5646, section 4.5.
 */
function CanonicalizeLanguageTag(locale) {
    assert(IsStructurallyValidLanguageTag(locale), "CanonicalizeLanguageTag");

    // The input
    // "Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE"
    // will be used throughout this method to illustrate how it works.

    // Language tags are compared and processed case-insensitively, so
    // technically it's not necessary to adjust case. But for easier processing,
    // and because the canonical form for most subtags is lower case, we start
    // with lower case for all.
    // "Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE" ->
    // "zh-nan-hans-bu-variant2-variant1-u-ca-chinese-t-zh-latn-x-private"
    locale = callFunction(std_String_toLowerCase, locale);

    // Handle mappings for complete tags.
    if (callFunction(std_Object_hasOwnProperty, langTagMappings, locale))
        return langTagMappings[locale];

    var subtags = StringSplitString(ToString(locale), "-");
    var i = 0;

    // Handle the standard part: All subtags before the first singleton or "x".
    // "zh-nan-hans-bu-variant2-variant1"
    while (i < subtags.length) {
        var subtag = subtags[i];

        // If we reach the start of an extension sequence or private use part,
        // we're done with this loop. We have to check for i > 0 because for
        // irregular language tags, such as i-klingon, the single-character
        // subtag "i" is not the start of an extension sequence.
        // In the example, we break at "u".
        if (subtag.length === 1 && (i > 0 || subtag === "x"))
            break;

        if (i !== 0) {
            if (subtag.length === 4) {
                // 4-character subtags that are not in initial position are
                // script codes; their first character needs to be capitalized.
                // "hans" -> "Hans"
                subtag = callFunction(std_String_toUpperCase, subtag[0]) +
                         callFunction(String_substring, subtag, 1);
            } else if (subtag.length === 2) {
                // 2-character subtags that are not in initial position are
                // region codes; they need to be upper case. "bu" -> "BU"
                subtag = callFunction(std_String_toUpperCase, subtag);
            }
        }
        if (callFunction(std_Object_hasOwnProperty, langSubtagMappings, subtag)) {
            // Replace deprecated subtags with their preferred values.
            // "BU" -> "MM"
            // This has to come after we capitalize region codes because
            // otherwise some language and region codes could be confused.
            // For example, "in" is an obsolete language code for Indonesian,
            // but "IN" is the country code for India.
            // Note that the script generating langSubtagMappings makes sure
            // that no regular subtag mapping will replace an extlang code.
            subtag = langSubtagMappings[subtag];
        } else if (callFunction(std_Object_hasOwnProperty, extlangMappings, subtag)) {
            // Replace deprecated extlang subtags with their preferred values,
            // and remove the preceding subtag if it's a redundant prefix.
            // "zh-nan" -> "nan"
            // Note that the script generating extlangMappings makes sure that
            // no extlang mapping will replace a normal language code.
            subtag = extlangMappings[subtag].preferred;
            if (i === 1 && extlangMappings[subtag].prefix === subtags[0]) {
                callFunction(std_Array_shift, subtags);
                i--;
            }
        }
        subtags[i] = subtag;
        i++;
    }
    var normal = ArrayJoinRange(subtags, "-", 0, i);

    // Extension sequences are sorted by their singleton characters.
    // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
    var extensions = new List();
    while (i < subtags.length && subtags[i] !== "x") {
        var extensionStart = i;
        i++;
        while (i < subtags.length && subtags[i].length > 1)
            i++;
        var extension = ArrayJoinRange(subtags, "-", extensionStart, i);
        callFunction(std_Array_push, extensions, extension);
    }
    callFunction(std_Array_sort, extensions);

    // Private use sequences are left as is. "x-private"
    var privateUse = "";
    if (i < subtags.length)
        privateUse = ArrayJoinRange(subtags, "-", i);

    // Put everything back together.
    var canonical = normal;
    if (extensions.length > 0)
        canonical += "-" + callFunction(std_Array_join, extensions, "-");
    if (privateUse.length > 0) {
        // Be careful of a Language-Tag that is entirely privateuse.
        if (canonical.length > 0)
            canonical += "-" + privateUse;
        else
            canonical = privateUse;
    }

    return canonical;
}

function localeContainsNoUnicodeExtensions(locale) {
    // No "-u-", no possible Unicode extension.
    if (callFunction(std_String_indexOf, locale, "-u-") === -1)
        return true;

    // "-u-" within privateuse also isn't one.
    if (callFunction(std_String_indexOf, locale, "-u-") > callFunction(std_String_indexOf, locale, "-x-"))
        return true;

    // An entirely-privateuse tag doesn't contain extensions.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return true;

    // Otherwise, we have a Unicode extension sequence.
    return false;
}


// The last-ditch locale is used if none of the available locales satisfies a
// request. "en-GB" is used based on the assumptions that English is the most
// common second language, that both en-GB and en-US are normally available in
// an implementation, and that en-GB is more representative of the English used
// in other locales.
function lastDitchLocale() {
    // Per bug 1177929, strings don't clone out of self-hosted code as atoms,
    // breaking IonBuilder::constant.  Put this in a function for now.
    return "en-GB";
}


// Certain old, commonly-used language tags that lack a script, are expected to
// nonetheless imply one.  This object maps these old-style tags to modern
// equivalents.
var oldStyleLanguageTagMappings = {
    "pa-PK": "pa-Arab-PK",
    "zh-CN": "zh-Hans-CN",
    "zh-HK": "zh-Hant-HK",
    "zh-SG": "zh-Hans-SG",
    "zh-TW": "zh-Hant-TW",
};


var localeCandidateCache = {
    runtimeDefaultLocale: undefined,
    candidateDefaultLocale: undefined,
};


var localeCache = {
    runtimeDefaultLocale: undefined,
    defaultLocale: undefined,
};


/**
 * Compute the candidate default locale: the locale *requested* to be used as
 * the default locale.  We'll use it if and only if ICU provides support (maybe
 * fallback support, e.g. supporting "de-ZA" through "de" support implied by a
 * "de-DE" locale).
 */
function DefaultLocaleIgnoringAvailableLocales() {
    const runtimeDefaultLocale = RuntimeDefaultLocale();
    if (runtimeDefaultLocale === localeCandidateCache.runtimeDefaultLocale)
        return localeCandidateCache.candidateDefaultLocale;

    // If we didn't get a cache hit, compute the candidate default locale and
    // cache it.  Fall back on the last-ditch locale when necessary.
    var candidate;
    if (!IsStructurallyValidLanguageTag(runtimeDefaultLocale)) {
        candidate = lastDitchLocale();
    } else {
        candidate = CanonicalizeLanguageTag(runtimeDefaultLocale);

        // The default locale must be in [[availableLocales]], and that list
        // must not contain any locales with Unicode extension sequences, so
        // remove any present in the candidate.
        candidate = removeUnicodeExtensions(candidate);

        if (callFunction(std_Object_hasOwnProperty, oldStyleLanguageTagMappings, candidate))
            candidate = oldStyleLanguageTagMappings[candidate];
    }

    // Cache the candidate locale until the runtime default locale changes.
    localeCandidateCache.candidateDefaultLocale = candidate;
    localeCandidateCache.runtimeDefaultLocale = runtimeDefaultLocale;

    assert(IsStructurallyValidLanguageTag(candidate),
           "the candidate must be structurally valid");
    assert(localeContainsNoUnicodeExtensions(candidate),
           "the candidate must not contain a Unicode extension sequence");

    return candidate;
}


/**
 * Returns the BCP 47 language tag for the host environment's current locale.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.4.
 */
function DefaultLocale() {
    const runtimeDefaultLocale = RuntimeDefaultLocale();
    if (runtimeDefaultLocale === localeCache.runtimeDefaultLocale)
        return localeCache.defaultLocale;

    // If we didn't have a cache hit, compute the candidate default locale.
    // Then use it as the actual default locale if ICU supports that locale
    // (perhaps via fallback).  Otherwise use the last-ditch locale.
    var candidate = DefaultLocaleIgnoringAvailableLocales();
    var locale;
    if (BestAvailableLocaleIgnoringDefault(callFunction(collatorInternalProperties.availableLocales,
                                                        collatorInternalProperties),
                                           candidate) &&
        BestAvailableLocaleIgnoringDefault(callFunction(numberFormatInternalProperties.availableLocales,
                                                        numberFormatInternalProperties),
                                           candidate) &&
        BestAvailableLocaleIgnoringDefault(callFunction(dateTimeFormatInternalProperties.availableLocales,
                                                        dateTimeFormatInternalProperties),
                                           candidate))
    {
        locale = candidate;
    } else {
        locale = lastDitchLocale();
    }

    assert(IsStructurallyValidLanguageTag(locale),
           "the computed default locale must be structurally valid");
    assert(locale === CanonicalizeLanguageTag(locale),
           "the computed default locale must be canonical");
    assert(localeContainsNoUnicodeExtensions(locale),
           "the computed default locale must not contain a Unicode extension sequence");

    localeCache.defaultLocale = locale;
    localeCache.runtimeDefaultLocale = runtimeDefaultLocale;

    return locale;
}


/**
 * Add old-style language tags without script code for locales that in current
 * usage would include a script subtag.  Also add an entry for the last-ditch
 * locale, in case ICU doesn't directly support it (but does support it through
 * fallback, e.g. supporting "en-GB" indirectly using "en" support).
 */
function addSpecialMissingLanguageTags(availableLocales) {
    // Certain old-style language tags lack a script code, but in current usage
    // they *would* include a script code.  Map these over to modern forms.
    var oldStyleLocales = std_Object_getOwnPropertyNames(oldStyleLanguageTagMappings);
    for (var i = 0; i < oldStyleLocales.length; i++) {
        var oldStyleLocale = oldStyleLocales[i];
        if (availableLocales[oldStyleLanguageTagMappings[oldStyleLocale]])
            availableLocales[oldStyleLocale] = true;
    }

    // Also forcibly provide the last-ditch locale.
    var lastDitch = lastDitchLocale();
    assert(lastDitch === "en-GB" && availableLocales["en"],
           "shouldn't be a need to add every locale implied by the last-" +
           "ditch locale, merely just the last-ditch locale");
    availableLocales[lastDitch] = true;
}


/**
 * Canonicalizes a locale list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.1.
 */
function CanonicalizeLocaleList(locales) {
    if (locales === undefined)
        return new List();
    var seen = new List();
    if (typeof locales === "string")
        locales = [locales];
    var O = ToObject(locales);
    var len = ToLength(O.length);
    var k = 0;
    while (k < len) {
        // Don't call ToString(k) - SpiderMonkey is faster with integers.
        var kPresent = HasProperty(O, k);
        if (kPresent) {
            var kValue = O[k];
            if (!(typeof kValue === "string" || IsObject(kValue)))
                ThrowTypeError(JSMSG_INVALID_LOCALES_ELEMENT);
            var tag = ToString(kValue);
            if (!IsStructurallyValidLanguageTag(tag))
                ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, tag);
            tag = CanonicalizeLanguageTag(tag);
            if (callFunction(ArrayIndexOf, seen, tag) === -1)
                callFunction(std_Array_push, seen, tag);
        }
        k++;
    }
    return seen;
}


function BestAvailableLocaleHelper(availableLocales, locale, considerDefaultLocale) {
    assert(IsStructurallyValidLanguageTag(locale), "invalid BestAvailableLocale locale structure");
    assert(locale === CanonicalizeLanguageTag(locale), "non-canonical BestAvailableLocale locale");
    assert(localeContainsNoUnicodeExtensions(locale), "locale must contain no Unicode extensions");

    // In the spec, [[availableLocales]] is formally a list of all available
    // locales.  But in our implementation, it's an *incomplete* list, not
    // necessarily including the default locale (and all locales implied by it,
    // e.g. "de" implied by "de-CH"), if that locale isn't in every
    // [[availableLocales]] list (because that locale is supported through
    // fallback, e.g. "de-CH" supported through "de").
    //
    // If we're considering the default locale, augment the spec loop with
    // additional checks to also test whether the current prefix is a prefix of
    // the default locale.

    var defaultLocale;
    if (considerDefaultLocale)
        defaultLocale = DefaultLocale();

    var candidate = locale;
    while (true) {
        if (availableLocales[candidate])
            return candidate;

        if (considerDefaultLocale && candidate.length <= defaultLocale.length) {
            if (candidate === defaultLocale)
                return candidate;
            if (callFunction(std_String_startsWith, defaultLocale, candidate + "-"))
                return candidate;
        }

        var pos = callFunction(std_String_lastIndexOf, candidate, "-");
        if (pos === -1)
            return undefined;

        if (pos >= 2 && candidate[pos - 2] === "-")
            pos -= 2;

        candidate = callFunction(String_substring, candidate, 0, pos);
    }
}


/**
 * Compares a BCP 47 language tag against the locales in availableLocales
 * and returns the best available match. Uses the fallback
 * mechanism of RFC 4647, section 3.4.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.2.
 * Spec: RFC 4647, section 3.4.
 */
function BestAvailableLocale(availableLocales, locale) {
    return BestAvailableLocaleHelper(availableLocales, locale, true);
}


/**
 * Identical to BestAvailableLocale, but does not consider the default locale
 * during computation.
 */
function BestAvailableLocaleIgnoringDefault(availableLocales, locale) {
    return BestAvailableLocaleHelper(availableLocales, locale, false);
}

var noRelevantExtensionKeys = [];

/**
 * Compares a BCP 47 language priority list against the set of locales in
 * availableLocales and determines the best available language to meet the
 * request. Options specified through Unicode extension subsequences are
 * ignored in the lookup, but information about such subsequences is returned
 * separately.
 *
 * This variant is based on the Lookup algorithm of RFC 4647 section 3.4.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.3.
 * Spec: RFC 4647, section 3.4.
 */
function LookupMatcher(availableLocales, requestedLocales) {
    var i = 0;
    var len = requestedLocales.length;
    var availableLocale;
    var locale, noExtensionsLocale;
    while (i < len && availableLocale === undefined) {
        locale = requestedLocales[i];
        noExtensionsLocale = removeUnicodeExtensions(locale);
        availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);
        i++;
    }

    var result = new Record();
    if (availableLocale !== undefined) {
        result.locale = availableLocale;
        if (locale !== noExtensionsLocale) {
            var unicodeLocaleExtensionSequenceRE = getUnicodeLocaleExtensionSequenceRE();
            var extensionMatch = regexp_exec_no_statics(unicodeLocaleExtensionSequenceRE, locale);
            var extension = extensionMatch[0];
            var extensionIndex = extensionMatch.index;
            result.extension = extension;
            result.extensionIndex = extensionIndex;
        }
    } else {
        result.locale = DefaultLocale();
    }
    return result;
}


/**
 * Compares a BCP 47 language priority list against the set of locales in
 * availableLocales and determines the best available language to meet the
 * request. Options specified through Unicode extension subsequences are
 * ignored in the lookup, but information about such subsequences is returned
 * separately.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.4.
 */
function BestFitMatcher(availableLocales, requestedLocales) {
    // this implementation doesn't have anything better
    return LookupMatcher(availableLocales, requestedLocales);
}


/**
 * Compares a BCP 47 language priority list against availableLocales and
 * determines the best available language to meet the request. Options specified
 * through Unicode extension subsequences are negotiated separately, taking the
 * caller's relevant extensions and locale data as well as client-provided
 * options into consideration.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.5.
 */
function ResolveLocale(availableLocales, requestedLocales, options, relevantExtensionKeys, localeData) {
    /*jshint laxbreak: true */

    // Steps 1-3.
    var matcher = options.localeMatcher;
    var r = (matcher === "lookup")
            ? LookupMatcher(availableLocales, requestedLocales)
            : BestFitMatcher(availableLocales, requestedLocales);

    // Step 4.
    var foundLocale = r.locale;

    // Step 5.a.
    var extension = r.extension;
    var extensionIndex, extensionSubtags, extensionSubtagsLength;

    // Step 5.
    if (extension !== undefined) {
        // Step 5.b.
        extensionIndex = r.extensionIndex;

        // Steps 5.d-e.
        extensionSubtags = StringSplitString(ToString(extension), "-");
        extensionSubtagsLength = extensionSubtags.length;
    }

    // Steps 6-7.
    var result = new Record();
    result.dataLocale = foundLocale;

    // Step 8.
    var supportedExtension = "-u";

    // Steps 9-11.
    var i = 0;
    var len = relevantExtensionKeys.length;
    while (i < len) {
        // Steps 11.a-c.
        var key = relevantExtensionKeys[i];

        // In this implementation, localeData is a function, not an object.
        var foundLocaleData = localeData(foundLocale);
        var keyLocaleData = foundLocaleData[key];

        // Locale data provides default value.
        // Step 11.d.
        var value = keyLocaleData[0];

        // Locale tag may override.

        // Step 11.e.
        var supportedExtensionAddition = "";

        // Step 11.f is implemented by Utilities.js.

        var valuePos;

        // Step 11.g.
        if (extensionSubtags !== undefined) {
            // Step 11.g.i.
            var keyPos = callFunction(ArrayIndexOf, extensionSubtags, key);

            // Step 11.g.ii.
            if (keyPos !== -1) {
                // Step 11.g.ii.1.
                if (keyPos + 1 < extensionSubtagsLength &&
                    extensionSubtags[keyPos + 1].length > 2)
                {
                    // Step 11.g.ii.1.a.
                    var requestedValue = extensionSubtags[keyPos + 1];

                    // Step 11.g.ii.1.b.
                    valuePos = callFunction(ArrayIndexOf, keyLocaleData, requestedValue);

                    // Step 11.g.ii.1.c.
                    if (valuePos !== -1) {
                        value = requestedValue;
                        supportedExtensionAddition = "-" + key + "-" + value;
                    }
                } else {
                    // Step 11.g.ii.2.

                    // According to the LDML spec, if there's no type value,
                    // and true is an allowed value, it's used.

                    // Step 11.g.ii.2.a.
                    valuePos = callFunction(ArrayIndexOf, keyLocaleData, "true");

                    // Step 11.g.ii.2.b.
                    if (valuePos !== -1)
                        value = "true";
                }
            }
        }

        // Options override all.

        // Step 11.h.i.
        var optionsValue = options[key];

        // Step 11.h, 11.h.ii.
        if (optionsValue !== undefined &&
            callFunction(ArrayIndexOf, keyLocaleData, optionsValue) !== -1)
        {
            // Step 11.h.ii.1.
            if (optionsValue !== value) {
                value = optionsValue;
                supportedExtensionAddition = "";
            }
        }

        // Steps 11.i-k.
        result[key] = value;
        supportedExtension += supportedExtensionAddition;
        i++;
    }

    // Step 12.
    if (supportedExtension.length > 2) {
        var preExtension = callFunction(String_substring, foundLocale, 0, extensionIndex);
        var postExtension = callFunction(String_substring, foundLocale, extensionIndex);
        foundLocale = preExtension + supportedExtension + postExtension;
    }

    // Steps 13-14.
    result.locale = foundLocale;
    return result;
}


/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.6.
 */
function LookupSupportedLocales(availableLocales, requestedLocales) {
    // Steps 1-2.
    var len = requestedLocales.length;
    var subset = new List();

    // Steps 3-4.
    var k = 0;
    while (k < len) {
        // Steps 4.a-b.
        var locale = requestedLocales[k];
        var noExtensionsLocale = removeUnicodeExtensions(locale);

        // Step 4.c-d.
        var availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);
        if (availableLocale !== undefined)
            callFunction(std_Array_push, subset, locale);

        // Step 4.e.
        k++;
    }

    // Steps 5-6.
    return callFunction(std_Array_slice, subset, 0);
}


/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.7.
 */
function BestFitSupportedLocales(availableLocales, requestedLocales) {
    // don't have anything better
    return LookupSupportedLocales(availableLocales, requestedLocales);
}


/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.8.
 */
function SupportedLocales(availableLocales, requestedLocales, options) {
    /*jshint laxbreak: true */

    // Step 1.
    var matcher;
    if (options !== undefined) {
        // Steps 1.a-b.
        options = ToObject(options);
        matcher = options.localeMatcher;

        // Step 1.c.
        if (matcher !== undefined) {
            matcher = ToString(matcher);
            if (matcher !== "lookup" && matcher !== "best fit")
                ThrowRangeError(JSMSG_INVALID_LOCALE_MATCHER, matcher);
        }
    }

    // Steps 2-3.
    var subset = (matcher === undefined || matcher === "best fit")
                 ? BestFitSupportedLocales(availableLocales, requestedLocales)
                 : LookupSupportedLocales(availableLocales, requestedLocales);

    // Step 4.
    for (var i = 0; i < subset.length; i++) {
        _DefineDataProperty(subset, i, subset[i],
                            ATTR_ENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);
    }
    _DefineDataProperty(subset, "length", subset.length,
                        ATTR_NONENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);

    // Step 5.
    return subset;
}


/**
 * Extracts a property value from the provided options object, converts it to
 * the required type, checks whether it is one of a list of allowed values,
 * and fills in a fallback value if necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.9.
 */
function GetOption(options, property, type, values, fallback) {
    // Step 1.
    var value = options[property];

    // Step 2.
    if (value !== undefined) {
        // Steps 2.a-c.
        if (type === "boolean")
            value = ToBoolean(value);
        else if (type === "string")
            value = ToString(value);
        else
            assert(false, "GetOption");

        // Step 2.d.
        if (values !== undefined && callFunction(ArrayIndexOf, values, value) === -1)
            ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, property, value);

        // Step 2.e.
        return value;
    }

    // Step 3.
    return fallback;
}

/**
 * Extracts a property value from the provided options object, converts it to a
 * Number value, checks whether it is in the allowed range, and fills in a
 * fallback value if necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.10.
 */
function GetNumberOption(options, property, minimum, maximum, fallback) {
    assert(typeof minimum === "number", "GetNumberOption");
    assert(typeof maximum === "number", "GetNumberOption");
    assert(fallback === undefined || (fallback >= minimum && fallback <= maximum), "GetNumberOption");

    // Step 1.
    var value = options[property];

    // Step 2.
    if (value !== undefined) {
        value = ToNumber(value);
        if (Number_isNaN(value) || value < minimum || value > maximum)
            ThrowRangeError(JSMSG_INVALID_DIGITS_VALUE, value);
        return std_Math_floor(value);
    }

    // Step 3.
    return fallback;
}


/**
 * Weak map used to track the initialize-as-Intl status (and, if an object has
 * been so initialized, the Intl-specific internal properties) of all objects.
 * Presence of an object as a key within this map indicates that the object has
 * its [[initializedIntlObject]] internal property set to true.  The associated
 * value is an object whose structure is documented in |initializeIntlObject|
 * below.
 *
 * Ideally we'd be using private symbols for internal properties, but
 * SpiderMonkey doesn't have those yet.
 */
var internalsMap = new WeakMap();


/**
 * Set the [[initializedIntlObject]] internal property of |obj| to true.
 */
function initializeIntlObject(obj) {
    assert(IsObject(obj), "Non-object passed to initializeIntlObject");

    // Intl-initialized objects are weird.  They have [[initializedIntlObject]]
    // set on them, but they don't *necessarily* have any other properties.

    var internals = std_Object_create(null);

    // The meaning of an internals object for an object |obj| is as follows.
    //
    // If the .type is "partial", |obj| has [[initializedIntlObject]] set but
    // nothing else.  No other property of |internals| can be used.  (This
    // occurs when InitializeCollator or similar marks an object as
    // [[initializedIntlObject]] but fails before marking it as the appropriate
    // more-specific type ["Collator", "DateTimeFormat", "NumberFormat"].)
    //
    // Otherwise, the .type indicates the type of Intl object that |obj| is:
    // "Collator", "DateTimeFormat", or "NumberFormat" (likely with more coming
    // in future Intl specs).  In these cases |obj| *conceptually* also has
    // [[initializedCollator]] or similar set, and all the other properties
    // implied by that.
    //
    // If |internals| doesn't have a "partial" .type, two additional properties
    // have meaning.  The .lazyData property stores information needed to
    // compute -- without observable side effects -- the actual internal Intl
    // properties of |obj|.  If it is non-null, then the actual internal
    // properties haven't been computed, and .lazyData must be processed by
    // |setInternalProperties| before internal Intl property values are
    // available.  If it is null, then the .internalProps property contains an
    // object whose properties are the internal Intl properties of |obj|.

    internals.type = "partial";
    internals.lazyData = null;
    internals.internalProps = null;

    callFunction(std_WeakMap_set, internalsMap, obj, internals);
    return internals;
}


/**
 * Mark |internals| as having the given type and lazy data.
 */
function setLazyData(internals, type, lazyData)
{
    assert(internals.type === "partial", "can't set lazy data for anything but a newborn");
    assert(type === "Collator" || type === "DateTimeFormat" ||
           type === "NumberFormat" || type === "PluralRules" ||
           type === "RelativeTimeFormat",
           "bad type");
    assert(IsObject(lazyData), "non-object lazy data");

    // Set in reverse order so that the .type change is a barrier.
    internals.lazyData = lazyData;
    internals.type = type;
}


/**
 * Set the internal properties object for an |internals| object previously
 * associated with lazy data.
 */
function setInternalProperties(internals, internalProps)
{
    assert(internals.type !== "partial", "newborn internals can't have computed internals");
    assert(IsObject(internals.lazyData), "lazy data must exist already");
    assert(IsObject(internalProps), "internalProps argument should be an object");

    // Set in reverse order so that the .lazyData nulling is a barrier.
    internals.internalProps = internalProps;
    internals.lazyData = null;
}


/**
 * Get the existing internal properties out of a non-newborn |internals|, or
 * null if none have been computed.
 */
function maybeInternalProperties(internals)
{
    assert(IsObject(internals), "non-object passed to maybeInternalProperties");
    assert(internals.type !== "partial", "maybeInternalProperties must only be used on completely-initialized internals objects");
    var lazyData = internals.lazyData;
    if (lazyData)
        return null;
    assert(IsObject(internals.internalProps), "missing lazy data and computed internals");
    return internals.internalProps;
}


/**
 * Return whether |obj| has an[[initializedIntlObject]] property set to true.
 */
function isInitializedIntlObject(obj) {
#ifdef DEBUG
    var internals = callFunction(std_WeakMap_get, internalsMap, obj);
    if (IsObject(internals)) {
        assert(callFunction(std_Object_hasOwnProperty, internals, "type"), "missing type");
        var type = internals.type;
        assert(type === "partial" || type === "Collator" ||
               type === "DateTimeFormat" || type === "NumberFormat" ||
               type === "PluralRules" || type === "RelativeTimeFormat",
               "unexpected type");
        assert(callFunction(std_Object_hasOwnProperty, internals, "lazyData"), "missing lazyData");
        assert(callFunction(std_Object_hasOwnProperty, internals, "internalProps"), "missing internalProps");
    } else {
        assert(internals === undefined, "bad mapping for |obj|");
    }
#endif
    return callFunction(std_WeakMap_has, internalsMap, obj);
}


/**
 * Check that |obj| meets the requirements for "this Collator object", "this
 * NumberFormat object", or "this DateTimeFormat object" as used in the method
 * with the given name.  Throw a TypeError if |obj| doesn't meet these
 * requirements.  But if it does, return |obj|'s internals object (*not* the
 * object holding its internal properties!), associated with it by
 * |internalsMap|, with structure specified above.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.
 * Spec: ECMAScript Internationalization API Specification, 11.3.
 * Spec: ECMAScript Internationalization API Specification, 12.3.
 */
function getIntlObjectInternals(obj, className, methodName) {
    assert(typeof className === "string", "bad className for getIntlObjectInternals");

    var internals = callFunction(std_WeakMap_get, internalsMap, obj);
    assert(internals === undefined || isInitializedIntlObject(obj), "bad mapping in internalsMap");

    if (internals === undefined || internals.type !== className)
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, className, methodName, className);

    return internals;
}


/**
 * Get the internal properties of known-Intl object |obj|.  For use only by
 * C++ code that knows what it's doing!
 */
function getInternals(obj)
{
    assert(isInitializedIntlObject(obj), "for use only on guaranteed Intl objects");

    var internals = callFunction(std_WeakMap_get, internalsMap, obj);

    assert(internals.type !== "partial", "must have been successfully initialized");
    var lazyData = internals.lazyData;
    if (!lazyData)
        return internals.internalProps;

    var internalProps;
    var type = internals.type;
    
    switch (type) {
      case "Collator":
        internalProps = resolveCollatorInternals(lazyData);
        break;
      case "DateTimeFormat":
        internalProps = resolveDateTimeFormatInternals(lazyData);
        break;
      case "PluralRules":
        internalProps = resolvePluralRulesInternals(lazyData);
        break;
      case "RelativeTimeFormat":
        internalProps = resolveRelativeTimeFormatInternals(lazyData);
        break;
      default: // type === "NumberFormat"
        internalProps = resolveNumberFormatInternals(lazyData);
        break;
    }
    setInternalProperties(internals, internalProps);
    return internalProps;
}

