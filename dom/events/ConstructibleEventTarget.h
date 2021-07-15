/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConstructibleEventTarget_h_
#define mozilla_dom_ConstructibleEventTarget_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "js/RootingAPI.h"

namespace mozilla {
namespace dom {

class ConstructibleEventTarget : public DOMEventTargetHelper
{
public:
  // We're not worrying about ISupports and Cycle Collection here just for a wrapper function.
  // This does mean that ConstructibleEventTarget will show up in CC and refcount logs as a
  // DOMEventTargetHelper, but that's OK.

  explicit ConstructibleEventTarget(nsIGlobalObject* aGlobalObject)
    : DOMEventTargetHelper(aGlobalObject)
  {
  }

  virtual JSObject* WrapObject(JSContext* cx,
			       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ConstructibleEventTarget_h_
