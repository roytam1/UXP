/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PDF_H
#define CAIRO_PDF_H

#include "cairo.h"

#if CAIRO_HAS_PDF_SURFACE

CAIRO_BEGIN_DECLS

/**
 * cairo_pdf_version_t:
 * @CAIRO_PDF_VERSION_1_4: The version 1.4 of the PDF specification.
 * @CAIRO_PDF_VERSION_1_5: The version 1.5 of the PDF specification.
 *
 * #cairo_pdf_version_t is used to describe the version number of the PDF
 * specification that a generated PDF file will conform to.
 *
 * Since 1.10
 */
typedef enum _cairo_pdf_version {
    CAIRO_PDF_VERSION_1_4,
    CAIRO_PDF_VERSION_1_5
} cairo_pdf_version_t;

cairo_public cairo_surface_t *
cairo_pdf_surface_create (const char		*filename,
			  double		 width_in_points,
			  double		 height_in_points);

cairo_public cairo_surface_t *
cairo_pdf_surface_create_for_stream (cairo_write_func_t	write_func,
				     void	       *closure,
				     double		width_in_points,
				     double		height_in_points);

cairo_public void
cairo_pdf_surface_restrict_to_version (cairo_surface_t 		*surface,
				       cairo_pdf_version_t  	 version);

cairo_public void
cairo_pdf_get_versions (cairo_pdf_version_t const	**versions,
                        int                      	 *num_versions);

cairo_public const char *
cairo_pdf_version_to_string (cairo_pdf_version_t version);

cairo_public void
cairo_pdf_surface_set_size (cairo_surface_t	*surface,
			    double		 width_in_points,
			    double		 height_in_points);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_PDF_SURFACE */
# error Cairo was not compiled with support for the pdf backend
#endif /* CAIRO_HAS_PDF_SURFACE */

#endif /* CAIRO_PDF_H */
