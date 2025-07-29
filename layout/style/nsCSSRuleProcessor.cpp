/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for CSS style sheets, responsible for selector
 * matching and cascading
 */

#include "nsAutoPtr.h"
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include <algorithm>
#include "nsIAtom.h"
#include "PLDHashTable.h"
#include "nsICSSPseudoComparator.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/ImportRule.h"
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
#include "RuleProcessorGroup.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------
// CSS Style rule processor implementation
//

nsCSSRuleProcessor::nsCSSRuleProcessor(const sheet_array_type& aSheets,
                                       SheetType aSheetType,
                                       Element* aScopeElement,
                                       nsCSSRuleProcessor*
                                         aPreviousCSSRuleProcessor,
                                       bool aIsShared)
  : nsCSSRuleProcessor(sheet_array_type(aSheets), aSheetType, aScopeElement,
                       aPreviousCSSRuleProcessor, aIsShared)
{
}

nsCSSRuleProcessor::nsCSSRuleProcessor(sheet_array_type&& aSheets,
                                       SheetType aSheetType,
                                       Element* aScopeElement,
                                       nsCSSRuleProcessor*
                                         aPreviousCSSRuleProcessor,
                                       bool aIsShared)
  : mSheets(aSheets)
  , mGroup(nullptr)
  , mPreviousCacheKey(aPreviousCSSRuleProcessor
                       ? aPreviousCSSRuleProcessor->CloneMQCacheKey()
                       : UniquePtr<nsMediaQueryResultCacheKey>())
  , mLastPresContext(nullptr)
  , mScopeElement(aScopeElement)
  , mStyleSetRefCnt(0)
  , mSheetType(aSheetType)
  , mIsShared(aIsShared)
  , mMustGatherDocumentRules(aIsShared)
  , mInRuleProcessorCache(false)
#ifdef DEBUG
  , mDocumentRulesAndCacheKeyValid(false)
#endif
{
  NS_ASSERTION(!!mScopeElement == (aSheetType == SheetType::ScopedDoc),
               "aScopeElement must be specified iff aSheetType is "
               "eScopedDocSheet");
  for (sheet_array_type::size_type i = mSheets.Length(); i-- != 0; ) {
    mSheets[i]->AddRuleProcessor(this);
  }
}

nsCSSRuleProcessor::~nsCSSRuleProcessor()
{
  if (mInRuleProcessorCache) {
    RuleProcessorCache::RemoveRuleProcessor(this);
  }
  MOZ_ASSERT(!mExpirationState.IsTracked());
  MOZ_ASSERT(mStyleSetRefCnt == 0);
  ClearSheets();
  ClearGroup();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSRuleProcessor)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRuleProcessor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSRuleProcessor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCSSRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCSSRuleProcessor)
  tmp->ClearSheets();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScopeElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCSSRuleProcessor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScopeElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void
nsCSSRuleProcessor::ClearSheets()
{
  for (sheet_array_type::size_type i = mSheets.Length(); i-- != 0; ) {
    mSheets[i]->DropRuleProcessor(this);
  }
  mSheets.Clear();
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(ElementRuleProcessorData *aData)
{
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      processor->RulesMatching(aData);
    }
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      processor->RulesMatching(aData);
    }
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      processor->RulesMatching(aData);
    }
  }
}

#ifdef MOZ_XUL
/* virtual */ void
nsCSSRuleProcessor::RulesMatching(XULTreeRuleProcessorData* aData)
{
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      processor->RulesMatching(aData);
    }
  }
}
#endif

nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(ElementDependentRuleProcessorData* aData,
                                           Element* aStatefulElement,
                                           CSSPseudoElementType aPseudoType,
                                           EventStates aStateMask)
{
  MOZ_ASSERT(!aData->mTreeMatchContext.mForScopedStyle,
             "mCurrentStyleScope will need to be saved and restored after the "
             "SelectorMatchesTree call");

  nsRestyleHint hint = nsRestyleHint(0);
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      layerProcessor->HasStateDependentStyle(
        aData, aStatefulElement, aPseudoType, aStateMask, hint);
    }
  }
  return hint;
}

/* virtual */ nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return HasStateDependentStyle(aData,
                                aData->mElement,
                                CSSPseudoElementType::NotPseudo,
                                aData->mStateMask);
}

/* virtual */ nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return HasStateDependentStyle(aData,
                                aData->mPseudoElement,
                                aData->mPseudoType,
                                aData->mStateMask);
}

