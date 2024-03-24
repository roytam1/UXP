/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSetHandleInlines_h
#define mozilla_StyleSetHandleInlines_h

#include "mozilla/StyleSheetInlines.h"
#include "nsStyleSet.h"

namespace mozilla {

void
StyleSetHandle::Ptr::Delete()
{
  if (mValue) {
    delete AsGecko();
  }
}

void
StyleSetHandle::Ptr::Init(nsPresContext* aPresContext)
{
  AsGecko()->Init(aPresContext);
}

void
StyleSetHandle::Ptr::BeginShutdown()
{
  AsGecko()->BeginShutdown();
}

void
StyleSetHandle::Ptr::Shutdown()
{
  AsGecko()->Shutdown();
}

bool
StyleSetHandle::Ptr::GetAuthorStyleDisabled() const
{
  return AsGecko()->GetAuthorStyleDisabled();
}

nsresult
StyleSetHandle::Ptr::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  return AsGecko()->SetAuthorStyleDisabled(aStyleDisabled);
}

void
StyleSetHandle::Ptr::BeginUpdate()
{
  AsGecko()->BeginUpdate();
}

nsresult
StyleSetHandle::Ptr::EndUpdate()
{
  return AsGecko()->EndUpdate();
}

// resolve a style context
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext)
{
  return AsGecko()->ResolveStyleFor(aElement, aParentContext);
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext,
                                     TreeMatchContext& aTreeMatchContext)
{
  return AsGecko()->ResolveStyleFor(aElement, aParentContext, aTreeMatchContext);
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForText(nsIContent* aTextNode,
                                         nsStyleContext* aParentContext)
{
  return AsGecko()->ResolveStyleForText(aTextNode, aParentContext);
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForOtherNonElement(nsStyleContext* aParentContext)
{
  return AsGecko()->ResolveStyleForOtherNonElement(aParentContext);
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolvePseudoElementStyle(dom::Element* aParentElement,
                                               CSSPseudoElementType aType,
                                               nsStyleContext* aParentContext,
                                               dom::Element* aPseudoElement)
{
  return AsGecko()->ResolvePseudoElementStyle(
    aParentElement, aType, aParentContext, aPseudoElement);
}

// aFlags is an nsStyleSet flags bitfield
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                              nsStyleContext* aParentContext,
                                              uint32_t aFlags)
{
  return AsGecko()->ResolveAnonymousBoxStyle(aPseudoTag, aParentContext, aFlags);
}

// manage the set of style sheets in the style set
nsresult
StyleSetHandle::Ptr::AppendStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  return AsGecko()->AppendStyleSheet(aType, aSheet->AsGecko());
}

nsresult
StyleSetHandle::Ptr::PrependStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  return AsGecko()->PrependStyleSheet(aType, aSheet->AsGecko());
}

nsresult
StyleSetHandle::Ptr::RemoveStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  return AsGecko()->RemoveStyleSheet(aType, aSheet->AsGecko());
}

nsresult
StyleSetHandle::Ptr::ReplaceSheets(SheetType aType,
                       const nsTArray<RefPtr<StyleSheet>>& aNewSheets)
{
  nsTArray<RefPtr<CSSStyleSheet>> newSheets(aNewSheets.Length());
  for (auto& sheet : aNewSheets) {
    newSheets.AppendElement(sheet->AsGecko());
  }
  return AsGecko()->ReplaceSheets(aType, newSheets);
}

nsresult
StyleSetHandle::Ptr::InsertStyleSheetBefore(SheetType aType,
                                StyleSheet* aNewSheet,
                                StyleSheet* aReferenceSheet)
{
  return AsGecko()->InsertStyleSheetBefore(
    aType, aNewSheet->AsGecko(), aReferenceSheet->AsGecko());
}

int32_t
StyleSetHandle::Ptr::SheetCount(SheetType aType) const
{
  return AsGecko()->SheetCount(aType);
}

StyleSheet*
StyleSetHandle::Ptr::StyleSheetAt(SheetType aType, int32_t aIndex) const
{
  return AsGecko()->StyleSheetAt(aType, aIndex);
}

nsresult
StyleSetHandle::Ptr::RemoveDocStyleSheet(StyleSheet* aSheet)
{
  return AsGecko()->RemoveDocStyleSheet(aSheet->AsGecko());
}

nsresult
StyleSetHandle::Ptr::AddDocStyleSheet(StyleSheet* aSheet,
                                      nsIDocument* aDocument)
{
  return AsGecko()->AddDocStyleSheet(aSheet->AsGecko(), aDocument);
}

// check whether there is ::before/::after style for an element
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext)
{
  return AsGecko()->ProbePseudoElementStyle(aParentElement, aType, aParentContext);
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext,
                                             TreeMatchContext& aTreeMatchContext,
                                             dom::Element* aPseudoElement)
{
  return AsGecko()->ProbePseudoElementStyle(
    aParentElement, aType, aParentContext, aTreeMatchContext, aPseudoElement);
}

nsRestyleHint
StyleSetHandle::Ptr::HasStateDependentStyle(dom::Element* aElement,
                                            EventStates aStateMask)
{
  return AsGecko()->HasStateDependentStyle(aElement, aStateMask);
}

nsRestyleHint
StyleSetHandle::Ptr::HasStateDependentStyle(dom::Element* aElement,
                                            CSSPseudoElementType aPseudoType,
                                            dom::Element* aPseudoElement,
                                            EventStates aStateMask)
{
  return AsGecko()->HasStateDependentStyle(aElement, aPseudoType, aPseudoElement, aStateMask);
}

void
StyleSetHandle::Ptr::RootStyleContextAdded()
{
  AsGecko()->RootStyleContextAdded();
}

void
StyleSetHandle::Ptr::RootStyleContextRemoved()
{
  RootStyleContextAdded();
}

} // namespace mozilla

#endif // mozilla_StyleSetHandleInlines_h
