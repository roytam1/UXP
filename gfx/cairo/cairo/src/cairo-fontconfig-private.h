/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CAIRO_FONTCONFIG_PRIVATE_H
#define _CAIRO_FONTCONFIG_PRIVATE_H

#include "cairo.h"

#if CAIRO_HAS_FC_FONT
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#endif

/* sub-pixel order */
#ifndef FC_RGBA_UNKNOWN
#define FC_RGBA_UNKNOWN	    0
#define FC_RGBA_RGB	    1
#define FC_RGBA_BGR	    2
#define FC_RGBA_VRGB	    3
#define FC_RGBA_VBGR	    4
#define FC_RGBA_NONE	    5
#endif

/* hinting style */
#ifndef FC_HINT_NONE
#define FC_HINT_NONE        0
#define FC_HINT_SLIGHT      1
#define FC_HINT_MEDIUM      2
#define FC_HINT_FULL        3
#endif

/* LCD filter */
#ifndef FC_LCD_NONE
#define FC_LCD_NONE	    0
#define FC_LCD_DEFAULT	    1
#define FC_LCD_LIGHT	    2
#define FC_LCD_LEGACY	    3
#endif

#endif /* _CAIRO_FONTCONFIG_PRIVATE_H */
