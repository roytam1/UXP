/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * data structures passed to nsIStyleRuleProcessor methods (to pull loop
 * invariant computations out of the loop)
 */

#ifndef nsRuleProcessorData_h_
#define nsRuleProcessorData_h_

#include "nsAutoPtr.h"
#include "nsChangeHint.h"
#include "nsCompatibility.h"
#include "nsCSSPseudoElements.h"
#include "nsRuleWalker.h"
#include "nsNthIndexCache.h"
#include "nsILoadContext.h"
#include "nsIDocument.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/EventStates.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/dom/Element.h"

class nsIAtom;
class nsIContent;
class nsICSSPseudoComparator;
struct TreeMatchContext;

/**
 * An AncestorFilter is used to keep track of ancestors so that we can
 * quickly tell that a particular selector is not relevant to a given
 * element.
 */
class MOZ_STACK_CLASS AncestorFilter {
  friend struct TreeMatchContext;
 public:
  /* Maintenance of our ancestor state */
  void PushAncestor(mozilla::dom::Element *aElement);
  void PopAncestor();

  /* Check whether we might have an ancestor matching one of the given
     atom hashes.  |hashes| must have length hashListLength */
  template<size_t hashListLength>
    bool MightHaveMatchingAncestor(const uint32_t* aHashes) const
  {
    MOZ_ASSERT(mFilter);
    for (size_t i = 0; i < hashListLength && aHashes[i]; ++i) {
      if (!mFilter->mightContain(aHashes[i])) {
        return false;
      }
    }

    return true;
  }

  bool HasFilter() const { return mFilter; }

#ifdef DEBUG
  void AssertHasAllAncestors(mozilla::dom::Element *aElement) const;
#endif
  
 private:
  // Using 2^12 slots makes the Bloom filter a nice round page in
  // size, so let's do that.  We get a false positive rate of 1% or
  // less even with several hundred things in the filter.  Note that
  // we allocate the filter lazily, because not all tree match
  // contexts can use one effectively.
  typedef mozilla::BloomFilter<12, nsIAtom> Filter;
  nsAutoPtr<Filter> mFilter;

  // Stack of indices to pop to.  These are indices into mHashes.
  nsTArray<uint32_t> mPopTargets;

  // List of hashes; this is what we pop using mPopTargets.  We store
  // hashes of our ancestor element tag names, ids, and classes in
  // here.
  nsTArray<uint32_t> mHashes;

  // A debug-only stack of Elements for use in assertions
#ifdef DEBUG
  nsTArray<mozilla::dom::Element*> mElements;
#endif
};

/**
 * A |TreeMatchContext| has data about a matching operation.  The
 * data are not node-specific but are invariants of the DOM tree the
 * nodes being matched against are in.
 *
 * Most of the members are in parameters to selector matching.  The
 * one out parameter is mHaveRelevantLink.  Consumers that use a
 * TreeMatchContext for more than one matching operation and care
 * about :visited and mHaveRelevantLink need to
 * ResetForVisitedMatching() and ResetForUnvisitedMatching() as
 * needed.
 */
struct MOZ_STACK_CLASS TreeMatchContext {
  // Reset this context for matching for the style-if-:visited.
  void ResetForVisitedMatching() {
    NS_PRECONDITION(mForStyling, "Why is this being called?");
    mHaveRelevantLink = false;
    mVisitedHandling = nsRuleWalker::eRelevantLinkVisited;
  }
  
  void ResetForUnvisitedMatching() {
    NS_PRECONDITION(mForStyling, "Why is this being called?");
    mHaveRelevantLink = false;
    mVisitedHandling = nsRuleWalker::eRelevantLinkUnvisited;
  }

  void SetHaveRelevantLink() { mHaveRelevantLink = true; }
  bool HaveRelevantLink() const { return mHaveRelevantLink; }

  nsRuleWalker::VisitedHandlingType VisitedHandling() const
  {
    return mVisitedHandling;
  }

  void AddScopeElement(mozilla::dom::Element* aElement) {
    NS_PRECONDITION(mHaveSpecifiedScope,
                    "Should be set before calling AddScopeElement()");
    mScopes.AppendElement(aElement);
  }
  bool IsScopeElement(mozilla::dom::Element* aElement) const {
    return mScopes.Contains(aElement);
  }
  void SetHasSpecifiedScope() {
    mHaveSpecifiedScope = true;
  }
  bool HasSpecifiedScope() const {
    return mHaveSpecifiedScope;
  }

