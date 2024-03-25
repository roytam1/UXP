/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DeclarationBlockInlines_h
#define mozilla_DeclarationBlockInlines_h

#include "mozilla/css/Declaration.h"

namespace mozilla {

MOZ_DEFINE_DEPRECATED_METHODS(DeclarationBlock, css::Declaration)

MozExternalRefCountType
DeclarationBlock::AddRef()
{
  return AsGecko()->AddRef();
}

MozExternalRefCountType
DeclarationBlock::Release()
{
  return AsGecko()->Release();
}

already_AddRefed<DeclarationBlock>
DeclarationBlock::Clone() const
{
  RefPtr<DeclarationBlock> result;
  result = new css::Declaration(*AsGecko());
  return result.forget();
}

already_AddRefed<DeclarationBlock>
DeclarationBlock::EnsureMutable()
{
#ifdef DEBUG
  AsGecko()->AssertNotExpanded();
#endif
  if (!IsMutable()) {
    return Clone();
  }
  return do_AddRef(this);
}

void
DeclarationBlock::ToString(nsAString& aString) const
{
  AsGecko()->ToString(aString);
}

uint32_t
DeclarationBlock::Count() const
{
  return AsGecko()->Count();
}

bool
DeclarationBlock::GetNthProperty(uint32_t aIndex, nsAString& aReturn) const
{
  return AsGecko()->GetNthProperty(aIndex, aReturn);
}

void
DeclarationBlock::GetPropertyValue(const nsAString& aProperty,
                                   nsAString& aValue) const
{
  AsGecko()->GetPropertyValue(aProperty, aValue);
}

void
DeclarationBlock::GetPropertyValueByID(nsCSSPropertyID aPropID,
                                       nsAString& aValue) const
{
  AsGecko()->GetPropertyValueByID(aPropID, aValue);
}

void
DeclarationBlock::GetAuthoredPropertyValue(const nsAString& aProperty,
                                           nsAString& aValue) const
{
  AsGecko()->GetAuthoredPropertyValue(aProperty, aValue);
}

bool
DeclarationBlock::GetPropertyIsImportant(const nsAString& aProperty) const
{
  return AsGecko()->GetPropertyIsImportant(aProperty);
}

void
DeclarationBlock::RemoveProperty(const nsAString& aProperty)
{
  AsGecko()->RemoveProperty(aProperty);
}

void
DeclarationBlock::RemovePropertyByID(nsCSSPropertyID aProperty)
{
  AsGecko()->RemovePropertyByID(aProperty);
}

} // namespace mozilla

#endif // mozilla_DeclarationBlockInlines_h
