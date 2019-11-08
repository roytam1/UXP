/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_XML_H
#define CAIRO_XML_H

#include "cairo.h"

#if CAIRO_HAS_XML_SURFACE

CAIRO_BEGIN_DECLS

cairo_public cairo_device_t *
cairo_xml_create (const char *filename);

cairo_public cairo_device_t *
cairo_xml_create_for_stream (cairo_write_func_t	 write_func,
			     void		*closure);

cairo_public cairo_surface_t *
cairo_xml_surface_create (cairo_device_t *xml,
			  cairo_content_t content,
			  double width, double height);

cairo_public cairo_status_t
cairo_xml_for_recording_surface (cairo_device_t *xml,
				 cairo_surface_t *surface);

CAIRO_END_DECLS

#else  /*CAIRO_HAS_XML_SURFACE*/
# error Cairo was not compiled with support for the XML backend
#endif /*CAIRO_HAS_XML_SURFACE*/

#endif /*CAIRO_XML_H*/
