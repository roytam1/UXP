/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_QUARTZ_IMAGE_H
#define CAIRO_QUARTZ_IMAGE_H

#include "cairo.h"

#if CAIRO_HAS_QUARTZ_IMAGE_SURFACE
#include "TargetConditionals.h"

#if !TARGET_OS_IPHONE
#include <Carbon/Carbon.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#endif

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_quartz_image_surface_create (cairo_surface_t *image_surface);

cairo_public cairo_surface_t *
cairo_quartz_image_surface_get_image (cairo_surface_t *surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_QUARTZ_IMAGE_SURFACE */
# error Cairo was not compiled with support for the quartz-image backend
#endif /* CAIRO_HAS_QUARTZ_IMAGE_SURFACE */

#endif /* CAIRO_QUARTZ_IMAGE_H */
