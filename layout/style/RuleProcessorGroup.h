/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RuleProcessorGroup_h___
#define RuleProcessorGroup_h___

struct RuleProcessorGroup
{
  RuleProcessorGroup(nsIAtom* aMedium)
    : mCacheKey(aMedium)
    , mNext(nullptr)
  {
  }

  ~RuleProcessorGroup() { mItems.Clear(); }

  nsTArray<nsCOMPtr<nsIStyleRuleProcessor>> mItems;
  nsMediaQueryResultCacheKey mCacheKey;
  RuleProcessorGroup* mNext; // for a different medium

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
};

#endif /* RuleProcessorGroup_h___ */
