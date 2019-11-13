/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_ARC_PRIVATE_H
#define CAIRO_ARC_PRIVATE_H

#include "cairoint.h"

cairo_private void
_cairo_arc_path (cairo_t *cr,
		 double	  xc,
		 double	  yc,
		 double	  radius,
		 double	  angle1,
		 double	  angle2);

cairo_private void
_cairo_arc_path_negative (cairo_t *cr,
			  double   xc,
			  double   yc,
			  double   radius,
			  double   angle1,
			  double   angle2);

#endif /* CAIRO_ARC_PRIVATE_H */
