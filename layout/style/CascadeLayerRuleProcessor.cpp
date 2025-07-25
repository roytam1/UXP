/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for cascade layers
 */

#include "nsAutoPtr.h"
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include <algorithm>
#include "nsIAtom.h"
#include "PLDHashTable.h"
#include "nsICSSPseudoComparator.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/GroupRule.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsUnicharUtils.h"
#include "nsError.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStyleUtil.h"
#include "nsQuickSort.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsAttrName.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsIMediaList.h"
#include "nsCSSRules.h"
#include "nsStyleSet.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsNthIndexCache.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Likely.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/TypedEnumBits.h"
#include "RuleProcessorCache.h"
#include "nsIDOMMutationEvent.h"
#include "nsIMozBrowserFrame.h"
#include "RuleCascadeData.h"
#include "nsCSSRuleUtils.h"
#include "CascadeLayerRuleProcessor.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------
// Cascade layer style rule processor implementation
//

CascadeLayerRuleProcessor::CascadeLayerRuleProcessor(
  CascadeLayer* aLayer)
  : mLayer(aLayer)
  , mCascade(aLayer->mData)
{
  MOZ_ASSERT(mLayer,
             "Layer rule processor must have an attached cascade layer");
  MOZ_ASSERT(mCascade, "Cascade layer is missing its data");
}

/* virtual */ void
CascadeLayerRuleProcessor::RulesMatching(ElementRuleProcessorData* aData)
{
  if (mCascade) {
    mCascade->RulesMatching(aData);
  }
}

/* virtual */ void
CascadeLayerRuleProcessor::RulesMatching(
  PseudoElementRuleProcessorData* aData)
{
  if (mCascade) {
    mCascade->RulesMatching(aData);
  }
}

/* virtual */ void
CascadeLayerRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  if (mCascade) {
    mCascade->RulesMatching(aData);
  }
}

/* virtual */ void
CascadeLayerRuleProcessor::RulesMatching(XULTreeRuleProcessorData* aData)
{
  if (mCascade) {
    mCascade->RulesMatching(aData);
  }
}

nsRestyleHint
CascadeLayerRuleProcessor::HasStateDependentStyle(
  ElementDependentRuleProcessorData* aData,
  Element* aStatefulElement,
  CSSPseudoElementType aPseudoType,
  EventStates aStateMask,
  nsRestyleHint& aHint)
{
  if (mCascade) {
    mCascade->HasStateDependentStyle(
      aData, aStatefulElement, aPseudoType, aStateMask, aHint);
  }
  return aHint;
}

nsRestyleHint
CascadeLayerRuleProcessor::HasStateDependentStyle(
  ElementDependentRuleProcessorData* aData,
  Element* aStatefulElement,
  CSSPseudoElementType aPseudoType,
  EventStates aStateMask)
{
  nsRestyleHint hint = nsRestyleHint(0);
  return HasStateDependentStyle(
    aData, aStatefulElement, aPseudoType, aStateMask, hint);
}

/* virtual */ nsRestyleHint
CascadeLayerRuleProcessor::HasStateDependentStyle(
  StateRuleProcessorData* aData)
{
  return HasStateDependentStyle(
    aData, aData->mElement, CSSPseudoElementType::NotPseudo, aData->mStateMask);
}

/* virtual */ nsRestyleHint
CascadeLayerRuleProcessor::HasStateDependentStyle(
  PseudoElementStateRuleProcessorData* aData)
{
  return HasStateDependentStyle(
    aData, aData->mPseudoElement, aData->mPseudoType, aData->mStateMask);
}

/* virtual */ bool
CascadeLayerRuleProcessor::HasDocumentStateDependentStyle(
  StateRuleProcessorData* aData)
{
  if (mCascade && mCascade->mSelectorDocumentStates.HasAtLeastOneOfStates(
        aData->mStateMask)) {
    return true;
  }

  return false;
}

