/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"

#include "cairo-atomic-private.h"
#include "cairo-mutex-private.h"

#ifdef HAS_ATOMIC_OPS
COMPILE_TIME_ASSERT(sizeof(void*) == sizeof(int) ||
		    sizeof(void*) == sizeof(long) ||
		    sizeof(void*) == sizeof(long long));
#else
void
_cairo_atomic_int_inc (cairo_atomic_intptr_t *x)
{
    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    *x += 1;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);
}

cairo_bool_t
_cairo_atomic_int_dec_and_test (cairo_atomic_intptr_t *x)
{
    cairo_bool_t ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = --*x == 0;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

cairo_atomic_intptr_t
_cairo_atomic_int_cmpxchg_return_old_impl (cairo_atomic_intptr_t *x, cairo_atomic_intptr_t oldv, cairo_atomic_intptr_t newv)
{
    cairo_atomic_intptr_t ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = *x;
    if (ret == oldv)
	*x = newv;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

void *
_cairo_atomic_ptr_cmpxchg_return_old_impl (void **x, void *oldv, void *newv)
{
    void *ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = *x;
    if (ret == oldv)
	*x = newv;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

#ifdef ATOMIC_OP_NEEDS_MEMORY_BARRIER
cairo_atomic_intptr_t
_cairo_atomic_int_get (cairo_atomic_intptr_t *x)
{
    cairo_atomic_intptr_t ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = *x;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

cairo_atomic_intptr_t
_cairo_atomic_int_get_relaxed (cairo_atomic_intptr_t *x)
{
    return _cairo_atomic_int_get (x);
}

void
_cairo_atomic_int_set_relaxed (cairo_atomic_intptr_t *x, cairo_atomic_intptr_t val)
{
    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    *x = val;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);
}
#endif

#endif
