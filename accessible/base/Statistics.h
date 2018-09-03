/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef A11Y_STATISTICS_H_
#define A11Y_STATISTICS_H_

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace a11y {
namespace statistics {

  inline void A11yInitialized()
    { /* STUB */ }

  inline void A11yConsumers(uint32_t aConsumer)
    { /* STUB */ }

  /**
   * Report that ISimpleDOM* has been used.
   */
  inline void ISimpleDOMUsed()
    { /* STUB */ }

  /**
   * Report that IAccessibleTable has been used.
   */
  inline void IAccessibleTableUsed()
    { /* STUB */ }

} // namespace statistics
} // namespace a11y
} // namespace mozilla

#endif

