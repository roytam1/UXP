/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancemark_h___
#define mozilla_dom_performancemark_h___

#include "mozilla/dom/PerformanceEntry.h"

namespace mozilla {
namespace dom {

struct PerformanceMarkOptions;

// http://www.w3.org/TR/user-timing/#performancemark
class PerformanceMark final : public PerformanceEntry
{
private:
  PerformanceMark(nsISupports* aParent,
                  const nsAString& aName,
                  DOMHighResTimeStamp aStartTime,
                  const JS::Handle<JS::Value>& aDetail);

  JS::Heap<JS::Value> mDetail;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PerformanceMark,
                                                         PerformanceEntry);

  static already_AddRefed<PerformanceMark> Constructor(
    const GlobalObject& aGlobal,
    const nsAString& aMarkName,
    const PerformanceMarkOptions& aMarkOptions,
    ErrorResult& aRv);

  static already_AddRefed<PerformanceMark> Constructor(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    const nsAString& aMarkName,
    const PerformanceMarkOptions& aMarkOptions,
    ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetDetail(JSContext* aCx, JS::MutableHandle<JS::Value> aRv);

  virtual DOMHighResTimeStamp StartTime() const override
  {
    return mStartTime;
  }

protected:
  virtual ~PerformanceMark();
  DOMHighResTimeStamp mStartTime;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_performancemark_h___ */
