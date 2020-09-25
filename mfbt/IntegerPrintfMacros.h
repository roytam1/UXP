/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implements the C99 <inttypes.h> interface. */

#ifndef mozilla_IntegerPrintfMacros_h_
#define mozilla_IntegerPrintfMacros_h_

/*
 * These macros should not be used with the NSPR printf-like functions or their
 * users, e.g. mozilla/Logging.h.  If you need to use NSPR's facilities, see the
 * comment on supported formats at the top of nsprpub/pr/include/prprf.h.
 */

/*
 * scanf is a footgun: if the input number exceeds the bounds of the target
 * type, behavior is undefined (in the compiler sense: that is, this code
 * could overwrite your hard drive with zeroes):
 *
 *   uint8_t u;
 *   sscanf("256", "%" SCNu8, &u); // BAD
 *
 * For this reason, *never* use the SCN* macros provided by this header!
 */

#include <inttypes.h>

#endif  /* mozilla_IntegerPrintfMacros_h_ */
