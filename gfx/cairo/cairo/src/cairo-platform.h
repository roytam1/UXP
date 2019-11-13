/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PLATFORM_H
#define CAIRO_PLATFORM_H

#include "prcpucfg.h"

/* we're replacing any definition from cairoint.h etc */
#undef cairo_public

#ifdef HAVE_VISIBILITY_HIDDEN_ATTRIBUTE
#define CVISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define CVISIBILITY_HIDDEN __hidden
#else
#define CVISIBILITY_HIDDEN
#endif

/* In libxul builds we don't ever want to export cairo symbols */
#define cairo_public extern CVISIBILITY_HIDDEN

#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#ifdef MOZILLA_VERSION
#include "cairo-rename.h"
#endif

#if defined(IS_BIG_ENDIAN)
#define WORDS_BIGENDIAN
#define FLOAT_WORDS_BIGENDIAN
#endif

#endif /* CAIRO_PLATFORM_H */
