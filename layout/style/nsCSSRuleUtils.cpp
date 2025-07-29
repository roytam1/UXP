/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCSSRuleUtils.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIMozBrowserFrame.h"
#include "nsRuleWalker.h"
#include "nsStyleUtil.h"
#include "StyleRule.h"

using namespace mozilla;
using namespace mozilla::dom;

#define VISITED_PSEUDO_PREF "layout.css.visited_links_enabled"

static bool gSupportVisitedPseudo = true;
static bool gLoadImportedSheetsInOrder = true;

static nsTArray<nsCOMPtr<nsIAtom>>* sSystemMetrics = 0;

#ifdef XP_WIN
uint8_t nsCSSRuleUtils::sWinThemeId = LookAndFeel::eWindowsTheme_Generic;
#endif

/* static */ void
nsCSSRuleUtils::Startup()
{
  Preferences::AddBoolVarCache(
    &gSupportVisitedPseudo, VISITED_PSEUDO_PREF, true);
  Preferences::AddBoolVarCache(&gLoadImportedSheetsInOrder,
                               "layout.css.load-imported-sheets-in-order",
                               true);
}

static bool
InitSystemMetrics()
{
  NS_ASSERTION(!sSystemMetrics, "already initialized");

  sSystemMetrics = new nsTArray<nsCOMPtr<nsIAtom>>;
  NS_ENSURE_TRUE(sSystemMetrics, false);

  /***************************************************************************
   * ANY METRICS ADDED HERE SHOULD ALSO BE ADDED AS MEDIA QUERIES IN         *
   * nsMediaFeatures.cpp                                                     *
   ***************************************************************************/

  int32_t metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollArrowStyle);
  if (metricResult & LookAndFeel::eScrollArrow_StartBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_StartForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_forward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_forward);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollSliderStyle);
  if (metricResult != LookAndFeel::eScrollThumbStyle_Normal) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_thumb_proportional);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::overlay_scrollbars);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::eIntID_MenuBarDrag);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::menubar_drag);
  }

  nsresult rv =
    LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsDefaultTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_default_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacGraphiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_graphite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacLionTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_lion_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacYosemiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_yosemite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsAccentColorApplies,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_accent_color_applies);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsAccentColorIsDark,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_accent_color_is_dark);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_ColorPickerAvailable,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::color_picker_available);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::touch_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_SwipeAnimationEnabled,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::swipe_animation_enabled);
  }

  rv =
    LookAndFeel::GetInt(LookAndFeel::eIntID_PhysicalHomeButton, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::physical_home_button);
  }

#ifdef XP_WIN
  if (NS_SUCCEEDED(LookAndFeel::GetInt(
        LookAndFeel::eIntID_WindowsThemeIdentifier, &metricResult))) {
    nsCSSRuleUtils::SetWindowsThemeIdentifier(
      static_cast<uint8_t>(metricResult));
    switch (metricResult) {
      case LookAndFeel::eWindowsTheme_Aero:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero);
        break;
      case LookAndFeel::eWindowsTheme_AeroLite:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero_lite);
        break;
      case LookAndFeel::eWindowsTheme_LunaBlue:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_blue);
        break;
      case LookAndFeel::eWindowsTheme_LunaOlive:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_olive);
        break;
      case LookAndFeel::eWindowsTheme_LunaSilver:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_silver);
        break;
      case LookAndFeel::eWindowsTheme_Royale:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_royale);
        break;
      case LookAndFeel::eWindowsTheme_Zune:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_zune);
        break;
      case LookAndFeel::eWindowsTheme_Generic:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_generic);
        break;
    }
  }
#endif

  return true;
}

/* static */ void
nsCSSRuleUtils::FreeSystemMetrics()
{
  delete sSystemMetrics;
  sSystemMetrics = nullptr;
}

/* static */ void
nsCSSRuleUtils::Shutdown()
{
  FreeSystemMetrics();
}

/* static */ bool
nsCSSRuleUtils::HasSystemMetric(nsIAtom* aMetric)
{
  if (!sSystemMetrics && !InitSystemMetrics()) {
    return false;
  }
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

/* static */ bool
nsCSSRuleUtils::LoadImportedSheetsInOrderEnabled()
{
  return gLoadImportedSheetsInOrder;
}

#ifdef XP_WIN
/* static */ uint8_t
nsCSSRuleUtils::GetWindowsThemeIdentifier()
{
  if (!sSystemMetrics)
    InitSystemMetrics();
  return sWinThemeId;
}
#endif

/* static */ bool
nsCSSRuleUtils::RestrictedSelectorListMatches(
  Element* aElement,
  TreeMatchContext& aTreeMatchContext,
  nsCSSSelectorList* aSelectorList)
{
  MOZ_ASSERT(!aTreeMatchContext.mForScopedStyle,
             "mCurrentStyleScope will need to be saved and restored after the "
             "SelectorMatchesTree call");

  NodeMatchContext nodeContext(EventStates(), false);
  SelectorMatchesFlags flags = aElement->IsInShadowTree()
                                 ? SelectorMatchesFlags::NONE
                                 : SelectorMatchesFlags::IS_OUTSIDE_SHADOW_TREE;
  return SelectorListMatches(
    aElement, aSelectorList, nodeContext, aTreeMatchContext, flags);
}

/* static */ EventStates
nsCSSRuleUtils::GetContentState(Element* aElement,
                                const TreeMatchContext& aTreeMatchContext)
{
  EventStates state = aElement->StyleState();

  // If we are not supposed to mark visited links as such, be sure to
  // flip the bits appropriately.  We want to do this here, rather
  // than in GetContentStateForVisitedHandling, so that we don't
  // expose that :visited support is disabled to the Web page.
  if (state.HasState(NS_EVENT_STATE_VISITED) &&
      (!gSupportVisitedPseudo || aElement->OwnerDoc()->IsBeingUsedAsImage() ||
       aTreeMatchContext.mUsingPrivateBrowsing)) {
    state &= ~NS_EVENT_STATE_VISITED;
    state |= NS_EVENT_STATE_UNVISITED;
  }
  return state;
}

/* static */ bool
nsCSSRuleUtils::IsLink(const Element* aElement)
{
  EventStates state = aElement->StyleState();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                     NS_EVENT_STATE_UNVISITED);
}

/* static */ EventStates
nsCSSRuleUtils::GetContentStateForVisitedHandling(
  Element* aElement,
  const TreeMatchContext& aTreeMatchContext,
  nsRuleWalker::VisitedHandlingType aVisitedHandling,
  bool aIsRelevantLink)
{
  EventStates contentState = GetContentState(aElement, aTreeMatchContext);
  if (contentState.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                         NS_EVENT_STATE_UNVISITED)) {
    MOZ_ASSERT(IsLink(aElement), "IsLink() should match state");
    contentState &= ~(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED);
    if (aIsRelevantLink) {
      switch (aVisitedHandling) {
        case nsRuleWalker::eRelevantLinkUnvisited:
          contentState |= NS_EVENT_STATE_UNVISITED;
          break;
        case nsRuleWalker::eRelevantLinkVisited:
          contentState |= NS_EVENT_STATE_VISITED;
          break;
        case nsRuleWalker::eLinksVisitedOrUnvisited:
          contentState |= NS_EVENT_STATE_UNVISITED | NS_EVENT_STATE_VISITED;
          break;
      }
    } else {
      contentState |= NS_EVENT_STATE_UNVISITED;
    }
  }
  return contentState;
}

