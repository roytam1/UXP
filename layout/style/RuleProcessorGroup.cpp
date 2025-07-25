/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RuleProcessorGroup.h"

size_t
RuleProcessorGroup::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  for (uint32_t i = 0; i < mItems.Length(); i++) {
    n += mItems[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mItems.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}
