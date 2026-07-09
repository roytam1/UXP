/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS Custom Property assignments for a Declaration at a given priority */

#include "CSSVariableDeclarations.h"

#include "CSSVariableResolver.h"
#include "CSSVariableValues.h"
#include "nsCSSScanner.h"
#include "nsRuleData.h"

// These special string values are used to represent specified values of
// CSS-wide keywords.  (Note that none of these are valid variable values.)
#define INITIAL_VALUE "!"
#define INHERIT_VALUE ";"
#define UNSET_VALUE   ")"
#define REVERT_VALUE  ">"
#define REVERT_LAYER_VALUE  "]"

static const int32_t kRevertLayerMask = 0x000fffff;
static const int32_t kRevertLayerImportant = 0x00100000;
static const int32_t kRevertLayerOriginShift = 21;
static const int32_t kCascadeLayerRankMax = 0x000fffff;
static const int32_t kCascadeRankStride = kCascadeLayerRankMax + 1;

namespace mozilla {

static bool
IsRevertLayerValue(const nsString& aValue)
{
  return StringBeginsWith(aValue, NS_LITERAL_STRING(REVERT_LAYER_VALUE));
}

static bool
ValueMightResolveToRevertLayer(const nsString& aValue)
{
  return IsRevertLayerValue(aValue) ||
         aValue.Find(NS_LITERAL_STRING("revert-layer")) != kNotFound;
}

static int32_t
PackRevertLayerData(SheetType aLevel, bool aIsImportant, int32_t aLayer)
{
  MOZ_ASSERT(aLayer >= 0 && aLayer <= kRevertLayerMask);
  return (uint32_t(aLevel) << kRevertLayerOriginShift) |
         (aIsImportant ? kRevertLayerImportant : 0) |
         (aLayer & kRevertLayerMask);
}

static void
SetRevertLayerData(nsString& aValue, const nsRuleData* aRuleData)
{
  aValue.AssignLiteral(REVERT_LAYER_VALUE);
  aValue.AppendInt(PackRevertLayerData(aRuleData->mLevel,
                                       aRuleData->mIsImportantRule,
                                       aRuleData->mCascadeLayer));
}

static bool
UnpackRevertLayerData(const nsString& aValue, SheetType& aLevel,
                      bool& aIsImportant, int32_t& aLayer)
{
  if (!IsRevertLayerValue(aValue) ||
      aValue.Length() == NS_LITERAL_STRING(REVERT_LAYER_VALUE).Length()) {
    return false;
  }

  nsresult rv;
  nsAutoString metadata;
  metadata.Assign(Substring(aValue,
                            NS_LITERAL_STRING(REVERT_LAYER_VALUE).Length()));
  int32_t packed = metadata.ToInteger(&rv);
  if (NS_FAILED(rv)) {
    return false;
  }

  aLevel = SheetType(uint32_t(packed) >> kRevertLayerOriginShift);
  aIsImportant = (packed & kRevertLayerImportant) != 0;
  aLayer = packed & kRevertLayerMask;
  return true;
}

static int32_t
CascadeLayerRank(bool aIsImportant, int32_t aCascadeLayer)
{
  MOZ_ASSERT(aCascadeLayer >= 0);
  MOZ_ASSERT(aCascadeLayer <= kCascadeLayerRankMax);
  return aIsImportant ? kCascadeLayerRankMax - aCascadeLayer : aCascadeLayer;
}

static int32_t
CascadePrecedenceRank(SheetType aLevel, bool aIsImportant,
                      int32_t aCascadeLayer)
{
  int32_t bucket = 0;
  bool isLayeredOrigin = false;

  switch (aLevel) {
    case SheetType::Agent:
      bucket = aIsImportant ? 14 : 0;
      isLayeredOrigin = true;
      break;
    case SheetType::User:
      bucket = aIsImportant ? 13 : 1;
      isLayeredOrigin = true;
      break;
    case SheetType::PresHint:
      bucket = 2;
      break;
    case SheetType::SVGAttrAnimation:
      bucket = 3;
      break;
    case SheetType::Doc:
      bucket = aIsImportant ? 10 : 4;
      isLayeredOrigin = true;
      break;
    case SheetType::ScopedDoc:
      bucket = aIsImportant ? 9 : 5;
      isLayeredOrigin = true;
      break;
    case SheetType::StyleAttr:
      bucket = aIsImportant ? 11 : 6;
      break;
    case SheetType::Override:
      bucket = aIsImportant ? 12 : 7;
      break;
    case SheetType::Animation:
      bucket = 8;
      break;
    case SheetType::Transition:
      bucket = 15;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected cascade level");
      break;
  }

  return bucket * kCascadeRankStride +
         (isLayeredOrigin ? CascadeLayerRank(aIsImportant, aCascadeLayer) : 0);
}

static bool
ShouldIgnoreForRevertLayerData(SheetType aSourceLevel,
                               bool aSourceImportant,
                               int32_t aSourceLayer,
                               SheetType aCandidateLevel,
                               bool aCandidateImportant,
                               int32_t aCandidateLayer)
{
  int32_t candidateRank =
    CascadePrecedenceRank(aCandidateLevel, aCandidateImportant,
                          aCandidateLayer);
  int32_t sourceRank =
    CascadePrecedenceRank(aSourceLevel, aSourceImportant, aSourceLayer);

  if (candidateRank >= sourceRank) {
    return true;
  }

  if (!aSourceImportant) {
    return false;
  }

  if (aSourceLevel == SheetType::StyleAttr) {
    return aCandidateLevel == SheetType::StyleAttr ||
           aCandidateLevel == SheetType::Animation;
  }

  int32_t normalBoundary =
    CascadePrecedenceRank(aSourceLevel, false, aSourceLayer);
  return candidateRank >= normalBoundary;
}

static bool
ShouldIgnoreForRevertLayerValue(const nsString& aTargetValue,
                                SheetType aCandidateLevel,
                                bool aCandidateImportant,
                                int32_t aCandidateLayer)
{
  SheetType sourceLevel;
  bool sourceImportant;
  int32_t sourceLayer;
  if (!UnpackRevertLayerData(aTargetValue, sourceLevel, sourceImportant,
                             sourceLayer)) {
    return false;
  }

  return ShouldIgnoreForRevertLayerData(sourceLevel, sourceImportant,
                                        sourceLayer, aCandidateLevel,
                                        aCandidateImportant, aCandidateLayer);
}

static bool
ShouldIgnoreForRevertLayerValue(const nsString& aTargetValue,
                                const nsRuleData* aRuleData)
{
  return ShouldIgnoreForRevertLayerValue(aTargetValue, aRuleData->mLevel,
                                         aRuleData->mIsImportantRule,
                                         aRuleData->mCascadeLayer);
}

static bool
ShouldIgnoreForRevertLayerSourceValue(const nsString& aTargetValue,
                                      const nsString& aCandidateValue)
{
  SheetType candidateLevel;
  bool candidateImportant;
  int32_t candidateLayer;
  if (!UnpackRevertLayerData(aCandidateValue, candidateLevel,
                             candidateImportant, candidateLayer)) {
    return false;
  }

  return ShouldIgnoreForRevertLayerValue(aTargetValue, candidateLevel,
                                         candidateImportant, candidateLayer);
}

static bool
IsComputedRevertLayerValue(const nsString& aValue)
{
  nsAutoString trimmed(aValue);
  trimmed.CompressWhitespace(true, true);
  return trimmed.EqualsLiteral("revert-layer");
}

CSSVariableDeclarations::CSSVariableDeclarations()
{
  MOZ_COUNT_CTOR(CSSVariableDeclarations);
}

CSSVariableDeclarations::CSSVariableDeclarations(const CSSVariableDeclarations& aOther)
{
  MOZ_COUNT_CTOR(CSSVariableDeclarations);
  CopyVariablesFrom(aOther);
}

#ifdef DEBUG
CSSVariableDeclarations::~CSSVariableDeclarations()
{
  MOZ_COUNT_DTOR(CSSVariableDeclarations);
}
#endif

CSSVariableDeclarations&
CSSVariableDeclarations::operator=(const CSSVariableDeclarations& aOther)
{
  if (this == &aOther) {
    return *this;
  }

  mVariables.Clear();
  mRevertLayerSources.Clear();
  mRevertLayerFallbacks.Clear();
  CopyVariablesFrom(aOther);
  return *this;
}

void
CSSVariableDeclarations::CopyVariablesFrom(const CSSVariableDeclarations& aOther)
{
  for (auto iter = aOther.mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    mVariables.Put(iter.Key(), iter.UserData());
  }
  for (auto iter = aOther.mRevertLayerSources.ConstIter();
       !iter.Done(); iter.Next()) {
    mRevertLayerSources.Put(iter.Key(), iter.UserData());
  }
  for (auto iter = aOther.mRevertLayerFallbacks.ConstIter();
       !iter.Done(); iter.Next()) {
    RevertLayerFallbackList* list = mRevertLayerFallbacks.LookupOrAdd(iter.Key());
    list->mItems = iter.UserData()->mItems;
  }
}

bool
CSSVariableDeclarations::Has(const nsAString& aName) const
{
  nsString value;
  return mVariables.Get(aName, &value);
}

bool
CSSVariableDeclarations::Get(const nsAString& aName,
                             Type& aType,
                             nsString& aTokenStream) const
{
  nsString value;
  if (!mVariables.Get(aName, &value)) {
    return false;
  }
  if (value.EqualsLiteral(INITIAL_VALUE)) {
    aType = eInitial;
    aTokenStream.Truncate();
  } else if (value.EqualsLiteral(INHERIT_VALUE)) {
    aType = eInitial;
    aTokenStream.Truncate();
  } else if (value.EqualsLiteral(UNSET_VALUE)) {
    aType = eUnset;
    aTokenStream.Truncate();
  } else if (value.EqualsLiteral(REVERT_VALUE)) {
    aType = eRevert;
    aTokenStream.Truncate();
  } else if (IsRevertLayerValue(value)) {
    aType = eRevertLayer;
    aTokenStream.Truncate();
  } else {
    aType = eTokenStream;
    aTokenStream = value;
  }
  return true;
}

void
CSSVariableDeclarations::PutTokenStream(const nsAString& aName,
                                        const nsString& aTokenStream)
{
  MOZ_ASSERT(!aTokenStream.EqualsLiteral(INITIAL_VALUE) &&
             !aTokenStream.EqualsLiteral(INHERIT_VALUE) &&
             !aTokenStream.EqualsLiteral(UNSET_VALUE) &&
             !aTokenStream.EqualsLiteral(REVERT_VALUE) &&
             !aTokenStream.EqualsLiteral(REVERT_LAYER_VALUE));
  mVariables.Put(aName, aTokenStream);
}

void
CSSVariableDeclarations::PutInitial(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(INITIAL_VALUE));
}

