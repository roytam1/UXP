/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SURFACE_CLIPPER_PRIVATE_H
#define CAIRO_SURFACE_CLIPPER_PRIVATE_H

#include "cairo-types-private.h"
#include "cairo-clip-private.h"

CAIRO_BEGIN_DECLS

typedef struct _cairo_surface_clipper cairo_surface_clipper_t;

typedef cairo_status_t
(*cairo_surface_clipper_intersect_clip_path_func_t) (cairo_surface_clipper_t *,
						     cairo_path_fixed_t *,
						     cairo_fill_rule_t,
						     double,
						     cairo_antialias_t);
struct _cairo_surface_clipper {
    cairo_clip_t clip;
    cairo_bool_t is_clipped;
    cairo_surface_clipper_intersect_clip_path_func_t intersect_clip_path;
};

cairo_private cairo_status_t
_cairo_surface_clipper_set_clip (cairo_surface_clipper_t *clipper,
				 cairo_clip_t *clip);

cairo_private void
_cairo_surface_clipper_init (cairo_surface_clipper_t *clipper,
			     cairo_surface_clipper_intersect_clip_path_func_t intersect);

cairo_private void
_cairo_surface_clipper_reset (cairo_surface_clipper_t *clipper);

CAIRO_END_DECLS

#endif /* CAIRO_SURFACE_CLIPPER_PRIVATE_H */
