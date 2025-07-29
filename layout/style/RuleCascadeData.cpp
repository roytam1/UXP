/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PL_ARENA_CONST_ALIGN_MASK 7
// We want page-sized arenas so there's no fragmentation involved.
// Including plarena.h must come first to avoid it being included by some
// header file thereby making PL_ARENA_CONST_ALIGN_MASK ineffective.
#define NS_WEIGHTEDRULEDATA_ARENA_BLOCK_SIZE (4096)
#include "plarena.h"

#include "RuleCascadeData.h"
#include "nsAutoPtr.h"
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include <algorithm>
#include "nsIAtom.h"
#include "PLDHashTable.h"
#include "nsICSSPseudoComparator.h"
#include "mozilla/MemoryReporting.h"
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
#include "nsCSSRuleUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// ------------------------------
// Rule hash table
//

// Uses any of the sets of ops below.
struct RuleHashTableEntry : public PLDHashEntryHdr
{
  // If you add members that have heap allocated memory be sure to change the
  // logic in SizeOfRuleHashTable().
  // Auto length 1, because we always have at least one entry in mRules.
  AutoTArray<RuleValue, 1> mRules;
};

struct RuleHashTagTableEntry : public RuleHashTableEntry
{
  // If you add members that have heap allocated memory be sure to change the
  // logic in RuleHash::SizeOf{In,Ex}cludingThis.
  nsCOMPtr<nsIAtom> mTag;
};

static PLDHashNumber
RuleHash_CIHashKey(const void* key)
{
  nsIAtom* atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));

  nsAutoString str;
  atom->ToString(str);
  nsContentUtils::ASCIIToLower(str);
  return HashString(str);
}

static inline nsCSSSelector*
SubjectSelectorForRuleHash(const PLDHashEntryHdr* hdr)
{
  auto entry = static_cast<const RuleHashTableEntry*>(hdr);
  nsCSSSelector* selector = entry->mRules[0].mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  return selector;
}

static inline bool
CIMatchAtoms(const void* key, nsIAtom* entry_atom)
{
  auto match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));

  // Check for case-sensitive match first.
  if (match_atom == entry_atom) {
    return true;
  }

  // Use EqualsIgnoreASCIICase instead of full on unicode case conversion
  // in order to save on performance. This is only used in quirks mode
  // anyway.
  return nsContentUtils::EqualsIgnoreASCIICase(
    nsDependentAtomString(entry_atom), nsDependentAtomString(match_atom));
}

static inline bool
CSMatchAtoms(const void* key, nsIAtom* entry_atom)
{
  auto match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  return match_atom == entry_atom;
}

static bool
RuleHash_ClassCIMatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  return CIMatchAtoms(key, SubjectSelectorForRuleHash(hdr)->mClassList->mAtom);
}

static bool
RuleHash_IdCIMatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  return CIMatchAtoms(key, SubjectSelectorForRuleHash(hdr)->mIDList->mAtom);
}

static bool
RuleHash_ClassCSMatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  return CSMatchAtoms(key, SubjectSelectorForRuleHash(hdr)->mClassList->mAtom);
}

static bool
RuleHash_IdCSMatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  return CSMatchAtoms(key, SubjectSelectorForRuleHash(hdr)->mIDList->mAtom);
}

static void
RuleHash_InitEntry(PLDHashEntryHdr* hdr, const void* key)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  new (KnownNotNull, entry) RuleHashTableEntry();
}

static void
RuleHash_ClearEntry(PLDHashTable* table, PLDHashEntryHdr* hdr)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  entry->~RuleHashTableEntry();
}

static void
RuleHash_MoveEntry(PLDHashTable* table,
                   const PLDHashEntryHdr* from,
                   PLDHashEntryHdr* to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  RuleHashTableEntry* oldEntry = const_cast<RuleHashTableEntry*>(
    static_cast<const RuleHashTableEntry*>(from));
  auto* newEntry = new (KnownNotNull, to) RuleHashTableEntry();
  newEntry->mRules.SwapElements(oldEntry->mRules);
  oldEntry->~RuleHashTableEntry();
}

static bool
RuleHash_TagTable_MatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  nsIAtom* match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  nsIAtom* entry_atom = static_cast<const RuleHashTagTableEntry*>(hdr)->mTag;

  return match_atom == entry_atom;
}

static void
RuleHash_TagTable_InitEntry(PLDHashEntryHdr* hdr, const void* key)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  new (KnownNotNull, entry) RuleHashTagTableEntry();
  entry->mTag = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
}

static void
RuleHash_TagTable_ClearEntry(PLDHashTable* table, PLDHashEntryHdr* hdr)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  entry->~RuleHashTagTableEntry();
}

static void
RuleHash_TagTable_MoveEntry(PLDHashTable* table,
                            const PLDHashEntryHdr* from,
                            PLDHashEntryHdr* to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  RuleHashTagTableEntry* oldEntry = const_cast<RuleHashTagTableEntry*>(
    static_cast<const RuleHashTagTableEntry*>(from));
  auto* newEntry = new (KnownNotNull, to) RuleHashTagTableEntry();
  newEntry->mTag.swap(oldEntry->mTag);
  newEntry->mRules.SwapElements(oldEntry->mRules);
  oldEntry->~RuleHashTagTableEntry();
}

static PLDHashNumber
RuleHash_NameSpaceTable_HashKey(const void* key)
{
  return NS_PTR_TO_INT32(key);
}

static bool
RuleHash_NameSpaceTable_MatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  const RuleHashTableEntry* entry = static_cast<const RuleHashTableEntry*>(hdr);

  nsCSSSelector* selector = entry->mRules[0].mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  return NS_PTR_TO_INT32(key) == selector->mNameSpace;
}

/* static */ const PLDHashTableOps RuleHash::TagTable_Ops = {
  PLDHashTable::HashVoidPtrKeyStub,
  RuleHash_TagTable_MatchEntry,
  RuleHash_TagTable_MoveEntry,
  RuleHash_TagTable_ClearEntry,
  RuleHash_TagTable_InitEntry
};

// Case-sensitive ops.
/* static */ const PLDHashTableOps RuleHash::ClassTable_CSOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  RuleHash_ClassCSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