  /**
   * Initialize the ancestor filter and list of style scopes.  If aElement is
   * not null, it and all its ancestors will be passed to
   * mAncestorFilter.PushAncestor and PushStyleScope, starting from the root and
   * going down the tree.  Must only be called for elements in a document.
   */
  void InitAncestors(mozilla::dom::Element *aElement);

  /**
   * Like InitAncestors, but only initializes the style scope list, not the
   * ancestor filter.  May be called for elements outside a document.
   */
  void InitStyleScopes(mozilla::dom::Element* aElement);

  void PushStyleScope(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (aElement->IsScopedStyleRoot()) {
      mStyleScopes.AppendElement(aElement);
    }
  }

  void PopStyleScope(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (mStyleScopes.SafeLastElement(nullptr) == aElement) {
      mStyleScopes.TruncateLength(mStyleScopes.Length() - 1);
    }
  }

  void PushShadowHost(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (aElement->GetShadowRoot()) {
      mShadowHosts.AppendElement(aElement);
    }
  }

  void PopShadowHost(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (mShadowHosts.SafeLastElement(nullptr) == aElement) {
      mShadowHosts.TruncateLength(mShadowHosts.Length() - 1);
    }
  }

 
  bool PopStyleScopeForSelectorMatching(mozilla::dom::Element* aElement)
  {
    NS_ASSERTION(mForScopedStyle, "only call PopStyleScopeForSelectorMatching "
                                  "when mForScopedStyle is true");

    if (!mCurrentStyleScope) {
      return false;
    }
    if (mCurrentStyleScope == aElement) {
      mCurrentStyleScope = nullptr;
    }
    return true;
  }

#ifdef DEBUG
  void AssertHasAllStyleScopes(mozilla::dom::Element* aElement) const;
#endif

  bool SetStyleScopeForSelectorMatching(mozilla::dom::Element* aSubject,
                                        mozilla::dom::Element* aScope)
  {
#ifdef DEBUG
    AssertHasAllStyleScopes(aSubject);
#endif

    mForScopedStyle = !!aScope;
    if (!aScope) {
      // This is not for a scoped style sheet; return true, as we want
      // selector matching to proceed.
      mCurrentStyleScope = nullptr;
      return true;
    }
    if (aScope == aSubject) {
      // Although the subject is the same element as the scope, as soon
      // as we continue with selector matching up the tree we don't want
      // to match any more elements.  So we return true to indicate that
      // we want to do the initial selector matching, but set
      // mCurrentStyleScope to null so that no ancestor elements will match.
      mCurrentStyleScope = nullptr;
      return true;
    }
    if (mStyleScopes.Contains(aScope)) {
      // mStyleScopes contains all of the scope elements that are ancestors of
      // aSubject, so if aScope is in mStyleScopes, then we do want selector
      // matching to proceed.
      mCurrentStyleScope = aScope;
      return true;
    }
    // Otherwise, we're not in the scope, and we don't want to proceed
    // with selector matching.
    mCurrentStyleScope = nullptr;
    return false;
  }

  bool IsWithinStyleScopeForSelectorMatching() const
  {
    NS_ASSERTION(mForScopedStyle, "only call IsWithinScopeForSelectorMatching "
                                  "when mForScopedStyle is true");
    return mCurrentStyleScope;
  }