/* virtual */ bool
nsCSSRuleProcessor::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      if (processor->HasDocumentStateDependentStyle(aData)) {
        return true;
      }
    }
  }

  return false;
}

nsRestyleHint
nsCSSRuleProcessor::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
{
  AttributeEnumData data(aData, aRestyleHintDataResult);
  if (RuleProcessorGroup* group = GetGroup(aData->mPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      layerProcessor->HasAttributeDependentStyle(
        aData, &data, aRestyleHintDataResult);
    }
  }
  return data.change;
}

/* virtual */ bool
nsCSSRuleProcessor::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  // We don't want to do anything if there aren't any sets of rules
  // cached yet, since we should not build the rule cascade too early
  // (e.g., before we know whether the quirk style sheet should be
  // enabled).  And if there's nothing cached, it doesn't matter if
  // anything changed.  But in the cases where it does matter, we've
  // cached a previous cache key to test against, instead of our current
  // rule cascades.  See bug 448281 and bug 1089417.
  MOZ_ASSERT(!(mGroup && mPreviousCacheKey));
  RuleProcessorGroup* old = mGroup;
  if (old) {  
    RefreshGroup(aPresContext);
    return (old != mGroup);
  }

  if (mPreviousCacheKey) {
    // RefreshGroup will get rid of mPreviousCacheKey anyway to
    // maintain the invariant that we can't have both an mGroup
    // and an mPreviousCacheKey.  But we need to hold it a little
    // longer.
    UniquePtr<nsMediaQueryResultCacheKey> previousCacheKey(
      Move(mPreviousCacheKey));
    RefreshGroup(aPresContext);

    // This test is a bit pessimistic since the cache key's operator==
    // just does list comparison rather than set comparison, but it
    // should catch all the cases we care about (i.e., where the cascade
    // order hasn't changed).  Other cases will do a restyle anyway, so
    // we shouldn't need to worry about posting a second.
    return !mGroup || // all sheets gone, but we had sheets before
           mGroup->mCacheKey != *previousCacheKey;
  }

  return false;
}

/* virtual */ nsTArray<nsCOMPtr<nsIStyleRuleProcessor>>*
nsCSSRuleProcessor::GetChildRuleProcessors()
{
  return mGroup
    ? &mGroup->mItems
    : nullptr;
}

UniquePtr<nsMediaQueryResultCacheKey>
nsCSSRuleProcessor::CloneMQCacheKey()
{
  MOZ_ASSERT(!(mGroup && mPreviousCacheKey));

  RuleProcessorGroup* c = mGroup;
  if (!c) {
    // We might have an mPreviousCacheKey.  It already comes from a call
    // to CloneMQCacheKey, so don't bother checking
    // HasFeatureConditions().
    if (mPreviousCacheKey) {
      NS_ASSERTION(mPreviousCacheKey->HasFeatureConditions(),
                   "we shouldn't have a previous cache key unless it has "
                   "feature conditions");
      return MakeUnique<nsMediaQueryResultCacheKey>(*mPreviousCacheKey);
    }

    return UniquePtr<nsMediaQueryResultCacheKey>();
  }

  if (!c->mCacheKey.HasFeatureConditions()) {
    return UniquePtr<nsMediaQueryResultCacheKey>();
  }

  return MakeUnique<nsMediaQueryResultCacheKey>(c->mCacheKey);
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mSheets.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (RuleProcessorGroup* group = mGroup; group;
       group = group->mNext) {
    n += group->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

bool
nsCSSRuleProcessor::AppendFontFaceRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsFontFaceRuleContainer>& aArray)
{
  if (RuleProcessorGroup* group = GetGroup(aPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      if (!layerProcessor->AppendFontFaceRules(aPresContext, aArray)) {
        return false;
      }
    }
  }

  return true;
}

nsCSSKeyframesRule*
nsCSSRuleProcessor::KeyframesRuleForName(nsPresContext* aPresContext,
                                         const nsString& aName)
{
  nsCSSKeyframesRule* rule = nullptr;

  if (RuleProcessorGroup* group = GetGroup(aPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      if (nsCSSKeyframesRule* newRule =
            layerProcessor->KeyframesRuleForName(aPresContext, aName)) {
        rule = newRule;
      }
    }
  }

  return rule;
}

nsCSSCounterStyleRule*
nsCSSRuleProcessor::CounterStyleRuleForName(nsPresContext* aPresContext,
                                            const nsAString& aName)
{
  nsCSSCounterStyleRule* rule = nullptr;
  if (RuleProcessorGroup* group = GetGroup(aPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      if (nsCSSCounterStyleRule* newRule =
            layerProcessor->CounterStyleRuleForName(aPresContext, aName)) {
        rule = newRule;
      }
    }
  }

  return rule;
}

bool
nsCSSRuleProcessor::AppendPageRules(
                              nsPresContext* aPresContext,
                              nsTArray<nsCSSPageRule*>& aArray)
{
  if (RuleProcessorGroup* group = GetGroup(aPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      if (!layerProcessor->AppendPageRules(aPresContext, aArray)) {
        return false;
      }
    }
  }

  return true;
}

bool
nsCSSRuleProcessor::AppendFontFeatureValuesRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray)
{
  if (RuleProcessorGroup* group = GetGroup(aPresContext)) {
    for (nsCOMPtr<nsIStyleRuleProcessor> processor : group->mItems) {
      CascadeLayerRuleProcessor* layerProcessor =
        static_cast<CascadeLayerRuleProcessor*>(processor.get());
      if (!layerProcessor->AppendFontFeatureValuesRules(aPresContext, aArray)) {
        return false;
      }
    }
  }

  return true;
}

