/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "irregexp/RegExpCharRanges.h"

// Generated table
#include "irregexp/RegExpCharacters-inl.h"

using namespace js::irregexp;

using mozilla::ArrayLength;

void
CharacterRange::AddCaseEquivalents(bool is_ascii, bool unicode, CharacterRangeVector* ranges)
{
    char16_t bottom = from();
    char16_t top = to();

    if (is_ascii && !RangeContainsLatin1Equivalents(*this, unicode)) {
        if (bottom > kMaxOneByteCharCode)
            return;
        if (top > kMaxOneByteCharCode)
            top = kMaxOneByteCharCode;
    }

    for (char16_t c = bottom;; c++) {
        char16_t chars[kEcma262UnCanonicalizeMaxWidth];
        size_t length = GetCaseIndependentLetters(c, is_ascii, unicode, chars);

        for (size_t i = 0; i < length; i++) {
            char16_t other = chars[i];
            if (other == c)
                continue;

            // Try to combine with an existing range.
            bool found = false;
            for (size_t i = 0; i < ranges->length(); i++) {
                CharacterRange& range = (*ranges)[i];
                if (range.Contains(other)) {
                    found = true;
                    break;
                } else if (other == range.from() - 1) {
                    range.set_from(other);
                    found = true;
                    break;
                } else if (other == range.to() + 1) {
                    range.set_to(other);
                    found = true;
                    break;
                }
            }

            if (!found)
                ranges->append(CharacterRange::Singleton(other));
        }

        if (c == top)
            break;
    }
}

/* static */
void
CharacterRange::AddClass(const int* elmv, int elmc, CharacterRangeVector* ranges)
{
    elmc--;
    MOZ_ASSERT(elmv[elmc] == 0x10000);
    for (int i = 0; i < elmc; i += 2) {
        MOZ_ASSERT(elmv[i] < elmv[i + 1]);
        ranges->append(CharacterRange(elmv[i], elmv[i + 1] - 1));
    }
}

/* static */ void
CharacterRange::AddClassNegated(const int* elmv, int elmc, CharacterRangeVector* ranges)
{
    elmc--;
    MOZ_ASSERT(elmv[elmc] == 0x10000);
    MOZ_ASSERT(elmv[0] != 0x0000);
    MOZ_ASSERT(elmv[elmc-1] != kMaxUtf16CodeUnit);
    char16_t last = 0x0000;
    for (int i = 0; i < elmc; i += 2) {
        MOZ_ASSERT(last <= elmv[i] - 1);
        MOZ_ASSERT(elmv[i] < elmv[i + 1]);
        ranges->append(CharacterRange(last, elmv[i] - 1));
        last = elmv[i + 1];
    }
    ranges->append(CharacterRange(last, kMaxUtf16CodeUnit));
}

/* static */ void
CharacterRange::AddClassEscape(LifoAlloc* alloc, char16_t type,
			       CharacterRangeVector* ranges)
{
    switch (type) {
      case 's':
        AddClass(kSpaceRanges, kSpaceRangeCount, ranges);
        break;
      case 'S':
        AddClassNegated(kSpaceRanges, kSpaceRangeCount, ranges);
        break;
      case 'w':
        AddClass(kWordRanges, kWordRangeCount, ranges);
        break;
      case 'W':
        AddClassNegated(kWordRanges, kWordRangeCount, ranges);
        break;
      case 'd':
        AddClass(kDigitRanges, kDigitRangeCount, ranges);
        break;
      case 'D':
        AddClassNegated(kDigitRanges, kDigitRangeCount, ranges);
        break;
      case '.':
        AddClassNegated(kLineTerminatorRanges, kLineTerminatorRangeCount, ranges);
        break;
        // This is not a character range as defined by the spec but a
        // convenient shorthand for a character class that matches any
        // character.
      case '*':
        ranges->append(CharacterRange::Everything());
        break;
        // This is the set of characters matched by the $ and ^ symbols
        // in multiline mode.
      case 'n':
        AddClass(kLineTerminatorRanges, kLineTerminatorRangeCount, ranges);
        break;
      default:
        MOZ_CRASH("Bad character class escape");
    }
}

