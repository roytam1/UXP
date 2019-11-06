/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_QUARTZ_PRIVATE_H
#define CAIRO_QUARTZ_PRIVATE_H

#include "cairoint.h"

#if CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-quartz.h"
#include "cairo-surface-clipper-private.h"

#ifdef CGFLOAT_DEFINED
typedef CGFloat cairo_quartz_float_t;
#else
typedef float cairo_quartz_float_t;
#endif

/* define CTFontRef for pre-10.5 SDKs */
typedef const struct __CTFont *CTFontRef;

typedef struct cairo_quartz_surface {
    cairo_surface_t base;

    CGContextRef cgContext;
    CGAffineTransform cgContextBaseCTM;

    void *imageData;
    cairo_surface_t *imageSurfaceEquiv;

    cairo_surface_clipper_t clipper;

    /**
     * If non-null, this is a CGImage representing the contents of the surface.
     * We clear this out before any painting into the surface, so that we
     * don't force a copy to be created.
     */
    CGImageRef bitmapContextImage;

    /**
     * If non-null, this is the CGLayer for the surface.
     */
    CGLayerRef cgLayer;

    cairo_rectangle_int_t extents;

    cairo_bool_t ownsData;
} cairo_quartz_surface_t;

typedef struct cairo_quartz_image_surface {
    cairo_surface_t base;

    cairo_rectangle_int_t extents;

    CGImageRef image;
    cairo_image_surface_t *imageSurface;
} cairo_quartz_image_surface_t;

cairo_bool_t
_cairo_quartz_verify_surface_size(int width, int height);

CGImageRef
_cairo_quartz_create_cgimage (cairo_format_t format,
			      unsigned int width,
			      unsigned int height,
			      unsigned int stride,
			      void *data,
			      cairo_bool_t interpolate,
			      CGColorSpaceRef colorSpaceOverride,
			      CGDataProviderReleaseDataCallback releaseCallback,
			      void *releaseInfo);

CGFontRef
_cairo_quartz_scaled_font_get_cg_font_ref (cairo_scaled_font_t *sfont);

CTFontRef
_cairo_quartz_scaled_font_get_ct_font_ref (cairo_scaled_font_t *sfont);

#else

# error Cairo was not compiled with support for the quartz backend

#endif /* CAIRO_HAS_QUARTZ_SURFACE */

#endif /* CAIRO_QUARTZ_PRIVATE_H */
