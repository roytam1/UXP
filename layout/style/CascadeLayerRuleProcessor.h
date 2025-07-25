/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for cascade layers
 */

#ifndef CascadeLayerRuleProcessor_h_
#define CascadeLayerRuleProcessor_h_

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

struct AttributeEnumData;
struct CascadeLayer;
struct RuleCascadeData;
struct ElementDependentRuleProcessorData;
struct nsFontFaceRuleContainer;
struct ResolvedRuleCascades;
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
 * The cascade layer rule processor handles rules collected by the CSS style
 * rule processor within the context of a specific cascade layer. It ensures
 * rules are applied in the correct order, as defined by the CSS specification.
 *
 * It also handles unlayered styles, which are treated as belonging to an
 * implicit layer.
 */
 
class CascadeLayerRuleProcessor : public nsIStyleRuleProcessor
{
public:
  CascadeLayerRuleProcessor(CascadeLayer* aLayer);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(CascadeLayerRuleProcessor)

public:
  // nsIStyleRuleProcessor
  virtual void RulesMatching(ElementRuleProcessorData* aData) override;

  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) override;

  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) override;

  virtual void RulesMatching(XULTreeRuleProcessorData* aData) override;

  virtual nsRestyleHint HasStateDependentStyle(
    StateRuleProcessorData* aData) override;
  virtual nsRestyleHint HasStateDependentStyle(
    PseudoElementStateRuleProcessorData* aData) override;

  virtual bool HasDocumentStateDependentStyle(
    StateRuleProcessorData* aData) override;

  virtual nsRestyleHint HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    mozilla::RestyleHintData& aRestyleHintDataResult) override;

  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) override;

  virtual nsTArray<nsCOMPtr<nsIStyleRuleProcessor>>* GetChildRuleProcessors()
    override;

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
    MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
    MOZ_MUST_OVERRIDE override;

  // The following API methods are consumed by nsStyleSet.

  nsCSSKeyframesRule* KeyframesRuleForName(nsPresContext* aPresContext,
                                           const nsString& aName);

  nsCSSCounterStyleRule* CounterStyleRuleForName(nsPresContext* aPresContext,
                                                 const nsAString& aName);

  bool AppendFontFaceRules(nsPresContext* aPresContext,
                           nsTArray<nsFontFaceRuleContainer>& aArray);

  bool AppendPageRules(nsPresContext* aPresContext,
                       nsTArray<nsCSSPageRule*>& aArray);

  bool AppendFontFeatureValuesRules(
    nsPresContext* aPresContext,
    nsTArray<nsCSSFontFeatureValuesRule*>& aArray);

  // The following API methods are consumed by nsCSSRuleProcessor only.
  nsRestyleHint HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    AttributeEnumData* aEnumData,
    mozilla::RestyleHintData& aRestyleHintDataResult);

  nsRestyleHint HasStateDependentStyle(
    ElementDependentRuleProcessorData* aData,
    mozilla::dom::Element* aStatefulElement,
    mozilla::CSSPseudoElementType aPseudoType,
    mozilla::EventStates aStateMask,
    nsRestyleHint& aHint);

protected:
  virtual ~CascadeLayerRuleProcessor();

  CascadeLayer* mLayer;
  RuleCascadeData* mCascade;

private:
  nsRestyleHint HasStateDependentStyle(
    ElementDependentRuleProcessorData* aData,
    mozilla::dom::Element* aStatefulElement,
    mozilla::CSSPseudoElementType aPseudoType,
    mozilla::EventStates aStateMask);
};

#endif /* CascadeLayerRuleProcessor_h_ */