// Case-insensitive ops.
/* static */ const PLDHashTableOps RuleHash::ClassTable_CIOps = {
  RuleHash_CIHashKey,
  RuleHash_ClassCIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

// Case-sensitive ops.
/* static */ const PLDHashTableOps RuleHash::IdTable_CSOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  RuleHash_IdCSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

// Case-insensitive ops.
/* static */ const PLDHashTableOps RuleHash::IdTable_CIOps = {
  RuleHash_CIHashKey,
  RuleHash_IdCIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

/* static */ const PLDHashTableOps RuleHash::NameSpaceTable_Ops = {
  RuleHash_NameSpaceTable_HashKey,
  RuleHash_NameSpaceTable_MatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

#undef RULE_HASH_STATS
#undef PRINT_UNIVERSAL_RULES

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT(var_)                                         \
  PR_BEGIN_MACRO++(var_);                                                      \
  PR_END_MACRO
#else
#define RULE_HASH_STAT_INCREMENT(var_) PR_BEGIN_MACRO PR_END_MACRO
#endif

RuleHash::RuleHash(bool aQuirksMode)
  : mRuleCount(0)
  , mIdTable(aQuirksMode ? &RuleHash::IdTable_CIOps : &RuleHash::IdTable_CSOps,
             sizeof(RuleHashTableEntry))
  , mClassTable(aQuirksMode ? &RuleHash::ClassTable_CIOps
                            : &RuleHash::ClassTable_CSOps,
                sizeof(RuleHashTableEntry))
  , mTagTable(&RuleHash::TagTable_Ops, sizeof(RuleHashTagTableEntry))
  , mNameSpaceTable(&RuleHash::NameSpaceTable_Ops, sizeof(RuleHashTableEntry))
  , mUniversalRules(0)
  , mEnumList(nullptr)
  , mEnumListSize(0)
  , mQuirksMode(aQuirksMode)
#ifdef RULE_HASH_STATS
  , mUniversalSelectors(0)
  , mNameSpaceSelectors(0)
  , mTagSelectors(0)
  , mClassSelectors(0)
  , mIdSelectors(0)
  , mElementsMatched(0)
  , mElementUniversalCalls(0)
  , mElementNameSpaceCalls(0)
  , mElementTagCalls(0)
  , mElementClassCalls(0)
  , mElementIdCalls(0)
#endif
{
  MOZ_COUNT_CTOR(RuleHash);
}

RuleHash::~RuleHash()
{
  MOZ_COUNT_DTOR(RuleHash);
#ifdef RULE_HASH_STATS
  printf("RuleHash(%p):\n"
         "  Selectors: Universal (%u) NameSpace(%u) Tag(%u) Class(%u) Id(%u)\n"
         "  Content Nodes: Elements(%u)\n"
         "  Element Calls: Universal(%u) NameSpace(%u) Tag(%u) Class(%u) "
         "Id(%u)\n" static_cast<void*>(this),
         mUniversalSelectors,
         mNameSpaceSelectors,
         mTagSelectors,
         mClassSelectors,
         mIdSelectors,
         mElementsMatched,
         mElementUniversalCalls,
         mElementNameSpaceCalls,
         mElementTagCalls,
         mElementClassCalls,
         mElementIdCalls);
#ifdef PRINT_UNIVERSAL_RULES
  {
    if (mUniversalRules.Length() > 0) {
      printf("  Universal rules:\n");
      for (uint32_t i = 0; i < mUniversalRules.Length(); ++i) {
        RuleValue* value = &(mUniversalRules[i]);
        nsAutoString selectorText;
        uint32_t lineNumber = value->mRule->GetLineNumber();
        RefPtr<CSSStyleSheet> cssSheet = value->mRule->GetStyleSheet();
        value->mSelector->ToString(selectorText, cssSheet);

        printf("    line %d, %s\n",
               lineNumber,
               NS_ConvertUTF16toUTF8(selectorText).get());
      }
    }
  }
#endif // PRINT_UNIVERSAL_RULES
#endif // RULE_HASH_STATS
  // Rule Values are arena allocated no need to delete them. Their destructor
  // isn't doing any cleanup. So we dont even bother to enumerate through
  // the hash tables and call their destructors.
  if (nullptr != mEnumList) {
    delete[] mEnumList;
  }
}

void
RuleHash::AppendRuleToTable(PLDHashTable* aTable,
                            const void* aKey,
                            const RuleSelectorPair& aRuleInfo)
{
  // Get a new or existing entry.
  auto entry = static_cast<RuleHashTableEntry*>(aTable->Add(aKey, fallible));
  if (!entry)
    return;
  entry->mRules.AppendElement(RuleValue(aRuleInfo, mRuleCount++, mQuirksMode));
}

/* static */ void
RuleHash::AppendRuleToTagTable(PLDHashTable* aTable,
                               nsIAtom* aKey,
                               const RuleValue& aRuleInfo)
{
  // Get a new or exisiting entry
  auto entry = static_cast<RuleHashTagTableEntry*>(aTable->Add(aKey, fallible));
  if (!entry)
    return;

  entry->mRules.AppendElement(aRuleInfo);
}

void
RuleHash::AppendUniversalRule(const RuleSelectorPair& aRuleInfo)
{
  mUniversalRules.AppendElement(
    RuleValue(aRuleInfo, mRuleCount++, mQuirksMode));
}

void
RuleHash::AppendRule(const RuleSelectorPair& aRuleInfo)
{
  nsCSSSelector* selector = aRuleInfo.mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  if (nullptr != selector->mIDList) {
    AppendRuleToTable(&mIdTable, selector->mIDList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mIdSelectors);
  } else if (nullptr != selector->mClassList) {
    AppendRuleToTable(&mClassTable, selector->mClassList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mClassSelectors);
  } else if (selector->mLowercaseTag) {
    RuleValue ruleValue(aRuleInfo, mRuleCount++, mQuirksMode);
    AppendRuleToTagTable(&mTagTable, selector->mLowercaseTag, ruleValue);
    RULE_HASH_STAT_INCREMENT(mTagSelectors);
    if (selector->mCasedTag && selector->mCasedTag != selector->mLowercaseTag) {
      AppendRuleToTagTable(&mTagTable, selector->mCasedTag, ruleValue);
      RULE_HASH_STAT_INCREMENT(mTagSelectors);
    }
  } else if (kNameSpaceID_Unknown != selector->mNameSpace) {
    AppendRuleToTable(
      &mNameSpaceTable, NS_INT32_TO_PTR(selector->mNameSpace), aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mNameSpaceSelectors);
  } else { // universal tag selector
    AppendUniversalRule(aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mUniversalSelectors);
  }
}

// this should cover practically all cases so we don't need to reallocate
#define MIN_ENUM_LIST_SIZE 8

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_)                       \
  (var_) += (list_).Length()
#else
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_)                       \
  PR_BEGIN_MACRO PR_END_MACRO
#endif

static inline bool
LookForTargetPseudo(nsCSSSelector* aSelector,
                    TreeMatchContext* aMatchContext,
                    nsRestyleHint* possibleChange)
{
  if (aMatchContext->mOnlyMatchHostPseudo) {
    while (aSelector && aSelector->mNext != nullptr) {
      aSelector = aSelector->mNext;
    }

    for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
         pseudoClass;
         pseudoClass = pseudoClass->mNext) {
      if (pseudoClass->mType == CSSPseudoClassType::host ||
          pseudoClass->mType == CSSPseudoClassType::hostContext) {
        if (possibleChange) {
          // :host-context will walk ancestors looking for a match of a
          // compound selector, thus any changes to ancestors may require
          // restyling the subtree.
          *possibleChange |= eRestyle_Subtree;
        }
        return true;
      }
    }
    return false;
  } else if (aMatchContext->mRestrictToSlottedPseudo) {
    for (nsCSSSelector* selector = aSelector; selector;
         selector = selector->mNext) {
      if (!selector->mPseudoClassList) {
        continue;
      }
      for (nsPseudoClassList* pseudoClass = selector->mPseudoClassList;
           pseudoClass;
           pseudoClass = pseudoClass->mNext) {
        if (pseudoClass->mType == CSSPseudoClassType::slotted) {
          return true;
        }
      }
    }
    return false;
  }
  // We're not restricted to a specific pseudo-class.
  return true;
}

