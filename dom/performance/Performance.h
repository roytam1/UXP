/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Performance_h
#define mozilla_dom_Performance_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsDOMNavigationTiming.h"

class nsITimedChannel;
class nsIHttpChannel;

namespace mozilla {

class ErrorResult;

namespace dom {

class OwningStringOrDouble;
class StringOrPerformanceMeasureOptions;
class PerformanceEntry;
class PerformanceMark;
struct PerformanceMarkOptions;
struct PerformanceMeasureOptions;
class PerformanceMeasure;
class PerformanceNavigation;
class PerformanceObserver;
class PerformanceService;
class PerformanceTiming;
struct StructuredSerializeOptions;

namespace workers {
class WorkerPrivate;
}

// Base class for main-thread and worker Performance API
class Performance : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Performance,
                                           DOMEventTargetHelper)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static bool IsObserverEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<Performance>
  CreateForMainThread(nsPIDOMWindowInner* aWindow,
                      nsDOMNavigationTiming* aDOMTiming,
                      nsITimedChannel* aChannel);

  static already_AddRefed<Performance>
  CreateForWorker(workers::WorkerPrivate* aWorkerPrivate);

  // This will return nullptr if called outside of a Window or Worker.
  static already_AddRefed<Performance> Get(JSContext* aCx,
                                           nsIGlobalObject* aGlobal);

  JSObject* WrapObject(JSContext *cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  virtual void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void GetEntriesByType(const nsAString& aEntryType,
                                nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void GetEntriesByName(const nsAString& aName,
                                const Optional<nsAString>& aEntryType,
                                nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel) = 0;

  void ClearResourceTimings();

  DOMHighResTimeStamp Now() const;

  DOMHighResTimeStamp TimeOrigin();

  already_AddRefed<PerformanceMark> Mark(
      JSContext* aCx, const nsAString& aName,
      const PerformanceMarkOptions& aMarkOptions, ErrorResult& aRv);

  void ClearMarks(const Optional<nsAString>& aName);

  already_AddRefed<PerformanceMeasure> Measure(
      JSContext* aCx, const nsAString& aName,
      const StringOrPerformanceMeasureOptions& aStartOrMeasureOptions,
      const Optional<nsAString>& aEndMark, ErrorResult& aRv);

  void ClearMeasures(const Optional<nsAString>& aName);

  void SetResourceTimingBufferSize(uint64_t aMaxSize);

  void AddObserver(PerformanceObserver* aObserver);
  void RemoveObserver(PerformanceObserver* aObserver);
  void NotifyObservers();
  void CancelNotificationObservers();

  virtual PerformanceTiming* Timing() = 0;

  virtual PerformanceNavigation* Navigation() = 0;

  IMPL_EVENT_HANDLER(resourcetimingbufferfull)

#ifdef MOZ_DEVTOOLS_SERVER
  virtual void GetMozMemory(JSContext *aCx,
                            JS::MutableHandle<JSObject*> aObj) = 0;
#endif

  virtual nsDOMNavigationTiming* GetDOMTiming() const = 0;

  virtual nsITimedChannel* GetChannel() const = 0;

  bool IsPerformanceTimingAttribute(const nsAString& aName) const;

  virtual bool IsGlobalObjectWindow() const
  {
    return false;
  }

protected:
  Performance();
  explicit Performance(nsPIDOMWindowInner* aWindow);

  virtual ~Performance();

  virtual void InsertUserEntry(PerformanceEntry* aEntry);
  void InsertResourceEntry(PerformanceEntry* aEntry);

  void ClearUserEntries(const Optional<nsAString>& aEntryName,
                        const nsAString& aEntryType);

  virtual nsISupports* GetAsISupports() = 0;

  virtual void DispatchBufferFullEvent() = 0;

  virtual TimeStamp CreationTimeStamp() const = 0;

  virtual DOMHighResTimeStamp CreationTime() const = 0;

  virtual DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName)
  {
    return 0;
  }

  bool IsResourceEntryLimitReached() const
  {
    return mResourceEntries.Length() >= mResourceTimingBufferSize;
  }

  void LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const;
  void TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner,
                          uint64_t epoch);

  void RunNotificationObserversTask();
  void QueueEntry(PerformanceEntry* aEntry);

  DOMHighResTimeStamp RoundTime(double aTime) const;

  nsTObserverArray<PerformanceObserver*> mObservers;

protected:
  nsTArray<RefPtr<PerformanceEntry>> mUserEntries;
  nsTArray<RefPtr<PerformanceEntry>> mResourceEntries;

  uint64_t mResourceTimingBufferSize;
  static const uint64_t kDefaultResourceTimingBufferSize = 1500;
  bool mPendingNotificationObserversTask;

  RefPtr<PerformanceService> mPerformanceService;

private:
  // The attributes of a PerformanceMeasureOptions that we call
  // ResolveTimestamp* on.
  enum class ResolveTimestampAttribute;

  DOMHighResTimeStamp ConvertMarkToTimestampWithString(const nsAString& aName,
                                                       ErrorResult& aRv);
  DOMHighResTimeStamp ConvertMarkToTimestampWithDOMHighResTimeStamp(
      const ResolveTimestampAttribute aAttribute, const double aTimestamp,
      ErrorResult& aRv);
  DOMHighResTimeStamp ConvertMarkToTimestamp(
      const ResolveTimestampAttribute aAttribute,
      const OwningStringOrDouble& aMarkNameOrTimestamp, ErrorResult& aRv);

  DOMHighResTimeStamp ConvertNameToTimestamp(const nsAString& aName,
                                             ErrorResult& aRv);

  DOMHighResTimeStamp ResolveEndTimeForMeasure(
      const Optional<nsAString>& aEndMark,
      const PerformanceMeasureOptions* aOptions,
      ErrorResult& aRv);
  DOMHighResTimeStamp ResolveStartTimeForMeasure(
      const nsAString* aStartMark,
      const PerformanceMeasureOptions* aOptions,
      ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Performance_h
