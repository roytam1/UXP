/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_IMAGE_INFO_PRIVATE_H
#define CAIRO_IMAGE_INFO_PRIVATE_H

#include "cairoint.h"

typedef struct _cairo_image_info {
    int		 width;
    int		 height;
    int		 num_components;
    int		 bits_per_component;
} cairo_image_info_t;

cairo_private cairo_int_status_t
_cairo_image_info_get_jpeg_info (cairo_image_info_t	*info,
				 const unsigned char	*data,
				 long			 length);

cairo_private cairo_int_status_t
_cairo_image_info_get_jpx_info (cairo_image_info_t	*info,
				const unsigned char	*data,
				unsigned long		 length);

cairo_private cairo_int_status_t
_cairo_image_info_get_png_info (cairo_image_info_t	*info,
				const unsigned char     *data,
				unsigned long            length);

#endif /* CAIRO_IMAGE_INFO_PRIVATE_H */
