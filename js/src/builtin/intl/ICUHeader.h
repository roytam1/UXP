/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_ICUHeader_h
#define builtin_intl_ICUHeader_h

#include "unicode/plurrule.h"
#include "unicode/ucal.h"
#include "unicode/ucol.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/udisplaycontext.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include "unicode/unum.h"
#include "unicode/unumsys.h"
#include "unicode/upluralrules.h"
#include "unicode/ureldatefmt.h"
#include "unicode/ustring.h"

/**
 * Cast char16_t* strings to UChar* strings used by ICU.
 */
inline const UChar*
Char16ToUChar(const char16_t* chars)
{
  return reinterpret_cast<const UChar*>(chars);
}

inline UChar*
Char16ToUChar(char16_t* chars)
{
  return reinterpret_cast<UChar*>(chars);
}

inline char16_t*
UCharToChar16(UChar* chars)
{
  return reinterpret_cast<char16_t*>(chars);
}

inline const char16_t*
UCharToChar16(const UChar* chars)
{
  return reinterpret_cast<const char16_t*>(chars);
}

#endif /* builtin_intl_ICUHeader_h */