static inline void
ContentEnumFunc(const RuleValue& value,
                nsCSSSelector* aSelector,
                ElementDependentRuleProcessorData* data,
                NodeMatchContext& nodeContext,
                AncestorFilter* ancestorFilter)
{
  if (nodeContext.mIsRelevantLink) {
    data->mTreeMatchContext.SetHaveRelevantLink();
  }
  // XXX: Ignore the ancestor filter if we're testing the assigned slot.
  bool useAncestorFilter = !(data->mTreeMatchContext.mForAssignedSlot);
  if (useAncestorFilter && ancestorFilter &&
      !ancestorFilter->MightHaveMatchingAncestor<RuleValue::eMaxAncestorHashes>(
        value.mAncestorSelectorHashes)) {
    // We won't match; nothing else to do here
    return;
  }

  if (!LookForTargetPseudo(aSelector, &data->mTreeMatchContext, nullptr)) {
    return;
  }

  if (!data->mTreeMatchContext.SetStyleScopeForSelectorMatching(data->mElement,
                                                                data->mScope)) {
    // The selector is for a rule in a scoped style sheet, and the subject
    // of the selector matching is not in its scope.
    return;
  }
  nsCSSSelector* selector = aSelector;
  if (selector->IsPseudoElement()) {
    PseudoElementRuleProcessorData* pdata =
      static_cast<PseudoElementRuleProcessorData*>(data);
    if (!pdata->mPseudoElement && selector->mPseudoClassList) {
      // We can get here when calling getComputedStyle(aElt, aPseudo) if:
      //
      //   * aPseudo is a pseudo-element that supports a user action
      //     pseudo-class, like "::placeholder";
      //   * there is a style rule that uses a pseudo-class on this
      //     pseudo-element in the document, like ::placeholder:hover; and
      //   * aElt does not have such a pseudo-element.
      //
      // We know that the selector can't match, since there is no element for
      // the user action pseudo-class to match against.
      return;
    }
    if (!nsCSSRuleUtils::StateSelectorMatches(pdata->mPseudoElement,
                                              aSelector,
                                              nodeContext,
                                              data->mTreeMatchContext,
                                              SelectorMatchesFlags::NONE)) {
      return;
    }
    selector = selector->mNext;
  }

  SelectorMatchesFlags selectorFlags = SelectorMatchesFlags::NONE;
  if (aSelector->IsPseudoElement()) {
    selectorFlags |= SelectorMatchesFlags::HAS_PSEUDO_ELEMENT;
  }
  if (nsCSSRuleUtils::SelectorMatches(data->mElement,
                                      selector,
                                      nodeContext,
                                      data->mTreeMatchContext,
                                      selectorFlags)) {
    nsCSSSelector* next = selector->mNext;
    if (!next || nsCSSRuleUtils::SelectorMatchesTree(
                   data->mElement,
                   next,
                   data->mTreeMatchContext,
                   nodeContext.mIsRelevantLink ? SelectorMatchesTreeFlags(0)
                                               : eLookForRelevantLink)) {
      css::Declaration* declaration = value.mRule->GetDeclaration();
      declaration->SetImmutable();
      data->mRuleWalker->Forward(declaration);
      // nsStyleSet will deal with the !important rule
    }
  }
}

void
RuleHash::EnumerateAllRules(Element* aElement,
                            ElementDependentRuleProcessorData* aData,
                            NodeMatchContext& aNodeContext)
{
  int32_t nameSpace = aElement->GetNameSpaceID();
  nsIAtom* tag = aElement->NodeInfo()->NameAtom();
  nsIAtom* id = aElement->GetID();
  const nsAttrValue* classList = aElement->GetClasses();

  MOZ_ASSERT(tag, "How could we not have a tag?");

  int32_t classCount = classList ? classList->GetAtomCount() : 0;

  // assume 1 universal, tag, id, and namespace, rather than wasting
  // time counting
  int32_t testCount = classCount + 4;

  if (mEnumListSize < testCount) {
    delete[] mEnumList;
    mEnumListSize = std::max(testCount, MIN_ENUM_LIST_SIZE);
    mEnumList = new EnumData[mEnumListSize];
  }

  int32_t valueCount = 0;
  RULE_HASH_STAT_INCREMENT(mElementsMatched);

  if (mUniversalRules.Length() != 0) { // universal rules
    mEnumList[valueCount++] = ToEnumData(mUniversalRules);
    RULE_HASH_STAT_INCREMENT_LIST_COUNT(mUniversalRules,
                                        mElementUniversalCalls);
  }
  // universal rules within the namespace
  if (kNameSpaceID_Unknown != nameSpace && mNameSpaceTable.EntryCount() > 0) {
    auto entry = static_cast<RuleHashTableEntry*>(
      mNameSpaceTable.Search(NS_INT32_TO_PTR(nameSpace)));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules,
                                          mElementNameSpaceCalls);
    }
  }
  if (mTagTable.EntryCount() > 0) {
    auto entry = static_cast<RuleHashTableEntry*>(mTagTable.Search(tag));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementTagCalls);
    }
  }
  if (id && mIdTable.EntryCount() > 0) {
    auto entry = static_cast<RuleHashTableEntry*>(mIdTable.Search(id));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementIdCalls);
    }
  }
  if (mClassTable.EntryCount() > 0) {
    for (int32_t index = 0; index < classCount; ++index) {
      auto entry = static_cast<RuleHashTableEntry*>(
        mClassTable.Search(classList->AtomAt(index)));
      if (entry) {
        mEnumList[valueCount++] = ToEnumData(entry->mRules);
        RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementClassCalls);
      }
    }
  }
  NS_ASSERTION(valueCount <= testCount, "values exceeded list size");

  if (valueCount > 0) {
    AncestorFilter* filter =
      aData->mTreeMatchContext.mAncestorFilter.HasFilter()
        ? &aData->mTreeMatchContext.mAncestorFilter
        : nullptr;
#ifdef DEBUG
    bool isRestricted = (aData->mTreeMatchContext.mShadowHosts.Length() > 0 ||
                         aData->mTreeMatchContext.mRestrictToSlottedPseudo ||
                         aData->mTreeMatchContext.mOnlyMatchHostPseudo ||
                         aData->mTreeMatchContext.mForAssignedSlot);
    if (filter && !isRestricted) {
      filter->AssertHasAllAncestors(aElement);
    }
#endif
    bool isForAssignedSlot = aData->mTreeMatchContext.mForAssignedSlot;
    // Merge the lists while there are still multiple lists to merge.
    while (valueCount > 1) {
      int32_t valueIndex = 0;
      int32_t lowestRuleIndex = mEnumList[valueIndex].mCurValue->mIndex;
      for (int32_t index = 1; index < valueCount; ++index) {
        int32_t ruleIndex = mEnumList[index].mCurValue->mIndex;
        if (ruleIndex < lowestRuleIndex) {
          valueIndex = index;
          lowestRuleIndex = ruleIndex;
        }
      }
      const RuleValue* cur = mEnumList[valueIndex].mCurValue;
      aData->mTreeMatchContext.mForAssignedSlot = isForAssignedSlot;
      ContentEnumFunc(*cur, cur->mSelector, aData, aNodeContext, filter);
      cur++;
      if (cur == mEnumList[valueIndex].mEnd) {
        mEnumList[valueIndex] = mEnumList[--valueCount];
      } else {
        mEnumList[valueIndex].mCurValue = cur;
      }
    }

    // Fast loop over single value.
    for (const RuleValue *value = mEnumList[0].mCurValue,
                              *end = mEnumList[0].mEnd;
         value != end;
         ++value) {
      aData->mTreeMatchContext.mForAssignedSlot = isForAssignedSlot;
      ContentEnumFunc(*value, value->mSelector, aData, aNodeContext, filter);
    }
  }
}

static size_t
SizeOfRuleHashTable(const PLDHashTable& aTable, MallocSizeOf aMallocSizeOf)
{
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<RuleHashTableEntry*>(iter.Get());
    n += entry->mRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

size_t
RuleHash::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  n += SizeOfRuleHashTable(mIdTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mClassTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mTagTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mNameSpaceTable, aMallocSizeOf);

  n += mUniversalRules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return n;
}

size_t
RuleHash::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

//--------------------------------