/* static */ bool
nsCSSRuleUtils::RestrictedSelectorMatches(
  Element* aElement,
  nsCSSSelector* aSelector,
  TreeMatchContext& aTreeMatchContext)
{
  MOZ_ASSERT(aSelector->IsRestrictedSelector(),
             "aSelector must not have a pseudo-element");

  NS_WARNING_ASSERTION(
    !HasPseudoClassSelectorArgsWithCombinators(aSelector),
    "processing eRestyle_SomeDescendants can be slow if pseudo-classes with "
    "selector arguments can now have combinators in them");

  // We match aSelector as if :visited and :link both match visited and
  // unvisited links.

  NodeMatchContext nodeContext(EventStates(),
                               nsCSSRuleUtils::IsLink(aElement));
  if (nodeContext.mIsRelevantLink) {
    aTreeMatchContext.SetHaveRelevantLink();
  }
  aTreeMatchContext.ResetForUnvisitedMatching();
  bool matches = SelectorMatches(aElement,
                                 aSelector,
                                 nodeContext,
                                 aTreeMatchContext,
                                 SelectorMatchesFlags::NONE);
  if (nodeContext.mIsRelevantLink) {
    aTreeMatchContext.ResetForVisitedMatching();
    if (SelectorMatches(aElement,
                        aSelector,
                        nodeContext,
                        aTreeMatchContext,
                        SelectorMatchesFlags::NONE)) {
      matches = true;
    }
  }
  return matches;
}

// Return whether the selector matches conditions for the :active and
// :hover quirk.
static inline bool
ActiveHoverQuirkMatches(nsCSSSelector* aSelector,
                        SelectorMatchesFlags aSelectorFlags)
{
  if (aSelector->HasTagSelector() || aSelector->mAttrList ||
      aSelector->mIDList || aSelector->mClassList ||
      aSelector->IsPseudoElement() || aSelector->IsHybridPseudoElement() ||
      // Having this quirk means that some selectors will no longer match,
      // so it's better to return false when we aren't sure (i.e., the
      // flags are unknown).
      aSelectorFlags & (SelectorMatchesFlags::UNKNOWN |
                        SelectorMatchesFlags::HAS_PSEUDO_ELEMENT |
                        SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT |
                        SelectorMatchesFlags::IS_OUTSIDE_SHADOW_TREE)) {
    return false;
  }

  // No pseudo-class other than :active and :hover.
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass;
       pseudoClass = pseudoClass->mNext) {
    if (pseudoClass->mType != CSSPseudoClassType::hover &&
        pseudoClass->mType != CSSPseudoClassType::active) {
      return false;
    }
  }

  return true;
}

