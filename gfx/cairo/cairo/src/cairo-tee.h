/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_TEE_H
#define CAIRO_TEE_H

#include "cairo.h"

#if CAIRO_HAS_TEE_SURFACE

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_tee_surface_create (cairo_surface_t *master);

cairo_public void
cairo_tee_surface_add (cairo_surface_t *surface,
		       cairo_surface_t *target);

cairo_public void
cairo_tee_surface_remove (cairo_surface_t *surface,
			  cairo_surface_t *target);

cairo_public cairo_surface_t *
cairo_tee_surface_index (cairo_surface_t *surface,
			 int index);

CAIRO_END_DECLS

#else  /*CAIRO_HAS_TEE_SURFACE*/
# error Cairo was not compiled with support for the TEE backend
#endif /*CAIRO_HAS_TEE_SURFACE*/

#endif /*CAIRO_TEE_H*/
