/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerHandle_h
#define mozilla_RestyleManagerHandle_h

#include "mozilla/Assertions.h"
#include "mozilla/EventStates.h"
#include "mozilla/HandleRefPtr.h"
#include "mozilla/RefCountType.h"
#include "nsChangeHint.h"

namespace mozilla {
class RestyleManager;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;
class nsStyleChangeList;

namespace mozilla {

/**
 * Smart pointer class that can hold a pointer to a RestyleManager.
 */
class RestyleManagerHandle
{
public:
  typedef HandleRefPtr<RestyleManagerHandle> RefPtr;

  // We define this Ptr class with a RestyleManager API that forwards on to the
  // wrapped pointer, rather than putting these methods on RestyleManagerHandle
  // itself, so that we can have RestyleManagerHandle behave like a smart
  // pointer and be dereferenced with operator->.
  class Ptr
  {
  public:
    friend class ::mozilla::RestyleManagerHandle;

    RestyleManager* AsGecko()
    {
      return reinterpret_cast<RestyleManager*>(mValue);
    }

    RestyleManager* GetAsGecko() { return AsGecko(); }

    const RestyleManager* AsGecko() const
    {
      return const_cast<Ptr*>(this)->AsGecko();
    }

    const RestyleManager* GetAsGecko() const { return AsGecko(); }

    // These inline methods are defined in RestyleManagerHandleInlines.h.
    inline MozExternalRefCountType AddRef();
    inline MozExternalRefCountType Release();

    // Restyle manager interface.  These inline methods are defined in
    // RestyleManagerHandleInlines.h and just forward to the underlying
    // RestyleManager.  See corresponding comments in
    // RestyleManager.h for descriptions of these methods.

    inline void Disconnect();
    inline void PostRestyleEvent(dom::Element* aElement,
                                 nsRestyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint);
    inline void PostRestyleEventForLazyConstruction();
    inline void RebuildAllStyleData(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);
    inline void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                             nsRestyleHint aRestyleHint);
    inline void ProcessPendingRestyles();
    inline void ContentInserted(nsINode* aContainer,
                                nsIContent* aChild);
    inline void ContentAppended(nsIContent* aContainer,
                                nsIContent* aFirstNewContent);
    inline void ContentRemoved(nsINode* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aFollowingSibling);
    inline void RestyleForInsertOrChange(nsINode* aContainer,
                                         nsIContent* aChild);
    inline void RestyleForAppend(nsIContent* aContainer,
                                 nsIContent* aFirstNewContent);
  inline void ContentStateChanged(nsIContent* aContent,
                                  EventStates aStateMask);
    inline void AttributeWillChange(dom::Element* aElement,
                                    int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType,
                                    const nsAttrValue* aNewValue);
    inline void AttributeChanged(dom::Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType,
                                 const nsAttrValue* aOldValue);
    inline nsresult ReparentStyleContext(nsIFrame* aFrame);
    inline bool HasPendingRestyles();
    inline uint64_t GetRestyleGeneration() const;
    inline uint32_t GetHoverGeneration() const;
    inline void SetObservingRefreshDriver(bool aObserving);
    inline nsresult ProcessRestyledFrames(nsStyleChangeList& aChangeList);
    inline void FlushOverflowChangedTracker();
    inline void NotifyDestroyingFrame(nsIFrame* aFrame);

  private:
    // Stores a pointer to an RestyleManager.
    uintptr_t mValue;
  };

  MOZ_IMPLICIT RestyleManagerHandle(decltype(nullptr) = nullptr)
  {
    mPtr.mValue = 0;
  }
  RestyleManagerHandle(const RestyleManagerHandle& aOth)
  {
    mPtr.mValue = aOth.mPtr.mValue;
  }
  MOZ_IMPLICIT RestyleManagerHandle(RestyleManager* aManager)
  {
    *this = aManager;
  }

  RestyleManagerHandle& operator=(decltype(nullptr))
  {
    mPtr.mValue = 0;
    return *this;
  }

  RestyleManagerHandle& operator=(RestyleManager* aManager)
  {
    mPtr.mValue = reinterpret_cast<uintptr_t>(aManager);
    return *this;
  }

  // Make RestyleManagerHandle usable in boolean contexts.
  explicit operator bool() const { return !!mPtr.mValue; }
  bool operator!() const { return !mPtr.mValue; }

  // Make RestyleManagerHandle behave like a smart pointer.
  Ptr* operator->() { return &mPtr; }
  const Ptr* operator->() const { return &mPtr; }

private:
  Ptr mPtr;
};

} // namespace mozilla

#endif // mozilla_RestyleManagerHandle_h
