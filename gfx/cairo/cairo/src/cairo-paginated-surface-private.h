/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_PAGINATED_SURFACE_H
#define CAIRO_PAGINATED_SURFACE_H

#include "cairo.h"

#include "cairo-surface-private.h"

typedef struct _cairo_paginated_surface {
    cairo_surface_t base;

    /* The target surface to hold the final result. */
    cairo_surface_t *target;

    cairo_content_t content;

    /* Paginated-surface specific functions for the target */
    const cairo_paginated_surface_backend_t *backend;

    /* A cairo_recording_surface to record all operations. To be replayed
     * against target, and also against image surface as necessary for
     * fallbacks. */
    cairo_surface_t *recording_surface;

    int page_num;
} cairo_paginated_surface_t;

#endif /* CAIRO_PAGINATED_SURFACE_H */
