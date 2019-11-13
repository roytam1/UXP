/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_FT_H
#define CAIRO_FT_H

#include "cairo.h"

#if CAIRO_HAS_FT_FONT

/* Fontconfig/Freetype platform-specific font interface */

#include <ft2build.h>
#include FT_FREETYPE_H

#if CAIRO_HAS_FC_FONT
#include <fontconfig/fontconfig.h>
#endif

CAIRO_BEGIN_DECLS

cairo_public cairo_font_face_t *
cairo_ft_font_face_create_for_ft_face (FT_Face         face,
				       int             load_flags);

cairo_public FT_Face
cairo_ft_scaled_font_lock_face (cairo_scaled_font_t *scaled_font);

cairo_public void
cairo_ft_scaled_font_unlock_face (cairo_scaled_font_t *scaled_font);

#if CAIRO_HAS_FC_FONT

cairo_public cairo_font_face_t *
cairo_ft_font_face_create_for_pattern (FcPattern *pattern);

cairo_public void
cairo_ft_font_options_substitute (const cairo_font_options_t *options,
				  FcPattern                  *pattern);

#endif

CAIRO_END_DECLS

#else  /* CAIRO_HAS_FT_FONT */
# error Cairo was not compiled with support for the freetype font backend
#endif /* CAIRO_HAS_FT_FONT */

#endif /* CAIRO_FT_H */