// Add class escape, excluding surrogate pair range.
/* static */ void
CharacterRange::AddClassEscapeUnicode(LifoAlloc* alloc, char16_t type,
                                      CharacterRangeVector* ranges, bool ignore_case)
{
    switch (type) {
      case 's':
      case 'd':
        return AddClassEscape(alloc, type, ranges);
        break;
      case 'S':
        AddClassNegated(kSpaceAndSurrogateRanges, kSpaceAndSurrogateRangeCount, ranges);
        break;
      case 'w':
        if (ignore_case)
            AddClass(kIgnoreCaseWordRanges, kIgnoreCaseWordRangeCount, ranges);
        else
            AddClassEscape(alloc, type, ranges);
        break;
      case 'W':
        if (ignore_case) {
            AddClass(kNegatedIgnoreCaseWordAndSurrogateRanges,
                     kNegatedIgnoreCaseWordAndSurrogateRangeCount, ranges);
        } else {
            AddClassNegated(kWordAndSurrogateRanges, kWordAndSurrogateRangeCount, ranges);
        }
        break;
      case 'D':
        AddClassNegated(kDigitAndSurrogateRanges, kDigitAndSurrogateRangeCount, ranges);
        break;
      default:
        MOZ_CRASH("Bad type!");
    }
}

/* static */ void
CharacterRange::AddCharOrEscape(LifoAlloc* alloc, CharacterRangeVector* ranges,
                                char16_t char_class, widechar c)
{
    if (char_class != kNoCharClass)
        AddClassEscape(alloc, char_class, ranges);
    else
        ranges->append(CharacterRange::Singleton(c));
}

/* static */ void
CharacterRange::AddCharOrEscapeUnicode(LifoAlloc* alloc,
                                       CharacterRangeVector* ranges,
                                       CharacterRangeVector* lead_ranges,
                                       CharacterRangeVector* trail_ranges,
                                       WideCharRangeVector* wide_ranges,
                                       char16_t char_class,
                                       widechar c,
                                       bool ignore_case)
{
    if (char_class != kNoCharClass) {
        AddClassEscapeUnicode(alloc, char_class, ranges, ignore_case);
        switch (char_class) {
          case 'S':
          case 'W':
          case 'D':
            lead_ranges->append(CharacterRange::LeadSurrogate());
            trail_ranges->append(CharacterRange::TrailSurrogate());
            wide_ranges->append(WideCharRange::NonBMP());
            break;
          case '.':
            MOZ_CRASH("Bad char_class!");
        }
        return;
    }

    if (unicode::IsLeadSurrogate(c))
        lead_ranges->append(CharacterRange::Singleton(c));
    else if (unicode::IsTrailSurrogate(c))
        trail_ranges->append(CharacterRange::Singleton(c));
    else if (c >= unicode::NonBMPMin)
        wide_ranges->append(WideCharRange::Singleton(c));
    else
        ranges->append(CharacterRange::Singleton(c));
}

/* static */ void
CharacterRange::AddCharUnicode(LifoAlloc* alloc,
                               CharacterRangeVector* ranges,
                               CharacterRangeVector* lead_ranges,
                               CharacterRangeVector* trail_ranges,
                               WideCharRangeVector* wide_ranges,
                               widechar c)
{
    if (unicode::IsLeadSurrogate(c))
        lead_ranges->append(CharacterRange::Singleton(c));
    else if (unicode::IsTrailSurrogate(c))
        trail_ranges->append(CharacterRange::Singleton(c));
    else if (c >= unicode::NonBMPMin)
        wide_ranges->append(WideCharRange::Singleton(c));
    else
        ranges->append(CharacterRange::Singleton(c));
}

