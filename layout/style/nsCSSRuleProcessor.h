/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for CSS style sheets, responsible for selector
 * matching and cascading
 */

#ifndef nsCSSRuleProcessor_h_
#define nsCSSRuleProcessor_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefCountType.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "nsExpirationTracker.h"
#include "nsIMediaList.h"
#include "nsIStyleRuleProcessor.h"
#include "nsRuleWalker.h"
#include "nsTArray.h"

struct CascadeLayer;
struct ElementDependentRuleProcessorData;
struct nsFontFaceRuleContainer;
struct RuleProcessorGroup;
class nsCSSKeyframesRule;
class nsCSSPageRule;
class nsCSSFontFeatureValuesRule;
class nsCSSCounterStyleRule;

namespace mozilla {
class CSSStyleSheet;
enum class CSSPseudoElementType : uint8_t;
namespace css {
class DocumentRule;
} // namespace css
} // namespace mozilla

/**
 * The CSS style rule processor provides a mechanism for sibling style
 * sheets to combine their rule processing in order to allow proper
 * cascading to happen.
 *
 * CSS style rule processors keep a live reference on all style sheets
 * bound to them.  The CSS style sheets keep a weak reference to all the
 * processors that they are bound to (many to many).  The CSS style sheet
 * is told when the rule processor is going away (via DropRuleProcessor).
 */

class nsCSSRuleProcessor : public nsIStyleRuleProcessor {
public:
  typedef nsTArray<RefPtr<mozilla::CSSStyleSheet>> sheet_array_type;

  // aScopeElement must be non-null iff aSheetType is
  // SheetType::ScopedDoc.
  // aPreviousCSSRuleProcessor is the rule processor (if any) that this
  // one is replacing.
  nsCSSRuleProcessor(const sheet_array_type& aSheets,
                     mozilla::SheetType aSheetType,
                     mozilla::dom::Element* aScopeElement,
                     nsCSSRuleProcessor* aPreviousCSSRuleProcessor,
                     bool aIsShared = false);
  nsCSSRuleProcessor(sheet_array_type&& aSheets,
                     mozilla::SheetType aSheetType,
                     mozilla::dom::Element* aScopeElement,
                     nsCSSRuleProcessor* aPreviousCSSRuleProcessor,
                     bool aIsShared = false);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsCSSRuleProcessor)

public:
  nsresult ClearGroup();

  // nsIStyleRuleProcessor
  virtual void RulesMatching(ElementRuleProcessorData* aData) override;

  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) override;

  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) override;

#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) override;
#endif

  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) override;

  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) override;

  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                               mozilla::RestyleHintData& aRestyleHintDataResult)
                                 override;

  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) override;

  virtual nsTArray<nsCOMPtr<nsIStyleRuleProcessor>>* GetChildRuleProcessors()
    override;

  /**
   * If this rule processor currently has a substantive media query
   * result cache key, return a copy of it.
   */
  mozilla::UniquePtr<nsMediaQueryResultCacheKey> CloneMQCacheKey();

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE override;

  // Append all the currently-active font face rules to aArray.  Return
  // true for success and false for failure.
  bool AppendFontFaceRules(nsPresContext* aPresContext,
                           nsTArray<nsFontFaceRuleContainer>& aArray);

  nsCSSKeyframesRule* KeyframesRuleForName(nsPresContext* aPresContext,
                                           const nsString& aName);

  nsCSSCounterStyleRule* CounterStyleRuleForName(nsPresContext* aPresContext,
                                                 const nsAString& aName);

  bool AppendPageRules(nsPresContext* aPresContext,
                       nsTArray<nsCSSPageRule*>& aArray);

  bool AppendFontFeatureValuesRules(nsPresContext* aPresContext,
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray);

  /**
   * Returns the scope element for the scoped style sheets this rule
   * processor is for.  If this is not a rule processor for scoped style
   * sheets, it returns null.
   */
  mozilla::dom::Element* GetScopeElement() const { return mScopeElement; }

  void TakeDocumentRulesAndCacheKey(
      nsPresContext* aPresContext,
      nsTArray<mozilla::css::DocumentRule*>& aDocumentRules,
      nsDocumentRuleResultCacheKey& aDocumentRuleResultCacheKey);

  bool IsShared() const { return mIsShared; }

  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  void AddStyleSetRef();
  void ReleaseStyleSetRef();
  void SetInRuleProcessorCache(bool aVal) {
    MOZ_ASSERT(mIsShared);
    mInRuleProcessorCache = aVal;
  }
  bool IsInRuleProcessorCache() const { return mInRuleProcessorCache; }
  bool IsUsedByMultipleStyleSets() const { return mStyleSetRefCnt > 1; }

protected:
  virtual ~nsCSSRuleProcessor();

private:
  static bool CascadeSheet(mozilla::CSSStyleSheet* aSheet,
                           CascadeLayer* aLayer);

  RuleProcessorGroup* GetGroup(nsPresContext* aPresContext);
  void RefreshGroup(nsPresContext* aPresContext);

  nsRestyleHint HasStateDependentStyle(ElementDependentRuleProcessorData* aData,
                                       mozilla::dom::Element* aStatefulElement,
                                       mozilla::CSSPseudoElementType aPseudoType,
                                       mozilla::EventStates aStateMask);

  void ClearSheets();

  // The sheet order here is the same as in nsStyleSet::mSheets
  sheet_array_type mSheets;

  // active first, then cached (most recent first)
  RuleProcessorGroup* mGroup;

  // If we cleared our mGroup or replaced a previous rule
  // processor, this is the media query result cache key that was used
  // before we lost the old group.
  mozilla::UniquePtr<nsMediaQueryResultCacheKey> mPreviousCacheKey;

  // The last pres context for which GetGroup was called.
  nsPresContext *mLastPresContext;

  // The scope element for this rule processor's scoped style sheets.
  // Only used if mSheetType == nsStyleSet::eScopedDocSheet.
  RefPtr<mozilla::dom::Element> mScopeElement;

  nsTArray<mozilla::css::DocumentRule*> mDocumentRules;
  nsDocumentRuleResultCacheKey mDocumentCacheKey;

  nsExpirationState mExpirationState;
  MozRefCountType mStyleSetRefCnt;

  // type of stylesheet using this processor
  mozilla::SheetType mSheetType;

  const bool mIsShared;

  // Whether we need to build up mDocumentCacheKey and mDocumentRules as
  // we build RuleProcessorGroup.  Is true only for shared rule processors
  // and only before we build the first RuleProcessorGroup.  See comment in
  // RefreshGroup for why.
  bool mMustGatherDocumentRules;

  bool mInRuleProcessorCache;

#ifdef DEBUG
  bool mDocumentRulesAndCacheKeyValid;
#endif
};

#endif /* nsCSSRuleProcessor_h_ */
