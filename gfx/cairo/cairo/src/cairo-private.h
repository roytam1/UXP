/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PRIVATE_H
#define CAIRO_PRIVATE_H

#include "cairo-reference-count-private.h"
#include "cairo-gstate-private.h"
#include "cairo-path-fixed-private.h"

struct _cairo {
    cairo_reference_count_t ref_count;

    cairo_status_t status;

    cairo_user_data_array_t user_data;

    cairo_gstate_t *gstate;
    cairo_gstate_t  gstate_tail[2];
    cairo_gstate_t *gstate_freelist;

    cairo_path_fixed_t path[1];
};

#endif /* CAIRO_PRIVATE_H */