// A hash table mapping atoms to lists of selectors
struct AtomSelectorEntry : public PLDHashEntryHdr
{
  nsIAtom* mAtom;
  // Auto length 2, because a decent fraction of these arrays ends up
  // with 2 elements, and each entry is cheap.
  AutoTArray<SelectorPair, 2> mSelectors;
};

static void
AtomSelector_ClearEntry(PLDHashTable* table, PLDHashEntryHdr* hdr)
{
  (static_cast<AtomSelectorEntry*>(hdr))->~AtomSelectorEntry();
}

static void
AtomSelector_InitEntry(PLDHashEntryHdr* hdr, const void* key)
{
  AtomSelectorEntry* entry = static_cast<AtomSelectorEntry*>(hdr);
  new (KnownNotNull, entry) AtomSelectorEntry();
  entry->mAtom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
}

static void
AtomSelector_MoveEntry(PLDHashTable* table,
                       const PLDHashEntryHdr* from,
                       PLDHashEntryHdr* to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  AtomSelectorEntry* oldEntry =
    const_cast<AtomSelectorEntry*>(static_cast<const AtomSelectorEntry*>(from));
  auto* newEntry = new (KnownNotNull, to) AtomSelectorEntry();
  newEntry->mAtom = oldEntry->mAtom;
  newEntry->mSelectors.SwapElements(oldEntry->mSelectors);
  oldEntry->~AtomSelectorEntry();
}

static bool
AtomSelector_CIMatchEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  const AtomSelectorEntry* entry = static_cast<const AtomSelectorEntry*>(hdr);
  return CIMatchAtoms(key, entry->mAtom);
}

// Case-sensitive ops.
/* static */ const PLDHashTableOps RuleCascadeData::AtomSelector_CSOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  PLDHashTable::MatchEntryStub,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  AtomSelector_InitEntry
};

// Case-insensitive ops.
/* static */ const PLDHashTableOps RuleCascadeData::AtomSelector_CIOps = {
  RuleHash_CIHashKey,
  AtomSelector_CIMatchEntry,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  AtomSelector_InitEntry
};

RuleCascadeData::RuleCascadeData(bool aQuirksMode)
  : mRuleHash(aQuirksMode)
  , mStateSelectors()
  , mSelectorDocumentStates(0)
  , mClassSelectors(aQuirksMode ? &AtomSelector_CIOps : &AtomSelector_CSOps,
                    sizeof(AtomSelectorEntry))
  , mIdSelectors(aQuirksMode ? &AtomSelector_CIOps : &AtomSelector_CSOps,
                 sizeof(AtomSelectorEntry))
  ,
  // mAttributeSelectors is matching on the attribute _name_, not the
  // value, and we case-fold names at parse-time, so this is a
  // case-sensitive match.
  mAttributeSelectors(&AtomSelector_CSOps, sizeof(AtomSelectorEntry))
  , mAnonBoxRules(&RuleHash::TagTable_Ops, sizeof(RuleHashTagTableEntry))
  , mXULTreeRules(&RuleHash::TagTable_Ops, sizeof(RuleHashTagTableEntry))
  , mKeyframesRuleTable()
  , mCounterStyleRuleTable()
  , mQuirksMode(aQuirksMode)
{
  memset(mPseudoElementRuleHashes, 0, sizeof(mPseudoElementRuleHashes));
}

RuleCascadeData::~RuleCascadeData()
{
  for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
    delete mPseudoElementRuleHashes[i];
  }
}

static size_t
SizeOfSelectorsHashTable(const PLDHashTable& aTable, MallocSizeOf aMallocSizeOf)
{
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<AtomSelectorEntry*>(iter.Get());
    n += entry->mSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

size_t
RuleCascadeData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mRuleHash.SizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
    if (mPseudoElementRuleHashes[i])
      n += mPseudoElementRuleHashes[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += mStateSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += SizeOfSelectorsHashTable(mIdSelectors, aMallocSizeOf);
  n += SizeOfSelectorsHashTable(mClassSelectors, aMallocSizeOf);

  n += mPossiblyNegatedClassSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mPossiblyNegatedIDSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += SizeOfSelectorsHashTable(mAttributeSelectors, aMallocSizeOf);
  n += SizeOfRuleHashTable(mAnonBoxRules, aMallocSizeOf);
  n += SizeOfRuleHashTable(mXULTreeRules, aMallocSizeOf);

  n += mFontFaceRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mKeyframesRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mFontFeatureValuesRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mPageRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mCounterStyleRules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mKeyframesRuleTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mKeyframesRuleTable.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't own the nsCSSKeyframesRule objects so we don't count them. We
    // do care about the size of the keys' nsAString members' buffers though.
    //
    // Note that we depend on nsStringHashKey::GetKey() returning a reference,
    // since otherwise aKey would be a copy of the string key and we would not
    // be measuring the right object here.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

nsTArray<SelectorPair>*
RuleCascadeData::AttributeListFor(nsIAtom* aAttribute)
{
  auto entry = static_cast<AtomSelectorEntry*>(
    mAttributeSelectors.Add(aAttribute, fallible));
  if (!entry) {
    return nullptr;
  }
  return &entry->mSelectors;
}

void
RuleCascadeData::RulesMatching(ElementRuleProcessorData* aData)
{
  NodeMatchContext nodeContext(EventStates(),
                                nsCSSRuleUtils::IsLink(aData->mElement),
                                aData->mElementIsFeatureless);
  // Test against the assigned slot rather than the slottable if we're
  // matching the ::slotted() pseudo.
  Element* targetElement = aData->mElement;
  if (aData->mTreeMatchContext.mForAssignedSlot) {
    targetElement = aData->mElement->GetAssignedSlot()->AsElement();
  }
  mRuleHash.EnumerateAllRules(targetElement, aData, nodeContext);
}

void
RuleCascadeData::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  RuleHash* ruleHash =
    mPseudoElementRuleHashes[static_cast<CSSPseudoElementTypeBase>(
      aData->mPseudoType)];
  if (ruleHash) {
    NodeMatchContext nodeContext(
      EventStates(), nsCSSRuleUtils::IsLink(aData->mElement));
    ruleHash->EnumerateAllRules(aData->mElement, aData, nodeContext);
  }
}

void
RuleCascadeData::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  if (mAnonBoxRules.EntryCount() == 0) {
    return;
  }
  auto entry = static_cast<RuleHashTagTableEntry*>(
    mAnonBoxRules.Search(aData->mPseudoTag));
  if (entry) {
    nsTArray<RuleValue>& rules = entry->mRules;
    for (RuleValue *value = rules.Elements(),
                        *end = value + rules.Length();
          value != end;
          ++value) {
      css::Declaration* declaration = value->mRule->GetDeclaration();
      declaration->SetImmutable();
      aData->mRuleWalker->Forward(declaration);
    }
  }
}

void
RuleCascadeData::RulesMatching(XULTreeRuleProcessorData* aData)
{
  if (mXULTreeRules.EntryCount() == 0) {
    return;
  }
  auto entry = static_cast<RuleHashTagTableEntry*>(
    mXULTreeRules.Search(aData->mPseudoTag));
  if (entry) {
    NodeMatchContext nodeContext(
      EventStates(), nsCSSRuleUtils::IsLink(aData->mElement));
    nsTArray<RuleValue>& rules = entry->mRules;
    for (RuleValue *value = rules.Elements(), *end = value + rules.Length();
          value != end;
          ++value) {
      if (aData->mComparator->PseudoMatches(value->mSelector)) {
        ContentEnumFunc(
          *value, value->mSelector->mNext, aData, nodeContext, nullptr);
      }
    }
  }
}

