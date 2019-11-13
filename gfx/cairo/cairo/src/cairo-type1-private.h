/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_TYPE1_PRIVATE_H
#define CAIRO_TYPE1_PRIVATE_H

#include "cairoint.h"

#if CAIRO_HAS_FONT_SUBSET

/* Magic constants for the type1 eexec encryption */
#define CAIRO_TYPE1_ENCRYPT_C1		((unsigned short) 52845)
#define CAIRO_TYPE1_ENCRYPT_C2		((unsigned short) 22719)
#define CAIRO_TYPE1_PRIVATE_DICT_KEY	((unsigned short) 55665)
#define CAIRO_TYPE1_CHARSTRING_KEY	((unsigned short) 4330)

#endif /* CAIRO_HAS_FONT_SUBSET */

#endif /* CAIRO_TYPE1_PRIVATE_H */
