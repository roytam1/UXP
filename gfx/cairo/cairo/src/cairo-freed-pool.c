/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"

#include "cairo-freed-pool-private.h"

#if HAS_FREED_POOL

void *
_freed_pool_get_search (freed_pool_t *pool)
{
    void *ptr;
    int i;

    for (i = ARRAY_LENGTH (pool->pool); i--;) {
	ptr = _atomic_fetch (&pool->pool[i]);
	if (ptr != NULL) {
	    _cairo_atomic_int_set_relaxed (&pool->top, i);
	    return ptr;
	}
    }

    /* empty */
    _cairo_atomic_int_set_relaxed (&pool->top, 0);
    return NULL;
}

void
_freed_pool_put_search (freed_pool_t *pool, void *ptr)
{
    int i;

    for (i = 0; i < ARRAY_LENGTH (pool->pool); i++) {
	if (_atomic_store (&pool->pool[i], ptr)) {
	    _cairo_atomic_int_set_relaxed (&pool->top, i + 1);
	    return;
	}
    }

    /* full */
    _cairo_atomic_int_set_relaxed (&pool->top, i);
    free (ptr);
}

void
_freed_pool_reset (freed_pool_t *pool)
{
    int i;

    for (i = 0; i < ARRAY_LENGTH (pool->pool); i++) {
	free (pool->pool[i]);
	pool->pool[i] = NULL;
    }

    _cairo_atomic_int_set_relaxed (&pool->top, 0);
}

#endif