static inline nsRestyleHint
RestyleHintForOp(char16_t oper)
{
  if (oper == char16_t('+') || oper == char16_t('~')) {
    return eRestyle_LaterSiblings;
  }

  if (oper != char16_t(0)) {
    return eRestyle_Subtree;
  }

  return eRestyle_Self;
}

/**
 * Look up the content node in the state rule list, which points to
 * any (CSS2 definition) simple selector (whether or not it is the
 * subject) that has a state pseudo-class on it.  This means that this
 * code will be matching selectors that aren't real selectors in any
 * stylesheet (e.g., if there is a selector "body > p:hover > a", then
 * "body > p:hover" will be in |mStateSelectors|).  Note that
 * |ComputeSelectorStateDependence| determines which selectors are in
 * |mStateSelectors|.
 */
void
RuleCascadeData::HasStateDependentStyle(
  ElementDependentRuleProcessorData* aData,
  Element* aStatefulElement,
  CSSPseudoElementType aPseudoType,
  EventStates aStateMask,
  nsRestyleHint& aHint)
{
  bool isPseudoElement = aPseudoType != CSSPseudoElementType::NotPseudo;

  StateSelector *iter = mStateSelectors.Elements(),
                     *end = iter + mStateSelectors.Length();
  NodeMatchContext nodeContext(aStateMask, false);
  for (; iter != end; ++iter) {
    nsCSSSelector* selector = iter->mSelector;
    EventStates states = iter->mStates;

    if (selector->IsPseudoElement() != isPseudoElement) {
      continue;
    }

    nsCSSSelector* selectorForPseudo;
    if (isPseudoElement) {
      if (selector->PseudoType() != aPseudoType) {
        continue;
      }
      selectorForPseudo = selector;
      selector = selector->mNext;
    }

    nsRestyleHint possibleChange = RestyleHintForOp(selector->mOperator);
    SelectorMatchesFlags selectorFlags = SelectorMatchesFlags::UNKNOWN;

    // If hint already includes all the bits of possibleChange,
    // don't bother calling SelectorMatches, since even if it returns false
    // hint won't change.
    // Also don't bother calling SelectorMatches if none of the
    // states passed in are relevant here.
    if ((possibleChange & ~aHint) && states.HasAtLeastOneOfStates(aStateMask) &&
        // We can optimize away testing selectors that only involve :hover, a
        // namespace, and a tag name against nodes that don't have the
        // NodeHasRelevantHoverRules flag: such a selector didn't match
        // the tag name or namespace the first time around (since the :hover
        // didn't set the NodeHasRelevantHoverRules flag), so it won't
        // match it now.  Check for our selector only having :hover states, or
        // the element having the hover rules flag, or the selector having
        // some sort of non-namespace, non-tagname data in it.
        (states != NS_EVENT_STATE_HOVER ||
         aStatefulElement->HasRelevantHoverRules() || selector->mIDList ||
         selector->mClassList ||
         // We generally expect an mPseudoClassList, since we have a :hover.
         // The question is whether we have anything else in there.
         (selector->mPseudoClassList &&
          (selector->mPseudoClassList->mNext ||
           selector->mPseudoClassList->mType != CSSPseudoClassType::hover)) ||
         selector->mAttrList || selector->mNegations) &&
        (!isPseudoElement ||
         nsCSSRuleUtils::StateSelectorMatches(aStatefulElement,
                                              selectorForPseudo,
                                              nodeContext,
                                              aData->mTreeMatchContext,
                                              selectorFlags,
                                              nullptr,
                                              aStateMask)) &&
        nsCSSRuleUtils::SelectorMatches(aData->mElement,
                                        selector,
                                        nodeContext,
                                        aData->mTreeMatchContext,
                                        selectorFlags) &&
        nsCSSRuleUtils::SelectorMatchesTree(
          aData->mElement,
          selector->mNext,
          aData->mTreeMatchContext,
          eMatchOnConditionalRestyleAncestor)) {
      aHint = nsRestyleHint(aHint | possibleChange);
    }
  }
}

static inline nsRestyleHint
RestyleHintForSelectorWithAttributeChange(nsRestyleHint aCurrentHint,
                                          nsCSSSelector* aSelector,
                                          nsCSSSelector* aRightmostSelector)
{
  MOZ_ASSERT(aSelector);

  char16_t oper = aSelector->mOperator;

  if (oper == char16_t('+') || oper == char16_t('~')) {
    return eRestyle_LaterSiblings;
  }

  if (oper == char16_t(':')) {
    return eRestyle_Subtree;
  }

  if (oper != char16_t(0)) {
    // Check whether the selector is in a form that supports
    // eRestyle_SomeDescendants.  If it isn't, return eRestyle_Subtree.

    if (aCurrentHint & eRestyle_Subtree) {
      // No point checking, since we'll end up restyling the whole
      // subtree anyway.
      return eRestyle_Subtree;
    }

    if (!aRightmostSelector) {
      // aSelector wasn't a top-level selector, which means we were inside
      // a :not() or :-moz-any().  We don't support that.
      return eRestyle_Subtree;
    }

    MOZ_ASSERT(aSelector != aRightmostSelector,
               "if aSelector == aRightmostSelector then we should have "
               "no operator");

    // Check that aRightmostSelector can be passed to RestrictedSelectorMatches.
    if (!aRightmostSelector->IsRestrictedSelector()) {
      return eRestyle_Subtree;
    }

    // We also don't support pseudo-elements on any of the selectors
    // between aRightmostSelector and aSelector.
    // XXX Can we lift this restriction, so that we don't have to loop
    // over all the selectors?
    for (nsCSSSelector* sel = aRightmostSelector->mNext; sel != aSelector;
         sel = sel->mNext) {
      MOZ_ASSERT(sel, "aSelector must be reachable from aRightmostSelector");
      if (sel->PseudoType() != CSSPseudoElementType::NotPseudo) {
        return eRestyle_Subtree;
      }
    }

    return eRestyle_SomeDescendants;
  }

  return eRestyle_Self;
}

static void
AttributeEnumFunc(nsCSSSelector* aSelector,
                  nsCSSSelector* aRightmostSelector,
                  AttributeEnumData* aData)
{
  AttributeRuleProcessorData* data = aData->data;

  if (!data->mTreeMatchContext.SetStyleScopeForSelectorMatching(data->mElement,
                                                                data->mScope)) {
    // The selector is for a rule in a scoped style sheet, and the subject
    // of the selector matching is not in its scope.
    return;
  }

  nsRestyleHint possibleChange = RestyleHintForSelectorWithAttributeChange(
    aData->change, aSelector, aRightmostSelector);

  if (!LookForTargetPseudo(
        aSelector, &data->mTreeMatchContext, &possibleChange)) {
    return;
  }

  // If, ignoring eRestyle_SomeDescendants, enumData->change already includes
  // all the bits of possibleChange, don't bother calling SelectorMatches, since
  // even if it returns false enumData->change won't change.  If possibleChange
  // has eRestyle_SomeDescendants, we need to call SelectorMatches(Tree)
  // regardless as it might give us new selectors to append to
  // mSelectorsForDescendants.
  NodeMatchContext nodeContext(EventStates(), false);
  if (((possibleChange & (~(aData->change) | eRestyle_SomeDescendants))) &&
      nsCSSRuleUtils::SelectorMatches(data->mElement,
                                      aSelector,
                                      nodeContext,
                                      data->mTreeMatchContext,
                                      SelectorMatchesFlags::UNKNOWN) &&
      nsCSSRuleUtils::SelectorMatchesTree(data->mElement,
                                          aSelector->mNext,
                                          data->mTreeMatchContext,
                                          eMatchOnConditionalRestyleAncestor)) {
    aData->change = nsRestyleHint(aData->change | possibleChange);
    if (possibleChange & eRestyle_SomeDescendants) {
      aData->hintData.mSelectorsForDescendants.AppendElement(
        aRightmostSelector);
    }
  }
}