  /* Helper class for maintaining the ancestor state */
  class MOZ_RAII AutoAncestorPusher {
  public:
    explicit AutoAncestorPusher(TreeMatchContext& aTreeMatchContext
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mPushedAncestor(false)
      , mPushedStyleScope(false)
      , mPushedShadowHost(false)
      , mTreeMatchContext(aTreeMatchContext)
      , mElement(nullptr)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void PushAncestorAndStyleScope(mozilla::dom::Element* aElement) {
      MOZ_ASSERT(!mElement);
      if (aElement) {
        mElement = aElement;
        mPushedAncestor = true;
        mPushedStyleScope = true;
	mPushedShadowHost = true;
        mTreeMatchContext.mAncestorFilter.PushAncestor(aElement);
        mTreeMatchContext.PushStyleScope(aElement);
	mTreeMatchContext.PushShadowHost(aElement);
      }
    }

    void PushAncestorAndStyleScope(nsIContent* aContent) {
      if (aContent && aContent->IsElement()) {
        PushAncestorAndStyleScope(aContent->AsElement());
      }
    }

    void PushStyleScope(mozilla::dom::Element* aElement) {
      MOZ_ASSERT(!mElement);
      if (aElement) {
        mElement = aElement;
        mPushedStyleScope = true;
        mTreeMatchContext.PushStyleScope(aElement);
      }
    }

    void PushStyleScope(nsIContent* aContent) {
      if (aContent && aContent->IsElement()) {
        PushStyleScope(aContent->AsElement());
      }
    }

    ~AutoAncestorPusher() {
      if (mPushedAncestor) {
        mTreeMatchContext.mAncestorFilter.PopAncestor();
      }
      if (mPushedStyleScope) {
        mTreeMatchContext.PopStyleScope(mElement);
      }
      if (mPushedShadowHost) {
	mTreeMatchContext.PopShadowHost(mElement);
      }
    }

  private:
    bool mPushedAncestor;
    bool mPushedStyleScope;
    bool mPushedShadowHost;
    TreeMatchContext& mTreeMatchContext;
    mozilla::dom::Element* mElement;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /* Helper class for tracking whether we're skipping the ApplyStyleFixups
   * code for special cases where child element style is modified based on
   * parent display value.
   *
   * The optional second parameter aSkipParentDisplayBasedStyleFixup allows
   * this class to be instantiated but only conditionally activated (e.g.
   * in cases where we may or may not want to be skipping flex/grid-item
   * style fixup for a particular chunk of code).
   */
  class MOZ_RAII AutoParentDisplayBasedStyleFixupSkipper {
  public:
    explicit AutoParentDisplayBasedStyleFixupSkipper(TreeMatchContext& aTreeMatchContext,
                                                     bool aSkipParentDisplayBasedStyleFixup = true
                                                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mAutoRestorer(aTreeMatchContext.mSkippingParentDisplayBasedStyleFixup)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      if (aSkipParentDisplayBasedStyleFixup) {
        aTreeMatchContext.mSkippingParentDisplayBasedStyleFixup = true;
      }
    }

  private:
    mozilla::AutoRestore<bool> mAutoRestorer;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  // Is this matching operation for the creation of a style context?
  // (If it is, we need to set slow selector bits on nodes indicating
  // that certain restyling needs to happen.)
  const bool mForStyling;

 private:
  // When mVisitedHandling is eRelevantLinkUnvisited, this is set to true if a
  // relevant link (see explanation in definition of VisitedHandling enum) was
  // encountered during the matching process, which means that matching needs
  // to be rerun with eRelevantLinkVisited.  Otherwise, its behavior is
  // undefined (it might get set appropriately, or might not).
  bool mHaveRelevantLink;

  // If true, then our contextual reference element set is specified,
  // and is given by mScopes.
  bool mHaveSpecifiedScope;

  // How matching should be performed.  See the documentation for
  // nsRuleWalker::VisitedHandlingType.
  nsRuleWalker::VisitedHandlingType mVisitedHandling;

  // For matching :scope
  AutoTArray<mozilla::dom::Element*, 1> mScopes;
 public:
  // The document we're working with.
  nsIDocument* const mDocument;

  // Only selectors that contain :host or :host-context pseudo class
  // should be matched against elements. All other selectors should not
  // match.
  bool mOnlyMatchHostPseudo;

  // Restrict matching to selectors that contain a :slotted() pseudo-class.
  bool mRestrictToSlottedPseudo;

  // Root of scoped stylesheet (set and unset by the supplier of the
  // scoped stylesheet).
  nsIContent* mScopedRoot;

  // Whether our document is HTML (as opposed to XML of some sort,
  // including XHTML).
  // XXX XBL2 issue: Should we be caching this?  What should it be for XBL2?
  const bool mIsHTMLDocument;

  // Possibly remove use of mCompatMode in SelectorMatches?
  // XXX XBL2 issue: Should we be caching this?  What should it be for XBL2?
  const nsCompatibility mCompatMode;

  // The nth-index cache we should use
  nsNthIndexCache mNthIndexCache;

  // An ancestor filter
  AncestorFilter mAncestorFilter;

  // Whether this document is using PB mode
  bool mUsingPrivateBrowsing;

  // Whether we're currently skipping the part of ApplyStyleFixups that changes
  // style of child elements based on their parent's display value
  // (e.g. for children of elements that have a mandatory frame-type for which
  // we ignore "display:flex/grid").
  bool mSkippingParentDisplayBasedStyleFixup;

  // Whether this TreeMatchContext is being used with an nsCSSRuleProcessor
  // for an HTML5 scoped style sheet.
  bool mForScopedStyle;

  // Whether we're currently in the topmost scope for shadow DOM.
  bool mIsTopmostScope;

  // Whether we're testing for the assigned slot instead of the slottable
  // when matching type/class/ID/attribute.
  bool mForAssignedSlot;

  enum MatchVisited {
    eNeverMatchVisited,
    eMatchVisitedDefault
  };

  // List of ancestor elements that define a style scope (due to having a
  // <style scoped> child).
  AutoTArray<mozilla::dom::Element*, 1> mStyleScopes;

  // List of ancestor elements that are a shadow root host.
  AutoTArray<mozilla::dom::Element*, 1> mShadowHosts;

  // The current style scope element for selector matching.
  mozilla::dom::Element* mCurrentStyleScope;

  // Constructor to use when creating a tree match context for styling
  TreeMatchContext(bool aForStyling,
                   nsRuleWalker::VisitedHandlingType aVisitedHandling,
                   nsIDocument* aDocument,
                   MatchVisited aMatchVisited = eMatchVisitedDefault)
    : mForStyling(aForStyling)
    , mHaveRelevantLink(false)
    , mHaveSpecifiedScope(false)
    , mVisitedHandling(aVisitedHandling)
    , mDocument(aDocument)
    , mOnlyMatchHostPseudo(false)
    , mRestrictToSlottedPseudo(false)
    , mScopedRoot(nullptr)
    , mIsHTMLDocument(aDocument->IsHTMLDocument())
    , mCompatMode(aDocument->GetCompatibilityMode())
    , mUsingPrivateBrowsing(false)
    , mSkippingParentDisplayBasedStyleFixup(false)
    , mForScopedStyle(false)
    , mCurrentStyleScope(nullptr)
    , mIsTopmostScope(false)
    , mForAssignedSlot(false)
  {
    if (aMatchVisited != eNeverMatchVisited) {
      nsILoadContext* loadContext = mDocument->GetLoadContext();
      if (loadContext) {
        mUsingPrivateBrowsing = loadContext->UsePrivateBrowsing();
      }
    }
  }

  enum ForFrameConstructionTag { ForFrameConstruction };

  TreeMatchContext(nsIDocument* aDocument,
                   ForFrameConstructionTag)
    : TreeMatchContext(true,
                       nsRuleWalker::eRelevantLinkUnvisited,
                       aDocument)
  {}
};

struct MOZ_STACK_CLASS RuleProcessorData {
  RuleProcessorData(nsPresContext* aPresContext,
                    nsRuleWalker* aRuleWalker)
    : mPresContext(aPresContext),
      mRuleWalker(aRuleWalker),
      mScope(nullptr)
  {
    NS_PRECONDITION(mPresContext, "Must have prescontext");
  }

  nsPresContext* const mPresContext;
  nsRuleWalker* const mRuleWalker; // Used to add rules to our results.
  mozilla::dom::Element* mScope;
};

/**
 * A |NodeMatchContext| has data about matching a selector (without
 * combinators) against a single node.  It contains only input to the
 * matching.
 *
 * Unlike |RuleProcessorData|, which is similar, a |NodeMatchContext|
 * can vary depending on the selector matching process.  In other words,
 * there might be multiple NodeMatchContexts corresponding to a single
 * node, but only one possible RuleProcessorData.
 */
struct NodeMatchContext
{
  // In order to implement nsCSSRuleProcessor::HasStateDependentStyle,
  // we need to be able to see if a node might match an
  // event-state-dependent selector for any value of that event state.
  // So mStateMask contains the states that should NOT be tested.
  //
  // NOTE: For |mStateMask| to work correctly, it's important that any
  // change that changes multiple state bits include all those state
  // bits in the notification.  Otherwise, if multiple states change but
  // we do separate notifications then we might determine the style is
  // not state-dependent when it really is (e.g., determining that a
  // :hover:active rule no longer matches when both states are unset).
  const mozilla::EventStates mStateMask;

  // Is this link the unique link whose visitedness can affect the style
  // of the node being matched?  (That link is the nearest link to the
  // node being matched that is itself or an ancestor.)
  //
  // Always false when TreeMatchContext::mForStyling is false.  (We
  // could figure it out for RestrictedSelectorListMatches, but we're
  // starting from the middle of the selector list when doing
  // Has{Attribute,State}DependentStyle, so we can't tell.  So when
  // mForStyling is false, we have to assume we don't know.)
  const bool mIsRelevantLink;

  // If the node should be considered featureless (as specified in
  // selectors 4), then mIsFeature should be set to true to prevent
  // matching unless the selector is a special pseudo class or pseudo
  // element that matches featureless elements.
  const bool mIsFeatureless;

  NodeMatchContext(mozilla::EventStates aStateMask,
                   bool aIsRelevantLink,
                   bool aIsFeatureless = false)
    : mStateMask(aStateMask)
    , mIsRelevantLink(aIsRelevantLink)
    , mIsFeatureless(aIsFeatureless)
  {
  }
};

struct MOZ_STACK_CLASS ElementDependentRuleProcessorData :
                          public RuleProcessorData {
  ElementDependentRuleProcessorData(nsPresContext* aPresContext,
                                    mozilla::dom::Element* aElement,
                                    nsRuleWalker* aRuleWalker,
                                    TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aRuleWalker)
    , mElement(aElement)
    , mTreeMatchContext(aTreeMatchContext)
    , mElementIsFeatureless(false)
  {
    NS_ASSERTION(aElement, "null element leaked into SelectorMatches");
    NS_ASSERTION(aElement->OwnerDoc(), "Document-less node here?");
    NS_PRECONDITION(aTreeMatchContext.mForStyling == !!aRuleWalker,
                    "Should be styling if and only if we have a rule walker");
  }
  
  mozilla::dom::Element* const mElement; // weak ref, must not be null
  TreeMatchContext& mTreeMatchContext;
  bool mElementIsFeatureless;
};

struct MOZ_STACK_CLASS ElementRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aElement, 
                           nsRuleWalker* aRuleWalker,
                           TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, aRuleWalker,
                                        aTreeMatchContext)
  {
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }
};

struct MOZ_STACK_CLASS PseudoElementRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  PseudoElementRuleProcessorData(nsPresContext* aPresContext,
                                 mozilla::dom::Element* aParentElement,
                                 nsRuleWalker* aRuleWalker,
                                 mozilla::CSSPseudoElementType aPseudoType,
                                 TreeMatchContext& aTreeMatchContext,
                                 mozilla::dom::Element* aPseudoElement)
    : ElementDependentRuleProcessorData(aPresContext, aParentElement, aRuleWalker,
                                        aTreeMatchContext),
      mPseudoType(aPseudoType),
      mPseudoElement(aPseudoElement)
  {
    NS_PRECONDITION(aPseudoType < mozilla::CSSPseudoElementType::Count,
                    "invalid aPseudoType value");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }

