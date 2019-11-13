/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PATH_PRIVATE_H
#define CAIRO_PATH_PRIVATE_H

#include "cairoint.h"

cairo_private cairo_path_t *
_cairo_path_create (cairo_path_fixed_t *path,
		    cairo_gstate_t     *gstate);

cairo_private cairo_path_t *
_cairo_path_create_flat (cairo_path_fixed_t *path,
			 cairo_gstate_t     *gstate);

cairo_private cairo_path_t *
_cairo_path_create_in_error (cairo_status_t status);

cairo_private cairo_status_t
_cairo_path_append_to_context (const cairo_path_t	*path,
			       cairo_t			*cr);

#endif /* CAIRO_PATH_DATA_PRIVATE_H */