/* static */ void
CharacterRange::AddUnicodeRange(LifoAlloc* alloc,
                                CharacterRangeVector* ranges,
                                CharacterRangeVector* lead_ranges,
                                CharacterRangeVector* trail_ranges,
                                WideCharRangeVector* wide_ranges,
                                widechar first,
                                widechar next)
{
    MOZ_ASSERT(first <= next);
    if (first < unicode::LeadSurrogateMin) {
        if (next < unicode::LeadSurrogateMin) {
            ranges->append(CharacterRange::Range(first, next));
            return;
        }
        ranges->append(CharacterRange::Range(first, unicode::LeadSurrogateMin - 1));
        first = unicode::LeadSurrogateMin;
    }
    if (first <= unicode::LeadSurrogateMax) {
        if (next <= unicode::LeadSurrogateMax) {
            lead_ranges->append(CharacterRange::Range(first, next));
            return;
        }
        lead_ranges->append(CharacterRange::Range(first, unicode::LeadSurrogateMax));
        first = unicode::LeadSurrogateMax + 1;
    }
    MOZ_ASSERT(unicode::LeadSurrogateMax + 1 == unicode::TrailSurrogateMin);
    if (first <= unicode::TrailSurrogateMax) {
        if (next <= unicode::TrailSurrogateMax) {
            trail_ranges->append(CharacterRange::Range(first, next));
            return;
        }
        trail_ranges->append(CharacterRange::Range(first, unicode::TrailSurrogateMax));
        first = unicode::TrailSurrogateMax + 1;
    }
    if (first <= unicode::UTF16Max) {
        if (next <= unicode::UTF16Max) {
            ranges->append(CharacterRange::Range(first, next));
            return;
        }
        ranges->append(CharacterRange::Range(first, unicode::UTF16Max));
        first = unicode::NonBMPMin;
    }
    MOZ_ASSERT(unicode::UTF16Max + 1 == unicode::NonBMPMin);
    wide_ranges->append(WideCharRange::Range(first, next));
}

/* static */ bool
CharacterRange::RangesContainLatin1Equivalents(const CharacterRangeVector& ranges, bool unicode)
{
    for (size_t i = 0; i < ranges.length(); i++) {
        // TODO(dcarney): this could be a lot more efficient.
        if (RangeContainsLatin1Equivalents(ranges[i], unicode))
            return true;
    }
    return false;
}

/* static */ bool
CharacterRange::CompareRanges(const CharacterRangeVector& ranges, const int* special_class, size_t length)
{
    length--;  // Remove final 0x10000.
    MOZ_ASSERT(special_class[length] == 0x10000);
    if (ranges.length() * 2 != length)
        return false;
    for (size_t i = 0; i < length; i += 2) {
        CharacterRange range = ranges[i >> 1];
        if (range.from() != special_class[i] || range.to() != special_class[i + 1] - 1)
            return false;
    }
    return true;
}

/* static */ bool
CharacterRange::CompareInverseRanges(const CharacterRangeVector& ranges, const int* special_class, size_t length)
{
    length--;  // Remove final 0x10000.
    MOZ_ASSERT(special_class[length] == 0x10000);
    MOZ_ASSERT(ranges.length() != 0);
    MOZ_ASSERT(length != 0);
    MOZ_ASSERT(special_class[0] != 0);
    if (ranges.length() != (length >> 1) + 1)
        return false;
    CharacterRange range = ranges[0];
    if (range.from() != 0)
        return false;
    for (size_t i = 0; i < length; i += 2) {
        if (special_class[i] != (range.to() + 1))
            return false;
        range = ranges[(i >> 1) + 1];
        if (special_class[i+1] != range.from())
            return false;
    }
    if (range.to() != 0xffff)
        return false;
    return true;
}

