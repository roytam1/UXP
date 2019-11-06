/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SURFACE_SNAPSHOT_PRIVATE_H
#define CAIRO_SURFACE_SNAPSHOT_PRIVATE_H

#include "cairo-surface-private.h"

struct _cairo_surface_snapshot {
    cairo_surface_t base;

    cairo_surface_t *target;
    cairo_surface_t *clone;
};

#endif /* CAIRO_SURFACE_SNAPSHOT_PRIVATE_H */
