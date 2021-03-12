/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc_abort.h"

#include <stdio.h>

#include "mozilla/Assertions.h"

void
mozalloc_abort(const char* const msg)
{
    fputs(msg, stderr);
    fputs("\n", stderr);
    MOZ_CRASH();
}

#if defined(XP_UNIX) && !defined(MOZ_ASAN)
// Define abort() here, so that it is used instead of the system abort(). This
// lets us control the behavior when aborting, in order to get better results
// on *NIX platforms. See mozalloc_abort for details.
//
// For AddressSanitizer, we must not redefine system abort because the ASan
// option "abort_on_error=1" calls abort() and therefore causes the following
// call chain with our redefined abort:
//
// ASan -> abort() -> moz_abort() -> MOZ_CRASH() -> Segmentation fault
//
// That segmentation fault will be interpreted as another bug by ASan and as a
// result, ASan will just exit(1) instead of aborting.
extern "C" void abort(void)
{
    const char* const msg = "Redirecting call to abort() to mozalloc_abort\n";

    mozalloc_abort(msg);

    // We won't reach here because mozalloc_abort() is MOZ_NORETURN. But that
    // annotation isn't used on ARM (see mozalloc_abort.h for why) so we add a
    // redundant MOZ_CRASH() here to avoid a "'noreturn' function does return"
    // warning.
    MOZ_CRASH();
}
#endif