nsresult
nsCSSRuleProcessor::ClearGroup()
{
  if (!mPreviousCacheKey) {
    mPreviousCacheKey = CloneMQCacheKey();
  }

  // No need to remove the rule processor from the RuleProcessorCache here,
  // since CSSStyleSheet::ClearGroup will have called
  // RuleProcessorCache::RemoveSheet() passing itself, which will catch
  // this rule processor (and any others for different @-moz-document
  // cache key results).
  MOZ_ASSERT(!RuleProcessorCache::HasRuleProcessor(this));

#ifdef DEBUG
  // For shared rule processors, if we've already gathered document
  // rules, then they will now be out of date.  We don't actually need
  // them to be up-to-date (see the comment in RefreshGroup), so
  // record their invalidity so we can assert if we try to use them.
  if (!mMustGatherDocumentRules) {
    mDocumentRulesAndCacheKeyValid = false;
  }
#endif

  // We rely on our caller (perhaps indirectly) to do something that
  // will rebuild style data and the user font set (either
  // nsIPresShell::RestyleForCSSRuleChanges or
  // nsPresContext::RebuildAllStyleData).
  RuleProcessorGroup* data = mGroup;
  mGroup = nullptr;
  while (data) {
    RuleProcessorGroup* next = data->mNext;
    delete data;
    data = next;
  }
  return NS_OK;
}


/**
 * Recursively traverses rules in order to:
 *  (1) add any @-moz-document rules into data->mDocumentRules.
 *  (2) record any @-moz-document rules whose conditions evaluate to true
 *      on data->mDocumentCacheKey.
 *
 * See also CascadeRuleEnumFunc below, which calls us via
 * EnumerateRulesForwards.  If modifying this function you may need to
 * update CascadeRuleEnumFunc too.
 */
static bool
GatherDocRuleEnumFunc(css::Rule* aRule, void* aData)
{
  CascadeLayer* layer = (CascadeLayer*)aData;
  int32_t type = aRule->GetType();

  MOZ_ASSERT(layer->mMustGatherDocumentRules,
             "should only call GatherDocRuleEnumFunc if "
             "mMustGatherDocumentRules is true");

  if (css::Rule::MEDIA_RULE == type || css::Rule::SUPPORTS_RULE == type ||
      css::Rule::LAYER_BLOCK_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    if (!groupRule->EnumerateRulesForwards(GatherDocRuleEnumFunc, aData)) {
      return false;
    }
  } else if (css::Rule::DOCUMENT_RULE == type) {
    css::DocumentRule* docRule = static_cast<css::DocumentRule*>(aRule);
    if (!layer->mDocumentRules.AppendElement(docRule)) {
      return false;
    }
    if (docRule->UseForPresentation(layer->mPresContext)) {
      if (!layer->mDocumentCacheKey.AddMatchingRule(docRule)) {
        return false;
      }
    }
    if (!docRule->EnumerateRulesForwards(GatherDocRuleEnumFunc, aData)) {
      return false;
    }
  }
  return true;
}