// This function is to be called once we have fetched a value for an attribute
// whose namespace and name match those of aAttrSelector.  This function
// performs comparisons on the value only, based on aAttrSelector->mFunction.
static bool
AttrMatchesValue(const nsAttrSelector* aAttrSelector,
                 const nsString& aValue,
                 bool isHTML)
{
  NS_PRECONDITION(aAttrSelector, "Must have an attribute selector");

  // http://lists.w3.org/Archives/Public/www-style/2008Apr/0038.html
  // *= (CONTAINSMATCH) ~= (INCLUDES) ^= (BEGINSMATCH) $= (ENDSMATCH)
  // all accept the empty string, but match nothing.
  if (aAttrSelector->mValue.IsEmpty() &&
      (aAttrSelector->mFunction == NS_ATTR_FUNC_INCLUDES ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_ENDSMATCH ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_BEGINSMATCH ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_CONTAINSMATCH))
    return false;

  const nsDefaultStringComparator defaultComparator;
  const nsASCIICaseInsensitiveStringComparator ciComparator;
  const nsStringComparator& comparator =
    aAttrSelector->IsValueCaseSensitive(isHTML)
      ? static_cast<const nsStringComparator&>(defaultComparator)
      : static_cast<const nsStringComparator&>(ciComparator);

  switch (aAttrSelector->mFunction) {
    case NS_ATTR_FUNC_EQUALS:
      return aValue.Equals(aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_INCLUDES:
      return nsStyleUtil::ValueIncludes(
        aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_DASHMATCH:
      return nsStyleUtil::DashMatchCompare(
        aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_ENDSMATCH:
      return StringEndsWith(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_BEGINSMATCH:
      return StringBeginsWith(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_CONTAINSMATCH:
      return FindInReadable(aAttrSelector->mValue, aValue, comparator);
    default:
      NS_NOTREACHED("Shouldn't be ending up here");
      return false;
  }
}

static inline bool
edgeChildMatches(Element* aElement,
                 TreeMatchContext& aTreeMatchContext,
                 bool checkFirst,
                 bool checkLast)
{
  nsIContent* parent = aElement->GetParent();
  if (parent && aTreeMatchContext.mForStyling)
    parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

  return (!checkFirst || aTreeMatchContext.mNthIndexCache.GetNthIndex(
                           aElement, false, false, true) == 1) &&
         (!checkLast || aTreeMatchContext.mNthIndexCache.GetNthIndex(
                          aElement, false, true, true) == 1);
}

static inline bool
nthChildGenericMatches(Element* aElement,
                       TreeMatchContext& aTreeMatchContext,
                       nsPseudoClassList* pseudoClass,
                       bool isOfType,
                       bool isFromEnd)
{
  nsIContent* parent = aElement->GetParent();
  if (parent && aTreeMatchContext.mForStyling) {
    if (isFromEnd)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  const int32_t index = aTreeMatchContext.mNthIndexCache.GetNthIndex(
    aElement, isOfType, isFromEnd, false);
  if (index <= 0) {
    // Node is anonymous content (not really a child of its parent).
    return false;
  }

  const int32_t a = pseudoClass->u.mNumbers[0];
  const int32_t b = pseudoClass->u.mNumbers[1];
  // result should be true if there exists n >= 0 such that
  // a * n + b == index.
  if (a == 0) {
    return b == index;
  }

  // Integer division in C does truncation (towards 0).  So
  // check that the result is nonnegative, and that there was no
  // truncation.
  const CheckedInt<int32_t> indexMinusB = CheckedInt<int32_t>(index) - b;
  const CheckedInt<int32_t> n = indexMinusB / a;
  return n.isValid() && n.value() >= 0 && a * n == indexMinusB;
}

static inline bool
edgeOfTypeMatches(Element* aElement,
                  TreeMatchContext& aTreeMatchContext,
                  bool checkFirst,
                  bool checkLast)
{
  nsIContent* parent = aElement->GetParent();
  if (parent && aTreeMatchContext.mForStyling) {
    if (checkLast)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  return (!checkFirst || aTreeMatchContext.mNthIndexCache.GetNthIndex(
                           aElement, true, false, true) == 1) &&
         (!checkLast || aTreeMatchContext.mNthIndexCache.GetNthIndex(
                          aElement, true, true, true) == 1);
}

static inline bool
checkGenericEmptyMatches(Element* aElement,
                         TreeMatchContext& aTreeMatchContext,
                         bool isWhitespaceSignificant)
{
  nsIContent* child = nullptr;
  int32_t index = -1;

  if (aTreeMatchContext.mForStyling)
    aElement->SetFlags(NODE_HAS_EMPTY_SELECTOR);

  do {
    child = aElement->GetChildAt(++index);
    // stop at first non-comment (and non-whitespace for
    // :-moz-only-whitespace) node
  } while (child && !nsStyleUtil::IsSignificantChild(
                      child, true, isWhitespaceSignificant));
  return (child == nullptr);
}

/* static */ bool
nsCSSRuleUtils::StateSelectorMatches(Element* aElement,
                                     nsCSSSelector* aSelector,
                                     NodeMatchContext& aNodeMatchContext,
                                     TreeMatchContext& aTreeMatchContext,
                                     SelectorMatchesFlags aSelectorFlags,
                                     bool* const aDependence,
                                     EventStates aStatesToCheck)
{
  NS_PRECONDITION(!aStatesToCheck.IsEmpty(),
                  "should only need to call StateSelectorMatches if "
                  "aStatesToCheck is not empty");

  // Bit-based pseudo-classes
  if (aStatesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_ACTIVE |
                                           NS_EVENT_STATE_HOVER) &&
      aTreeMatchContext.mCompatMode == eCompatibility_NavQuirks &&
      ActiveHoverQuirkMatches(aSelector, aSelectorFlags) &&
      aElement->IsHTMLElement() && !nsCSSRuleUtils::IsLink(aElement)) {
    // In quirks mode, only make links sensitive to selectors ":active"
    // and ":hover".
    return false;
  }

  if (aTreeMatchContext.mForStyling &&
      aStatesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER)) {
    // Mark the element as having :hover-dependent style
    aElement->SetHasRelevantHoverRules();
  }

  if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(aStatesToCheck)) {
    if (aDependence) {
      *aDependence = true;
    }
  } else {
    EventStates contentState =
      nsCSSRuleUtils::GetContentStateForVisitedHandling(
        aElement,
        aTreeMatchContext,
        aTreeMatchContext.VisitedHandling(),
        aNodeMatchContext.mIsRelevantLink);
    if (!contentState.HasAtLeastOneOfStates(aStatesToCheck)) {
      return false;
    }
  }

  return true;
}

/* static */ bool
nsCSSRuleUtils::StateSelectorMatches(Element* aElement,
                                     nsCSSSelector* aSelector,
                                     NodeMatchContext& aNodeMatchContext,
                                     TreeMatchContext& aTreeMatchContext,
                                     SelectorMatchesFlags aSelectorFlags)
{
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass;
       pseudoClass = pseudoClass->mNext) {
    if (pseudoClass->mType == CSSPseudoClassType::autofill ||
        pseudoClass->mType == CSSPseudoClassType::mozAutofillHighlight) {
      // Match if the element has the autofill state, regardless of focus
      if (!aElement->State().HasState(NS_EVENT_STATE_AUTOFILL)) {
        return false;
      }
      continue; // This pseudo-class matches
    }
    auto idx = static_cast<CSSPseudoClassTypeBase>(pseudoClass->mType);
    EventStates statesToCheck = nsCSSPseudoClasses::sPseudoClassStates[idx];
    if (!statesToCheck.IsEmpty() && !StateSelectorMatches(aElement,
                                                          aSelector,
                                                          aNodeMatchContext,
                                                          aTreeMatchContext,
                                                          aSelectorFlags,
                                                          nullptr,
                                                          statesToCheck)) {
      return false;
    }
  }
  return true;
}

// Returns whether aSelector can match featureless elements.
/* static */ bool
nsCSSRuleUtils::CanMatchFeaturelessElement(nsCSSSelector* aSelector)
{
  if (aSelector->HasFeatureSelectors()) {
    return false;
  }

  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass;
       pseudoClass = pseudoClass->mNext) {
    if (pseudoClass->mType == CSSPseudoClassType::host ||
        pseudoClass->mType == CSSPseudoClassType::hostContext) {
      return true;
    }
  }

  return false;
}

// |aDependence| has two functions:
//  * when non-null, it indicates that we're processing a negation,
//    which is done only when SelectorMatches calls itself recursively
//  * what it points to should be set to true whenever a test is skipped
//    because of aNodeMatchContent.mStateMask
/* static */ bool
nsCSSRuleUtils::SelectorMatches(Element* aElement,
                                nsCSSSelector* aSelector,
                                NodeMatchContext& aNodeMatchContext,
                                TreeMatchContext& aTreeMatchContext,
                                SelectorMatchesFlags aSelectorFlags,
                                bool* const aDependence)
{
  NS_PRECONDITION(!aSelector->IsPseudoElement(),
                  "Pseudo-element snuck into SelectorMatches?");
  MOZ_ASSERT(aTreeMatchContext.mForStyling ||
               !aNodeMatchContext.mIsRelevantLink,
             "mIsRelevantLink should be set to false when mForStyling "
             "is false since we don't know how to set it correctly in "
             "Has(Attribute|State)DependentStyle");

  if (aNodeMatchContext.mIsFeatureless &&
      !CanMatchFeaturelessElement(aSelector)) {
    return false;
  }

  Element* targetElement = aElement;
  if (aTreeMatchContext.mForAssignedSlot) {
    HTMLSlotElement* slot = aElement->GetAssignedSlot();
    // We're likely testing the slottable's ancestors and it might
    // not have an assigned slot, so return early.
    if (!slot) {
      return false;
    }
    targetElement = slot->AsElement();
  }

  // namespace/tag match
  // optimization : bail out early if we can
  if ((kNameSpaceID_Unknown != aSelector->mNameSpace &&
       targetElement->GetNameSpaceID() != aSelector->mNameSpace))
    return false;

  if (aSelector->mLowercaseTag) {
    nsIAtom* selectorTag =
      (aTreeMatchContext.mIsHTMLDocument && targetElement->IsHTMLElement())
        ? aSelector->mLowercaseTag
        : aSelector->mCasedTag;
    if (selectorTag != targetElement->NodeInfo()->NameAtom()) {
      return false;
    }
  }

  nsAtomList* IDList = aSelector->mIDList;
  if (IDList) {
    nsIAtom* id = targetElement->GetID();
    if (id) {
      // case sensitivity: bug 93371
      const bool isCaseSensitive =
        aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

      if (isCaseSensitive) {
        do {
          if (IDList->mAtom != id) {
            return false;
          }
          IDList = IDList->mNext;
        } while (IDList);
      } else {
        // Use EqualsIgnoreASCIICase instead of full on unicode case conversion
        // in order to save on performance. This is only used in quirks mode
        // anyway.
        nsDependentAtomString id1Str(id);
        do {
          if (!nsContentUtils::EqualsIgnoreASCIICase(
                id1Str, nsDependentAtomString(IDList->mAtom))) {
            return false;
          }
          IDList = IDList->mNext;
        } while (IDList);
      }
    } else {
      // Element has no id but we have an id selector
      return false;
    }
  }

  nsAtomList* classList = aSelector->mClassList;
  if (classList) {
    // test for class match
    const nsAttrValue* elementClasses = targetElement->GetClasses();
    if (!elementClasses) {
      // Element has no classes but we have a class selector
      return false;
    }

    // case sensitivity: bug 93371
    const bool isCaseSensitive =
      aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

    while (classList) {
      if (!elementClasses->Contains(
            classList->mAtom, isCaseSensitive ? eCaseMatters : eIgnoreCase)) {
        return false;
      }
      classList = classList->mNext;
    }
  }

  const bool isOutsideShadowTree =
    !!(aSelectorFlags & SelectorMatchesFlags::IS_OUTSIDE_SHADOW_TREE);
  const bool isNegated = (aDependence != nullptr);
  // The selectors for which we set node bits are, unfortunately, early
  // in this function (because they're pseudo-classes, which are
  // generally quick to test, and thus earlier).  If they were later,
  // we'd probably avoid setting those bits in more cases where setting
  // them is unnecessary.
  NS_ASSERTION(aNodeMatchContext.mStateMask.IsEmpty() ||
                 !aTreeMatchContext.mForStyling,
               "mForStyling must be false if we're just testing for "
               "state-dependence");

  // test for pseudo class match
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass;
       pseudoClass = pseudoClass->mNext) {
    auto idx = static_cast<CSSPseudoClassTypeBase>(pseudoClass->mType);
    EventStates statesToCheck = nsCSSPseudoClasses::sPseudoClassStates[idx];
    if (statesToCheck.IsEmpty()) {
      // keep the cases here in the same order as the list in
      // nsCSSPseudoClassList.h
      switch (pseudoClass->mType) {
        case CSSPseudoClassType::empty:
          if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozOnlyWhitespace:
          if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, false)) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozEmptyExceptChildrenWithLocalname: {
          NS_ASSERTION(pseudoClass->u.mString, "Must have string!");
          nsIContent* child = nullptr;
          int32_t index = -1;

          if (aTreeMatchContext.mForStyling)
            // FIXME:  This isn't sufficient to handle:
            //   :-moz-empty-except-children-with-localname() + E
            //   :-moz-empty-except-children-with-localname() ~ E
            // because we don't know to restyle the grandparent of the
            // inserted/removed element (as in bug 534804 for :empty).
            aElement->SetFlags(NODE_HAS_SLOW_SELECTOR);
          do {
            child = aElement->GetChildAt(++index);
          } while (child &&
                   (!nsStyleUtil::IsSignificantChild(child, true, false) ||
                    (child->GetNameSpaceID() == aElement->GetNameSpaceID() &&
                     child->NodeInfo()->NameAtom()->Equals(
                       nsDependentString(pseudoClass->u.mString)))));
          if (child != nullptr) {
            return false;
          }
        } break;

        case CSSPseudoClassType::lang: {
          NS_ASSERTION(nullptr != pseudoClass->u.mString,
                       "null lang parameter");
          if (!pseudoClass->u.mString || !*pseudoClass->u.mString) {
            return false;
          }

          // We have to determine the language of the current element.  Since
          // this is currently no property and since the language is inherited
          // from the parent we have to be prepared to look at all parent
          // nodes.  The language itself is encoded in the LANG attribute.
          nsAutoString language;
          if (aElement->GetLang(language)) {
            if (!nsStyleUtil::DashMatchCompare(
                  language,
                  nsDependentString(pseudoClass->u.mString),
                  nsASCIICaseInsensitiveStringComparator())) {
              return false;
            }
            // This pseudo-class matched; move on to the next thing
            break;
          }

          nsIDocument* doc = aTreeMatchContext.mDocument;
          if (doc) {
            // Try to get the language from the HTTP header or if this
            // is missing as well from the preferences.
            // The content language can be a comma-separated list of
            // language codes.
            doc->GetContentLanguage(language);

            nsDependentString langString(pseudoClass->u.mString);
            language.StripWhitespace();
            int32_t begin = 0;
            int32_t len = language.Length();
            while (begin < len) {
              int32_t end = language.FindChar(char16_t(','), begin);
              if (end == kNotFound) {
                end = len;
              }
              if (nsStyleUtil::DashMatchCompare(
                    Substring(language, begin, end - begin),
                    langString,
                    nsASCIICaseInsensitiveStringComparator())) {
                break;
              }
              begin = end + 1;
            }
            if (begin < len) {
              // This pseudo-class matched
              break;
            }
          }
        }
          return false;

        case CSSPseudoClassType::mozBoundElement:
          if (aTreeMatchContext.mScopedRoot != aElement) {
            return false;
          }
          break;

        case CSSPseudoClassType::root:
          if (aElement != aElement->OwnerDoc()->GetRootElement()) {
            return false;
          }
          break;

        case CSSPseudoClassType::is:
        case CSSPseudoClassType::matches:
        case CSSPseudoClassType::any:
        case CSSPseudoClassType::where: {
          if (!SelectorListMatches(aElement,
                                   pseudoClass,
                                   aNodeMatchContext,
                                   aTreeMatchContext,
                                   true)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozAny: {
          // XXX: For compatibility, we retain :-moz-any()'s original behavior,
          // which is to be unforgiving and reject complex selectors in
          // its selector list argument.
          if (!SelectorListMatches(aElement,
                                   pseudoClass,
                                   aNodeMatchContext,
                                   aTreeMatchContext,
                                   false,
                                   true)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozAnyPrivate: {
          if (!SelectorListMatches(
                aElement, pseudoClass, aNodeMatchContext, aTreeMatchContext)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::slotted: {
          if (aTreeMatchContext.mForAssignedSlot) {
            aTreeMatchContext.mForAssignedSlot = false;
          }

          // Slottables cannot be matched from the outer tree.
          if (isOutsideShadowTree) {
            return false;
          }

          // Slot elements cannot be matched.
          if (aElement->IsHTMLElement(nsGkAtoms::slot)) {
            return false;
          }

          // The current element must have an assigned slot.
          if (!aElement->GetAssignedSlot()) {
            return false;
          }

          if (!SelectorListMatches(
                aElement, pseudoClass, aNodeMatchContext, aTreeMatchContext)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::host: {
          ShadowRoot* shadow = aElement->GetShadowRoot();
          // In order to match :host, the element must be a shadow root host,
          // we must be matching only against host pseudo selectors, and the
          // selector's context must be the shadow root (the selector must be
          // featureless, the left-most selector, and be in a shadow root
          // style).
          if (!shadow || aSelector->HasFeatureSelectors() ||
              isOutsideShadowTree) {
            return false;
          }

          // We're matching :host from inside the shadow root.
          if (!aTreeMatchContext.mOnlyMatchHostPseudo) {
            // Check if the element has the same shadow root.
            if (aTreeMatchContext.mScopedRoot) {
              if (shadow != aTreeMatchContext.mScopedRoot->GetShadowRoot()) {
                return false;
              }
            }
            // We were called elsewhere.
          }

          // Reject if the next selector is an explicit universal selector.
          if (aSelector->mNext && aSelector->mNext->mExplicitUniversal) {
            return false;
          }

          // The :host selector may also be be functional, with a compound
          // selector. If this is the case, then also ensure that the host
          // element matches against the compound selector.
          if (!pseudoClass->u.mSelectorList) {
            break;
          }

          // Match if any selector in the argument list matches.
          // FIXME: What this effectively does is bypass the "featureless"
          // selector check under SelectorMatches.
          NodeMatchContext nodeContext(aNodeMatchContext.mStateMask,
                                       aNodeMatchContext.mIsRelevantLink);
          if (!SelectorListMatches(
                aElement, pseudoClass, nodeContext, aTreeMatchContext)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::hostContext: {
          // In order to match host-context, the element must be a
          // shadow root host and the selector's context must be the
          // shadow root (aTreeMatchContext.mScopedRoot is set to the
          // host of the shadow root where the style is contained,
          // thus the element must be mScopedRoot). If the UNKNOWN
          // selector flag is set, relax the shadow root host
          // requirement because this pseudo class walks through
          // ancestors looking for a match, thus the selector can be
          // dependant on aElement even though it is not the host. The
          // dependency would otherwise be missed because when UNKNOWN
          // is set, selector matching may not have started from the top.
          if (!((aElement->GetShadowRoot() &&
                 aElement == aTreeMatchContext.mScopedRoot) ||
                aSelectorFlags & SelectorMatchesFlags::UNKNOWN)) {
            return false;
          }

          Element* currentElement = aElement;
          while (currentElement) {
            NodeMatchContext nodeContext(
              EventStates(), nsCSSRuleUtils::IsLink(currentElement));
            if (SelectorListMatches(currentElement,
                                    pseudoClass,
                                    nodeContext,
                                    aTreeMatchContext)) {
              break;
            }

            nsIContent* flattenedParent =
              currentElement->GetFlattenedTreeParent();
            currentElement = flattenedParent && flattenedParent->IsElement()
                               ? flattenedParent->AsElement()
                               : nullptr;
          }
          if (!currentElement) {
            return false;
          }
        } break;

        case CSSPseudoClassType::firstChild:
          if (!edgeChildMatches(aElement, aTreeMatchContext, true, false)) {
            return false;
          }
          break;

        case CSSPseudoClassType::firstNode: {
          nsIContent* firstNode = nullptr;
          nsIContent* parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

            int32_t index = -1;
            do {
              firstNode = parent->GetChildAt(++index);
              // stop at first non-comment and non-whitespace node
            } while (firstNode &&
                     !nsStyleUtil::IsSignificantChild(firstNode, true, false));
          }
          if (aElement != firstNode) {
            return false;
          }
        } break;

        case CSSPseudoClassType::lastChild:
          if (!edgeChildMatches(aElement, aTreeMatchContext, false, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::lastNode: {
          nsIContent* lastNode = nullptr;
          nsIContent* parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

            uint32_t index = parent->GetChildCount();
            do {
              lastNode = parent->GetChildAt(--index);
              // stop at first non-comment and non-whitespace node
            } while (lastNode &&
                     !nsStyleUtil::IsSignificantChild(lastNode, true, false));
          }
          if (aElement != lastNode) {
            return false;
          }
        } break;

        case CSSPseudoClassType::onlyChild:
          if (!edgeChildMatches(aElement, aTreeMatchContext, true, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::firstOfType:
          if (!edgeOfTypeMatches(aElement, aTreeMatchContext, true, false)) {
            return false;
          }
          break;

        case CSSPseudoClassType::lastOfType:
          if (!edgeOfTypeMatches(aElement, aTreeMatchContext, false, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::onlyOfType:
          if (!edgeOfTypeMatches(aElement, aTreeMatchContext, true, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::nthChild:
          if (!nthChildGenericMatches(
                aElement, aTreeMatchContext, pseudoClass, false, false)) {
            return false;
          }
          break;

        case CSSPseudoClassType::nthLastChild:
          if (!nthChildGenericMatches(
                aElement, aTreeMatchContext, pseudoClass, false, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::nthOfType:
          if (!nthChildGenericMatches(
                aElement, aTreeMatchContext, pseudoClass, true, false)) {
            return false;
          }
          break;

        case CSSPseudoClassType::nthLastOfType:
          if (!nthChildGenericMatches(
                aElement, aTreeMatchContext, pseudoClass, true, true)) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozIsHTML:
          if (!aTreeMatchContext.mIsHTMLDocument ||
              !aElement->IsHTMLElement()) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozNativeAnonymous:
          if (!aElement->IsInNativeAnonymousSubtree()) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozSystemMetric: {
          nsCOMPtr<nsIAtom> metric = NS_Atomize(pseudoClass->u.mString);
          if (!nsCSSRuleUtils::HasSystemMetric(metric)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozLocaleDir: {
          bool docIsRTL =
            aTreeMatchContext.mDocument->GetDocumentState().HasState(
              NS_DOCUMENT_STATE_RTL_LOCALE);

          nsDependentString dirString(pseudoClass->u.mString);

          if (dirString.EqualsLiteral("rtl")) {
            if (!docIsRTL) {
              return false;
            }
          } else if (dirString.EqualsLiteral("ltr")) {
            if (docIsRTL) {
              return false;
            }
          } else {
            // Selectors specifying other directions never match.
            return false;
          }
        } break;

        case CSSPseudoClassType::mozLWTheme: {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() <=
              nsIDocument::Doc_Theme_None) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozLWThemeBrightText: {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
              nsIDocument::Doc_Theme_Bright) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozLWThemeDarkText: {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
              nsIDocument::Doc_Theme_Dark) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozWindowInactive:
          if (!aTreeMatchContext.mDocument->GetDocumentState().HasState(
                NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
            return false;
          }
          break;

        case CSSPseudoClassType::mozTableBorderNonzero: {
          if (!aElement->IsHTMLElement(nsGkAtoms::table)) {
            return false;
          }
          const nsAttrValue* val = aElement->GetParsedAttr(nsGkAtoms::border);
          if (!val || (val->Type() == nsAttrValue::eInteger &&
                       val->GetIntegerValue() == 0)) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozBrowserFrame: {
          nsCOMPtr<nsIMozBrowserFrame> browserFrame =
            do_QueryInterface(aElement);
          if (!browserFrame || !browserFrame->GetReallyIsBrowser()) {
            return false;
          }
        } break;

        case CSSPseudoClassType::mozDir:
        case CSSPseudoClassType::dir: {
          if (aDependence) {
            EventStates states =
              nsCSSPseudoClasses::sPseudoClassStateDependences[static_cast<CSSPseudoClassTypeBase>(
                pseudoClass->mType)];
            if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(states)) {
              *aDependence = true;
              return false;
            }
          }

          // If we only had to consider HTML, directionality would be
          // exclusively LTR or RTL.
          //
          // However, in markup languages where there is no direction attribute
          // we have to consider the possibility that neither dir(rtl) nor
          // dir(ltr) matches.
          EventStates state = aElement->StyleState();
          nsDependentString dirString(pseudoClass->u.mString);

          if (dirString.EqualsLiteral("rtl")) {
            if (!state.HasState(NS_EVENT_STATE_RTL)) {
              return false;
            }
          } else if (dirString.EqualsLiteral("ltr")) {
            if (!state.HasState(NS_EVENT_STATE_LTR)) {
              return false;
            }
          } else {
            // Selectors specifying other directions never match.
            return false;
          }
        } break;

        case CSSPseudoClassType::scope:
          if (aTreeMatchContext.mForScopedStyle) {
            if (aTreeMatchContext.mCurrentStyleScope) {
              // If mCurrentStyleScope is null, aElement must be the style
              // scope root.  This is because the PopStyleScopeForSelectorMatching
              // call in SelectorMatchesTree sets mCurrentStyleScope to null
              // as soon as we visit the style scope element, and we won't
              // progress further up the tree after this call to
              // SelectorMatches.  Thus if mCurrentStyleScope is still set,
              // we know the selector does not match.
              return false;
            }
          } else if (aTreeMatchContext.HasSpecifiedScope()) {
            if (!aTreeMatchContext.IsScopeElement(aElement)) {
              return false;
            }
          } else {
            if (aElement != aElement->OwnerDoc()->GetRootElement()) {
              return false;
            }
          }
          break;

        default:
          MOZ_ASSERT(false, "How did that happen?");
      }
    } else {
      if (!StateSelectorMatches(aElement,
                                aSelector,
                                aNodeMatchContext,
                                aTreeMatchContext,
                                aSelectorFlags,
                                aDependence,
                                statesToCheck)) {
        return false;
      }
    }
  }

  bool result = true;
  if (aSelector->mAttrList) {
    // test for attribute match
    if (!targetElement->HasAttrs()) {
      // if no attributes on the content, no match
      return false;
    } else {
      result = true;
      nsAttrSelector* attr = aSelector->mAttrList;
      nsIAtom* matchAttribute;

      do {
        bool isHTML =
          (aTreeMatchContext.mIsHTMLDocument && targetElement->IsHTMLElement());
        matchAttribute = isHTML ? attr->mLowercaseAttr : attr->mCasedAttr;
        if (attr->mNameSpace == kNameSpaceID_Unknown) {
          // Attr selector with a wildcard namespace.  We have to examine all
          // the attributes on our content node....  This sort of selector is
          // essentially a boolean OR, over all namespaces, of equivalent attr
          // selectors with those namespaces.  So to evaluate whether it
          // matches, evaluate for each namespace (the only namespaces that
          // have a chance at matching, of course, are ones that the element
          // actually has attributes in), short-circuiting if we ever match.
          result = false;
          const nsAttrName* attrName;
          for (uint32_t i = 0; (attrName = targetElement->GetAttrNameAt(i));
               ++i) {
            if (attrName->LocalName() != matchAttribute) {
              continue;
            }
            if (attr->mFunction == NS_ATTR_FUNC_SET) {
              result = true;
            } else {
              nsAutoString value;
#ifdef DEBUG
              bool hasAttr =
#endif
                targetElement->GetAttr(
                  attrName->NamespaceID(), attrName->LocalName(), value);
              NS_ASSERTION(hasAttr, "GetAttrNameAt lied");
              result = AttrMatchesValue(attr, value, isHTML);
            }

            // At this point |result| has been set by us
            // explicitly in this loop.  If it's false, we may still match
            // -- the content may have another attribute with the same name but
            // in a different namespace.  But if it's true, we are done (we
            // can short-circuit the boolean OR described above).
            if (result) {
              break;
            }
          }
        } else if (attr->mFunction == NS_ATTR_FUNC_EQUALS) {
          result = targetElement->AttrValueIs(
            attr->mNameSpace,
            matchAttribute,
            attr->mValue,
            attr->IsValueCaseSensitive(isHTML) ? eCaseMatters : eIgnoreCase);
        } else if (!targetElement->HasAttr(attr->mNameSpace, matchAttribute)) {
          result = false;
        } else if (attr->mFunction != NS_ATTR_FUNC_SET) {
          nsAutoString value;
#ifdef DEBUG
          bool hasAttr =
#endif
            targetElement->GetAttr(attr->mNameSpace, matchAttribute, value);
          NS_ASSERTION(hasAttr, "HasAttr lied");
          result = AttrMatchesValue(attr, value, isHTML);
        }

        attr = attr->mNext;
      } while (attr && result);
    }
  }

  // apply SelectorMatches to the negated selectors in the chain
  if (!isNegated) {
    for (nsCSSSelector* negation = aSelector->mNegations; result && negation;
         negation = negation->mNegations) {
      bool dependence = false;
      result = !SelectorMatches(targetElement,
                                negation,
                                aNodeMatchContext,
                                aTreeMatchContext,
                                SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT,
                                &dependence);
      // If the selector does match due to the dependence on
      // aNodeMatchContext.mStateMask, then we want to keep result true
      // so that the final result of SelectorMatches is true.  Doing so
      // tells StateEnumFunc that there is a dependence on the state.
      result = result || dependence;
    }
  }
  return result;
}

#undef STATE_CHECK

#ifdef DEBUG
/* static */ bool
nsCSSRuleUtils::HasPseudoClassSelectorArgsWithCombinators(nsCSSSelector* aSelector)
{
  for (nsPseudoClassList* p = aSelector->mPseudoClassList; p; p = p->mNext) {
    if (nsCSSPseudoClasses::HasSelectorListArg(p->mType)) {
      for (nsCSSSelectorList* l = p->u.mSelectorList; l; l = l->mNext) {
        if (l->mSelectors->mNext) {
          return true;
        }
      }
    }
  }
  for (nsCSSSelector* n = aSelector->mNegations; n; n = n->mNegations) {
    if (n->mNext) {
      return true;
    }
  }
  return false;
}
#endif

/* static */ bool
nsCSSRuleUtils::SelectorMatchesTree(Element* aPrevElement,
                                    nsCSSSelector* aSelector,
                                    TreeMatchContext& aTreeMatchContext,
                                    SelectorMatchesTreeFlags aFlags)
{
  MOZ_ASSERT(!aSelector || !aSelector->IsPseudoElement());
  nsCSSSelector* selector = aSelector;
  Element* prevElement = aPrevElement;
  bool crossedShadowRootBoundary = false;
  while (selector) { // check compound selectors
    bool contentIsFeatureless = false;
    NS_ASSERTION(!selector->mNext || selector->mNext->mOperator != char16_t(0),
                 "compound selector without combinator");

    // If after the previous selector match we are now outside the
    // current style scope, we don't need to match any further.
    if (aTreeMatchContext.mForScopedStyle &&
        !aTreeMatchContext.IsWithinStyleScopeForSelectorMatching()) {
      return false;
    }

    // for adjacent sibling combinators, the content to test against the
    // selector is the previous sibling *element*
    Element* element = nullptr;
    if (char16_t('+') == selector->mOperator ||
        char16_t('~') == selector->mOperator) {
      // The relevant link must be an ancestor of the node being matched.
      aFlags = SelectorMatchesTreeFlags(aFlags & ~eLookForRelevantLink);
      nsIContent* parent = prevElement->GetParent();
      // Operate on the flattened element tree when matching the
      // ::slotted() pseudo-element.
      if (aTreeMatchContext.mRestrictToSlottedPseudo) {
        parent = prevElement->GetFlattenedTreeParent();
      }
      if (parent) {
        if (aTreeMatchContext.mForStyling)
          parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);

        element = prevElement->GetPreviousElementSibling();
      }
    }
    // for descendant combinators and child combinators, the element
    // to test against is the parent
    else {
      nsIContent* content = prevElement->GetParent();
      // Operate on the flattened element tree when matching the
      // ::slotted() pseudo-element.
      if (aTreeMatchContext.mRestrictToSlottedPseudo) {
        content = prevElement->GetFlattenedTreeParent();
      }

      // In the shadow tree, the shadow host behaves as if it
      // is a featureless parent of top-level elements of the shadow
      // tree. Only cross shadow root boundary when the selector is the
      // left most selector because ancestors of the host are not in
      // the selector match list.
      ShadowRoot* shadowRoot =
        content ? ShadowRoot::FromNode(content) : nullptr;
      if (shadowRoot && !selector->mNext && !crossedShadowRootBoundary) {
        content = shadowRoot->GetHost();
        crossedShadowRootBoundary = true;
        contentIsFeatureless = true;
      }

      // GetParent could return a document fragment; we only want
      // element parents.
      if (content && content->IsElement()) {
        element = content->AsElement();
        if (aTreeMatchContext.mForScopedStyle) {
          // We are moving up to the parent element; tell the
          // TreeMatchContext, so that in case this element is the
          // style scope element, selector matching stops before we
          // traverse further up the tree.
          aTreeMatchContext.PopStyleScopeForSelectorMatching(element);
        }

        // Compatibility hack: First try matching this selector as though the
        // <xbl:children> element wasn't in the tree to allow old selectors
        // were written before <xbl:children> participated in CSS selector
        // matching to work.
        if (selector->mOperator == '>' && element->IsActiveChildrenElement()) {
          Element* styleScope = aTreeMatchContext.mCurrentStyleScope;
          if (SelectorMatchesTree(
                element, selector, aTreeMatchContext, aFlags)) {
            // It matched, don't try matching on the <xbl:children> element at
            // all.
            return true;
          }
          // We want to reset mCurrentStyleScope on aTreeMatchContext
          // back to its state before the SelectorMatchesTree call, in
          // case that call happens to traverse past the style scope element
          // and sets it to null.
          aTreeMatchContext.mCurrentStyleScope = styleScope;
        }
      }
    }
    if (!element) {
      return false;
    }
    if ((aFlags & eMatchOnConditionalRestyleAncestor) &&
        element->HasFlag(ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR)) {
      // If we're looking at an element that we already generated an
      // eRestyle_SomeDescendants restyle hint for, then we should pretend
      // that we matched here, because we don't know what the values of
      // attributes on |element| were at the time we generated the
      // eRestyle_SomeDescendants.  This causes AttributeEnumFunc and
      // HasStateDependentStyle below to generate a restyle hint for the
      // change we're currently looking at, as we don't know whether the LHS
      // of the selector we looked up matches or not.  (We only pass in aFlags
      // to cause us to look for eRestyle_SomeDescendants here under
      // AttributeEnumFunc and HasStateDependentStyle.)
      return true;
    }
    const bool isRelevantLink =
      (aFlags & eLookForRelevantLink) && nsCSSRuleUtils::IsLink(element);

    NodeMatchContext nodeContext(
      EventStates(), isRelevantLink, contentIsFeatureless);
    if (isRelevantLink) {
      // If we find an ancestor of the matched node that is a link
      // during the matching process, then it's the relevant link (see
      // constructor call above).
      // Since we are still matching against selectors that contain
      // :visited (they'll just fail), we will always find such a node
      // during the selector matching process if there is a relevant
      // link that can influence selector matching.
      aFlags = SelectorMatchesTreeFlags(aFlags & ~eLookForRelevantLink);
      aTreeMatchContext.SetHaveRelevantLink();
    }
    if (SelectorMatches(element,
                        selector,
                        nodeContext,
                        aTreeMatchContext,
                        SelectorMatchesFlags::NONE)) {
      // to avoid greedy matching, we need to recur if this is a
      // descendant or general sibling combinator and the next
      // combinator is different, but we can make an exception for
      // sibling, then parent, since a sibling's parent is always the
      // same.
      if (NS_IS_GREEDY_OPERATOR(selector->mOperator) && selector->mNext &&
          selector->mNext->mOperator != selector->mOperator &&
          !(selector->mOperator == '~' &&
            NS_IS_ANCESTOR_OPERATOR(selector->mNext->mOperator))) {

        // pretend the selector didn't match, and step through content
        // while testing the same selector

        // This approach is slightly strange in that when it recurs
        // it tests from the top of the content tree, down.  This
        // doesn't matter much for performance since most selectors
        // don't match.  (If most did, it might be faster...)
        Element* styleScope = aTreeMatchContext.mCurrentStyleScope;
        if (SelectorMatchesTree(element, selector, aTreeMatchContext, aFlags)) {
          return true;
        }
        // We want to reset mCurrentStyleScope on aTreeMatchContext
        // back to its state before the SelectorMatchesTree call, in
        // case that call happens to traverse past the style scope element
        // and sets it to null.
        aTreeMatchContext.mCurrentStyleScope = styleScope;
      }
      selector = selector->mNext;
      if (!selector && !aTreeMatchContext.mIsTopmostScope &&
          aTreeMatchContext.mRestrictToSlottedPseudo &&
          aTreeMatchContext.mScopedRoot != element) {
        return false;
      }
    } else {
      // for adjacent sibling and child combinators, if we didn't find
      // a match, we're done
      if (!NS_IS_GREEDY_OPERATOR(selector->mOperator)) {
        return false; // parent was required to match
      }
    }
    prevElement = element;
  }
  return true; // all the selectors matched.
}

/* static */ bool
nsCSSRuleUtils::SelectorListMatches(Element* aElement,
                                    nsCSSSelectorList* aList,
                                    NodeMatchContext& aNodeMatchContext,
                                    TreeMatchContext& aTreeMatchContext,
                                    SelectorMatchesFlags aSelectorFlags,
                                    bool aIsForgiving,
                                    bool aPreventComplexSelectors)
{
  while (aList) {
    nsCSSSelector* selector = aList->mSelectors;
    // Forgiving selector lists are allowed to be empty, but they
    // don't match anything.
    if (!selector && aIsForgiving) {
      return false;
    }
    NS_ASSERTION(selector, "Should have *some* selectors");
    NS_ASSERTION(!selector->IsPseudoElement(), "Shouldn't have been called");
    if (aPreventComplexSelectors) {
      NS_ASSERTION(!selector->mNext, "Shouldn't have complex selectors");
    }
    if (SelectorMatches(aElement,
                        selector,
                        aNodeMatchContext,
                        aTreeMatchContext,
                        aSelectorFlags)) {
      nsCSSSelector* next = selector->mNext;
      SelectorMatchesTreeFlags selectorTreeFlags = SelectorMatchesTreeFlags(0);
      // Try to look for the closest ancestor link element if we're processing
      // the selector list argument of a pseudo-class, but only if for a new style context (see SelectorMatches).
      if (!aNodeMatchContext.mIsRelevantLink && aTreeMatchContext.mForStyling &&
          (aSelectorFlags & SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT)) {
        selectorTreeFlags = eLookForRelevantLink;
      }

      if (!next || SelectorMatchesTree(
                     aElement, next, aTreeMatchContext, selectorTreeFlags)) {
        return true;
      }
    }

    aList = aList->mNext;
  }

  return false;
}

/* static */ bool
nsCSSRuleUtils::SelectorListMatches(Element* aElement,
                                    nsPseudoClassList* aList,
                                    NodeMatchContext& aNodeMatchContext,
                                    TreeMatchContext& aTreeMatchContext,
                                    bool aIsForgiving,
                                    bool aPreventComplexSelectors)
{
  return SelectorListMatches(aElement,
                             aList->u.mSelectorList,
                             aNodeMatchContext,
                             aTreeMatchContext,
                             SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT,
                             aIsForgiving,
                             aPreventComplexSelectors);
}

// TreeMatchContext and AncestorFilter out of line methods
void
TreeMatchContext::InitAncestors(Element* aElement)
{
  MOZ_ASSERT(!mAncestorFilter.mFilter);
  MOZ_ASSERT(mAncestorFilter.mHashes.IsEmpty());
  MOZ_ASSERT(mStyleScopes.IsEmpty());

  mAncestorFilter.mFilter = new AncestorFilter::Filter();

  if (MOZ_LIKELY(aElement)) {
    MOZ_ASSERT(aElement->GetUncomposedDoc() ||
                 aElement->HasFlag(NODE_IS_IN_SHADOW_TREE),
               "aElement must be in the document or in shadow tree "
               "for the assumption that GetParentNode() is non-null "
               "on all element ancestors of aElement to be true");
    // Collect up the ancestors
    AutoTArray<Element*, 50> ancestors;
    Element* cur = aElement;
    do {
      ancestors.AppendElement(cur);
      cur = cur->GetParentElementCrossingShadowRoot();
    } while (cur);

    // Now push them in reverse order.
    for (uint32_t i = ancestors.Length(); i-- != 0;) {
      mAncestorFilter.PushAncestor(ancestors[i]);
      PushStyleScope(ancestors[i]);
      PushShadowHost(ancestors[i]);
    }
  }
}

void
TreeMatchContext::InitStyleScopes(Element* aElement)
{
  MOZ_ASSERT(mStyleScopes.IsEmpty());

  if (MOZ_LIKELY(aElement)) {
    // Collect up the ancestors
    AutoTArray<Element*, 50> ancestors;
    Element* cur = aElement;
    do {
      ancestors.AppendElement(cur);
      cur = cur->GetParentElementCrossingShadowRoot();
    } while (cur);

    // Now push them in reverse order.
    for (uint32_t i = ancestors.Length(); i-- != 0;) {
      PushStyleScope(ancestors[i]);
    }
  }
}

void
AncestorFilter::PushAncestor(Element* aElement)
{
  MOZ_ASSERT(mFilter);

  uint32_t oldLength = mHashes.Length();

  mPopTargets.AppendElement(oldLength);
#ifdef DEBUG
  mElements.AppendElement(aElement);
#endif
  mHashes.AppendElement(aElement->NodeInfo()->NameAtom()->hash());
  nsIAtom* id = aElement->GetID();
  if (id) {
    mHashes.AppendElement(id->hash());
  }
  const nsAttrValue* classes = aElement->GetClasses();
  if (classes) {
    uint32_t classCount = classes->GetAtomCount();
    for (uint32_t i = 0; i < classCount; ++i) {
      mHashes.AppendElement(classes->AtomAt(i)->hash());
    }
  }

  uint32_t newLength = mHashes.Length();
  for (uint32_t i = oldLength; i < newLength; ++i) {
    mFilter->add(mHashes[i]);
  }
}

void
AncestorFilter::PopAncestor()
{
  MOZ_ASSERT(!mPopTargets.IsEmpty());
  MOZ_ASSERT(mPopTargets.Length() == mElements.Length());

  uint32_t popTargetLength = mPopTargets.Length();
  uint32_t newLength = mPopTargets[popTargetLength - 1];

  mPopTargets.TruncateLength(popTargetLength - 1);
#ifdef DEBUG
  mElements.TruncateLength(popTargetLength - 1);
#endif

  uint32_t oldLength = mHashes.Length();
  for (uint32_t i = newLength; i < oldLength; ++i) {
    mFilter->remove(mHashes[i]);
  }
  mHashes.TruncateLength(newLength);
}

#ifdef DEBUG
void
AncestorFilter::AssertHasAllAncestors(Element* aElement) const
{
  Element* cur = aElement->GetParentElementCrossingShadowRoot();
  while (cur) {
    MOZ_ASSERT(mElements.Contains(cur));
    cur = cur->GetParentElementCrossingShadowRoot();
  }
}

void
TreeMatchContext::AssertHasAllStyleScopes(Element* aElement) const
{
  if (aElement->IsInNativeAnonymousSubtree()) {
    // Document style sheets are never applied to native anonymous content,
    // so it's not possible for them to be in a <style scoped> scope.
    return;
  }
  Element* cur = aElement->GetParentElementCrossingShadowRoot();
  while (cur) {
    if (cur->IsScopedStyleRoot()) {
      MOZ_ASSERT(mStyleScopes.Contains(cur));
    }
    cur = cur->GetParentElementCrossingShadowRoot();
  }
}
#endif