template <typename RangeType>
/* static */ void
CharacterRange::NegateUnicodeRanges(LifoAlloc* alloc, InfallibleVector<RangeType, 1>** ranges,
                                    RangeType full_range)
{
    typedef InfallibleVector<RangeType, 1> RangeVector;
    RangeVector* tmp_ranges = alloc->newInfallible<RangeVector>(*alloc);
    tmp_ranges->append(full_range);
    RangeVector* result_ranges = alloc->newInfallible<RangeVector>(*alloc);

    // Perform the following calculation:
    //   result_ranges = tmp_ranges - ranges
    // with the following steps:
    //   result_ranges = tmp_ranges - ranges[0]
    //   SWAP(result_ranges, tmp_ranges)
    //   result_ranges = tmp_ranges - ranges[1]
    //   SWAP(result_ranges, tmp_ranges)
    //   ...
    //   result_ranges = tmp_ranges - ranges[N-1]
    //   SWAP(result_ranges, tmp_ranges)
    // The last SWAP is just for simplicity of the loop.
    for (size_t i = 0; i < (*ranges)->length(); i++) {
        result_ranges->clear();

        const RangeType& range = (**ranges)[i];
        for (size_t j = 0; j < tmp_ranges->length(); j++) {
            const RangeType& tmpRange = (*tmp_ranges)[j];
            auto from1 = tmpRange.from();
            auto to1 = tmpRange.to();
            auto from2 = range.from();
            auto to2 = range.to();

            if (from1 < from2) {
                if (to1 < from2) {
                    result_ranges->append(tmpRange);
                } else if (to1 <= to2) {
                    result_ranges->append(RangeType::Range(from1, from2 - 1));
                } else {
                    result_ranges->append(RangeType::Range(from1, from2 - 1));
                    result_ranges->append(RangeType::Range(to2 + 1, to1));
                }
            } else if (from1 <= to2) {
                if (to1 > to2)
                    result_ranges->append(RangeType::Range(to2 + 1, to1));
            } else {
                result_ranges->append(tmpRange);
            }
        }

        auto tmp = tmp_ranges;
        tmp_ranges = result_ranges;
        result_ranges = tmp;
    }

    // After the loop, result is pointed at by tmp_ranges, instead of
    // result_ranges.
    *ranges = tmp_ranges;
}

// Explicit specialization for NegateUnicodeRanges
template void CharacterRange::NegateUnicodeRanges<CharacterRange>(LifoAlloc* alloc, InfallibleVector<CharacterRange, 1>** ranges, CharacterRange full_range);
template void CharacterRange::NegateUnicodeRanges<WideCharRange>(LifoAlloc* alloc, InfallibleVector<WideCharRange, 1>** ranges, WideCharRange full_range);

/* static */ bool
CharacterRange::IsCanonical(const CharacterRangeVector& ranges)
{
    int n = ranges.length();
    if (n <= 1)
        return true;

    int max = ranges[0].to();
    for (int i = 1; i < n; i++) {
        CharacterRange next_range = ranges[i];
        if (next_range.from() <= max + 1)
            return false;
        max = next_range.to();
    }
    return true;
}

/* static */ void
CharacterRange::Canonicalize(CharacterRangeVector& character_ranges)
{
    if (character_ranges.length() <= 1) return;
    // Check whether ranges are already canonical (increasing, non-overlapping,
    // non-adjacent).
    int n = character_ranges.length();
    int max = character_ranges[0].to();
    int i = 1;
    while (i < n) {
        CharacterRange current = character_ranges[i];
        if (current.from() <= max + 1) {
            break;
        }
        max = current.to();
        i++;
    }
    // Canonical until the i'th range. If that's all of them, we are done.
    if (i == n) return;

    // The ranges at index i and forward are not canonicalized. Make them so by
    // doing the equivalent of insertion sort (inserting each into the previous
    // list, in order).
    // Notice that inserting a range can reduce the number of ranges in the
    // result due to combining of adjacent and overlapping ranges.
    int read = i;  // Range to insert.
    size_t num_canonical = i;  // Length of canonicalized part of list.
    do {
        num_canonical = InsertRangeInCanonicalList(character_ranges,
                                                   num_canonical,
                                                   character_ranges[read]);
        read++;
    } while (read < n);

    while (character_ranges.length() > num_canonical)
        character_ranges.popBack();

    MOZ_ASSERT(IsCanonical(character_ranges));
}

