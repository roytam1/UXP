/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_TEE_SURFACE_PRIVATE_H
#define CAIRO_TEE_SURFACE_PRIVATE_H

#include "cairoint.h"

cairo_private cairo_surface_t *
_cairo_tee_surface_find_match (void *abstract_surface,
			       const cairo_surface_backend_t *backend,
			       cairo_content_t content);

#endif /* CAIRO_TEE_SURFACE_PRIVATE_H */
