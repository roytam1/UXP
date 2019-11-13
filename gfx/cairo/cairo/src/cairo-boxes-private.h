/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_BOXES_H
#define CAIRO_BOXES_H

#include "cairo-types-private.h"
#include "cairo-compiler-private.h"

struct _cairo_boxes_t {
    cairo_status_t status;
    cairo_box_t limit;
    const cairo_box_t *limits;
    int num_limits;
    int num_boxes;
    unsigned int is_pixel_aligned : 1;

    struct _cairo_boxes_chunk {
	struct _cairo_boxes_chunk *next;
	cairo_box_t *base;
	int count;
	int size;
    } chunks, *tail;
    cairo_box_t boxes_embedded[32];
};

cairo_private void
_cairo_boxes_init (cairo_boxes_t *boxes);

cairo_private void
_cairo_boxes_init_for_array (cairo_boxes_t *boxes,
			     cairo_box_t *array,
			     int num_boxes);

cairo_private void
_cairo_boxes_limit (cairo_boxes_t	*boxes,
		    const cairo_box_t	*limits,
		    int			 num_limits);

cairo_private cairo_status_t
_cairo_boxes_add (cairo_boxes_t *boxes,
		  const cairo_box_t *box);

cairo_private void
_cairo_boxes_extents (const cairo_boxes_t *boxes,
		      cairo_rectangle_int_t *extents);

cairo_private void
_cairo_boxes_clear (cairo_boxes_t *boxes);

cairo_private void
_cairo_boxes_fini (cairo_boxes_t *boxes);

#endif /* CAIRO_BOXES_H */
