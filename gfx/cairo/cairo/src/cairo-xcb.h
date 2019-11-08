/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_XCB_H
#define CAIRO_XCB_H

#include "cairo.h"

#if CAIRO_HAS_XCB_SURFACE

#include <xcb/xcb.h>
#include <xcb/render.h>

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_xcb_surface_create (xcb_connection_t	*connection,
			  xcb_drawable_t	 drawable,
			  xcb_visualtype_t	*visual,
			  int			 width,
			  int			 height);

cairo_public cairo_surface_t *
cairo_xcb_surface_create_for_bitmap (xcb_connection_t	*connection,
				     xcb_screen_t	*screen,
				     xcb_pixmap_t	 bitmap,
				     int		 width,
				     int		 height);

cairo_public cairo_surface_t *
cairo_xcb_surface_create_with_xrender_format (xcb_connection_t			*connection,
					      xcb_screen_t			*screen,
					      xcb_drawable_t			 drawable,
					      xcb_render_pictforminfo_t		*format,
					      int				 width,
					      int				 height);

cairo_public void
cairo_xcb_surface_set_size (cairo_surface_t *surface,
			    int		     width,
			    int		     height);

/* debug interface */

cairo_public void
cairo_xcb_device_debug_cap_xshm_version (cairo_device_t *device,
                                         int major_version,
                                         int minor_version);

cairo_public void
cairo_xcb_device_debug_cap_xrender_version (cairo_device_t *device,
                                            int major_version,
                                            int minor_version);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_XCB_SURFACE */
# error Cairo was not compiled with support for the xcb backend
#endif /* CAIRO_HAS_XCB_SURFACE */

#endif /* CAIRO_XCB_H */
