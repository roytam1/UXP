/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Environment variables affecting the backend:
 *
 *  %CAIRO_DIRECTFB_NO_ACCEL (boolean)
 *      if found, disables acceleration at all
 *
 *  %CAIRO_DIRECTFB_ARGB_FONT (boolean)
 *      if found, enables using ARGB fonts instead of A8
 */

#ifndef CAIRO_DIRECTFB_H
#define CAIRO_DIRECTFB_H

#include "cairo.h"

#if  CAIRO_HAS_DIRECTFB_SURFACE

#include <directfb.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_directfb_surface_create (IDirectFB *dfb, IDirectFBSurface *surface);

CAIRO_END_DECLS

#else  /*CAIRO_HAS_DIRECTFB_SURFACE*/
# error Cairo was not compiled with support for the directfb backend
#endif /*CAIRO_HAS_DIRECTFB_SURFACE*/

#endif /*CAIRO_DIRECTFB_H*/
