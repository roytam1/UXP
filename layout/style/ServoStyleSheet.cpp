/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServoBindings.h"
#include "mozilla/ServoStyleSheet.h"
#include "CSSRuleList.h"

using namespace mozilla::dom;

namespace mozilla {

ServoStyleSheet::ServoStyleSheet(css::SheetParsingMode aParsingMode,
                                 CORSMode aCORSMode,
                                 net::ReferrerPolicy aReferrerPolicy,
                                 const dom::SRIMetadata& aIntegrity)
  : StyleSheet(aParsingMode)
  , mSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
}

ServoStyleSheet::~ServoStyleSheet()
{
}

bool
ServoStyleSheet::HasRules() const
{
  return false;
}

void
ServoStyleSheet::SetAssociatedDocument(nsIDocument* aDocument,
                                       DocumentAssociationMode aAssociationMode)
{
}

ServoStyleSheet*
ServoStyleSheet::GetParentSheet() const
{
  return nullptr;
}

void
ServoStyleSheet::AppendStyleSheet(ServoStyleSheet* aSheet)
{
}

nsresult
ServoStyleSheet::ParseSheet(const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber)
{
  return NS_OK;
}

void
ServoStyleSheet::LoadFailed()
{
}

void
ServoStyleSheet::DropSheet()
{
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0;
}

#ifdef DEBUG
void
ServoStyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  MOZ_CRASH("stylo: not implemented");
}
#endif

nsMediaList*
ServoStyleSheet::Media()
{
  return nullptr;
}

nsIDOMCSSRule*
ServoStyleSheet::GetDOMOwnerRule() const
{
  return nullptr;
}

CSSRuleList*
ServoStyleSheet::GetCssRulesInternal(ErrorResult& aRv)
{
  return nullptr;
}

uint32_t
ServoStyleSheet::InsertRuleInternal(const nsAString& aRule,
                                    uint32_t aIndex, ErrorResult& aRv)
{
  return 0;
}

void
ServoStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
}

} // namespace mozilla