nsRestyleHint
CascadeLayerRuleProcessor::HasAttributeDependentStyle(
  AttributeRuleProcessorData* aData,
  AttributeEnumData* aEnumData,
  mozilla::RestyleHintData& aRestyleHintDataResult)
{
  // We could try making use of aData->mModType, but :not rules make it a bit
  // of a pain to do so...  So just ignore it for now.

  // Don't do our special handling of certain attributes if the attr
  // hasn't changed yet.
  if (aData->mAttrHasChanged) {
    // check for the lwtheme and lwthemetextcolor attribute on root XUL elements
    if ((aData->mAttribute == nsGkAtoms::lwtheme ||
         aData->mAttribute == nsGkAtoms::lwthemetextcolor) &&
        aData->mElement->GetNameSpaceID() == kNameSpaceID_XUL &&
        aData->mElement == aData->mElement->OwnerDoc()->GetRootElement()) {
      aEnumData->change = nsRestyleHint(aEnumData->change | eRestyle_Subtree);
    }

    // We don't know the namespace of the attribute, and xml:lang applies to
    // all elements.  If the lang attribute changes, we need to restyle our
    // whole subtree, since the :lang selector on our descendants can examine
    // our lang attribute.
    if (aData->mAttribute == nsGkAtoms::lang) {
      aEnumData->change = nsRestyleHint(aEnumData->change | eRestyle_Subtree);
    }
  }
  if (mCascade) {
    mCascade->HasAttributeDependentStyle(aData, aEnumData, aRestyleHintDataResult);
  }
  return aEnumData->change;
}

/* virtual */ nsRestyleHint
CascadeLayerRuleProcessor::HasAttributeDependentStyle(
  AttributeRuleProcessorData* aData,
  mozilla::RestyleHintData& aRestyleHintDataResult)
{
  AttributeEnumData data(aData, aRestyleHintDataResult);
  return HasAttributeDependentStyle(aData, &data, aRestyleHintDataResult);
}

/* virtual */ bool
CascadeLayerRuleProcessor::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  // This is handled by our parent rule processor, so we don't need to do
  // anything here.
  return false;
}

/* virtual */ nsTArray<nsCOMPtr<nsIStyleRuleProcessor>>*
CascadeLayerRuleProcessor::GetChildRuleProcessors()
{
  return nullptr;
}

/* virtual */ size_t
CascadeLayerRuleProcessor::SizeOfExcludingThis(
  mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  // The cascade layer owns the rule cascade, so we don't count it here.
  // We do count the layer itself, though.
  n += mLayer->SizeOfIncludingThis(aMallocSizeOf);

  return n;
}

/* virtual */ size_t
CascadeLayerRuleProcessor::SizeOfIncludingThis(
  mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

nsCSSKeyframesRule*
CascadeLayerRuleProcessor::KeyframesRuleForName(nsPresContext* aPresContext,
                                                const nsString& aName)
{
  if (mCascade) {
    return mCascade->mKeyframesRuleTable.Get(aName);
  }

  return nullptr;
}

nsCSSCounterStyleRule*
CascadeLayerRuleProcessor::CounterStyleRuleForName(
  nsPresContext* aPresContext,
  const nsAString& aName)
{
  if (mCascade) {
    return mCascade->mCounterStyleRuleTable.Get(aName);
  }

  return nullptr;
}

// Append all the currently-active font face rules to aArray.  Return
// true for success and false for failure.
bool
CascadeLayerRuleProcessor::AppendFontFaceRules(
  nsPresContext* aPresContext,
  nsTArray<nsFontFaceRuleContainer>& aArray)
{
  if (mCascade) {
    if (!aArray.AppendElements(mCascade->mFontFaceRules)) {
      return false;
    }
  }

  return true;
}

// Append all the currently-active page rules to aArray.  Return
// true for success and false for failure.
bool
CascadeLayerRuleProcessor::AppendPageRules(nsPresContext* aPresContext,
                                             nsTArray<nsCSSPageRule*>& aArray)
{
  if (mCascade) {
    if (!aArray.AppendElements(mCascade->mPageRules)) {
      return false;
    }
  }

  return true;
}

bool
CascadeLayerRuleProcessor::AppendFontFeatureValuesRules(
  nsPresContext* aPresContext,
  nsTArray<nsCSSFontFeatureValuesRule*>& aArray)
{
  if (mCascade) {
    if (!aArray.AppendElements(mCascade->mFontFeatureValuesRules)) {
      return false;
    }
  }

  return true;
}

CascadeLayerRuleProcessor::~CascadeLayerRuleProcessor()
{
  mCascade = nullptr;
  if (mLayer) {
    delete mLayer;
  }
  mLayer = nullptr;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CascadeLayerRuleProcessor)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRuleProcessor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CascadeLayerRuleProcessor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CascadeLayerRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_CLASS(CascadeLayerRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CascadeLayerRuleProcessor)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CascadeLayerRuleProcessor)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
