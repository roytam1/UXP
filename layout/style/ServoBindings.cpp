/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"

#include "ChildIterator.h"
#include "StyleStructContext.h"
#include "gfxFontFamilyList.h"
#include "nsAttrValueInlines.h"
#include "nsCSSRuleProcessor.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsIContentInlines.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsNameSpaceManager.h"
#include "nsRuleNode.h"
#include "nsString.h"
#include "nsStyleStruct.h"
#include "nsStyleUtil.h"
#include "nsTArray.h"

#include "mozilla/EventStates.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

#define IMPL_STRONG_REF_TYPE_FOR(type_) \
  already_AddRefed<type_>               \
  type_##Strong::Consume() {            \
    RefPtr<type_> result;               \
    result.swap(mPtr);                  \
    return result.forget();             \
  }

IMPL_STRONG_REF_TYPE_FOR(ServoComputedValues)
IMPL_STRONG_REF_TYPE_FOR(RawServoStyleSheet)
IMPL_STRONG_REF_TYPE_FOR(RawServoDeclarationBlock)

#undef IMPL_STRONG_REF_TYPE_FOR

uint32_t
Gecko_ChildrenCount(RawGeckoNodeBorrowed aNode)
{
  return 0;
}

bool
Gecko_NodeIsElement(RawGeckoNodeBorrowed aNode)
{
  return false;
}

