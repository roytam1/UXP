/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SVG_H
#define CAIRO_SVG_H

#include "cairo.h"

#if CAIRO_HAS_SVG_SURFACE

CAIRO_BEGIN_DECLS

/**
 * cairo_svg_version_t:
 * @CAIRO_SVG_VERSION_1_1: The version 1.1 of the SVG specification.
 * @CAIRO_SVG_VERSION_1_2: The version 1.2 of the SVG specification.
 *
 * #cairo_svg_version_t is used to describe the version number of the SVG
 * specification that a generated SVG file will conform to.
 */
typedef enum _cairo_svg_version {
    CAIRO_SVG_VERSION_1_1,
    CAIRO_SVG_VERSION_1_2
} cairo_svg_version_t;

cairo_public cairo_surface_t *
cairo_svg_surface_create (const char   *filename,
			  double	width_in_points,
			  double	height_in_points);

cairo_public cairo_surface_t *
cairo_svg_surface_create_for_stream (cairo_write_func_t	write_func,
				     void	       *closure,
				     double		width_in_points,
				     double		height_in_points);

cairo_public void
cairo_svg_surface_restrict_to_version (cairo_surface_t 		*surface,
				       cairo_svg_version_t  	 version);

cairo_public void
cairo_svg_get_versions (cairo_svg_version_t const	**versions,
                        int                      	 *num_versions);

cairo_public const char *
cairo_svg_version_to_string (cairo_svg_version_t version);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_SVG_SURFACE */
# error Cairo was not compiled with support for the svg backend
#endif /* CAIRO_HAS_SVG_SURFACE */

#endif /* CAIRO_SVG_H */
