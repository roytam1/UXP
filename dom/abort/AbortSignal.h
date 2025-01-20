/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortSignal_h
#define mozilla_dom_AbortSignal_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class AbortSignal;

class AbortSignal final : public DOMEventTargetHelper
{
public:
  // This class must be implemented by objects who want to follow a AbortSignal.
  class Follower
  {
  public:
    virtual void Aborted() = 0;

  protected:
    virtual ~Follower();

    void
    Follow(AbortSignal* aSignal);

    void
    Unfollow();

    RefPtr<AbortSignal> mFollowingSignal;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AbortSignal, DOMEventTargetHelper)

  AbortSignal(nsIGlobalObject* aGlobalObject, bool aAborted);
  explicit AbortSignal(bool aAborted);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool
  Aborted() const;

  void Abort();
  void Timeout();

  IMPL_EVENT_HANDLER(abort);
  IMPL_EVENT_HANDLER(timeout);

  static already_AddRefed<AbortSignal> Abort(GlobalObject& aGlobal);
  static already_AddRefed<AbortSignal> Timeout(GlobalObject& aGlobal, uint64_t aMilliseconds);

  void
  AddFollower(Follower* aFollower);

  void
  RemoveFollower(Follower* aFollower);

private:
  ~AbortSignal() = default;

  // Raw pointers. Follower unregisters itself in the DTOR.
  nsTArray<Follower*> mFollowers;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_AbortSignal_h