RawGeckoNodeBorrowedOrNull
Gecko_GetParentNode(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

RawGeckoNodeBorrowedOrNull
Gecko_GetFirstChild(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

RawGeckoNodeBorrowedOrNull
Gecko_GetLastChild(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

RawGeckoNodeBorrowedOrNull
Gecko_GetPrevSibling(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

RawGeckoNodeBorrowedOrNull
Gecko_GetNextSibling(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull
Gecko_GetParentElement(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull
Gecko_GetFirstChildElement(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull Gecko_GetLastChildElement(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull
Gecko_GetPrevSiblingElement(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull
Gecko_GetNextSiblingElement(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

RawGeckoElementBorrowedOrNull
Gecko_GetDocumentElement(RawGeckoDocumentBorrowed aDoc)
{
  return nullptr;
}

StyleChildrenIteratorOwnedOrNull
Gecko_MaybeCreateStyleChildrenIterator(RawGeckoNodeBorrowed aNode)
{
  return nullptr;
}

void
Gecko_DropStyleChildrenIterator(StyleChildrenIteratorOwned aIterator)
{
}

RawGeckoNodeBorrowed
Gecko_GetNextStyleChild(StyleChildrenIteratorBorrowedMut aIterator)
{
  return nullptr;
}

EventStates::ServoType
Gecko_ElementState(RawGeckoElementBorrowed aElement)
{
  return 0;
}

bool
Gecko_IsHTMLElementInHTMLDocument(RawGeckoElementBorrowed aElement)
{
  return false;
}

bool
Gecko_IsLink(RawGeckoElementBorrowed aElement)
{
  return false;
}

bool
Gecko_IsTextNode(RawGeckoNodeBorrowed aNode)
{
  return false;
}

bool
Gecko_IsVisitedLink(RawGeckoElementBorrowed aElement)
{
  return false;
}

bool
Gecko_IsUnvisitedLink(RawGeckoElementBorrowed aElement)
{
  return false;
}

bool
Gecko_IsRootElement(RawGeckoElementBorrowed aElement)
{
  return false;
}

nsIAtom*
Gecko_LocalName(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

nsIAtom*
Gecko_Namespace(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

nsIAtom*
Gecko_GetElementId(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

// Dirtiness tracking.
uint32_t
Gecko_GetNodeFlags(RawGeckoNodeBorrowed aNode)
{
  return 0;
}

void
Gecko_SetNodeFlags(RawGeckoNodeBorrowed aNode, uint32_t aFlags)
{
}

void
Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed aNode, uint32_t aFlags)
{
}

nsStyleContext*
Gecko_GetStyleContext(RawGeckoNodeBorrowed aNode, nsIAtom* aPseudoTagOrNull)
{
  return nullptr;
}

nsChangeHint
Gecko_CalcStyleDifference(nsStyleContext* aOldStyleContext,
                          ServoComputedValuesBorrowed aComputedValues)
{
  return nsChangeHint(0);
}

void
Gecko_StoreStyleDifference(RawGeckoNodeBorrowed aNode, nsChangeHint aChangeHintToStore)
{
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetServoDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  return nullptr;
}

void
Gecko_FillAllBackgroundLists(nsStyleImageLayers* aLayers, uint32_t aMaxLen)
{
}

void
Gecko_FillAllMaskLists(nsStyleImageLayers* aLayers, uint32_t aMaxLen)
{
}

template <typename Implementor>
static nsIAtom*
AtomAttrValue(Implementor* aElement, nsIAtom* aName)
{
  return nullptr;
}

template <typename Implementor, typename MatchFn>
static bool
DoMatch(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName, MatchFn aMatch)
{
  return false;
}

template <typename Implementor>
static bool
HasAttr(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName)
{
  return false;
}

template <typename Implementor>
static bool
AttrEquals(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName, nsIAtom* aStr,
           bool aIgnoreCase)
{
  return false;
}

template <typename Implementor>
static bool
AttrDashEquals(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
               nsIAtom* aStr)
{
  return false;
}

template <typename Implementor>
static bool
AttrIncludes(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
             nsIAtom* aStr)
{
  return false;
}

template <typename Implementor>
static bool
AttrHasSubstring(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
                 nsIAtom* aStr)
{
  return false;
}

template <typename Implementor>
static bool
AttrHasPrefix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr)
{
  return false;
}

template <typename Implementor>
static bool
AttrHasSuffix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr)
{
  return false;
}

/**
 * Gets the class or class list (if any) of the implementor. The calling
 * convention here is rather hairy, and is optimized for getting Servo the
 * information it needs for hot calls.
 *
 * The return value indicates the number of classes. If zero, neither outparam
 * is valid. If one, the class_ outparam is filled with the atom of the class.
 * If two or more, the classList outparam is set to point to an array of atoms
 * representing the class list.
 *
 * The array is borrowed and the atoms are not addrefed. These values can be
 * invalidated by any DOM mutation. Use them in a tight scope.
 */
template <typename Implementor>
static uint32_t
ClassOrClassList(Implementor* aElement, nsIAtom** aClass, nsIAtom*** aClassList)
{
  return 0;
}

#define SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)      \
  nsIAtom* prefix_##AtomAttrValue(implementor_ aElement, nsIAtom* aName)       \
  {                                                                            \
    return AtomAttrValue(aElement, aName);                                     \
  }                                                                            \
  bool prefix_##HasAttr(implementor_ aElement, nsIAtom* aNS, nsIAtom* aName)   \
  {                                                                            \
    return HasAttr(aElement, aNS, aName);                                      \
  }                                                                            \
  bool prefix_##AttrEquals(implementor_ aElement, nsIAtom* aNS,                \
                           nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)    \
  {                                                                            \
    return AttrEquals(aElement, aNS, aName, aStr, aIgnoreCase);                \
  }                                                                            \
  bool prefix_##AttrDashEquals(implementor_ aElement, nsIAtom* aNS,            \
                               nsIAtom* aName, nsIAtom* aStr)                  \
  {                                                                            \
    return AttrDashEquals(aElement, aNS, aName, aStr);                         \
  }                                                                            \
  bool prefix_##AttrIncludes(implementor_ aElement, nsIAtom* aNS,              \
                             nsIAtom* aName, nsIAtom* aStr)                    \
  {                                                                            \
    return AttrIncludes(aElement, aNS, aName, aStr);                           \
  }                                                                            \
  bool prefix_##AttrHasSubstring(implementor_ aElement, nsIAtom* aNS,          \
                                 nsIAtom* aName, nsIAtom* aStr)                \
  {                                                                            \
    return AttrHasSubstring(aElement, aNS, aName, aStr);                       \
  }                                                                            \
  bool prefix_##AttrHasPrefix(implementor_ aElement, nsIAtom* aNS,             \
                              nsIAtom* aName, nsIAtom* aStr)                   \
  {                                                                            \
    return AttrHasPrefix(aElement, aNS, aName, aStr);                          \
  }                                                                            \
  bool prefix_##AttrHasSuffix(implementor_ aElement, nsIAtom* aNS,             \
                              nsIAtom* aName, nsIAtom* aStr)                   \
  {                                                                            \
    return AttrHasSuffix(aElement, aNS, aName, aStr);                          \
  }                                                                            \
  uint32_t prefix_##ClassOrClassList(implementor_ aElement, nsIAtom** aClass,  \
                                     nsIAtom*** aClassList)                    \
  {                                                                            \
    return ClassOrClassList(aElement, aClass, aClassList);                     \
  }

SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, RawGeckoElementBorrowed)
SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_Snapshot, ServoElementSnapshot*)

#undef SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS

nsIAtom*
Gecko_Atomize(const char* aString, uint32_t aLength)
{
  return nullptr;
}

void
Gecko_AddRefAtom(nsIAtom* aAtom)
{
}

void
Gecko_ReleaseAtom(nsIAtom* aAtom)
{
}

const uint16_t*
Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength)
{
  return nullptr;
}

