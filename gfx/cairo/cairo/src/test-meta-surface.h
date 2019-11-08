/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_META_SURFACE_H
#define TEST_META_SURFACE_H

#include "cairo.h"

CAIRO_BEGIN_DECLS

cairo_surface_t *
_cairo_test_meta_surface_create (cairo_content_t	content,
			   int			width,
			   int			height);

CAIRO_END_DECLS

#endif /* TEST_META_SURFACE_H */
