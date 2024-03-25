/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInlines_h
#define mozilla_StyleSheetInlines_h

#include "mozilla/StyleSheetInfo.h"
#include "mozilla/CSSStyleSheet.h"

namespace mozilla {

MOZ_DEFINE_DEPRECATED_METHODS(StyleSheet, CSSStyleSheet)

StyleSheetInfo&
StyleSheet::SheetInfo()
{
  return *AsGecko()->mInner;
}

const StyleSheetInfo&
StyleSheet::SheetInfo() const
{
  return *AsGecko()->mInner;
}

bool
StyleSheet::IsInline() const
{
  return !SheetInfo().mOriginalSheetURI;
}

nsIURI*
StyleSheet::GetSheetURI() const
{
  return SheetInfo().mSheetURI;
}

nsIURI*
StyleSheet::GetOriginalURI() const
{
  return SheetInfo().mOriginalSheetURI;
}

nsIURI*
StyleSheet::GetBaseURI() const
{
  return SheetInfo().mBaseURI;
}

void
StyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                    nsIURI* aBaseURI)
{
  NS_PRECONDITION(aSheetURI && aBaseURI, "null ptr");
  StyleSheetInfo& info = SheetInfo();
  MOZ_ASSERT(!HasRules() && !info.mComplete,
             "Can't call SetURIs on sheets that are complete or have rules");
  info.mSheetURI = aSheetURI;
  info.mOriginalSheetURI = aOriginalSheetURI;
  info.mBaseURI = aBaseURI;
}

bool
StyleSheet::IsApplicable() const
{
  return !mDisabled && SheetInfo().mComplete;
}

bool
StyleSheet::HasRules() const
{
  return AsGecko()->HasRules();
}

void
StyleSheet::SetAssociatedDocument(nsIDocument* aDocument,
                                  DocumentAssociationMode aAssociationMode)
{
  MOZ_ASSERT(aDocument);
  AsGecko()->SetAssociatedDocument(aDocument, aAssociationMode);
}

void
StyleSheet::ClearAssociatedDocument()
{
  AsGecko()->SetAssociatedDocument(nullptr, NotOwnedByDocument);
}

StyleSheet*
StyleSheet::GetParentSheet() const
{
  return AsGecko()->GetParentSheet();
}

StyleSheet*
StyleSheet::GetParentStyleSheet() const
{
  return GetParentSheet();
}

dom::ParentObject
StyleSheet::GetParentObject() const
{
  if (mOwningNode) {
    return dom::ParentObject(mOwningNode);
  }
  return dom::ParentObject(GetParentSheet());
}

void
StyleSheet::AppendStyleSheet(StyleSheet* aSheet)
{
  AsGecko()->AppendStyleSheet(aSheet->AsGecko());
}

nsIPrincipal*
StyleSheet::Principal() const
{
  return SheetInfo().mPrincipal;
}

void
StyleSheet::SetPrincipal(nsIPrincipal* aPrincipal)
{
  StyleSheetInfo& info = SheetInfo();
  NS_PRECONDITION(!info.mPrincipalSet, "Should only set principal once");
  if (aPrincipal) {
    info.mPrincipal = aPrincipal;
#ifdef DEBUG
    info.mPrincipalSet = true;
#endif
  }
}

CORSMode
StyleSheet::GetCORSMode() const
{
  return SheetInfo().mCORSMode;
}

net::ReferrerPolicy
StyleSheet::GetReferrerPolicy() const
{
  return SheetInfo().mReferrerPolicy;
}

void
StyleSheet::GetIntegrity(dom::SRIMetadata& aResult) const
{
  aResult = SheetInfo().mIntegrity;
}

size_t
StyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return AsGecko()->SizeOfIncludingThis(aMallocSizeOf);
}

#ifdef DEBUG
void
StyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  AsGecko()->List(aOut, aIndex);
}
#endif

void StyleSheet::WillDirty() { AsGecko()->WillDirty(); }
void StyleSheet::DidDirty() { AsGecko()->DidDirty(); }


}

#endif // mozilla_StyleSheetInlines_h