bool
Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  return false;
}

bool
Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  return false;
}

void
Gecko_Utf8SliceToString(nsString* aString,
                        const uint8_t* aBuffer,
                        size_t aBufferLen)
{
}

void
Gecko_FontFamilyList_Clear(FontFamilyList* aList) {
}

void
Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName)
{
}

void
Gecko_FontFamilyList_AppendGeneric(FontFamilyList* aList, FontFamilyType aType)
{
}

void
Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src)
{
}

void
Gecko_SetListStyleType(nsStyleList* style_struct, uint32_t type)
{
}

void
Gecko_CopyListStyleTypeFrom(nsStyleList* dst, const nsStyleList* src)
{
}

NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIPrincipal, Principal)
NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIURI, URI)

void
Gecko_SetMozBinding(nsStyleDisplay* aDisplay,
                    const uint8_t* aURLString, uint32_t aURLStringLength,
                    ThreadSafeURIHolder* aBaseURI,
                    ThreadSafeURIHolder* aReferrer,
                    ThreadSafePrincipalHolder* aPrincipal)
{
}

void
Gecko_CopyMozBindingFrom(nsStyleDisplay* aDest, const nsStyleDisplay* aSrc)
{
}


void
Gecko_SetNullImageValue(nsStyleImage* aImage)
{
}

void
Gecko_SetGradientImageValue(nsStyleImage* aImage, nsStyleGradient* aGradient)
{
}

static already_AddRefed<nsStyleImageRequest>
CreateStyleImageRequest(nsStyleImageRequest::Mode aModeFlags,
                        const uint8_t* aURLString, uint32_t aURLStringLength,
                        ThreadSafeURIHolder* aBaseURI,
                        ThreadSafeURIHolder* aReferrer,
                        ThreadSafePrincipalHolder* aPrincipal)
{
  return nullptr;
}

void
Gecko_SetUrlImageValue(nsStyleImage* aImage,
                       const uint8_t* aURLString, uint32_t aURLStringLength,
                       ThreadSafeURIHolder* aBaseURI,
                       ThreadSafeURIHolder* aReferrer,
                       ThreadSafePrincipalHolder* aPrincipal)
{
}

void
Gecko_CopyImageValueFrom(nsStyleImage* aImage, const nsStyleImage* aOther)
{
}

nsStyleGradient*
Gecko_CreateGradient(uint8_t aShape,
                     uint8_t aSize,
                     bool aRepeating,
                     bool aLegacySyntax,
                     uint32_t aStopCount)
{
  return nullptr;
}

void
Gecko_SetListStyleImageNone(nsStyleList* aList)
{
}

void
Gecko_SetListStyleImage(nsStyleList* aList,
                        const uint8_t* aURLString, uint32_t aURLStringLength,
                        ThreadSafeURIHolder* aBaseURI,
                        ThreadSafeURIHolder* aReferrer,
                        ThreadSafePrincipalHolder* aPrincipal)
{
}

void
Gecko_CopyListStyleImageFrom(nsStyleList* aList, const nsStyleList* aSource)
{
}

void
Gecko_EnsureTArrayCapacity(void* aArray, size_t aCapacity, size_t aElemSize)
{
}

