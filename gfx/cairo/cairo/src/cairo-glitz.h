/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_GLITZ_H
#define CAIRO_GLITZ_H

#include "cairo.h"

#if CAIRO_HAS_GLITZ_SURFACE

#include <glitz.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_glitz_surface_create (glitz_surface_t *surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_GLITZ_SURFACE */
# error Cairo was not compiled with support for the glitz backend
#endif /* CAIRO_HAS_GLITZ_SURFACE */

#endif /* CAIRO_GLITZ_H */
