/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/AbortSignalBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(nsIGlobalObject* aGlobalObject,
                         bool aAborted)
  : DOMEventTargetHelper(aGlobalObject)
  , mAborted(aAborted)
{}

AbortSignal::AbortSignal(bool aAborted)
  : mAborted(aAborted)
{}

JSObject*
AbortSignal::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AbortSignalBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<AbortSignal> AbortSignal::Abort(GlobalObject& aGlobal) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, true);
  return abortSignal.forget();
}

already_AddRefed<AbortSignal> AbortSignal::Timeout(GlobalObject& aGlobal, uint64_t aMilliseconds) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  
  // Stub implementation, just return an AbortSignal object
  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, false);
  
  return abortSignal.forget();
}

bool
AbortSignal::Aborted() const
{
  return mAborted;
}

void
AbortSignal::Abort()
{
  // Re-entrancy guard
  if (mAborted) {
    return;
  }
  mAborted = true;

  // We might be deleted as a result of aborting a follower, so ensure we
  // stay alive until all followers have been aborted.
  RefPtr<AbortSignal> pinThis = this;

  // Let's inform the followers.
  for (uint32_t i = 0; i < mFollowers.Length(); ++i) {
    mFollowers[i]->Aborted();
  }

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
    Event::Constructor(this, NS_LITERAL_STRING("abort"), init);
  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
AbortSignal::AddFollower(AbortSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  if (!mFollowers.Contains(aFollower)) {
    mFollowers.AppendElement(aFollower);
  }
}

void
AbortSignal::RemoveFollower(AbortSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  mFollowers.RemoveElement(aFollower);
}

// AbortSignal::Follower
// ----------------------------------------------------------------------------

AbortSignal::Follower::~Follower()
{
  Unfollow();
}

void
AbortSignal::Follower::Follow(AbortSignal* aSignal)
{
  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  mFollowingSignal = aSignal;
  aSignal->AddFollower(this);
}

void
AbortSignal::Follower::Unfollow()
{
  if (mFollowingSignal) {
    mFollowingSignal->RemoveFollower(this);
    mFollowingSignal = nullptr;
  }
}

} // dom namespace
} // mozilla namespace