void
Gecko_ClearPODTArray(void* aArray, size_t aElementSize, size_t aElementAlign)
{
}

void
Gecko_ClearStyleContents(nsStyleContent* aContent)
{
}

void
Gecko_CopyStyleContentsFrom(nsStyleContent* aContent, const nsStyleContent* aOther)
{
}

void
Gecko_EnsureImageLayersLength(nsStyleImageLayers* aLayers, size_t aLen,
                              nsStyleImageLayers::LayerType aLayerType)
{
}

void
Gecko_ResetStyleCoord(nsStyleUnit* aUnit, nsStyleUnion* aValue)
{
}

void
Gecko_SetStyleCoordCalcValue(nsStyleUnit* aUnit, nsStyleUnion* aValue, nsStyleCoord::CalcValue aCalc)
{
}

void
Gecko_CopyClipPathValueFrom(mozilla::StyleClipPath* aDst, const mozilla::StyleClipPath* aSrc)
{
}

void
Gecko_DestroyClipPath(mozilla::StyleClipPath* aClip)
{
}

mozilla::StyleBasicShape*
Gecko_NewBasicShape(mozilla::StyleBasicShapeType aType)
{
  return nullptr;
}

void
Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len)
{
}

void
Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest)
{
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray*
Gecko_NewCSSShadowArray(uint32_t aLen)
{
  return nullptr;
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

nsStyleQuoteValues*
Gecko_NewStyleQuoteValues(uint32_t aLen)
{
  return nullptr;
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsStyleQuoteValues, QuoteValues);

nsCSSValueSharedList*
Gecko_NewCSSValueSharedList(uint32_t aLen)
{
  return nullptr;
}

void
Gecko_CSSValue_SetAbsoluteLength(nsCSSValueBorrowedMut aCSSValue, nscoord aLen)
{
}

void
Gecko_CSSValue_SetNumber(nsCSSValueBorrowedMut aCSSValue, float aNumber)
{
}

void
Gecko_CSSValue_SetKeyword(nsCSSValueBorrowedMut aCSSValue, nsCSSKeyword aKeyword)
{
}

void
Gecko_CSSValue_SetPercentage(nsCSSValueBorrowedMut aCSSValue, float aPercent)
{
}

void
Gecko_CSSValue_SetAngle(nsCSSValueBorrowedMut aCSSValue, float aRadians)
{
}

void
Gecko_CSSValue_SetCalc(nsCSSValueBorrowedMut aCSSValue, nsStyleCoord::CalcValue aCalc)
{
}

void
Gecko_CSSValue_SetFunction(nsCSSValueBorrowedMut aCSSValue, int32_t aLen)
{
}

nsCSSValueBorrowedMut
Gecko_CSSValue_GetArrayItem(nsCSSValueBorrowedMut aCSSValue, int32_t aIndex)
{
  return nullptr;
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsCSSValueSharedList, CSSValueSharedList);

#define STYLE_STRUCT(name, checkdata_cb)                                      \
                                                                              \
void                                                                          \
Gecko_Construct_nsStyle##name(nsStyle##name* ptr)                             \
{                                                                             \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                         \
                                  const nsStyle##name* other)                 \
{                                                                             \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_Destroy_nsStyle##name(nsStyle##name* ptr)                               \
{                                                                             \
}

#include "nsStyleStructList.h"

#undef STYLE_STRUCT

#ifndef MOZ_STYLO
#define SERVO_BINDING_FUNC(name_, return_, ...)                               \
  return_ name_(__VA_ARGS__) {                                                \
    MOZ_CRASH("stylo: shouldn't be calling " #name_ "in a non-stylo build");  \
  }
#include "ServoBindingList.h"
#undef SERVO_BINDING_FUNC
#endif

#ifdef MOZ_STYLO
const nsStyleVariables*
Servo_GetStyleVariables(ServoComputedValuesBorrowed aComputedValues)
{
  // Servo can't provide us with Variables structs yet, so instead of linking
  // to a Servo_GetStyleVariables defined in Servo we define one here that
  // always returns the same, empty struct.
  static nsStyleVariables variables(StyleStructContext::ServoContext());
  return &variables;
}
#endif
