/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SURFACE_PRIVATE_H
#define CAIRO_SURFACE_PRIVATE_H

#include "cairo.h"

#include "cairo-types-private.h"
#include "cairo-list-private.h"
#include "cairo-reference-count-private.h"
#include "cairo-clip-private.h"

struct _cairo_surface {
    const cairo_surface_backend_t *backend;
    cairo_device_t *device;

    /* We allow surfaces to override the backend->type by shoving something
     * else into surface->type. This is for "wrapper" surfaces that want to
     * hide their internal type from the user-level API. */
    cairo_surface_type_t type;

    cairo_content_t content;

    cairo_reference_count_t ref_count;
    cairo_status_t status;
    unsigned int unique_id;

    unsigned finished : 1;
    unsigned is_clear : 1;
    unsigned has_font_options : 1;
    unsigned owns_device : 1;
    unsigned permit_subpixel_antialiasing : 1;

    cairo_user_data_array_t user_data;
    cairo_user_data_array_t mime_data;

    cairo_matrix_t device_transform;
    cairo_matrix_t device_transform_inverse;
    cairo_list_t device_transform_observers;

    /* The actual resolution of the device, in dots per inch. */
    double x_resolution;
    double y_resolution;

    /* The resolution that should be used when generating image-based
     * fallback; generally only used by the analysis/paginated
     * surfaces
     */
    double x_fallback_resolution;
    double y_fallback_resolution;

    /* A "snapshot" surface is immutable. See _cairo_surface_snapshot. */
    cairo_surface_t *snapshot_of;
    cairo_surface_func_t snapshot_detach;
    /* current snapshots of this surface*/
    cairo_list_t snapshots;
    /* place upon snapshot list */
    cairo_list_t snapshot;

    /*
     * Surface font options, falling back to backend's default options,
     * and set using _cairo_surface_set_font_options(), and propagated by
     * cairo_surface_create_similar().
     */
    cairo_font_options_t font_options;
};

#endif /* CAIRO_SURFACE_PRIVATE_H */
