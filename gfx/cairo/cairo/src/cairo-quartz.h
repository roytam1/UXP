/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_QUARTZ_H
#define CAIRO_QUARTZ_H

#include "cairo.h"

#if CAIRO_HAS_QUARTZ_SURFACE
#include "TargetConditionals.h"

#if !TARGET_OS_IPHONE
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_quartz_surface_create (cairo_format_t format,
                             unsigned int width,
                             unsigned int height);

cairo_public cairo_surface_t *
cairo_quartz_surface_create_for_data (unsigned char *data,
				      cairo_format_t format,
				      unsigned int width,
				      unsigned int height,
				      unsigned int stride);

cairo_public cairo_surface_t *
cairo_quartz_surface_create_cg_layer (cairo_surface_t *surface,
                                      cairo_content_t content,
                                      unsigned int width,
                                      unsigned int height);

cairo_public cairo_surface_t *
cairo_quartz_surface_create_for_cg_context (CGContextRef cgContext,
                                            unsigned int width,
                                            unsigned int height);

cairo_public CGContextRef
cairo_quartz_surface_get_cg_context (cairo_surface_t *surface);

cairo_public CGContextRef
cairo_quartz_get_cg_context_with_clip (cairo_t *cr);

cairo_public void
cairo_quartz_finish_cg_context_with_clip (cairo_t *cr);

cairo_public cairo_surface_t *
cairo_quartz_surface_get_image (cairo_surface_t *surface);

#if CAIRO_HAS_QUARTZ_FONT

/*
 * Quartz font support
 */

cairo_public cairo_font_face_t *
cairo_quartz_font_face_create_for_cgfont (CGFontRef font);

#if !defined(__LP64__) && !TARGET_OS_IPHONE
cairo_public cairo_font_face_t *
cairo_quartz_font_face_create_for_atsu_font_id (ATSUFontID font_id);
#endif

#endif /* CAIRO_HAS_QUARTZ_FONT */

CAIRO_END_DECLS

#else

# error Cairo was not compiled with support for the quartz backend

#endif /* CAIRO_HAS_QUARTZ_SURFACE */

#endif /* CAIRO_QUARTZ_H */
