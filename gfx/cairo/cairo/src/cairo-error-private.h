/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CAIRO_ERROR_PRIVATE_H_
#define _CAIRO_ERROR_PRIVATE_H_

#include "cairo.h"
#include "cairo-compiler-private.h"

CAIRO_BEGIN_DECLS

#define _cairo_status_is_error(status) \
    (status != CAIRO_STATUS_SUCCESS && status <= CAIRO_STATUS_LAST_STATUS)

cairo_private cairo_status_t
_cairo_error (cairo_status_t status);

/* hide compiler warnings when discarding the return value */
#define _cairo_error_throw(status) do { \
    cairo_status_t status__ = _cairo_error (status); \
    (void) status__; \
} while (0)

CAIRO_END_DECLS

#endif /* _CAIRO_ERROR_PRIVATE_H_ */
