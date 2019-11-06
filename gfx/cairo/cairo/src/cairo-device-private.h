/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CAIRO_DEVICE_PRIVATE_H_
#define _CAIRO_DEVICE_PRIVATE_H_

#include "cairo-compiler-private.h"
#include "cairo-mutex-private.h"
#include "cairo-reference-count-private.h"
#include "cairo-types-private.h"

struct _cairo_device {
    cairo_reference_count_t ref_count;
    cairo_status_t status;
    cairo_user_data_array_t user_data;

    const cairo_device_backend_t *backend;

    cairo_recursive_mutex_t mutex;
    unsigned mutex_depth;

    cairo_bool_t finished;
};

struct _cairo_device_backend {
    cairo_device_type_t type;

    void (*lock) (void *device);
    void (*unlock) (void *device);

    cairo_warn cairo_status_t (*flush) (void *device);
    void (*finish) (void *device);
    void (*destroy) (void *device);
};

cairo_private cairo_device_t *
_cairo_device_create_in_error (cairo_status_t status);

cairo_private void
_cairo_device_init (cairo_device_t *device,
		    const cairo_device_backend_t *backend);

cairo_private cairo_status_t
_cairo_device_set_error (cairo_device_t *device,
		         cairo_status_t error);

slim_hidden_proto_no_warn (cairo_device_reference);
slim_hidden_proto (cairo_device_acquire);
slim_hidden_proto (cairo_device_release);
slim_hidden_proto (cairo_device_flush);
slim_hidden_proto (cairo_device_finish);
slim_hidden_proto (cairo_device_destroy);

#endif /* _CAIRO_DEVICE_PRIVATE_H_ */
