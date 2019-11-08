/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SURFACE_OFFSET_PRIVATE_H
#define CAIRO_SURFACE_OFFSET_PRIVATE_H

#include "cairo-types-private.h"

CAIRO_BEGIN_DECLS

cairo_private cairo_status_t
_cairo_surface_offset_paint (cairo_surface_t *target,
			     int x, int y,
			     cairo_operator_t	 op,
			     const cairo_pattern_t *source,
			     cairo_clip_t	    *clip);

cairo_private cairo_status_t
_cairo_surface_offset_mask (cairo_surface_t *target,
			    int x, int y,
			    cairo_operator_t	 op,
			    const cairo_pattern_t *source,
			    const cairo_pattern_t *mask,
			    cairo_clip_t	    *clip);

cairo_private cairo_status_t
_cairo_surface_offset_stroke (cairo_surface_t *surface,
			      int x, int y,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_path_fixed_t	*path,
			      const cairo_stroke_style_t	*stroke_style,
			      const cairo_matrix_t		*ctm,
			      const cairo_matrix_t		*ctm_inverse,
			      double			 tolerance,
			      cairo_antialias_t	 antialias,
			      cairo_clip_t		*clip);

cairo_private cairo_status_t
_cairo_surface_offset_fill (cairo_surface_t	*surface,
			    int x, int y,
			    cairo_operator_t	 op,
			    const cairo_pattern_t*source,
			    cairo_path_fixed_t	*path,
			    cairo_fill_rule_t	 fill_rule,
			    double		 tolerance,
			    cairo_antialias_t	 antialias,
			    cairo_clip_t		*clip);

cairo_private cairo_status_t
_cairo_surface_offset_glyphs (cairo_surface_t		*surface,
			      int x, int y,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_scaled_font_t	*scaled_font,
			      cairo_glyph_t		*glyphs,
			      int			 num_glyphs,
			      cairo_clip_t		*clip);

#endif /* CAIRO_SURFACE_OFFSET_PRIVATE_H */