  mozilla::CSSPseudoElementType mPseudoType;
  mozilla::dom::Element* const mPseudoElement; // weak ref
};

struct MOZ_STACK_CLASS AnonBoxRuleProcessorData : public RuleProcessorData {
  AnonBoxRuleProcessorData(nsPresContext* aPresContext,
                           nsIAtom* aPseudoTag,
                           nsRuleWalker* aRuleWalker)
    : RuleProcessorData(aPresContext, aRuleWalker),
      mPseudoTag(aPseudoTag)
  {
    NS_PRECONDITION(aPseudoTag, "Must have pseudo tag");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }

  nsIAtom* mPseudoTag;
};

#ifdef MOZ_XUL
struct MOZ_STACK_CLASS XULTreeRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  XULTreeRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aParentElement,
                           nsRuleWalker* aRuleWalker,
                           nsIAtom* aPseudoTag,
                           nsICSSPseudoComparator* aComparator,
                           TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aParentElement,
                                        aRuleWalker, aTreeMatchContext),
      mPseudoTag(aPseudoTag),
      mComparator(aComparator)
  {
    NS_PRECONDITION(aPseudoTag, "null pointer");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
    NS_PRECONDITION(aComparator, "must have a comparator");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
  }

  nsIAtom*                 mPseudoTag;
  nsICSSPseudoComparator*  mComparator;
};
#endif

