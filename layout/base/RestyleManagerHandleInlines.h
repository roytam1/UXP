/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerHandleInlines_h
#define mozilla_RestyleManagerHandleInlines_h

#include "mozilla/RestyleManager.h"

namespace mozilla {

MozExternalRefCountType
RestyleManagerHandle::Ptr::AddRef()
{
  return AsGecko()->AddRef();
}

MozExternalRefCountType
RestyleManagerHandle::Ptr::Release()
{
  return AsGecko()->Release();
}

void
RestyleManagerHandle::Ptr::Disconnect()
{
  AsGecko()->Disconnect();
}

void
RestyleManagerHandle::Ptr::PostRestyleEvent(dom::Element* aElement,
                                            nsRestyleHint aRestyleHint,
                                            nsChangeHint aMinChangeHint)
{
  AsGecko()->PostRestyleEvent(aElement, aRestyleHint, aMinChangeHint);
}

void
RestyleManagerHandle::Ptr::PostRestyleEventForLazyConstruction()
{
  AsGecko()->PostRestyleEventForLazyConstruction();
}

void
RestyleManagerHandle::Ptr::RebuildAllStyleData(nsChangeHint aExtraHint,
                                               nsRestyleHint aRestyleHint)
{
  AsGecko()->RebuildAllStyleData(aExtraHint, aRestyleHint);
}

void
RestyleManagerHandle::Ptr::PostRebuildAllStyleDataEvent(
    nsChangeHint aExtraHint,
    nsRestyleHint aRestyleHint)
{
  AsGecko()->PostRebuildAllStyleDataEvent(aExtraHint, aRestyleHint);
}

void
RestyleManagerHandle::Ptr::ProcessPendingRestyles()
{
  AsGecko()->ProcessPendingRestyles();
}

nsresult
RestyleManagerHandle::Ptr::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  return AsGecko()->ProcessRestyledFrames(aChangeList);
}

void
RestyleManagerHandle::Ptr::FlushOverflowChangedTracker()
{
  AsGecko()->FlushOverflowChangedTracker();
}

void
RestyleManagerHandle::Ptr::ContentInserted(nsINode* aContainer,
                                           nsIContent* aChild)
{
  AsGecko()->ContentInserted(aContainer, aChild);
}

void
RestyleManagerHandle::Ptr::ContentAppended(nsIContent* aContainer,
                                           nsIContent* aFirstNewContent)
{
  AsGecko()->ContentAppended(aContainer, aFirstNewContent);
}

void
RestyleManagerHandle::Ptr::ContentRemoved(nsINode* aContainer,
                                          nsIContent* aOldChild,
                                          nsIContent* aFollowingSibling)
{
  AsGecko()->ContentRemoved(aContainer, aOldChild, aFollowingSibling);
}

void
RestyleManagerHandle::Ptr::RestyleForInsertOrChange(nsINode* aContainer,
                                                    nsIContent* aChild)
{
  AsGecko()->RestyleForInsertOrChange(aContainer, aChild);
}

void
RestyleManagerHandle::Ptr::RestyleForAppend(nsIContent* aContainer,
                                            nsIContent* aFirstNewContent)
{
  AsGecko()->RestyleForAppend(aContainer, aFirstNewContent);
}

void
RestyleManagerHandle::Ptr::ContentStateChanged(nsIContent* aContent,
                                          EventStates aStateMask)
{
  AsGecko()->ContentStateChanged(aContent, aStateMask);
}

void
RestyleManagerHandle::Ptr::AttributeWillChange(dom::Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsIAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aNewValue)
{
  AsGecko()->AttributeWillChange(aElement, aNameSpaceID, aAttribute, aModType,
                                 aNewValue);
}

void
RestyleManagerHandle::Ptr::AttributeChanged(dom::Element* aElement,
                                            int32_t aNameSpaceID,
                                            nsIAtom* aAttribute,
                                            int32_t aModType,
                                            const nsAttrValue* aOldValue)
{
  AsGecko()->AttributeChanged(aElement, aNameSpaceID, aAttribute, aModType,
                              aOldValue);
}

nsresult
RestyleManagerHandle::Ptr::ReparentStyleContext(nsIFrame* aFrame)
{
  return AsGecko()->ReparentStyleContext(aFrame);
}

bool
RestyleManagerHandle::Ptr::HasPendingRestyles()
{
  return AsGecko()->HasPendingRestyles();
}

uint64_t
RestyleManagerHandle::Ptr::GetRestyleGeneration() const
{
  return AsGecko()->GetRestyleGeneration();
}

uint32_t
RestyleManagerHandle::Ptr::GetHoverGeneration() const
{
  return AsGecko()->GetHoverGeneration();
}

void
RestyleManagerHandle::Ptr::SetObservingRefreshDriver(bool aObserving)
{
  AsGecko()->SetObservingRefreshDriver(aObserving);
}

void
RestyleManagerHandle::Ptr::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  AsGecko()->NotifyDestroyingFrame(aFrame);
}

} // namespace mozilla

#endif // mozilla_RestyleManagerHandleInlines_h
