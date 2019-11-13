/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_FIXED_TYPE_PRIVATE_H
#define CAIRO_FIXED_TYPE_PRIVATE_H

#include "cairo-wideint-type-private.h"

/*
 * Fixed-point configuration
 */

typedef int32_t		cairo_fixed_16_16_t;
typedef cairo_int64_t	cairo_fixed_32_32_t;
typedef cairo_int64_t	cairo_fixed_48_16_t;
typedef cairo_int128_t	cairo_fixed_64_64_t;
typedef cairo_int128_t	cairo_fixed_96_32_t;

/* Eventually, we should allow changing this, but I think
 * there are some assumptions in the tesselator about the
 * size of a fixed type.  For now, it must be 32.
 */
#define CAIRO_FIXED_BITS	32

/* The number of fractional bits.  Changing this involves
 * making sure that you compute a double-to-fixed magic number.
 * (see below).
 */
#define CAIRO_FIXED_FRAC_BITS	8

/* A signed type %CAIRO_FIXED_BITS in size; the main fixed point type */
typedef int32_t cairo_fixed_t;

/* An unsigned type of the same size as #cairo_fixed_t */
typedef uint32_t cairo_fixed_unsigned_t;

typedef struct _cairo_point {
    cairo_fixed_t x;
    cairo_fixed_t y;
} cairo_point_t;

#endif /* CAIRO_FIXED_TYPE_PRIVATE_H */