struct MOZ_STACK_CLASS StateRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  StateRuleProcessorData(nsPresContext* aPresContext,
                         mozilla::dom::Element* aElement,
                         mozilla::EventStates aStateMask,
                         TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, nullptr,
                                        aTreeMatchContext),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  // |HasStateDependentStyle| for which state(s)?
  // Constants defined in mozilla/EventStates.h .
  const mozilla::EventStates mStateMask;
};

struct MOZ_STACK_CLASS PseudoElementStateRuleProcessorData :
                          public StateRuleProcessorData {
  PseudoElementStateRuleProcessorData(nsPresContext* aPresContext,
                                      mozilla::dom::Element* aElement,
                                      mozilla::EventStates aStateMask,
                                      mozilla::CSSPseudoElementType aPseudoType,
                                      TreeMatchContext& aTreeMatchContext,
                                      mozilla::dom::Element* aPseudoElement)
    : StateRuleProcessorData(aPresContext, aElement, aStateMask,
                             aTreeMatchContext),
      mPseudoType(aPseudoType),
      mPseudoElement(aPseudoElement)
  {
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }

  // We kind of want to inherit from both StateRuleProcessorData and
  // PseudoElementRuleProcessorData.  Instead we've just copied those
  // members from PseudoElementRuleProcessorData to this struct.
  mozilla::CSSPseudoElementType mPseudoType;
  mozilla::dom::Element* const mPseudoElement; // weak ref
};

