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

#ifndef V8_JSREGEXPCHARRANGES_H_
#define V8_JSREGEXPCHARRANGES_H_

#include "irregexp/RegExpCharacters.h"
#include "irregexp/InfallibleVector.h"

namespace js {

namespace irregexp {

// Characters parsed by RegExpParser can be either char16_t or kEndMarker.
typedef uint32_t widechar;

static const int kMaxOneByteCharCode = 0xff;
static const int kMaxUtf16CodeUnit = 0xffff;
static const size_t kEcma262UnCanonicalizeMaxWidth = 4;
static const char16_t kNoCharClass = 0;

static inline char16_t
MaximumCharacter(bool ascii)
{
    return ascii ? kMaxOneByteCharCode : kMaxUtf16CodeUnit;
}


// Returns the number of characters in the equivalence class, omitting those
// that cannot occur in the source string if it is a one byte string.
int
GetCaseIndependentLetters(char16_t character,
                          bool ascii_subject,
                          bool unicode,
                          const char16_t* choices,
                          size_t choices_length,
                          char16_t* letters);

int
GetCaseIndependentLetters(char16_t character,
                          bool ascii_subject,
                          bool unicode,
                          char16_t* letters);

class CharacterRange;
class WideCharRange;
typedef InfallibleVector<CharacterRange, 1> CharacterRangeVector;
typedef InfallibleVector<WideCharRange, 1> WideCharRangeVector;

// Represents code units in the range from from_ to to_, both ends are
// inclusive.
class CharacterRange
{
  public:
    // static methods for dealing with CharacterRangeVectors

    static void AddClass(const int* elmv, int elmc, CharacterRangeVector* ranges);
    static void AddClassNegated(const int* elmv, int elmc, CharacterRangeVector* ranges);
    static void AddClassEscape(LifoAlloc* alloc, char16_t type, CharacterRangeVector* ranges);
    static void AddClassEscapeUnicode(LifoAlloc* alloc, char16_t type,
                                      CharacterRangeVector* ranges, bool ignoreCase);

    // Adds a character or pre-defined character class to character ranges.
    // If char_class is not kNoCharClass, it's interpreted as a class
    // escape (i.e., 's' means whitespace, from '\s').
    static void AddCharOrEscape(LifoAlloc* alloc, CharacterRangeVector* ranges,
                                char16_t char_class, widechar c);
    static void AddCharOrEscapeUnicode(LifoAlloc* alloc,
                                       CharacterRangeVector* ranges,
                                       CharacterRangeVector* lead_ranges,
                                       CharacterRangeVector* trail_ranges,
                                       WideCharRangeVector* wide_ranges,
                                       char16_t char_class,
                                       widechar c,
                                       bool ignore_case);
    // Simplified version of AddUnicodeRange for single characters
    static void AddCharUnicode(LifoAlloc* alloc,
                               CharacterRangeVector* ranges,
                               CharacterRangeVector* lead_ranges,
                               CharacterRangeVector* trail_ranges,
                               WideCharRangeVector* wide_ranges,
                               widechar c);
    static void AddUnicodeRange(LifoAlloc* alloc,
                                CharacterRangeVector* ranges,
                                CharacterRangeVector* lead_ranges,
                                CharacterRangeVector* trail_ranges,
                                WideCharRangeVector* wide_ranges,
                                widechar first,
                                widechar next);

    static bool RangesContainLatin1Equivalents(const CharacterRangeVector& ranges, bool unicode);
    static bool CompareRanges(const CharacterRangeVector& ranges, const int* special_class, size_t length);
    static bool CompareInverseRanges(const CharacterRangeVector& ranges, const int* special_class, size_t length);

    // Negate a vector of ranges by subtracting its ranges from a range
    // encompassing the full range of possible values.
    template <typename RangeType>
    static void NegateUnicodeRanges(LifoAlloc* alloc, InfallibleVector<RangeType, 1>** ranges,
                                    RangeType full_range);

    // static methods for dealing with canonical CharacterRangeVectors

    // Whether a range list is in canonical form: Ranges ordered by from value,
    // and ranges non-overlapping and non-adjacent.
    static bool IsCanonical(const CharacterRangeVector& ranges);

    // Convert range list to canonical form. The characters covered by the ranges
    // will still be the same, but no character is in more than one range, and
    // adjacent ranges are merged. The resulting list may be shorter than the
    // original, but cannot be longer.
    static void Canonicalize(CharacterRangeVector& ranges);

    static int InsertRangeInCanonicalList(CharacterRangeVector& list, int count, CharacterRange insert);

    // Negate the contents of a character range in canonical form.
    static void Negate(const LifoAlloc* alloc,
                       CharacterRangeVector src,
                       CharacterRangeVector* dst);
  public:
    CharacterRange()
      : from_(0), to_(0)
    {}

    CharacterRange(char16_t from, char16_t to)
      : from_(from), to_(to)
    {}

    static inline CharacterRange Singleton(char16_t value) {
        return CharacterRange(value, value);
    }
    static inline CharacterRange Range(char16_t from, char16_t to) {
        MOZ_ASSERT(from <= to);
        return CharacterRange(from, to);
    }
    static inline CharacterRange Everything() {
        return CharacterRange(0, kMaxUtf16CodeUnit);
    }
    static inline CharacterRange LeadSurrogate() {
        return CharacterRange(unicode::LeadSurrogateMin, unicode::LeadSurrogateMax);
    }
    static inline CharacterRange TrailSurrogate() {
        return CharacterRange(unicode::TrailSurrogateMin, unicode::TrailSurrogateMax);
    }
    bool Contains(char16_t i) { return from_ <= i && i <= to_; }
    char16_t from() const { return from_; }
    void set_from(char16_t value) { from_ = value; }
    char16_t to() const { return to_; }
    void set_to(char16_t value) { to_ = value; }
    bool is_valid() { return from_ <= to_; }
    bool IsEverything(char16_t max) { return from_ == 0 && to_ >= max; }
    bool IsSingleton() { return (from_ == to_); }

    void AddCaseEquivalents(bool is_ascii, bool unicode, CharacterRangeVector* ranges);
  private:
    char16_t from_;
    char16_t to_;
};


class WideCharRange
{
  public:
    WideCharRange()
      : from_(0), to_(0)
    {}

    WideCharRange(widechar from, widechar to)
      : from_(from), to_(to)
    {}

    static inline WideCharRange Singleton(widechar value) {
        return WideCharRange(value, value);
    }
    static inline WideCharRange Range(widechar from, widechar to) {
        MOZ_ASSERT(from <= to);
        return WideCharRange(from, to);
    }
    static inline WideCharRange NonBMP() {
        return WideCharRange(unicode::NonBMPMin, unicode::NonBMPMax);
    }

    bool Contains(widechar i) const { return from_ <= i && i <= to_; }
    widechar from() const { return from_; }
    widechar to() const { return to_; }

  private:
    widechar from_;
    widechar to_;
};


} } // namespace js::irregexp

#endif // V8_JSREGEXPCHARRANGES_H_