static MOZ_ALWAYS_INLINE void
EnumerateSelectors(nsTArray<SelectorPair>& aSelectors, AttributeEnumData* aData)
{
  SelectorPair *iter = aSelectors.Elements(), *end = iter + aSelectors.Length();
  for (; iter != end; ++iter) {
    AttributeEnumFunc(iter->mSelector, iter->mRightmostSelector, aData);
  }
}

static MOZ_ALWAYS_INLINE void
EnumerateSelectors(nsTArray<nsCSSSelector*>& aSelectors,
                   AttributeEnumData* aData)
{
  nsCSSSelector **iter = aSelectors.Elements(),
                **end = iter + aSelectors.Length();
  for (; iter != end; ++iter) {
    AttributeEnumFunc(*iter, nullptr, aData);
  }
}

void
RuleCascadeData::HasAttributeDependentStyle(
  AttributeRuleProcessorData* aData,
  AttributeEnumData* aEnumData,
  mozilla::RestyleHintData& aRestyleHintDataResult)
{
  // Since we get both before and after notifications for attributes, we
  // don't have to ignore aData->mAttribute while matching.  Just check
  // whether we have selectors relevant to aData->mAttribute that we
  // match.  If this is the before change notification, that will catch
  // rules we might stop matching; if the after change notification, the
  // ones we might have started matching.

  if (aData->mAttribute == nsGkAtoms::id) {
    nsIAtom* id = aData->mElement->GetID();
    if (id) {
      auto entry =
        static_cast<AtomSelectorEntry*>(mIdSelectors.Search(id));
      if (entry) {
        EnumerateSelectors(entry->mSelectors, aEnumData);
      }
    }

    EnumerateSelectors(mPossiblyNegatedIDSelectors, aEnumData);
  }

  if (aData->mAttribute == nsGkAtoms::_class &&
      aData->mNameSpaceID == kNameSpaceID_None) {
    const nsAttrValue* otherClasses = aData->mOtherValue;
    NS_ASSERTION(otherClasses ||
                    aData->mModType == nsIDOMMutationEvent::REMOVAL,
                  "All class values should be StoresOwnData and parsed"
                  "via Element::BeforeSetAttr, so available here");
    // For WillChange, enumerate classes that will be removed to see which
    // rules apply before the change.
    // For Changed, enumerate classes that have been added to see which rules
    // apply after the change.
    // In both cases we're interested in the classes that are currently on
    // the element but not in mOtherValue.
    const nsAttrValue* elementClasses = aData->mElement->GetClasses();
    if (elementClasses) {
      int32_t atomCount = elementClasses->GetAtomCount();
      if (atomCount > 0) {
        nsTHashtable<nsPtrHashKey<nsIAtom>> otherClassesTable;
        if (otherClasses) {
          int32_t otherClassesCount = otherClasses->GetAtomCount();
          for (int32_t i = 0; i < otherClassesCount; ++i) {
            otherClassesTable.PutEntry(otherClasses->AtomAt(i));
          }
        }
        for (int32_t i = 0; i < atomCount; ++i) {
          nsIAtom* curClass = elementClasses->AtomAt(i);
          if (!otherClassesTable.Contains(curClass)) {
            auto entry = static_cast<AtomSelectorEntry*>(
              mClassSelectors.Search(curClass));
            if (entry) {
              EnumerateSelectors(entry->mSelectors, aEnumData);
            }
          }
        }
      }
    }

    EnumerateSelectors(mPossiblyNegatedClassSelectors, aEnumData);
  }

  auto entry = static_cast<AtomSelectorEntry*>(
    mAttributeSelectors.Search(aData->mAttribute));
  if (entry) {
    EnumerateSelectors(entry->mSelectors, aEnumData);
  }
}

// This function should return the set of states that this selector
// depends on; this is used to implement HasStateDependentStyle.  It
// does NOT recur down into things like :not and :-moz-any.
inline EventStates
ComputeSelectorStateDependence(nsCSSSelector& aSelector)
{
  EventStates states;
  for (nsPseudoClassList* pseudoClass = aSelector.mPseudoClassList; pseudoClass;
       pseudoClass = pseudoClass->mNext) {
    // Tree pseudo-elements overload mPseudoClassList for things that
    // aren't pseudo-classes.
    if (pseudoClass->mType >= CSSPseudoClassType::Count) {
      continue;
    }

    if (pseudoClass->mType == CSSPseudoClassType::autofill ||
        pseudoClass->mType == CSSPseudoClassType::mozAutofillHighlight) {
      states |= NS_EVENT_STATE_AUTOFILL;
      continue;
    }

    auto idx = static_cast<CSSPseudoClassTypeBase>(pseudoClass->mType);
    states |= nsCSSPseudoClasses::sPseudoClassStateDependences[idx];
  }
  return states;
}