void
CSSVariableDeclarations::PutInherit(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(INHERIT_VALUE));
}

void
CSSVariableDeclarations::PutUnset(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(UNSET_VALUE));
}

void
CSSVariableDeclarations::PutRevert(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(REVERT_VALUE));
}

void
CSSVariableDeclarations::PutRevertLayer(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(REVERT_LAYER_VALUE));
}

void
CSSVariableDeclarations::Remove(const nsAString& aName)
{
  mVariables.Remove(aName);
}

void
CSSVariableDeclarations::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (!(aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Variables))) {
    return;
  }

  if (!aRuleData->mVariables) {
    aRuleData->mVariables = new CSSVariableDeclarations;
  }

  for (auto iter = mVariables.Iter(); !iter.Done(); iter.Next()) {
    nsDataHashtable<nsStringHashKey, nsString>& variables =
      aRuleData->mVariables->mVariables;
    const nsAString& name = iter.Key();
    nsString value = iter.UserData();
    nsString valueSource;
    SetRevertLayerData(valueSource, aRuleData);
    if (value.EqualsLiteral(REVERT_LAYER_VALUE)) {
      value = valueSource;
      aRuleData->mConditions.SetUncacheable();
    }

    nsString existingValue;
    if (variables.Get(name, &existingValue)) {
      if (IsRevertLayerValue(existingValue)) {
        if (ShouldIgnoreForRevertLayerValue(existingValue, aRuleData)) {
          continue;
        }
      } else {
        nsString sourceValue;
        if (aRuleData->mVariables->mRevertLayerSources.Get(name,
                                                           &sourceValue) &&
            !ShouldIgnoreForRevertLayerValue(sourceValue, aRuleData)) {
          RevertLayerFallbackList* fallbacks =
            aRuleData->mVariables->mRevertLayerFallbacks.LookupOrAdd(name);
          RevertLayerFallback* fallback = fallbacks->mItems.AppendElement();
          fallback->mValue = value;
          fallback->mSource = valueSource;
        }
        continue;
      }
    }

    variables.Put(name, value);
    if (ValueMightResolveToRevertLayer(value)) {
      aRuleData->mVariables->mRevertLayerSources.Put(name, valueSource);
    } else {
      aRuleData->mVariables->mRevertLayerSources.Remove(name);
      aRuleData->mVariables->mRevertLayerFallbacks.Remove(name);
    }
  }
}

