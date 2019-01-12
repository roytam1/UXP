/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PotentialCheckerboardDurationTracker.h"

namespace mozilla {
namespace layers {

PotentialCheckerboardDurationTracker::PotentialCheckerboardDurationTracker()
  : mInCheckerboard(false)
  , mInTransform(false)
{
}

void
PotentialCheckerboardDurationTracker::CheckerboardSeen()
{
  mInCheckerboard = true;
}

void
PotentialCheckerboardDurationTracker::CheckerboardDone()
{
  MOZ_ASSERT(Tracking());
  mInCheckerboard = false;
}

void
PotentialCheckerboardDurationTracker::InTransform(bool aInTransform)
{
  if (aInTransform == mInTransform) {
    // no-op
    return;
  }

  if (!Tracking()) {
    // Because !Tracking(), mInTransform must be false, and so aInTransform
    // must be true (or we would have early-exited this function already).
    // Therefore, we are starting a potential checkerboard period.
    mInTransform = aInTransform;
    return;
  }

  mInTransform = aInTransform;
}

bool
PotentialCheckerboardDurationTracker::Tracking() const
{
  return mInTransform || mInCheckerboard;
}

} // namespace layers
} // namespace mozilla
