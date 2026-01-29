/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SubmitEvent.h"

#include "mozilla/ContentEvents.h"
#include "nsIContent.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(SubmitEvent, Event, mSubmitter)

NS_IMPL_ADDREF_INHERITED(SubmitEvent, Event)
NS_IMPL_RELEASE_INHERITED(SubmitEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SubmitEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

SubmitEvent::SubmitEvent(EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         InternalFormEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
{
  if (aEvent) {
    nsIContent* originator = aEvent->mOriginator;
    if (originator && originator->IsHTMLElement()) {
      mSubmitter = static_cast<::nsGenericHTMLElement*>(originator);
    }
  }
}

already_AddRefed<SubmitEvent>
SubmitEvent::Constructor(const GlobalObject& aGlobal,
                         const nsAString& aType,
                         const SubmitEventInit& aParam,
                         ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<SubmitEvent> e = new SubmitEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  e->mSubmitter = aParam.mSubmitter;
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<SubmitEvent>
NS_NewDOMSubmitEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     InternalFormEvent* aEvent)
{
  RefPtr<SubmitEvent> it = new SubmitEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
