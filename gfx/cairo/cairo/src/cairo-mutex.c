/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"

#include "cairo-mutex-private.h"

#define CAIRO_MUTEX_DECLARE(mutex) cairo_mutex_t mutex = CAIRO_MUTEX_NIL_INITIALIZER;
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE

#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER || _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER

# if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER
#  define _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE FALSE
# else
#  define _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE TRUE
# endif

cairo_bool_t _cairo_mutex_initialized = _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE;

# undef _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE

#endif

#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER
void _cairo_mutex_initialize (void)
{
    if (_cairo_mutex_initialized)
        return;

    _cairo_mutex_initialized = TRUE;

#define  CAIRO_MUTEX_DECLARE(mutex) CAIRO_MUTEX_INIT (mutex);
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE
}
#endif

#if _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER
void _cairo_mutex_finalize (void)
{
    if (!_cairo_mutex_initialized)
        return;

    _cairo_mutex_initialized = FALSE;

#define  CAIRO_MUTEX_DECLARE(mutex) CAIRO_MUTEX_FINI (mutex);
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE
}
#endif
