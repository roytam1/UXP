/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SCRIPT_H
#define CAIRO_SCRIPT_H

#include "cairo.h"

#if CAIRO_HAS_SCRIPT_SURFACE

CAIRO_BEGIN_DECLS

typedef enum {
    CAIRO_SCRIPT_MODE_BINARY,
    CAIRO_SCRIPT_MODE_ASCII
} cairo_script_mode_t;

cairo_public cairo_device_t *
cairo_script_create (const char *filename);

cairo_public cairo_device_t *
cairo_script_create_for_stream (cairo_write_func_t	 write_func,
				void			*closure);

cairo_public void
cairo_script_write_comment (cairo_device_t *script,
			    const char *comment,
			    int len);

cairo_public void
cairo_script_set_mode (cairo_device_t *script,
		       cairo_script_mode_t mode);

cairo_public cairo_script_mode_t
cairo_script_get_mode (cairo_device_t *script);

cairo_public cairo_surface_t *
cairo_script_surface_create (cairo_device_t *script,
			     cairo_content_t content,
			     double width,
			     double height);

cairo_public cairo_surface_t *
cairo_script_surface_create_for_target (cairo_device_t *script,
					cairo_surface_t *target);

cairo_public cairo_status_t
cairo_script_from_recording_surface (cairo_device_t	*script,
				     cairo_surface_t	*recording_surface);

CAIRO_END_DECLS

#else  /*CAIRO_HAS_SCRIPT_SURFACE*/
# error Cairo was not compiled with support for the CairoScript backend
#endif /*CAIRO_HAS_SCRIPT_SURFACE*/

#endif /*CAIRO_SCRIPT_H*/
