/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_GL_H
#define CAIRO_GL_H

#include "cairo.h"

#if CAIRO_HAS_GL_SURFACE

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_gl_surface_create (cairo_device_t *device,
			 cairo_content_t content,
			 int width, int height);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_texture (cairo_device_t *abstract_device,
				     cairo_content_t content,
				     unsigned int tex,
                                     int width, int height);
cairo_public void
cairo_gl_surface_set_size (cairo_surface_t *surface, int width, int height);

cairo_public int
cairo_gl_surface_get_width (cairo_surface_t *abstract_surface);

cairo_public int
cairo_gl_surface_get_height (cairo_surface_t *abstract_surface);

cairo_public void
cairo_gl_surface_swapbuffers (cairo_surface_t *surface);

#if CAIRO_HAS_GLX_FUNCTIONS
#include <GL/glx.h>

cairo_public cairo_device_t *
cairo_glx_device_create (Display *dpy, GLXContext gl_ctx);

cairo_public Display *
cairo_glx_device_get_display (cairo_device_t *device);

cairo_public GLXContext
cairo_glx_device_get_context (cairo_device_t *device);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_window (cairo_device_t *device,
				    Window win,
				    int width, int height);
#endif

#if CAIRO_HAS_WGL_FUNCTIONS
#include <windows.h>

cairo_public cairo_device_t *
cairo_wgl_device_create (HGLRC rc);

cairo_public HGLRC
cairo_wgl_device_get_context (cairo_device_t *device);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_dc (cairo_device_t		*device,
				HDC			 dc,
				int			 width,
				int			 height);
#endif

#if CAIRO_HAS_EGL_FUNCTIONS
#include <EGL/egl.h>

cairo_public cairo_device_t *
cairo_egl_device_create (EGLDisplay dpy, EGLContext egl);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_egl (cairo_device_t	*device,
				 EGLSurface	 egl,
				 int		 width,
				 int		 height);

#endif

CAIRO_END_DECLS

#else  /* CAIRO_HAS_GL_SURFACE */
# error Cairo was not compiled with support for the GL backend
#endif /* CAIRO_HAS_GL_SURFACE */

#endif /* CAIRO_GL_H */
