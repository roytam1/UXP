/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_RTREE_PRIVATE_H
#define CAIRO_RTREE_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"

#include "cairo-freelist-private.h"
#include "cairo-list-private.h"

enum {
    CAIRO_RTREE_NODE_AVAILABLE,
    CAIRO_RTREE_NODE_DIVIDED,
    CAIRO_RTREE_NODE_OCCUPIED,
};

typedef struct _cairo_rtree_node {
    struct _cairo_rtree_node *children[4], *parent;
    void **owner;
    cairo_list_t link;
    uint16_t pinned;
    uint16_t state;
    uint16_t x, y;
    uint16_t width, height;
} cairo_rtree_node_t;

typedef struct _cairo_rtree {
    cairo_rtree_node_t root;
    int min_size;
    cairo_list_t pinned;
    cairo_list_t available;
    cairo_list_t evictable;
    cairo_freepool_t node_freepool;
} cairo_rtree_t;

cairo_private cairo_rtree_node_t *
_cairo_rtree_node_create (cairo_rtree_t		 *rtree,
		          cairo_rtree_node_t	 *parent,
			  int			  x,
			  int			  y,
			  int			  width,
			  int			  height);

cairo_private cairo_status_t
_cairo_rtree_node_insert (cairo_rtree_t *rtree,
	                  cairo_rtree_node_t *node,
			  int width,
			  int height,
			  cairo_rtree_node_t **out);

cairo_private void
_cairo_rtree_node_collapse (cairo_rtree_t *rtree, cairo_rtree_node_t *node);

cairo_private void
_cairo_rtree_node_remove (cairo_rtree_t *rtree, cairo_rtree_node_t *node);

cairo_private void
_cairo_rtree_node_destroy (cairo_rtree_t *rtree, cairo_rtree_node_t *node);

cairo_private void
_cairo_rtree_init (cairo_rtree_t	*rtree,
	           int			 width,
		   int			 height,
		   int			 min_size,
		   int			 node_size);

cairo_private cairo_int_status_t
_cairo_rtree_insert (cairo_rtree_t	     *rtree,
		     int		      width,
	             int		      height,
	             cairo_rtree_node_t	    **out);

cairo_private cairo_int_status_t
_cairo_rtree_evict_random (cairo_rtree_t	 *rtree,
		           int			  width,
		           int			  height,
		           cairo_rtree_node_t	**out);

static inline void *
_cairo_rtree_pin (cairo_rtree_t *rtree, cairo_rtree_node_t *node)
{
    if (! node->pinned) {
	cairo_list_move (&node->link, &rtree->pinned);
	node->pinned = 1;
    }

    return node;
}

cairo_private void
_cairo_rtree_unpin (cairo_rtree_t *rtree);

cairo_private void
_cairo_rtree_reset (cairo_rtree_t *rtree);

cairo_private void
_cairo_rtree_fini (cairo_rtree_t *rtree);

#endif /* CAIRO_RTREE_PRIVATE_H */
