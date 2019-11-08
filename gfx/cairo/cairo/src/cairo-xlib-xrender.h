/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_XLIB_XRENDER_H
#define CAIRO_XLIB_XRENDER_H

#include "cairo.h"

#if CAIRO_HAS_XLIB_XRENDER_SURFACE

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display		 *dpy,
                                               Drawable		  drawable,
					       Screen		 *screen,
                                               XRenderPictFormat *format,
                                               int		  width,
                                               int		  height);

cairo_public XRenderPictFormat *
cairo_xlib_surface_get_xrender_format (cairo_surface_t *surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_XLIB_XRENDER_SURFACE */
# error Cairo was not compiled with support for the xlib XRender backend
#endif /* CAIRO_HAS_XLIB_XRENDER_SURFACE */

#endif /* CAIRO_XLIB_XRENDER_H */
