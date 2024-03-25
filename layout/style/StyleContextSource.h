/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleContextSource_h
#define mozilla_StyleContextSource_h

#include "nsRuleNode.h"

namespace mozilla {

// Temporary holder of Gecko Rule Nodes.
// TODO: This struct is marked for removal.
//
// The rule node is the node in the lexicographic tree of rule nodes
// (the "rule tree") that indicates which style rules are used to
// compute the style data, and in what cascading order.  The least
// specific rule matched is the one whose rule node is a child of the
// root of the rule tree, and the most specific rule matched is the
// |mRule| member of the rule node.

// Underlying pointer without any strong ownership semantics.
struct NonOwningStyleContextSource
{
  MOZ_IMPLICIT NonOwningStyleContextSource(nsRuleNode* aRuleNode)
    : mBits(reinterpret_cast<uintptr_t>(aRuleNode)) {}

  bool operator==(const NonOwningStyleContextSource& aOther) const {
    return mBits == aOther.mBits;
  }
  bool operator!=(const NonOwningStyleContextSource& aOther) const {
    return !(*this == aOther);
  }

  bool IsNull() const { return !mBits; }

  nsRuleNode* AsGeckoRuleNode() const {
    return reinterpret_cast<nsRuleNode*>(mBits);
  }

  bool MatchesNoRules() const {
    return AsGeckoRuleNode()->IsRoot();
  }

private:
  uintptr_t mBits;
};

// Higher-level struct that owns a strong reference to the source. The source
// is never null.
struct OwningStyleContextSource
{
  explicit OwningStyleContextSource(already_AddRefed<nsRuleNode> aRuleNode)
    : mRaw(aRuleNode.take())
  {
    MOZ_COUNT_CTOR(OwningStyleContextSource);
    MOZ_ASSERT(!mRaw.IsNull());
  };

  OwningStyleContextSource(OwningStyleContextSource&& aOther)
    : mRaw(aOther.mRaw)
  {
    MOZ_COUNT_CTOR(OwningStyleContextSource);
    aOther.mRaw = nullptr;
  }

  OwningStyleContextSource& operator=(OwningStyleContextSource&) = delete;
  OwningStyleContextSource(OwningStyleContextSource&) = delete;

  ~OwningStyleContextSource() {
    MOZ_COUNT_DTOR(OwningStyleContextSource);
    if (mRaw.IsNull()) {
      // We must have invoked the move constructor.
    } else {
      RefPtr<nsRuleNode> releaseme = dont_AddRef(AsGeckoRuleNode());
    }
  }

  bool operator==(const OwningStyleContextSource& aOther) const {
    return mRaw == aOther.mRaw;
  }
  bool operator!=(const OwningStyleContextSource& aOther) const {
    return !(*this == aOther);
  }
  bool IsNull() const { return mRaw.IsNull(); }

  NonOwningStyleContextSource AsRaw() const { return mRaw; }
  nsRuleNode* AsGeckoRuleNode() const { return mRaw.AsGeckoRuleNode(); }

  bool MatchesNoRules() const { return mRaw.MatchesNoRules(); }

private:
  NonOwningStyleContextSource mRaw;
};

} // namespace mozilla

#endif // mozilla_StyleContextSource_h