struct MOZ_STACK_CLASS AttributeRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                             mozilla::dom::Element* aElement,
                             int32_t aNameSpaceID,
                             nsIAtom* aAttribute,
                             int32_t aModType,
                             bool aAttrHasChanged,
                             const nsAttrValue* aOtherValue,
                             TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, nullptr,
                                        aTreeMatchContext),
      mNameSpaceID(aNameSpaceID),
      mAttribute(aAttribute),
      mOtherValue(aOtherValue),
      mModType(aModType),
      mAttrHasChanged(aAttrHasChanged)
  {
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  int32_t mNameSpaceID; // Namespace of the attribute involved.
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  // non-null if we have the value.
  const nsAttrValue* mOtherValue;
  int32_t mModType;    // The type of modification (see nsIDOMMutationEvent).
  bool mAttrHasChanged; // Whether the attribute has already changed.
};

/**
 * Additional information about a selector (without combinators) that is
 * being matched.
 */
enum class SelectorMatchesFlags : uint8_t
{
  NONE = 0,

  // The selector's flags are unknown.  This happens when you don't know
  // if you're starting from the top of a selector.  Only used in cases
  // where it's acceptable for matching to return a false positive.
  // (It's not OK to return a false negative.)
  UNKNOWN = 1 << 0,

  // The selector is part of a compound selector which has been split in
  // half, where the other half is a pseudo-element.  The current
  // selector is not a pseudo-element itself.
  HAS_PSEUDO_ELEMENT = 1 << 1,

  // The selector is part of an argument to a functional pseudo-class or
  // pseudo-element.
  IS_PSEUDO_CLASS_ARGUMENT = 1 << 2,

  // The selector should be blocked from matching because it is called
  // from outside the shadow tree.
  IS_OUTSIDE_SHADOW_TREE = 1 << 3
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SelectorMatchesFlags)

/**
 * Flags for SelectorMatchesTree.
 */
enum SelectorMatchesTreeFlags
{
  // Whether we still have not found the closest ancestor link element and
  // thus have to check the current element for it.
  eLookForRelevantLink = 0x1,

  // Whether SelectorMatchesTree should check for, and return true upon
  // finding, an ancestor element that has an eRestyle_SomeDescendants
  // restyle hint pending.
  eMatchOnConditionalRestyleAncestor = 0x2,
};

#endif /* !defined(nsRuleProcessorData_h_) */