/*
 * This enumerates style rules in a sheet (and recursively into any
 * grouping rules) in order to:
 *  (1) add any style rules, in order, into data->mRulesByWeight (for
 *      the primary CSS cascade), where they are separated by weight
 *      but kept in order per-weight, and
 *  (2) add any @font-face rules, in order, into data->mFontFaceRules.
 *  (3) add any @keyframes rules, in order, into data->mKeyframesRules.
 *  (4) add any @font-feature-value rules, in order,
 *      into data->mFontFeatureValuesRules.
 *  (5) add any @page rules, in order, into data->mPageRules.
 *  (6) add any @counter-style rules, in order, into data->mCounterStyleRules.
 *  (7) add any @-moz-document rules into data->mDocumentRules.
 *  (8) record any @-moz-document rules whose conditions evaluate to true
 *      on data->mDocumentCacheKey.
 *
 * See also GatherDocRuleEnumFunc above, which we call to traverse into
 * @-moz-document rules even if their (or an ancestor's) condition
 * fails.  This means we might look at the result of some @-moz-document
 * rules that don't actually affect whether a RuleProcessorCache lookup
 * is a hit or a miss.  The presence of @-moz-document rules inside
 * @media etc. rules should be rare, and looking at all of them in the
 * sheets lets us avoid the complication of having different document
 * cache key results for different media.
 *
 * If modifying this function you may need to update
 * GatherDocRuleEnumFunc too.
 */
static bool
CascadeRuleEnumFunc(css::Rule* aRule, void* aData)
{
  CascadeLayer* layer = (CascadeLayer*)aData;
  int32_t type = aRule->GetType();

  if (css::Rule::STYLE_RULE == type) {
    css::StyleRule* styleRule = static_cast<css::StyleRule*>(aRule);
    if (!layer->mStyleRules.AppendElement(styleRule)) {
      return false;
    }
  } else if (css::Rule::MEDIA_RULE == type ||
             css::Rule::SUPPORTS_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    const bool use = groupRule->UseForPresentation(layer->mPresContext,
                                                   layer->mCacheKey);
    if (use || layer->mMustGatherDocumentRules) {
      if (!groupRule->EnumerateRulesForwards(
            use ? CascadeRuleEnumFunc : GatherDocRuleEnumFunc, aData)) {
        return false;
      }
    }
  } else if (css::Rule::LAYER_STATEMENT_RULE == type) {
    CSSLayerStatementRule* layerRule =
      static_cast<CSSLayerStatementRule*>(aRule);
    nsTArray<nsTArray<nsString>> pathList;
    layerRule->GetPathList(pathList);
    for (const nsTArray<nsString>& path : pathList) {
      layer->CreateNamedChildLayer(path);
    }
  } else if (css::Rule::LAYER_BLOCK_RULE == type) {
    CSSLayerBlockRule* layerRule = static_cast<CSSLayerBlockRule*>(aRule);
    nsTArray<nsString> path;
    layerRule->GetPath(path);
    nsString name;
    layerRule->GetName(name);
    CascadeLayer* targetLayer = name.IsEmpty()
                                  ? layer->CreateAnonymousChildLayer()
                                  : layer->CreateNamedChildLayer(path);
    const bool use = layerRule->UseForPresentation(layer->mPresContext,
                                                   layer->mCacheKey);
    if (use || layer->mMustGatherDocumentRules) {
      if (!layerRule->EnumerateRulesForwards(
            use ? CascadeRuleEnumFunc : GatherDocRuleEnumFunc, targetLayer)) {
        return false;
      }
    }
  } else if (css::Rule::DOCUMENT_RULE == type) {
    css::DocumentRule* docRule = static_cast<css::DocumentRule*>(aRule);
    if (layer->mMustGatherDocumentRules) {
      if (!layer->mDocumentRules.AppendElement(docRule)) {
        return false;
      }
    }
    const bool use = docRule->UseForPresentation(layer->mPresContext);
    if (use && layer->mMustGatherDocumentRules) {
      if (!layer->mDocumentCacheKey.AddMatchingRule(docRule)) {
        return false;
      }
    }
    if (use || layer->mMustGatherDocumentRules) {
      if (!docRule->EnumerateRulesForwards(
            use ? CascadeRuleEnumFunc : GatherDocRuleEnumFunc, aData)) {
        return false;
      }
    }
  } else if (css::Rule::FONT_FACE_RULE == type) {
    nsCSSFontFaceRule* fontFaceRule = static_cast<nsCSSFontFaceRule*>(aRule);
    nsFontFaceRuleContainer* ptr = layer->mData->mFontFaceRules.AppendElement();
    if (!ptr)
      return false;
    ptr->mRule = fontFaceRule;
    ptr->mSheetType = layer->mSheetType;
  } else if (css::Rule::KEYFRAMES_RULE == type) {
    nsCSSKeyframesRule* keyframesRule = static_cast<nsCSSKeyframesRule*>(aRule);
    if (!layer->mData->mKeyframesRules.AppendElement(keyframesRule)) {
      return false;
    }
  } else if (css::Rule::FONT_FEATURE_VALUES_RULE == type) {
    nsCSSFontFeatureValuesRule* fontFeatureValuesRule =
      static_cast<nsCSSFontFeatureValuesRule*>(aRule);
    if (!layer->mData->mFontFeatureValuesRules.AppendElement(
          fontFeatureValuesRule)) {
      return false;
    }
  } else if (css::Rule::PAGE_RULE == type) {
    nsCSSPageRule* pageRule = static_cast<nsCSSPageRule*>(aRule);
    if (!layer->mData->mPageRules.AppendElement(pageRule)) {
      return false;
    }
  } else if (css::Rule::COUNTER_STYLE_RULE == type) {
    nsCSSCounterStyleRule* counterStyleRule =
      static_cast<nsCSSCounterStyleRule*>(aRule);
    if (!layer->mData->mCounterStyleRules.AppendElement(counterStyleRule)) {
      return false;
    }
  } else if (css::Rule::IMPORT_RULE == type &&
             nsCSSRuleUtils::LoadImportedSheetsInOrderEnabled()) {
    css::ImportRule* importRule = static_cast<css::ImportRule*>(aRule);
    nsCSSRuleProcessor::CascadeSheet(
      importRule->GetStyleSheet()->AsConcrete(),
      layer);
  }
  return true;
}