void
CSSVariableDeclarations::AddVariablesToResolver(
                                           CSSVariableResolver* aResolver) const
{
  for (auto iter = mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    const nsAString& name = iter.Key();
    nsString value = iter.UserData();
    if (value.EqualsLiteral(INITIAL_VALUE)) {
      // Values of 'initial' are treated the same as an invalid value in the
      // variable resolver.
      aResolver->Put(name, EmptyString(),
                     eCSSTokenSerialization_Nothing,
                     eCSSTokenSerialization_Nothing,
                     false);
    } else if (value.EqualsLiteral(INHERIT_VALUE) ||
               value.EqualsLiteral(UNSET_VALUE) ||
               value.EqualsLiteral(REVERT_VALUE) ||
               IsRevertLayerValue(value)) {
      // Values of 'inherit', 'unset', 'revert', and 'revert-layer' don't need any handling,
      // since it means we just need to keep whatever value is currently in
      // the resolver.  This is because the specified variable declarations
      // already have only the winning declaration for the variable and no
      // longer have any of the others.
    } else {
      // At this point, we don't know what token types are at the start and end
      // of the specified variable value.  These will be determined later during
      // the resolving process.
      aResolver->Put(name, value,
                     eCSSTokenSerialization_Nothing,
                     eCSSTokenSerialization_Nothing,
                     false);
    }
  }
}

