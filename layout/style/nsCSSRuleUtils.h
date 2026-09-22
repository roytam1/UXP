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
#include "mozilla/RefCounted.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "nsExpirationTracker.h"
#include "nsIMediaList.h"
#include "nsIStyleRuleProcessor.h"
#include "nsRuleWalker.h"
#include "nsRuleProcessorData.h"
#include "nsTArray.h"
#include "StyleRule.h"

// No selector or DOM pointers: nodes can safely keep this after a rule is
// removed, without keeping either the stylesheet or an anchor alive.
struct nsCSSHasSelectorData : public mozilla::RefCounted<nsCSSHasSelectorData> {
  MOZ_DECLARE_REFCOUNTED_TYPENAME(nsCSSHasSelectorData)

  struct Branch {
    mozilla::EventStates mStates;
    nsTArray<nsCOMPtr<nsIAtom>> mAttributes;
    nsTArray<nsCOMPtr<nsIAtom>> mClasses;
    bool mAllAttributes = false;
    bool mIsLocal = false;
    char16_t mCombinator = ' ';

    void AddSelector(nsCSSSelector* aSelector);
    void Merge(const Branch& aOther);
    bool MightDependOnAttribute(mozilla::dom::Element* aElement,
                                nsIAtom* aAttribute,
                                const nsAttrValue* aNewClasses,
                                bool aCompareClasses) const;
    bool IsSibling() const {
      return mCombinator == '+' || mCombinator == '~';
    }
  };

  explicit nsCSSHasSelectorData(nsCSSSelectorList* aList);
  nsTArray<Branch> mBranches;
  bool mHasSibling = false;
};

struct nsCSSRuleUtils
{
  // Stored on nodes under hasSelectorDependency. Dependencies accumulate
  // across matching passes, so a failed/short-circuited branch stays watched.
  struct HasSelectorDependency {
    // Merge rules with the same search relationship. Retaining a reference
    // to every rule ever matched would grow without bound on long-lived pages
    // that replace stylesheets. Only the most recent registration is cached.
    nsTArray<nsCSSHasSelectorData::Branch> mBranches;
    nsTArray<nsCSSHasSelectorData::Branch> mSiblingBranches;
    RefPtr<nsCSSHasSelectorData> mLastSelector;
    RefPtr<nsCSSHasSelectorData> mLastSiblingSelector;
    bool mRestyleLaterSiblings = false;

    void AddSelectorData(nsCSSHasSelectorData* aData, bool aSibling);
    bool MightDependOnChange(Element* aAnchor, nsINode* aNode,
                             mozilla::EventStates aStateMask,
                             nsIAtom* aAttribute,
                             const nsAttrValue* aNewClasses,
                             bool aCompareClasses,
                             bool aSibling,
                             bool aNodeIsFollowingSibling = false) const;
    bool MightAffectSiblingAnchor(nsINode* aParent, nsINode* aNode,
                                  mozilla::EventStates aStateMask,
                                  nsIAtom* aAttribute,
                                  const nsAttrValue* aNewClasses,
                                  bool aCompareClasses) const;
  };

  static void Startup();
  static void Shutdown();
  static void FreeSystemMetrics();
  static bool HasSystemMetric(nsIAtom* aMetric);

  static bool LoadImportedSheetsInOrderEnabled();

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

  static bool RelativeSelectorListMatches(
    Element* aAnchor,
    nsCSSSelectorList* aList,
    TreeMatchContext& aTreeMatchContext);

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