/* static */ bool
nsCSSRuleProcessor::CascadeSheet(CSSStyleSheet* aSheet, CascadeLayer* aLayer)
{
  if (aSheet->IsApplicable() &&
      aSheet->UseForPresentation(aLayer->mPresContext,
                                 aLayer->mCacheKey) &&
      aSheet->mInner) {
    if (!nsCSSRuleUtils::LoadImportedSheetsInOrderEnabled()) {
      CSSStyleSheet* child = aSheet->mInner->mFirstChild;
      while (child) {
        CascadeSheet(child, aLayer);
        child = child->mNext;
      }
    }

    if (!aSheet->mInner->mOrderedRules.EnumerateForwards(CascadeRuleEnumFunc,
                                                         aLayer))
      return false;
  }
  return true;
}

RuleProcessorGroup*
nsCSSRuleProcessor::GetGroup(nsPresContext* aPresContext)
{
  // FIXME:  Make this infallible!

  // If anything changes about the presentation context, we will be
  // notified.  Otherwise, our cache is valid if mLastPresContext
  // matches aPresContext.  (The only rule processors used for multiple
  // pres contexts are for XBL.  These rule processors are probably less
  // likely to have @media rules, and thus the cache is pretty likely to
  // hit instantly even when we're switching between pres contexts.)

  if (!mGroup || aPresContext != mLastPresContext) {
    RefreshGroup(aPresContext);
  }
  mLastPresContext = aPresContext;

  return mGroup;
}

/**
 * This enumerates layers in a sheet and creates child rule processors
 * for each layer. The child rule processors are created in the order
 * that the layers are encountered.
 */
static void
CreateChildProcessorsEnumFunc(CascadeLayer* aLayer, void* aData)
{
  aLayer->AddRules();
  RuleProcessorGroup* data = static_cast<RuleProcessorGroup*>(aData);
  data->mItems.AppendElement(new CascadeLayerRuleProcessor(aLayer));
}

