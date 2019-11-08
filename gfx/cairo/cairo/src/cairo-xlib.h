/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_XLIB_H
#define CAIRO_XLIB_H

#include "cairo.h"

#if CAIRO_HAS_XLIB_SURFACE

#include <X11/Xlib.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_xlib_surface_create (Display     *dpy,
			   Drawable	drawable,
			   Visual      *visual,
			   int		width,
			   int		height);

cairo_public cairo_surface_t *
cairo_xlib_surface_create_for_bitmap (Display  *dpy,
				      Pixmap	bitmap,
				      Screen	*screen,
				      int	width,
				      int	height);

cairo_public void
cairo_xlib_surface_set_size (cairo_surface_t *surface,
			     int              width,
			     int              height);

cairo_public void
cairo_xlib_surface_set_drawable (cairo_surface_t *surface,
				 Drawable	  drawable,
				 int              width,
				 int              height);

cairo_public Display *
cairo_xlib_surface_get_display (cairo_surface_t *surface);

cairo_public Drawable
cairo_xlib_surface_get_drawable (cairo_surface_t *surface);

cairo_public Screen *
cairo_xlib_surface_get_screen (cairo_surface_t *surface);

cairo_public Visual *
cairo_xlib_surface_get_visual (cairo_surface_t *surface);

cairo_public int
cairo_xlib_surface_get_depth (cairo_surface_t *surface);

cairo_public int
cairo_xlib_surface_get_width (cairo_surface_t *surface);

cairo_public int
cairo_xlib_surface_get_height (cairo_surface_t *surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_XLIB_SURFACE */
# error Cairo was not compiled with support for the xlib backend
#endif /* CAIRO_HAS_XLIB_SURFACE */

#endif /* CAIRO_XLIB_H */
