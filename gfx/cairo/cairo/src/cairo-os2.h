/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CAIRO_OS2_H_
#define _CAIRO_OS2_H_

#define INCL_DOS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI

#include "cairo.h"

#include <os2.h>

CAIRO_BEGIN_DECLS

/* The OS/2 Specific Cairo API */

cairo_public void
cairo_os2_init (void);

cairo_public void
cairo_os2_fini (void);

#if CAIRO_HAS_OS2_SURFACE

cairo_public cairo_surface_t *
cairo_os2_surface_create (HPS hps_client_window,
                          int width,
                          int height);

cairo_public cairo_surface_t *
cairo_os2_surface_create_for_window (HWND hwnd_client_window,
                                     int  width,
                                     int  height);

cairo_public void
cairo_os2_surface_set_hwnd (cairo_surface_t *surface,
                            HWND             hwnd_client_window);

cairo_public int
cairo_os2_surface_set_size (cairo_surface_t *surface,
                            int              new_width,
                            int              new_height,
                            int              timeout);

cairo_public void
cairo_os2_surface_refresh_window (cairo_surface_t *surface,
                                  HPS              hps_begin_paint,
                                  PRECTL           prcl_begin_paint_rect);

cairo_public void
cairo_os2_surface_set_manual_window_refresh (cairo_surface_t *surface,
                                             cairo_bool_t     manual_refresh);

cairo_public cairo_bool_t
cairo_os2_surface_get_manual_window_refresh (cairo_surface_t *surface);

cairo_public cairo_status_t
cairo_os2_surface_get_hps (cairo_surface_t *surface,
                           HPS             *hps);

cairo_public cairo_status_t
cairo_os2_surface_set_hps (cairo_surface_t *surface,
                           HPS              hps);

#else  /* CAIRO_HAS_OS2_SURFACE */
# error Cairo was not compiled with support for the OS/2 backend
#endif /* CAIRO_HAS_OS2_SURFACE */

CAIRO_END_DECLS

#endif /* _CAIRO_OS2_H_ */
