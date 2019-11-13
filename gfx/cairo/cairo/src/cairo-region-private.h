/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_REGION_PRIVATE_H
#define CAIRO_REGION_PRIVATE_H

#include "cairo-types-private.h"
#include "cairo-reference-count-private.h"

#include <pixman.h>

CAIRO_BEGIN_DECLS

struct _cairo_region {
    cairo_reference_count_t ref_count;
    cairo_status_t status;

    pixman_region32_t rgn;
};

cairo_private cairo_region_t *
_cairo_region_create_in_error (cairo_status_t status);

cairo_private void
_cairo_region_init (cairo_region_t *region);

cairo_private void
_cairo_region_init_rectangle (cairo_region_t *region,
			      const cairo_rectangle_int_t *rectangle);

cairo_private void
_cairo_region_fini (cairo_region_t *region);

CAIRO_END_DECLS

#endif /* CAIRO_REGION_PRIVATE_H */
