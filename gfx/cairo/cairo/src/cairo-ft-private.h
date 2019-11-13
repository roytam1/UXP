/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_FT_PRIVATE_H
#define CAIRO_FT_PRIVATE_H

#include "cairo-ft.h"
#include "cairoint.h"

#if CAIRO_HAS_FT_FONT

CAIRO_BEGIN_DECLS

typedef struct _cairo_ft_unscaled_font cairo_ft_unscaled_font_t;

cairo_private cairo_bool_t
_cairo_scaled_font_is_ft (cairo_scaled_font_t *scaled_font);

/* These functions are needed by the PDF backend, which needs to keep track of the
 * the different fonts-on-disk used by a document, so it can embed them
 */
cairo_private cairo_unscaled_font_t *
_cairo_ft_scaled_font_get_unscaled_font (cairo_scaled_font_t *scaled_font);

cairo_private FT_Face
_cairo_ft_unscaled_font_lock_face (cairo_ft_unscaled_font_t *unscaled);

cairo_private void
_cairo_ft_unscaled_font_unlock_face (cairo_ft_unscaled_font_t *unscaled);

cairo_private cairo_bool_t
_cairo_ft_scaled_font_is_vertical (cairo_scaled_font_t *scaled_font);

cairo_private unsigned int
_cairo_ft_scaled_font_get_load_flags (cairo_scaled_font_t *scaled_font);

CAIRO_END_DECLS

#endif /* CAIRO_HAS_FT_FONT */
#endif /* CAIRO_FT_PRIVATE_H */
