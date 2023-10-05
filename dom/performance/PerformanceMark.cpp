/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMark.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceMarkBinding.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/ErrorResult.h"

using mozilla::dom::StructuredSerializeOptions;
using namespace mozilla::dom;

PerformanceMark::PerformanceMark(nsISupports* aParent,
                                 const nsAString& aName,
                                 DOMHighResTimeStamp aStartTime,
                                 const JS::Handle<JS::Value>& aDetail)
  : PerformanceEntry(aParent, aName, NS_LITERAL_STRING("mark"))
  , mStartTime(aStartTime)
  , mDetail(aDetail)
{
  // mParent is null in workers.
  MOZ_ASSERT(mParent || !NS_IsMainThread());
  mozilla::HoldJSObjects(this);
}


already_AddRefed<PerformanceMark> PerformanceMark::Constructor(
  const GlobalObject& aGlobal,
  const nsAString& aMarkName,
  const PerformanceMarkOptions& aMarkOptions,
  ErrorResult& aRv)
{
  const nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(aGlobal.GetAsSupports());
  return PerformanceMark::Constructor(aGlobal.Context(), global, aMarkName,
                                      aMarkOptions, aRv);
}

already_AddRefed<PerformanceMark> PerformanceMark::Constructor(
    JSContext* aCx,
    nsIGlobalObject* aGlobal,
    const nsAString& aMarkName,
    const PerformanceMarkOptions& aMarkOptions,
    ErrorResult& aRv)
{
  RefPtr<Performance> performance = Performance::Get(aCx, aGlobal);
  if (!performance) {
    // This is similar to the message that occurs when accessing `performance`
    // from outside a valid global.
    aRv.ThrowTypeError<MSG_PMO_CONSTRUCTOR_INACCESSIBLE>();
    return nullptr;
  }

  if (performance->IsGlobalObjectWindow() &&
      performance->IsPerformanceTimingAttribute(aMarkName)) {
    aRv.Throw(NS_ERROR_DOM_UT_INVALID_TIMING_ATTR);
    return nullptr;
  }

  DOMHighResTimeStamp startTime = aMarkOptions.mStartTime.WasPassed() ?
                                  aMarkOptions.mStartTime.Value() :
                                  performance->Now();
  if (startTime < 0) {
    aRv.ThrowTypeError<MSG_PMO_UNEXPECTED_START_TIME>();
    return nullptr;
  }

  JS::Rooted<JS::Value> detail(aCx);
  if (aMarkOptions.mDetail.isNullOrUndefined()) {
    detail.setNull();
  } else {
    StructuredSerializeOptions serializeOptions;
    JS::Rooted<JS::Value> valueToClone(aCx, aMarkOptions.mDetail);
    nsContentUtils::StructuredClone(aCx, aGlobal, valueToClone,
                                    serializeOptions, &detail, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return do_AddRef(new PerformanceMark(aGlobal, aMarkName, startTime, detail));
}

PerformanceMark::~PerformanceMark()
{
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PerformanceMark)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PerformanceMark,
                                                PerformanceEntry)
  tmp->mDetail.setUndefined();
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PerformanceMark,
                                                  PerformanceEntry)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PerformanceMark,
                                               PerformanceEntry)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDetail)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceMark)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)
NS_IMPL_ADDREF_INHERITED(PerformanceMark, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(PerformanceMark, PerformanceEntry)

JSObject*
PerformanceMark::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceMarkBinding::Wrap(aCx, this, aGivenProto);
}

void PerformanceMark::GetDetail(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aRv)
{
  // Return a copy so that this method always returns the value it is set to
  // (i.e. it'll return the same value even if the caller assigns to it). Note
  // that if detail is an object, its contents can be mutated and this is
  // expected.
  aRv.set(mDetail);
}
