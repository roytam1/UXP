/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_VG_H
#define CAIRO_VG_H

#include "cairo.h"

#if CAIRO_HAS_VG_SURFACE

#include <VG/openvg.h>

CAIRO_BEGIN_DECLS

typedef struct _cairo_vg_context cairo_vg_context_t;

#if CAIRO_HAS_GLX_FUNCTIONS
typedef struct __GLXcontextRec *GLXContext;
typedef struct _XDisplay Display;

cairo_public cairo_vg_context_t *
cairo_vg_context_create_for_glx (Display *dpy,
				 GLXContext ctx);
#endif

#if CAIRO_HAS_EGL_FUNCTIONS
#include <EGL/egl.h>

cairo_public cairo_vg_context_t *
cairo_vg_context_create_for_egl (EGLDisplay egl_display,
				 EGLContext egl_context);
#endif

cairo_public cairo_status_t
cairo_vg_context_status (cairo_vg_context_t *context);

cairo_public void
cairo_vg_context_destroy (cairo_vg_context_t *context);

cairo_public cairo_surface_t *
cairo_vg_surface_create (cairo_vg_context_t *context,
			 cairo_content_t content, int width, int height);

cairo_public cairo_surface_t *
cairo_vg_surface_create_for_image (cairo_vg_context_t *context,
				   VGImage image,
				   VGImageFormat format,
				   int width, int height);

cairo_public VGImage
cairo_vg_surface_get_image (cairo_surface_t *abstract_surface);

cairo_public VGImageFormat
cairo_vg_surface_get_format (cairo_surface_t *abstract_surface);

cairo_public int
cairo_vg_surface_get_height (cairo_surface_t *abstract_surface);

cairo_public int
cairo_vg_surface_get_width (cairo_surface_t *abstract_surface);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_VG_SURFACE*/
# error Cairo was not compiled with support for the OpenVG backend
#endif /* CAIRO_HAS_VG_SURFACE*/

#endif /* CAIRO_VG_H */