void
nsCSSRuleProcessor::RefreshGroup(nsPresContext* aPresContext)
{
  // Having RuleCascadeData objects be per-medium (over all variation
  // caused by media queries, handled through mCacheKey) works for now
  // since nsCSSRuleProcessor objects are per-document.  (For a given
  // set of stylesheets they can vary based on medium (@media) or
  // document (@-moz-document).)
  for (RuleProcessorGroup **groupPointer = &mGroup, *group;
       (group = *groupPointer);
       groupPointer = &group->mNext) {
    if (group->mCacheKey.Matches(aPresContext)) {
      // Ensure that the current one is always mGroup.
      *groupPointer = group->mNext;
      group->mNext = mGroup;
      mGroup = group;

      return;
    }
  }

  // We're going to make a new rule cascade; this means that we should
  // now stop using the previous cache key that we're holding on to from
  // the last time we had rule cascades.
  mPreviousCacheKey = nullptr;

  if (mSheets.Length() != 0) {
    nsAutoPtr<RuleProcessorGroup> ruleProcessorSet(
      new RuleProcessorGroup(aPresContext->Medium()));
    CascadeLayer* implicitLayer(new CascadeLayer(aPresContext,
#if DEBUG
                                                 nullptr,
#endif
                                                 mDocumentRules,
                                                 mDocumentCacheKey,
                                                 mSheetType,
                                                 mMustGatherDocumentRules,
                                                 ruleProcessorSet->mCacheKey));
    if (implicitLayer->mData) {
      for (uint32_t i = 0; i < mSheets.Length(); ++i) {
        if (!CascadeSheet(mSheets.ElementAt(i), implicitLayer)) {
          return; /* out of memory */
        }
      }

      // Ensure that the current one is always mGroup.
      ruleProcessorSet->mNext = mGroup;
      mGroup = ruleProcessorSet.forget();

      implicitLayer->EnumerateAllLayers(CreateChildProcessorsEnumFunc, mGroup);

      // mMustGatherDocumentRules controls whether we build mDocumentRules
      // and mDocumentCacheKey so that they can be used as keys by the
      // RuleProcessorCache, as obtained by TakeDocumentRulesAndCacheKey
      // later.  We set it to false just below so that we only do this
      // the first time we build a RuleProcessorCache for a shared rule
      // processor.
      //
      // An up-to-date mDocumentCacheKey is only needed if we
      // are still in the RuleProcessorCache (as we store a copy of the
      // cache key in the RuleProcessorCache), and an up-to-date
      // mDocumentRules is only needed at the time TakeDocumentRulesAndCacheKey
      // is called, which is immediately after the rule processor is created
      // (by nsStyleSet).
      //
      // Note that when nsCSSRuleProcessor::ClearGroup is called,
      // by CSSStyleSheet::ClearGroup, we will have called
      // RuleProcessorCache::RemoveSheet, which will remove the rule
      // processor from the cache.  (This is because the list of document
      // rules now may not match the one used as they key in the
      // RuleProcessorCache.)
      //
      // Thus, as we'll no longer be in the RuleProcessorCache, and we won't
      // have TakeDocumentRulesAndCacheKey called on us, we don't need to ensure
      // mDocumentCacheKey and mDocumentRules are up-to-date after the
      // first time GetGroup is called.
      if (mMustGatherDocumentRules) {
        mDocumentRules.Sort();
        mDocumentCacheKey.Finalize();
        mMustGatherDocumentRules = false;
#ifdef DEBUG
        mDocumentRulesAndCacheKeyValid = true;
#endif
      }
    }
  }
  return;
}

void
nsCSSRuleProcessor::TakeDocumentRulesAndCacheKey(
    nsPresContext* aPresContext,
    nsTArray<css::DocumentRule*>& aDocumentRules,
    nsDocumentRuleResultCacheKey& aCacheKey)
{
  MOZ_ASSERT(mIsShared);

  GetGroup(aPresContext);
  MOZ_ASSERT(mDocumentRulesAndCacheKeyValid);

  aDocumentRules.Clear();
  aDocumentRules.SwapElements(mDocumentRules);
  aCacheKey.Swap(mDocumentCacheKey);

#ifdef DEBUG
  mDocumentRulesAndCacheKeyValid = false;
#endif
}

void
nsCSSRuleProcessor::AddStyleSetRef()
{
  MOZ_ASSERT(mIsShared);
  if (++mStyleSetRefCnt == 1) {
    RuleProcessorCache::StopTracking(this);
  }
}

void
nsCSSRuleProcessor::ReleaseStyleSetRef()
{
  MOZ_ASSERT(mIsShared);
  MOZ_ASSERT(mStyleSetRefCnt > 0);
  if (--mStyleSetRefCnt == 0 && mInRuleProcessorCache) {
    RuleProcessorCache::StartTracking(this);
  }
}
