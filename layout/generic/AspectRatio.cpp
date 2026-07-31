/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AspectRatio.h"

#include "mozilla/WritingModes.h"

namespace mozilla {

nscoord
AspectRatio::ComputeRatioDependentSize(
    LogicalAxis aRatioDependentAxis,
    const WritingMode& aWM,
    nscoord aRatioDeterminingSize,
    const LogicalSize& aContentBoxToBoxSizingAdjust) const
{
  MOZ_DIAGNOSTIC_ASSERT(*this);

  LogicalSize adjust(aWM);
  if (mUseBoxSizing == UseBoxSizing::Yes) {
    adjust = aContentBoxToBoxSizingAdjust;
  }

  AspectRatio logicalRatio = aWM.IsVertical() ? Inverted() : *this;
  nscoord result;
  if (aRatioDependentAxis == eLogicalAxisInline) {
    result = logicalRatio.ApplyTo(aRatioDeterminingSize +
                                  adjust.BSize(aWM)) -
             adjust.ISize(aWM);
  } else {
    result = logicalRatio.Inverted().ApplyTo(aRatioDeterminingSize +
                                             adjust.ISize(aWM)) -
             adjust.BSize(aWM);
  }
  return std::max(nscoord(0), result);
}

} // namespace mozilla
