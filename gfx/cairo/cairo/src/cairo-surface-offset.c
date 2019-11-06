/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-surface-offset-private.h"

/* A collection of routines to facilitate drawing to an alternate surface. */

static void
_copy_transformed_pattern (cairo_pattern_t *pattern,
			   const cairo_pattern_t *original,
			   const cairo_matrix_t  *ctm_inverse)
{
    _cairo_pattern_init_static_copy (pattern, original);

    if (! _cairo_matrix_is_identity (ctm_inverse))
	_cairo_pattern_transform (pattern, ctm_inverse);
}

cairo_status_t
_cairo_surface_offset_paint (cairo_surface_t		*target,
			     int x, int y,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_copy;

    if (unlikely (target->status))
	return target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (x | y) {
	cairo_matrix_t m;

	if (clip != NULL) {
	    cairo_matrix_init_translate (&m, -x, -y);
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip, &m);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_init_translate (&m, x, y);
	_copy_transformed_pattern (&source_copy.base, source, &m);
	source = &source_copy.base;
    }

    status = _cairo_surface_paint (target, op, source, dev_clip);

  FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);

    return status;
}

cairo_status_t
_cairo_surface_offset_mask (cairo_surface_t		*target,
			    int x, int y,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    const cairo_pattern_t	*mask,
			    cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_copy;
    cairo_pattern_union_t mask_copy;

    if (unlikely (target->status))
	return target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (x | y) {
	cairo_matrix_t m;

	if (clip != NULL) {
	    cairo_matrix_init_translate (&m, -x, -y);
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip, &m);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_init_translate (&m, x, y);
	_copy_transformed_pattern (&source_copy.base, source, &m);
	_copy_transformed_pattern (&mask_copy.base, mask, &m);
	source = &source_copy.base;
	mask = &mask_copy.base;
    }

    status = _cairo_surface_mask (target, op,
				  source, mask,
				  dev_clip);

  FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);

    return status;
}

cairo_status_t
_cairo_surface_offset_stroke (cairo_surface_t		*surface,
			      int x, int y,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_path_fixed_t	*path,
			      const cairo_stroke_style_t*stroke_style,
			      const cairo_matrix_t	*ctm,
			      const cairo_matrix_t	*ctm_inverse,
			      double			 tolerance,
			      cairo_antialias_t		 antialias,
			      cairo_clip_t		*clip)
{
    cairo_path_fixed_t path_copy, *dev_path = path;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_matrix_t dev_ctm = *ctm;
    cairo_matrix_t dev_ctm_inverse = *ctm_inverse;
    cairo_pattern_union_t source_copy;
    cairo_status_t status;

    if (unlikely (surface->status))
	return surface->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (x | y) {
	cairo_matrix_t m;

	status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	if (unlikely (status))
	    goto FINISH;

	_cairo_path_fixed_translate (&path_copy,
				     _cairo_fixed_from_int (-x),
				     _cairo_fixed_from_int (-y));
	dev_path = &path_copy;

	cairo_matrix_init_translate (&m, -x, -y);
	cairo_matrix_multiply (&dev_ctm, &dev_ctm, &m);
	if (clip != NULL) {
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip, &m);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_init_translate (&m, x, y);
	_copy_transformed_pattern (&source_copy.base, source, &m);
	source = &source_copy.base;
	cairo_matrix_multiply (&dev_ctm_inverse, &m, &dev_ctm_inverse);
    }

    status = _cairo_surface_stroke (surface, op, source,
				    dev_path, stroke_style,
				    &dev_ctm, &dev_ctm_inverse,
				    tolerance, antialias,
				    dev_clip);

 FINISH:
    if (dev_path != path)
	_cairo_path_fixed_fini (dev_path);
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);

    return status;
}

cairo_status_t
_cairo_surface_offset_fill (cairo_surface_t	*surface,
			    int x, int y,
			    cairo_operator_t	 op,
			    const cairo_pattern_t*source,
			    cairo_path_fixed_t	*path,
			    cairo_fill_rule_t	 fill_rule,
			    double		 tolerance,
			    cairo_antialias_t	 antialias,
			    cairo_clip_t	*clip)
{
    cairo_status_t status;
    cairo_path_fixed_t path_copy, *dev_path = path;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_copy;

    if (unlikely (surface->status))
	return surface->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (x | y) {
	cairo_matrix_t m;

	status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	if (unlikely (status))
	    goto FINISH;

	_cairo_path_fixed_translate (&path_copy,
				     _cairo_fixed_from_int (-x),
				     _cairo_fixed_from_int (-y));
	dev_path = &path_copy;

	if (clip != NULL) {
	    cairo_matrix_init_translate (&m, -x, -y);
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip, &m);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_init_translate (&m, x, y);
	_copy_transformed_pattern (&source_copy.base, source, &m);
	source = &source_copy.base;
    }

    status = _cairo_surface_fill (surface, op, source,
				  dev_path, fill_rule,
				  tolerance, antialias,
				  dev_clip);

 FINISH:
    if (dev_path != path)
	_cairo_path_fixed_fini (dev_path);
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);

    return status;
}

cairo_status_t
_cairo_surface_offset_glyphs (cairo_surface_t		*surface,
			      int x, int y,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_scaled_font_t	*scaled_font,
			      cairo_glyph_t		*glyphs,
			      int			 num_glyphs,
			      cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_copy;
    cairo_glyph_t *dev_glyphs;
    int i;

    if (unlikely (surface->status))
	return surface->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    dev_glyphs = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
    if (dev_glyphs == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    memcpy (dev_glyphs, glyphs, sizeof (cairo_glyph_t) * num_glyphs);

    if (x | y) {
	cairo_matrix_t m;

	if (clip != NULL) {
	    cairo_matrix_init_translate (&m, -x, -y);
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip, &m);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_init_translate (&m, x, y);
	_copy_transformed_pattern (&source_copy.base, source, &m);
	source = &source_copy.base;

	for (i = 0; i < num_glyphs; i++) {
	    dev_glyphs[i].x -= x;
	    dev_glyphs[i].y -= y;
	}
    }

    status = _cairo_surface_show_text_glyphs (surface, op, source,
					      NULL, 0,
					      dev_glyphs, num_glyphs,
					      NULL, 0, 0,
					      scaled_font,
					      dev_clip);

 FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    free (dev_glyphs);

    return status;
}
