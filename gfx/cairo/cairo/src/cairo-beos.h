/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_BEOS_H
#define CAIRO_BEOS_H

#include "cairo.h"

#if CAIRO_HAS_BEOS_SURFACE

#include <View.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_beos_surface_create (BView* view);

cairo_public cairo_surface_t *
cairo_beos_surface_create_for_bitmap (BView*   view,
				      BBitmap* bmp);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_BEOS_SURFACE */
# error Cairo was not compiled with support for the beos backend
#endif /* CAIRO_HAS_BEOS_SURFACE */

#endif /* CAIRO_BEOS_H */
