/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerPolicyFactory.h"
#include "SandboxInfo.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "SpecialSystemDirectory.h"

#ifdef ANDROID
#include "cutils/properties.h"
#endif

namespace mozilla {

/* static */ bool
SandboxBrokerPolicyFactory::IsSystemSupported() {
#ifdef ANDROID
  char hardware[PROPERTY_VALUE_MAX];
  int length = property_get("ro.hardware", hardware, nullptr);
  // "goldfish" -> emulator.  Other devices can be added when we're
  // reasonably sure they work.  Eventually this won't be needed....
  if (length > 0 && strcmp(hardware, "goldfish") == 0) {
    return true;
  }

  // When broker is running in permissive mode, we enable it
  // automatically regardless of the device.
  if (SandboxInfo::Get().Test(SandboxInfo::kPermissive)) {
    return true;
  }
#endif
  return false;
}

SandboxBrokerPolicyFactory::SandboxBrokerPolicyFactory()
{
  // Policy entries that are the same in every process go here, and
  // are cached over the lifetime of the factory.
}

} // namespace mozilla
