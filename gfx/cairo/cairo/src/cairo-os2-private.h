/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_OS2_PRIVATE_H
#define CAIRO_OS2_PRIVATE_H

#include "cairo-os2.h"
#include "cairoint.h"

typedef struct _cairo_os2_surface
{
    cairo_surface_t        base;

    /* Mutex semaphore to protect private fields from concurrent access */
    HMTX                   hmtx_use_private_fields;
    /* Private fields: */
    HPS                    hps_client_window;
    HWND                   hwnd_client_window;
    BITMAPINFO2            bitmap_info;
    unsigned char         *pixels;
    cairo_image_surface_t *image_surface;
    int                    pixel_array_lend_count;
    HEV                    hev_pixel_array_came_back;

    RECTL                  rcl_dirty_area;
    cairo_bool_t           dirty_area_present;

    /* General flags: */
    cairo_bool_t           blit_as_changes;
    cairo_bool_t           use_24bpp;
} cairo_os2_surface_t;

#endif /* CAIRO_OS2_PRIVATE_H */
