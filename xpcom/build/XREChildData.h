/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XREChildData_h
#define XREChildData_h

#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gmp {
class GMPLoader;
}
}

/**
 * Data needed to start a child process.
 */
struct XREChildData
{
  /**
   * Used to load the GMP binary.
   */
  mozilla::UniquePtr<mozilla::gmp::GMPLoader> gmpLoader;
};

#endif // XREChildData_h
