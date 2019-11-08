/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PS_H
#define CAIRO_PS_H

#include "cairo.h"

#if CAIRO_HAS_PS_SURFACE

#include <stdio.h>

CAIRO_BEGIN_DECLS

/* PS-surface functions */

/**
 * cairo_ps_level_t:
 * @CAIRO_PS_LEVEL_2: The language level 2 of the PostScript specification.
 * @CAIRO_PS_LEVEL_3: The language level 3 of the PostScript specification.
 *
 * #cairo_ps_level_t is used to describe the language level of the
 * PostScript Language Reference that a generated PostScript file will
 * conform to.
 */
typedef enum _cairo_ps_level {
    CAIRO_PS_LEVEL_2,
    CAIRO_PS_LEVEL_3
} cairo_ps_level_t;

cairo_public cairo_surface_t *
cairo_ps_surface_create (const char		*filename,
			 double			 width_in_points,
			 double			 height_in_points);

cairo_public cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t	write_func,
				    void	       *closure,
				    double		width_in_points,
				    double		height_in_points);

cairo_public void
cairo_ps_surface_restrict_to_level (cairo_surface_t    *surface,
                                    cairo_ps_level_t    level);

cairo_public void
cairo_ps_get_levels (cairo_ps_level_t const  **levels,
                     int                      *num_levels);

cairo_public const char *
cairo_ps_level_to_string (cairo_ps_level_t level);

cairo_public void
cairo_ps_surface_set_eps (cairo_surface_t	*surface,
			  cairo_bool_t           eps);

cairo_public cairo_bool_t
cairo_ps_surface_get_eps (cairo_surface_t	*surface);

cairo_public void
cairo_ps_surface_set_size (cairo_surface_t	*surface,
			   double		 width_in_points,
			   double		 height_in_points);

cairo_public void
cairo_ps_surface_dsc_comment (cairo_surface_t	*surface,
			      const char	*comment);

cairo_public void
cairo_ps_surface_dsc_begin_setup (cairo_surface_t *surface);

cairo_public void
cairo_ps_surface_dsc_begin_page_setup (cairo_surface_t *surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_PS_SURFACE */
# error Cairo was not compiled with support for the ps backend
#endif /* CAIRO_HAS_PS_SURFACE */

#endif /* CAIRO_PS_H */
