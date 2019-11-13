/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_FEATURES_H
/* This block is to just make this header file standalone */
#define CAIRO_MUTEX_DECLARE(mutex)
#endif

CAIRO_MUTEX_DECLARE (_cairo_pattern_solid_surface_cache_lock)

CAIRO_MUTEX_DECLARE (_cairo_image_solid_cache_mutex)

CAIRO_MUTEX_DECLARE (_cairo_error_mutex)
CAIRO_MUTEX_DECLARE (_cairo_toy_font_face_mutex)
CAIRO_MUTEX_DECLARE (_cairo_intern_string_mutex)
CAIRO_MUTEX_DECLARE (_cairo_scaled_font_map_mutex)
CAIRO_MUTEX_DECLARE (_cairo_scaled_glyph_page_cache_mutex)
CAIRO_MUTEX_DECLARE (_cairo_scaled_font_error_mutex)

#if CAIRO_HAS_FT_FONT
CAIRO_MUTEX_DECLARE (_cairo_ft_unscaled_font_map_mutex)
#endif

#if CAIRO_HAS_WIN32_FONT
CAIRO_MUTEX_DECLARE (_cairo_win32_font_face_mutex)
#endif

#if CAIRO_HAS_XLIB_SURFACE
CAIRO_MUTEX_DECLARE (_cairo_xlib_display_mutex)
#endif

#if CAIRO_HAS_XCB_SURFACE
CAIRO_MUTEX_DECLARE (_cairo_xcb_connections_mutex)
#endif

#if CAIRO_HAS_GL_SURFACE
CAIRO_MUTEX_DECLARE (_cairo_gl_context_mutex)
#endif

#if !defined (HAS_ATOMIC_OPS) || defined (ATOMIC_OP_NEEDS_MEMORY_BARRIER)
CAIRO_MUTEX_DECLARE (_cairo_atomic_mutex)
#endif

#if CAIRO_HAS_DRM_SURFACE
CAIRO_MUTEX_DECLARE (_cairo_drm_device_mutex)
#endif
/* Undefine, to err on unintended inclusion */
#undef   CAIRO_MUTEX_DECLARE
