/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_DRM_H
#define CAIRO_DRM_H

#include "cairo.h"

#if CAIRO_HAS_DRM_SURFACE

CAIRO_BEGIN_DECLS

struct udev_device;

cairo_public cairo_device_t *
cairo_drm_device_get (struct udev_device *device);

cairo_public cairo_device_t *
cairo_drm_device_get_for_fd (int fd);

cairo_public cairo_device_t *
cairo_drm_device_default (void);

cairo_public int
cairo_drm_device_get_fd (cairo_device_t *device);

cairo_public void
cairo_drm_device_throttle (cairo_device_t *device);

cairo_public cairo_surface_t *
cairo_drm_surface_create (cairo_device_t *device,
			  cairo_format_t format,
			  int width, int height);

cairo_public cairo_surface_t *
cairo_drm_surface_create_for_name (cairo_device_t *device,
				   unsigned int name,
	                           cairo_format_t format,
				   int width, int height, int stride);

cairo_public cairo_surface_t *
cairo_drm_surface_create_from_cacheable_image (cairo_device_t *device,
	                                       cairo_surface_t *surface);

cairo_public cairo_status_t
cairo_drm_surface_enable_scan_out (cairo_surface_t *surface);

cairo_public unsigned int
cairo_drm_surface_get_handle (cairo_surface_t *surface);

cairo_public unsigned int
cairo_drm_surface_get_name (cairo_surface_t *surface);

cairo_public cairo_format_t
cairo_drm_surface_get_format (cairo_surface_t *surface);

cairo_public int
cairo_drm_surface_get_width (cairo_surface_t *surface);

cairo_public int
cairo_drm_surface_get_height (cairo_surface_t *surface);

cairo_public int
cairo_drm_surface_get_stride (cairo_surface_t *surface);

/* XXX map/unmap, general surface layer? */

/* Rough outline, culled from a conversation on IRC:
 *   map() returns an image-surface representation of the drm-surface,
 *   which you unmap() when you are finished, i.e. map() pulls the buffer back
 *   from the GPU, maps it into the CPU domain and gives you direct access to
 *   the pixels.  With the unmap(), the buffer is ready to be used again by the
 *   GPU and *until* the unmap(), all operations will be done in software.
 *
 *  (Technically calling cairo_surface_flush() on the underlying drm-surface
 *  will also disassociate the mapping.)
*/
cairo_public cairo_surface_t *
cairo_drm_surface_map_to_image (cairo_surface_t *surface);

cairo_public void
cairo_drm_surface_unmap (cairo_surface_t *drm_surface,
	                 cairo_surface_t *image_surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_DRM_SURFACE */
# error Cairo was not compiled with support for the DRM backend
#endif /* CAIRO_HAS_DRM_SURFACE */

#endif /* CAIRO_DRM_H */
