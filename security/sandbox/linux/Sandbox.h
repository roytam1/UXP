/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "mozilla/Types.h"
#include "nsXULAppAPI.h"

// This defines the entry points for a content process to start
// sandboxing itself.  See also SandboxInfo.h for what parts of
// sandboxing are enabled/supported.

namespace mozilla {

// This must be called early, while the process is still single-threaded.
MOZ_EXPORT void SandboxEarlyInit(GeckoProcessType aType);

} // namespace mozilla

#endif // mozilla_Sandbox_h
