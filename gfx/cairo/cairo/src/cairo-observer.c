/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"

void
_cairo_observers_notify (cairo_list_t *observers, void *arg)
{
    cairo_observer_t *obs, *next;

    cairo_list_foreach_entry_safe (obs, next,
				   cairo_observer_t,
				   observers, link)
    {
	obs->callback (obs, arg);
    }
}
