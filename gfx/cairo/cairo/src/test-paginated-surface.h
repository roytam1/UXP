/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_PAGINATED_SURFACE_H
#define TEST_PAGINATED_SURFACE_H

#include "cairo.h"

CAIRO_BEGIN_DECLS

cairo_surface_t *
_cairo_test_paginated_surface_create (cairo_surface_t *target);

CAIRO_END_DECLS

#endif /* TEST_PAGINATED_SURFACE_H */