/* static */ int
CharacterRange::InsertRangeInCanonicalList(CharacterRangeVector& list,
                                           int count,
                                           CharacterRange insert)
{
    // Inserts a range into list[0..count[, which must be sorted
    // by from value and non-overlapping and non-adjacent, using at most
    // list[0..count] for the result. Returns the number of resulting
    // canonicalized ranges. Inserting a range may collapse existing ranges into
    // fewer ranges, so the return value can be anything in the range 1..count+1.
    char16_t from = insert.from();
    char16_t to = insert.to();
    int start_pos = 0;
    int end_pos = count;
    for (int i = count - 1; i >= 0; i--) {
        CharacterRange current = list[i];
        if (current.from() > to + 1) {
            end_pos = i;
        } else if (current.to() + 1 < from) {
            start_pos = i + 1;
            break;
        }
    }

    // Inserted range overlaps, or is adjacent to, ranges at positions
    // [start_pos..end_pos[. Ranges before start_pos or at or after end_pos are
    // not affected by the insertion.
    // If start_pos == end_pos, the range must be inserted before start_pos.
    // if start_pos < end_pos, the entire range from start_pos to end_pos
    // must be merged with the insert range.

    if (start_pos == end_pos) {
        // Insert between existing ranges at position start_pos.
        if (start_pos < count) {
            list.moveReplace(start_pos, start_pos + 1, count - start_pos);
        }
        list[start_pos] = insert;
        return count + 1;
    }
    if (start_pos + 1 == end_pos) {
        // Replace single existing range at position start_pos.
        CharacterRange to_replace = list[start_pos];
        int new_from = Min(to_replace.from(), from);
        int new_to = Max(to_replace.to(), to);
        list[start_pos] = CharacterRange(new_from, new_to);
        return count;
    }
    // Replace a number of existing ranges from start_pos to end_pos - 1.
    // Move the remaining ranges down.

    int new_from = Min(list[start_pos].from(), from);
    int new_to = Max(list[end_pos - 1].to(), to);
    if (end_pos < count) {
        list.moveReplace(end_pos, start_pos + 1, count - end_pos);
    }
    list[start_pos] = CharacterRange(new_from, new_to);
    return count - (end_pos - start_pos) + 1;
}

int
irregexp::GetCaseIndependentLetters(char16_t character,
                                    bool ascii_subject,
                                    bool unicode,
                                    const char16_t* choices,
                                    size_t choices_length,
                                    char16_t* letters)
{
    size_t count = 0;
    for (size_t i = 0; i < choices_length; i++) {
        char16_t c = choices[i];

        // Skip characters that can't appear in one byte strings.
        if (!unicode && ascii_subject && c > kMaxOneByteCharCode)
            continue;

        // Watch for duplicates.
        bool found = false;
        for (size_t j = 0; j < count; j++) {
            if (letters[j] == c) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        letters[count++] = c;
    }

    return count;
}

int
irregexp::GetCaseIndependentLetters(char16_t character,
                                    bool ascii_subject,
                                    bool unicode,
                                    char16_t* letters)
{
    if (unicode) {
        const char16_t choices[] = {
            character,
            unicode::FoldCase(character),
            unicode::ReverseFoldCase1(character),
            unicode::ReverseFoldCase2(character),
            unicode::ReverseFoldCase3(character),
        };
        return GetCaseIndependentLetters(character, ascii_subject, unicode,
                                         choices, ArrayLength(choices), letters);
    }

    char16_t upper = unicode::ToUpperCase(character);
    unicode::CodepointsWithSameUpperCase others(character);
    char16_t other1 = others.other1();
    char16_t other2 = others.other2();
    char16_t other3 = others.other3();

    // ES 2017 draft 996af87b7072b3c3dd2b1def856c66f456102215 21.2.4.2
    // step 3.g.
    // The standard requires that non-ASCII characters cannot have ASCII
    // character codes in their equivalence class, even though this
    // situation occurs multiple times in the Unicode tables.
    static const unsigned kMaxAsciiCharCode = 127;
    if (upper <= kMaxAsciiCharCode) {
        if (character > kMaxAsciiCharCode) {
            // If Canonicalize(character) == character, all other characters
            // should be ignored.
            return GetCaseIndependentLetters(character, ascii_subject, unicode,
                                             &character, 1, letters);
        }

        if (other1 > kMaxAsciiCharCode)
            other1 = character;
        if (other2 > kMaxAsciiCharCode)
            other2 = character;
        if (other3 > kMaxAsciiCharCode)
            other3 = character;
    }

    const char16_t choices[] = {
        character,
        upper,
        other1,
        other2,
        other3
    };
    return GetCaseIndependentLetters(character, ascii_subject, unicode,
                                     choices, ArrayLength(choices), letters);
}
