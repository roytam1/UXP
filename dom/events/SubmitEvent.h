/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SubmitEvent_h_
#define mozilla_dom_SubmitEvent_h_

#include "mozilla/dom/Event.h"
#include "mozilla/dom/SubmitEventBinding.h"
#include "mozilla/EventForwards.h"

class nsGenericHTMLElement;

namespace mozilla {
namespace dom {

class SubmitEvent final : public Event
{
public:
  SubmitEvent(EventTarget* aOwner,
              nsPresContext* aPresContext,
              InternalFormEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SubmitEvent, Event)

  // Forward to base class
  NS_FORWARD_TO_EVENT

  static already_AddRefed<SubmitEvent> Constructor(const GlobalObject& aGlobal,
                                                   const nsAString& aType,
                                                   const SubmitEventInit& aParam,
                                                   ErrorResult& aRv);

  ::nsGenericHTMLElement* GetSubmitter() const
  {
    return mSubmitter;
  }

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return SubmitEventBinding::Wrap(aCx, this, aGivenProto);
  }

protected:
  ~SubmitEvent() {}

private:
  RefPtr<::nsGenericHTMLElement> mSubmitter;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::SubmitEvent>
NS_NewDOMSubmitEvent(mozilla::dom::EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     mozilla::InternalFormEvent* aEvent);

#endif // mozilla_dom_SubmitEvent_h_