void
CSSVariableDeclarations::ResolveRevertLayerFallbacks(
                                             const CSSVariableValues* aInherited,
                                             CSSVariableValues* aOutput) const
{
  for (auto iter = mRevertLayerSources.ConstIter();
       !iter.Done(); iter.Next()) {
    const nsAString& name = iter.Key();
    nsString value;
    nsCSSTokenSerializationType firstToken, lastToken;
    if (!aOutput->Get(name, value, firstToken, lastToken)) {
      continue;
    }

    if (!IsComputedRevertLayerValue(value)) {
      continue;
    }

    const RevertLayerFallbackList* fallbacks = mRevertLayerFallbacks.Get(name);
    if (fallbacks) {
      nsString sourceValue = iter.UserData();
      bool foundFallback = false;
      for (uint32_t i = 0, n = fallbacks->mItems.Length(); i < n; i++) {
        const RevertLayerFallback& fallback = fallbacks->mItems[i];
        if (ShouldIgnoreForRevertLayerSourceValue(sourceValue,
                                                 fallback.mSource)) {
          continue;
        }

        if (IsRevertLayerValue(fallback.mValue)) {
          sourceValue = fallback.mSource;
          continue;
        }

        CSSVariableValues inheritedValues(*aOutput);
        if (aInherited->Get(name, value, firstToken, lastToken)) {
          inheritedValues.Put(name, value, firstToken, lastToken);
        } else {
          inheritedValues.Remove(name);
        }

        CSSVariableDeclarations fallbackDeclarations;
        fallbackDeclarations.mVariables.Put(name, fallback.mValue);

        CSSVariableValues fallbackValues;
        CSSVariableResolver resolver(&fallbackValues);
        resolver.Resolve(&inheritedValues, &fallbackDeclarations);

        if (!fallbackValues.Get(name, value, firstToken, lastToken)) {
          aOutput->Remove(name);
          foundFallback = true;
          break;
        }

        if (IsComputedRevertLayerValue(value)) {
          sourceValue = fallback.mSource;
          continue;
        }

        aOutput->Put(name, value, firstToken, lastToken);
        foundFallback = true;
        break;
      }

      if (foundFallback) {
        continue;
      }
    }

    if (aInherited->Get(name, value, firstToken, lastToken)) {
      aOutput->Put(name, value, firstToken, lastToken);
    } else {
      aOutput->Remove(name);
    }
  }
}

size_t
CSSVariableDeclarations::SizeOfIncludingThis(
                                      mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mVariables.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += iter.Data().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  n += mRevertLayerSources.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mRevertLayerSources.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += iter.Data().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  n += mRevertLayerFallbacks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mRevertLayerFallbacks.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    const RevertLayerFallbackList* list = iter.UserData();
    n += aMallocSizeOf(list);
    n += list->mItems.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (uint32_t i = 0, len = list->mItems.Length(); i < len; i++) {
      n += list->mItems[i].mValue.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
      n += list->mItems[i].mSource.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
  }
  return n;
}

} // namespace mozilla
