/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/dom/Element.h"
#include "nsIContentInlines.h"
#include "nsContentUtils.h"

namespace mozilla {

ServoElementSnapshot::ServoElementSnapshot(Element* aElement)
  : mContains(Flags(0))
  , mState(0)
  , mExplicitRestyleHint(nsRestyleHint(0))
  , mExplicitChangeHint(nsChangeHint(0))
{
}

void
ServoElementSnapshot::AddAttrs(Element* aElement)
{
}

} // namespace mozilla
