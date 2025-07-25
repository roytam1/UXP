/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSRuleUtils_h___
#define nsCSSRuleUtils_h___

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
#include "nsRuleProcessorData.h"
#include "nsTArray.h"
#include "StyleRule.h"

struct nsCSSRuleUtils
{
  static void Startup();
  static void Shutdown();
  static void FreeSystemMetrics();
  static bool HasSystemMetric(nsIAtom* aMetric);

#ifdef XP_WIN
  // Cached theme identifier for the moz-windows-theme media query.
  static uint8_t GetWindowsThemeIdentifier();
  static void SetWindowsThemeIdentifier(uint8_t aId) { sWinThemeId = aId; }
#endif

  static bool StateSelectorMatches(Element* aElement,
                                   nsCSSSelector* aSelector,
                                   NodeMatchContext& aNodeMatchContext,
                                   TreeMatchContext& aTreeMatchContext,
                                   SelectorMatchesFlags aSelectorFlags,
                                   bool* const aDependence,
                                   mozilla::EventStates aStatesToCheck);

  static bool StateSelectorMatches(Element* aElement,
                                   nsCSSSelector* aSelector,
                                   NodeMatchContext& aNodeMatchContext,
                                   TreeMatchContext& aTreeMatchContext,
                                   SelectorMatchesFlags aSelectorFlags);

  static bool SelectorMatches(Element* aElement,
                              nsCSSSelector* aSelector,
                              NodeMatchContext& aNodeMatchContext,
                              TreeMatchContext& aTreeMatchContext,
                              SelectorMatchesFlags aSelectorFlags,
                              bool* const aDependence = nullptr);

  static bool SelectorMatchesTree(Element* aPrevElement,
                                  nsCSSSelector* aSelector,
                                  TreeMatchContext& aTreeMatchContext,
                                  SelectorMatchesTreeFlags aFlags);

  static bool SelectorListMatches(Element* aElement,
                                  nsCSSSelectorList* aList,
                                  NodeMatchContext& aNodeMatchContext,
                                  TreeMatchContext& aTreeMatchContext,
                                  SelectorMatchesFlags aSelectorFlags,
                                  bool aIsForgiving = false,
                                  bool aPreventComplexSelectors = false);

  static bool SelectorListMatches(Element* aElement,
                                  nsPseudoClassList* aList,
                                  NodeMatchContext& aNodeMatchContext,
                                  TreeMatchContext& aTreeMatchContext,
                                  bool aIsForgiving = false,
                                  bool aPreventComplexSelectors = false);

#ifdef DEBUG
  static bool HasPseudoClassSelectorArgsWithCombinators(
    nsCSSSelector* aSelector);
#endif

  /**
   * Returns true if the given aElement matches aSelector.
   * Like nsCSSRuleUtil.cpp's SelectorMatches (and unlike
   * SelectorMatchesTree), this does not check an entire selector list
   * separated by combinators.
   *
   * :visited and :link will match both visited and non-visited links,
   * as if aTreeMatchContext->mVisitedHandling were eLinksVisitedOrUnvisited.
   *
   * aSelector is restricted to not containing pseudo-elements.
   */
  static bool RestrictedSelectorMatches(mozilla::dom::Element* aElement,
                                        nsCSSSelector* aSelector,
                                        TreeMatchContext& aTreeMatchContext);

  /**
   * Returns true if the given aElement matches one of the
   * selectors in aSelectorList.  Note that this method will assume
   * the given aElement is not a relevant link.  aSelectorList must not
   * include any pseudo-element selectors.  aSelectorList is allowed
   * to be null; in this case false will be returned.
   */
  static bool RestrictedSelectorListMatches(mozilla::dom::Element* aElement,
                                            TreeMatchContext& aTreeMatchContext,
                                            nsCSSSelectorList* aSelectorList);

  static bool CanMatchFeaturelessElement(nsCSSSelector* aSelector);

  /**
   * Helper to get the content state for a content node.  This may be
   * slightly adjusted from IntrinsicState().
   */
  static mozilla::EventStates GetContentState(
    mozilla::dom::Element* aElement,
    const TreeMatchContext& aTreeMatchContext);

  /**
   * Helper to get the content state for :visited handling for an element
   */
  static mozilla::EventStates GetContentStateForVisitedHandling(
    mozilla::dom::Element* aElement,
    const TreeMatchContext& aTreeMatchContext,
    nsRuleWalker::VisitedHandlingType aVisitedHandling,
    bool aIsRelevantLink);

  /*
   * Helper to test whether a node is a link
   */
  static bool IsLink(const mozilla::dom::Element* aElement);

#ifdef XP_WIN
  static uint8_t sWinThemeId;
#endif
};

#endif /* nsCSSRuleUtils_h___ */
