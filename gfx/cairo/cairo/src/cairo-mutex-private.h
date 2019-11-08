/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_MUTEX_PRIVATE_H
#define CAIRO_MUTEX_PRIVATE_H

#include "cairo-mutex-type-private.h"

CAIRO_BEGIN_DECLS

#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER
cairo_private void _cairo_mutex_initialize (void);
#endif
#if _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER
cairo_private void _cairo_mutex_finalize (void);
#endif
/* only if using static initializer and/or finalizer define the boolean */
#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER || _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER
  cairo_private extern cairo_bool_t _cairo_mutex_initialized;
#endif

/* Finally, extern the static mutexes and undef */

#define CAIRO_MUTEX_DECLARE(mutex) cairo_private extern cairo_mutex_t mutex;
#include "cairo-mutex-list-private.h"
#undef CAIRO_MUTEX_DECLARE

CAIRO_END_DECLS

#endif
