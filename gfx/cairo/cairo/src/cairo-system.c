/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file should include code that is system-specific, not
 * feature-specific.  For example, the DLL initialization/finalization
 * code on Win32 or OS/2 must live here (not in cairo-whatever-surface.c).
 * Same about possible ELF-specific code.
 *
 * And no other function should live here.
 */


#include "cairoint.h"



#if CAIRO_MUTEX_IMPL_WIN32
#if !CAIRO_WIN32_STATIC_BUILD

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as ETO_PDY */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairo-clip-private.h"
#include "cairo-paginated-private.h"
#include "cairo-win32-private.h"
#include "cairo-scaled-font-subsets-private.h"

#include <windows.h>

/* declare to avoid "no previous prototype for 'DllMain'" warning */
BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            CAIRO_MUTEX_INITIALIZE ();
            break;

        case DLL_PROCESS_DETACH:
            CAIRO_MUTEX_FINALIZE ();
            break;
    }

    return TRUE;
}

#endif
#endif