bool
RuleCascadeData::AddSelector(
  // The part between combinators at the top level of the selector
  nsCSSSelector* aSelectorInTopLevel,
  // The part we should look through (might be in :not or :-moz-any())
  nsCSSSelector* aSelectorPart,
  // The right-most selector at the top level
  nsCSSSelector* aRightmostSelector)
{
  // It's worth noting that this loop over negations isn't quite
  // optimal for two reasons.  One, we could add something to one of
  // these lists twice, which means we'll check it twice, but I don't
  // think that's worth worrying about.   (We do the same for multiple
  // attribute selectors on the same attribute.)  Two, we don't really
  // need to check negations past the first in the current
  // implementation (and they're rare as well), but that might change
  // in the future if :not() is extended.
  for (nsCSSSelector* negation = aSelectorPart; negation;
       negation = negation->mNegations) {
    // Track both document states and attribute dependence in pseudo-classes.
    for (nsPseudoClassList* pseudoClass = negation->mPseudoClassList;
         pseudoClass;
         pseudoClass = pseudoClass->mNext) {
      switch (pseudoClass->mType) {
        case CSSPseudoClassType::mozLocaleDir: {
          mSelectorDocumentStates |= NS_DOCUMENT_STATE_RTL_LOCALE;
          break;
        }
        case CSSPseudoClassType::mozWindowInactive: {
          mSelectorDocumentStates |=
            NS_DOCUMENT_STATE_WINDOW_INACTIVE;
          break;
        }
        case CSSPseudoClassType::mozTableBorderNonzero: {
          nsTArray<SelectorPair>* array =
            AttributeListFor(nsGkAtoms::border);
          if (!array) {
            return false;
          }
          array->AppendElement(
            SelectorPair(aSelectorInTopLevel, aRightmostSelector));
          break;
        }
        default: {
          break;
        }
      }
    }

    // Build mStateSelectors.
    EventStates dependentStates = ComputeSelectorStateDependence(*negation);
    if (!dependentStates.IsEmpty()) {
      mStateSelectors.AppendElement(
        StateSelector(dependentStates, aSelectorInTopLevel));
    }

    // Build mIDSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curID = negation->mIDList; curID; curID = curID->mNext) {
        auto entry = static_cast<AtomSelectorEntry*>(
          mIdSelectors.Add(curID->mAtom, fallible));
        if (entry) {
          entry->mSelectors.AppendElement(
            SelectorPair(aSelectorInTopLevel, aRightmostSelector));
        }
      }
    } else if (negation->mIDList) {
      mPossiblyNegatedIDSelectors.AppendElement(aSelectorInTopLevel);
    }

    // Build mClassSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curClass = negation->mClassList; curClass;
           curClass = curClass->mNext) {
        auto entry = static_cast<AtomSelectorEntry*>(
          mClassSelectors.Add(curClass->mAtom, fallible));
        if (entry) {
          entry->mSelectors.AppendElement(
            SelectorPair(aSelectorInTopLevel, aRightmostSelector));
        }
      }
    } else if (negation->mClassList) {
      mPossiblyNegatedClassSelectors.AppendElement(
        aSelectorInTopLevel);
    }

    // Build mAttributeSelectors.
    for (nsAttrSelector* attr = negation->mAttrList; attr; attr = attr->mNext) {
      nsTArray<SelectorPair>* array = AttributeListFor(attr->mCasedAttr);
      if (!array) {
        return false;
      }
      array->AppendElement(
        SelectorPair(aSelectorInTopLevel, aRightmostSelector));
      if (attr->mLowercaseAttr != attr->mCasedAttr) {
        array = AttributeListFor(attr->mLowercaseAttr);
        if (!array) {
          return false;
        }
        array->AppendElement(
          SelectorPair(aSelectorInTopLevel, aRightmostSelector));
      }
    }

    // Recur through any pseudo-class that has a selector list argument.
    for (nsPseudoClassList* pseudoClass = negation->mPseudoClassList;
         pseudoClass;
         pseudoClass = pseudoClass->mNext) {
      if (nsCSSPseudoClasses::HasSelectorListArg(pseudoClass->mType)) {
        for (nsCSSSelectorList* l = pseudoClass->u.mSelectorList; l;
             l = l->mNext) {
          nsCSSSelector* s = l->mSelectors;
          if (!AddSelector(aSelectorInTopLevel, s, aRightmostSelector)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

bool
RuleCascadeData::AddRule(RuleSelectorPair* aRuleInfo)
{
  // Build the rule hash.
  CSSPseudoElementType pseudoType = aRuleInfo->mSelector->PseudoType();
  if (MOZ_LIKELY(pseudoType == CSSPseudoElementType::NotPseudo)) {
    mRuleHash.AppendRule(*aRuleInfo);
  } else if (pseudoType < CSSPseudoElementType::Count) {
    RuleHash*& ruleHash =
      mPseudoElementRuleHashes[static_cast<CSSPseudoElementTypeBase>(
        pseudoType)];
    if (!ruleHash) {
      ruleHash = new RuleHash(mQuirksMode);
      if (!ruleHash) {
        // Out of memory; give up
        return false;
      }
    }
    NS_ASSERTION(aRuleInfo->mSelector->mNext,
                 "Must have mNext; parser screwed up");
    NS_ASSERTION(aRuleInfo->mSelector->mNext->mOperator == ':',
                 "Unexpected mNext combinator");
    ruleHash->AppendRule(*aRuleInfo);
  } else if (pseudoType == CSSPseudoElementType::AnonBox) {
    NS_ASSERTION(
      !aRuleInfo->mSelector->mCasedTag && !aRuleInfo->mSelector->mIDList &&
        !aRuleInfo->mSelector->mClassList &&
        !aRuleInfo->mSelector->mPseudoClassList &&
        !aRuleInfo->mSelector->mAttrList && !aRuleInfo->mSelector->mNegations &&
        !aRuleInfo->mSelector->mNext &&
        aRuleInfo->mSelector->mNameSpace == kNameSpaceID_Unknown,
      "Parser messed up with anon box selector");

    // Index doesn't matter here, since we'll just be walking these
    // rules in order; just pass 0.
    RuleHash::AppendRuleToTagTable(
      &mAnonBoxRules,
      aRuleInfo->mSelector->mLowercaseTag,
      RuleValue(*aRuleInfo, 0, mQuirksMode));
  } else {
#ifdef MOZ_XUL
    NS_ASSERTION(pseudoType == CSSPseudoElementType::XULTree,
                 "Unexpected pseudo type");
    // Index doesn't matter here, since we'll just be walking these
    // rules in order; just pass 0.
    RuleHash::AppendRuleToTagTable(
      &mXULTreeRules,
      aRuleInfo->mSelector->mLowercaseTag,
      RuleValue(*aRuleInfo, 0, mQuirksMode));
#else
    NS_NOTREACHED("Unexpected pseudo type");
#endif
  }

  for (nsCSSSelector* selector = aRuleInfo->mSelector; selector;
       selector = selector->mNext) {
    if (selector->IsPseudoElement()) {
      CSSPseudoElementType pseudo = selector->PseudoType();
      if (pseudo >= CSSPseudoElementType::Count ||
          !nsCSSPseudoElements::PseudoElementSupportsUserActionState(pseudo)) {
        NS_ASSERTION(!selector->mNegations, "Shouldn't have negations");
        // We do store selectors ending with pseudo-elements that allow :hover
        // and :active after them in the hashtables corresponding to that
        // selector's mNext (i.e. the thing that matches against the element),
        // but we want to make sure that selectors for any other kinds of
        // pseudo-elements don't end up in the hashtables.  In particular, tree
        // pseudos store strange things in mPseudoClassList that we don't want
        // to try to match elements against.
        continue;
      }
    }
    if (!AddSelector(selector, selector, aRuleInfo->mSelector)) {
      return false;
    }
  }

  return true;
}

struct PerWeightDataListItem : public RuleSelectorPair
{
  PerWeightDataListItem(css::StyleRule* aRule, nsCSSSelector* aSelector)
    : RuleSelectorPair(aRule, aSelector)
    , mNext(nullptr)
  {
  }
  // No destructor; these are arena-allocated

  // Placement new to arena allocate the PerWeightDataListItem
  void* operator new(size_t aSize, PLArenaPool& aArena) CPP_THROW_NEW
  {
    void* mem;
    PL_ARENA_ALLOCATE(mem, &aArena, aSize);
    return mem;
  }

  PerWeightDataListItem* mNext;
};

struct PerWeightData
{
  PerWeightData()
    : mRuleSelectorPairs(nullptr)
    , mTail(&mRuleSelectorPairs)
  {
  }

  int32_t mWeight;
  PerWeightDataListItem* mRuleSelectorPairs;
  PerWeightDataListItem** mTail;
};

struct RuleByWeightEntry : public PLDHashEntryHdr
{
  PerWeightData data; // mWeight is key, mRuleSelectorPairs are value
};

static PLDHashNumber
HashIntKey(const void* key)
{
  return PLDHashNumber(NS_PTR_TO_INT32(key));
}

static bool
MatchWeightEntry(const PLDHashEntryHdr* hdr, const void* key)
{
  const RuleByWeightEntry* entry = (const RuleByWeightEntry*)hdr;
  return entry->data.mWeight == NS_PTR_TO_INT32(key);
}

static void
InitWeightEntry(PLDHashEntryHdr* hdr, const void* key)
{
  RuleByWeightEntry* entry = static_cast<RuleByWeightEntry*>(hdr);
  new (KnownNotNull, entry) RuleByWeightEntry();
}

/* static */ const PLDHashTableOps
CascadeLayer::WeightedRuleData::sRulesByWeightOps = {
  HashIntKey,
  MatchWeightEntry,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  InitWeightEntry
};

CascadeLayer::WeightedRuleData::WeightedRuleData()
  : mRulesByWeight(&sRulesByWeightOps, sizeof(RuleByWeightEntry), 32)
{
  // Initialize our arena
  PL_INIT_ARENA_POOL(
    &mArena, "WeightedRuleDataArena", NS_WEIGHTEDRULEDATA_ARENA_BLOCK_SIZE);
}

CascadeLayer::WeightedRuleData::~WeightedRuleData()
{
  PL_FinishArenaPool(&mArena);
}

static int
CompareWeightData(const void* aArg1, const void* aArg2, void* closure)
{
  const PerWeightData* arg1 = static_cast<const PerWeightData*>(aArg1);
  const PerWeightData* arg2 = static_cast<const PerWeightData*>(aArg2);
  return arg1->mWeight - arg2->mWeight; // put lower weight first
}

UniquePtr<PerWeightData[]>
CascadeLayer::WeightedRuleData::Consume(nsTArray<css::StyleRule*>& aStyleRules)
{
  for (css::StyleRule* styleRule : aStyleRules) {
    for (nsCSSSelectorList* sel = styleRule->Selector(); sel;
         sel = sel->mNext) {
      int32_t weight = sel->mWeight;
      auto entry = static_cast<RuleByWeightEntry*>(
        mRulesByWeight.Add(NS_INT32_TO_PTR(weight), fallible));
      if (!entry) {
        return nullptr;
      }
      entry->data.mWeight = weight;
      // entry->data.mRuleSelectorPairs should be linked in forward order;
      // entry->data.mTail is the slot to write to.
      auto* newItem =
        new (mArena) PerWeightDataListItem(styleRule, sel->mSelectors);
      if (newItem) {
        *(entry->data.mTail) = newItem;
        entry->data.mTail = &newItem->mNext;
      }
    }
  }

  // There's no point in keeping pointers to the style rules around
  // after we've consumed them, so clear the array.
  aStyleRules.Clear();

  // Sort the hash table of per-weight linked lists by weight.
  uint32_t weightCount = mRulesByWeight.EntryCount();
  auto weightArray = MakeUnique<PerWeightData[]>(weightCount);
  int32_t j = 0;
  for (auto iter = mRulesByWeight.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<const RuleByWeightEntry*>(iter.Get());
    weightArray[j++] = entry->data;
  }
  NS_QuickSort(weightArray.get(),
               weightCount,
               sizeof(PerWeightData),
               CompareWeightData,
               nullptr);

  return weightArray;
}

CascadeLayer::CascadeLayer(nsPresContext* aPresContext,
#ifdef DEBUG
                           CascadeLayer* aParent,
#endif
                           nsTArray<css::DocumentRule*>& aDocumentRules,
                           nsDocumentRuleResultCacheKey& aDocumentKey,
                           SheetType aSheetType,
                           bool aMustGatherDocumentRules,
                           nsMediaQueryResultCacheKey& aCacheKey)
  : mPresContext(aPresContext)
  , mIsAnonymous(true)
#ifdef DEBUG
  , mIsStrong(false)
#endif
  , mRulesAdded(false)
#ifdef DEBUG
  , mParent(aParent)
#endif
  , mDocumentRules(aDocumentRules)
  , mDocumentCacheKey(aDocumentKey)
  , mSheetType(aSheetType)
  , mMustGatherDocumentRules(aMustGatherDocumentRules)
  , mCacheKey(aCacheKey)
{
  mData = new RuleCascadeData(eCompatibility_NavQuirks ==
                              mPresContext->CompatibilityMode());
}

CascadeLayer::~CascadeLayer()
{
  delete mData;
}

size_t
CascadeLayer::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  if (mData) {
    n += mData->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += mStyleRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  // While we do create the child layers, they are not owned by us, so we
  // don't count them. Their ownership is managed by the rule processor
  // to which they are eventually attached (see nsCSSRuleProcessor).
  n += mPreLayers.ShallowSizeOfExcludingThis(aMallocSizeOf);
#ifdef DEBUG
  n += mPostLayers.ShallowSizeOfExcludingThis(aMallocSizeOf);
#endif
  n += mLayers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mLayers.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't own the CascadeLayer objects so we don't count them. We
    // do care about the size of the keys' nsAString members' buffers though.
    //
    // Note that we depend on nsStringHashKey::GetKey() returning a reference,
    // since otherwise aKey would be a copy of the string key and we would not
    // be measuring the right object here.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

CascadeLayer*
CascadeLayer::CreateNamedChildLayer(const nsTArray<nsString>& aPath)
{
  if (aPath.IsEmpty()) {
    return this;
  }

  const nsString& name = aPath[0];
  CascadeLayer* childLayer = nullptr;

  // Create new layer if it doesn't exist.
  if (!mLayers.Get(name, &childLayer)) {
    childLayer = new CascadeLayer(mPresContext,
#ifdef DEBUG
                                  this,
#endif
                                  mDocumentRules,
                                  mDocumentCacheKey,
                                  mSheetType,
                                  mMustGatherDocumentRules,
                                  mCacheKey);
    childLayer->mName = name;
    childLayer->mIsAnonymous = false;
    mPreLayers.AppendElement(childLayer);
    mLayers.Put(name, childLayer);
  }

  // Final layer in the path.
  if (aPath.Length() == 1) {
    return childLayer;
  }

  // Continue with the tail of the path.
  nsTArray<nsString> tail;
  tail.AppendElements(aPath.Elements() + 1, aPath.Length() - 1);
  return childLayer->CreateNamedChildLayer(tail);
}

CascadeLayer*
CascadeLayer::CreateAnonymousChildLayer()
{
  CascadeLayer* childLayer = new CascadeLayer(mPresContext,
#ifdef DEBUG
                                              this,
#endif
                                              mDocumentRules,
                                              mDocumentCacheKey,
                                              mSheetType,
                                              mMustGatherDocumentRules,
                                              mCacheKey);
  mPreLayers.AppendElement(childLayer);
  return childLayer;
}

void
CascadeLayer::AddRules()
{
  MOZ_ASSERT(!mRulesAdded, "Rule cascade data already filled");

  WeightedRuleData ruleData;
  auto weightArray = ruleData.Consume(mStyleRules);

  // Put things into the rule hash.
  // The primary sort is by weight...
  for (uint32_t i = 0; i < ruleData.mRulesByWeight.EntryCount(); ++i) {
    // and the secondary sort is by order.  mRuleSelectorPairs is already in
    // the right order..
    for (PerWeightDataListItem* cur = weightArray[i].mRuleSelectorPairs; cur;
         cur = cur->mNext) {
      if (!mData->AddRule(cur)) {
        return; /* out of memory */
      }
    }
  }

  // Build mKeyframesRuleTable.
  for (nsTArray<nsCSSKeyframesRule*>::size_type
         i = 0,
         iEnd = mData->mKeyframesRules.Length();
       i < iEnd;
       ++i) {
    nsCSSKeyframesRule* rule = mData->mKeyframesRules[i];
    mData->mKeyframesRuleTable.Put(rule->GetName(), rule);
  }

  // Build mCounterStyleRuleTable
  for (nsTArray<nsCSSCounterStyleRule*>::size_type
         i = 0,
         iEnd = mData->mCounterStyleRules.Length();
       i < iEnd;
       ++i) {
    nsCSSCounterStyleRule* rule = mData->mCounterStyleRules[i];
    mData->mCounterStyleRuleTable.Put(rule->GetName(), rule);
  }

  mRulesAdded = true;
}

void
CascadeLayer::EnumerateAllLayers(nsLayerEnumFunc aFunc, void* aData)
{
  if (mPreLayers.Length() > 0) {
    for (CascadeLayer* pre : mPreLayers) {
      pre->EnumerateAllLayers(aFunc, aData);
    }
  }

  (*aFunc)(this, aData);

#ifdef DEBUG
  if (mPostLayers.Length() > 0) {
    for (CascadeLayer* post : mPostLayers) {
      post->EnumerateAllLayers(aFunc, aData);
    }
  }
#endif
}
