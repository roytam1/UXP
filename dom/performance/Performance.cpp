/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Performance.h"

#include "GeckoProfiler.h"
#include "PerformanceEntry.h"
#include "PerformanceMainThread.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceService.h"
#include "PerformanceWorker.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryEvent.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/dom/PerformanceNavigationTiming.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimerClamping.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

class PrefEnabledRunnable final
  : public WorkerCheckAPIExposureOnMainThreadRunnable
{
public:
  PrefEnabledRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsCString& aPrefName)
    : WorkerCheckAPIExposureOnMainThreadRunnable(aWorkerPrivate)
    , mEnabled(false)
    , mPrefName(aPrefName)
  { }

  bool MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mEnabled = Preferences::GetBool(mPrefName.get(), false);
    return true;
  }

  bool IsEnabled() const
  {
    return mEnabled;
  }

private:
  bool mEnabled;
  nsCString mPrefName;
};

} // anonymous namespace

enum class Performance::ResolveTimestampAttribute {
  Start,
  End,
  Duration,
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Performance)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Performance,
                                   DOMEventTargetHelper,
                                   mUserEntries,
                                   mResourceEntries);

NS_IMPL_ADDREF_INHERITED(Performance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Performance, DOMEventTargetHelper)

/* static */ already_AddRefed<Performance>
Performance::CreateForMainThread(nsPIDOMWindowInner* aWindow,
                                 nsDOMNavigationTiming* aDOMTiming,
                                 nsITimedChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Performance> performance =
    new PerformanceMainThread(aWindow, aDOMTiming, aChannel);
  return performance.forget();
}

/* static */ already_AddRefed<Performance>
Performance::CreateForWorker(workers::WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Performance> performance = new PerformanceWorker(aWorkerPrivate);
  return performance.forget();
}

/* static */
already_AddRefed<Performance> Performance::Get(JSContext* aCx,
                                               nsIGlobalObject* aGlobal) {
  RefPtr<Performance> performance;
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
  if (window) {
    performance = window->GetPerformance();
  } else {
    const WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    if (!workerPrivate) {
      return nullptr;
    }

    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);
    performance = scope->GetPerformance();
  }

  return performance.forget();
}

Performance::Performance()
  : mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(!NS_IsMainThread());
}

Performance::Performance(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

Performance::~Performance()
{}

DOMHighResTimeStamp
Performance::Now() const
{
  TimeDuration duration = TimeStamp::Now() - CreationTimeStamp();
  return RoundTime(duration.ToMilliseconds());
}

DOMHighResTimeStamp
Performance::TimeOrigin()
{
  if (!mPerformanceService) {
    mPerformanceService = PerformanceService::GetOrCreate();
  }

  MOZ_ASSERT(mPerformanceService);
  return mPerformanceService->TimeOrigin(CreationTimeStamp());
}

JSObject*
Performance::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceBinding::Wrap(aCx, this, aGivenProto);
}

void
Performance::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  aRetval = mResourceEntries;
  aRetval.AppendElements(mUserEntries);
  aRetval.Sort(PerformanceEntryComparator());
}

void
Performance::GetEntriesByType(const nsAString& aEntryType,
                              nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  if (aEntryType.EqualsLiteral("resource")) {
    aRetval = mResourceEntries;
    return;
  }

  aRetval.Clear();

  if (aEntryType.EqualsLiteral("mark") ||
      aEntryType.EqualsLiteral("measure")) {
    for (PerformanceEntry* entry : mUserEntries) {
      if (entry->GetEntryType().Equals(aEntryType)) {
        aRetval.AppendElement(entry);
      }
    }
  }
}

void
Performance::GetEntriesByName(const nsAString& aName,
                              const Optional<nsAString>& aEntryType,
                              nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();

  for (PerformanceEntry* entry : mResourceEntries) {
    if (entry->GetName().Equals(aName) &&
        (!aEntryType.WasPassed() ||
         entry->GetEntryType().Equals(aEntryType.Value()))) {
      aRetval.AppendElement(entry);
    }
  }

  for (PerformanceEntry* entry : mUserEntries) {
    if (entry->GetName().Equals(aName) &&
        (!aEntryType.WasPassed() ||
         entry->GetEntryType().Equals(aEntryType.Value()))) {
      aRetval.AppendElement(entry);
    }
  }

  aRetval.Sort(PerformanceEntryComparator());
}

void
Performance::ClearUserEntries(const Optional<nsAString>& aEntryName,
                              const nsAString& aEntryType)
{
  for (uint32_t i = 0; i < mUserEntries.Length();) {
    if ((!aEntryName.WasPassed() ||
         mUserEntries[i]->GetName().Equals(aEntryName.Value())) &&
        (aEntryType.IsEmpty() ||
         mUserEntries[i]->GetEntryType().Equals(aEntryType))) {
      mUserEntries.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
}

void
Performance::ClearResourceTimings()
{
  MOZ_ASSERT(NS_IsMainThread());
  mResourceEntries.Clear();
}

DOMHighResTimeStamp
Performance::RoundTime(double aTime) const
{
  // Round down to the nearest 2ms, because if the timer is too accurate people
  // can do nasty timing attacks with it.
  const double maxResolutionMs = 2;
  return floor(aTime / maxResolutionMs) * maxResolutionMs;
}

already_AddRefed<PerformanceMark> Performance::Mark(
  JSContext* aCx,
  const nsAString& aName,
  const PerformanceMarkOptions& aMarkOptions,
  ErrorResult& aRv)
{
  // Don't add the entry if the buffer is full. XXX should be removed by bug 1159003.
  if (mUserEntries.Length() >= mResourceTimingBufferSize) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> parent = GetParentObject();
  if (!parent || parent->IsDying() || !parent->GetGlobalJSObject()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
   }

  GlobalObject global(aCx, parent->GetGlobalJSObject());
  if (global.Failed()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<PerformanceMark> performanceMark =
    PerformanceMark::Constructor(global, aName, aMarkOptions, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  InsertUserEntry(performanceMark);

  if (profiler_is_active()) {
    PROFILER_MARKER(NS_ConvertUTF16toUTF8(aName).get());
  }

  return performanceMark.forget();
}

void
Performance::ClearMarks(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("mark"));
}

// To be removed once bug 1124165 lands
bool
Performance::IsPerformanceTimingAttribute(const nsAString& aName) const
{
  // Note that toJSON is added to this list due to bug 1047848
  static const char* attributes[] =
    {"navigationStart", "unloadEventStart", "unloadEventEnd", "redirectStart",
     "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd",
     "connectStart", "secureConnectionStart", "connectEnd", "requestStart", "responseStart",
     "responseEnd", "domLoading", "domInteractive",
     "domContentLoadedEventStart", "domContentLoadedEventEnd", "domComplete",
     "loadEventStart", "loadEventEnd", nullptr};

  for (uint32_t i = 0; attributes[i]; ++i) {
    if (aName.EqualsASCII(attributes[i])) {
      return true;
    }
  }

  return false;
}

DOMHighResTimeStamp
Performance::ConvertMarkToTimestampWithString(const nsAString& aName,
                                              ErrorResult& aRv)
{
  if (IsPerformanceTimingAttribute(aName)) {
    return ConvertNameToTimestamp(aName, aRv);
  }

  AutoTArray<RefPtr<PerformanceEntry>, 1> arr;
  Optional<nsAString> typeParam;
  nsAutoString str;
  str.AssignLiteral("mark");
  typeParam = &str;
  GetEntriesByName(aName, typeParam, arr);
  if (!arr.IsEmpty()) {
    return arr.LastElement()->StartTime();
  }

  aRv.ThrowTypeError<MSG_PMO_UNKNOWN_MARK_NAME>(aName);
  return 0;
}

DOMHighResTimeStamp
Performance::ConvertMarkToTimestampWithDOMHighResTimeStamp(
  const ResolveTimestampAttribute aAttribute,
  const DOMHighResTimeStamp aTimestamp,
  ErrorResult& aRv)
{
  if (aTimestamp < 0) {
    nsAutoString attributeName;
    switch (aAttribute) {
      case ResolveTimestampAttribute::Start:
        attributeName = NS_LITERAL_STRING("start");
        break;
      case ResolveTimestampAttribute::End:
        attributeName = NS_LITERAL_STRING("end");
        break;
      case ResolveTimestampAttribute::Duration:
        attributeName = NS_LITERAL_STRING("duration");
        break;
    }

    aRv.ThrowTypeError<MSG_NO_NEGATIVE_ATTR>(attributeName);
  }
  return aTimestamp;
}

DOMHighResTimeStamp
Performance::ConvertMarkToTimestamp(
  const ResolveTimestampAttribute aAttribute,
  const OwningStringOrDouble& aMarkNameOrTimestamp,
  ErrorResult& aRv)
{
  if (aMarkNameOrTimestamp.IsString()) {
    return ConvertMarkToTimestampWithString(aMarkNameOrTimestamp.GetAsString(),
                                            aRv);
  }
  return ConvertMarkToTimestampWithDOMHighResTimeStamp(
      aAttribute, aMarkNameOrTimestamp.GetAsDouble(), aRv);
}

DOMHighResTimeStamp Performance::ConvertNameToTimestamp(const nsAString& aName,
                                                        ErrorResult& aRv) {
  if (!IsGlobalObjectWindow()) {
    aRv.ThrowTypeError<MSG_PMO_INVALID_ATTR_FOR_NON_GLOBAL>(aName);
    return 0;
  }

  if (aName.EqualsASCII("navigationStart")) {
    return 0;
  }

  // We use GetPerformanceTimingFromString, rather than calling the
  // navigationStart method timing function directly, because the former handles
  // reducing precision against timing attacks.
  const DOMHighResTimeStamp startTime =
    GetPerformanceTimingFromString(NS_LITERAL_STRING("navigationStart"));
  const DOMHighResTimeStamp endTime =
    GetPerformanceTimingFromString(aName);
  MOZ_ASSERT(endTime >= 0);
  if (endTime == 0) {
    // Was given a PerformanceTiming attribute which isn't available yet.
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return 0;
  }

  return endTime - startTime;
}

DOMHighResTimeStamp
Performance::ResolveEndTimeForMeasure(
  const Optional<nsAString>& aEndMark,
  const PerformanceMeasureOptions* aOptions,
  ErrorResult& aRv)
{
  DOMHighResTimeStamp endTime;
  if (aEndMark.WasPassed()) {
    endTime = ConvertMarkToTimestampWithString(aEndMark.Value(), aRv);
  } else if (aOptions != nullptr && aOptions->mEnd.WasPassed()) {
    endTime = ConvertMarkToTimestamp(ResolveTimestampAttribute::End,
                                     aOptions->mEnd.Value(), aRv);
  } else if (aOptions != nullptr && aOptions->mStart.WasPassed() &&
             aOptions->mDuration.WasPassed()) {
    const DOMHighResTimeStamp start = ConvertMarkToTimestamp(
        ResolveTimestampAttribute::Start, aOptions->mStart.Value(), aRv);
    if (aRv.Failed()) {
      return 0;
    }
    const DOMHighResTimeStamp duration =
      ConvertMarkToTimestampWithDOMHighResTimeStamp(
        ResolveTimestampAttribute::Duration,
        aOptions->mDuration.Value(),
        aRv);
    if (aRv.Failed()) {
      return 0;
    }

    endTime = start + duration;
  } else {
    endTime = Now();
  }
  return endTime;
}

DOMHighResTimeStamp
Performance::ResolveStartTimeForMeasure(
  const nsAString* aStartMark,
  const PerformanceMeasureOptions* aOptions,
  ErrorResult& aRv)
{
  DOMHighResTimeStamp startTime;
  if (aOptions != nullptr && aOptions->mStart.WasPassed()) {
    startTime = ConvertMarkToTimestamp(ResolveTimestampAttribute::Start,
                                       aOptions->mStart.Value(),
                                       aRv);
  } else if (aOptions != nullptr && aOptions->mDuration.WasPassed() &&
             aOptions->mEnd.WasPassed()) {
    const DOMHighResTimeStamp duration =
      ConvertMarkToTimestampWithDOMHighResTimeStamp(
        ResolveTimestampAttribute::Duration,
        aOptions->mDuration.Value(),
        aRv);
    if (aRv.Failed()) {
      return 0;
    }

    const DOMHighResTimeStamp end = ConvertMarkToTimestamp(
      ResolveTimestampAttribute::End, aOptions->mEnd.Value(), aRv);
    if (aRv.Failed()) {
      return 0;
    }

    startTime = end - duration;
  } else if (aStartMark != nullptr) {
    startTime = ConvertMarkToTimestampWithString(*aStartMark, aRv);
  } else {
    startTime = 0;
  }

  return startTime;
}

already_AddRefed<PerformanceMeasure>
Performance::Measure(JSContext* aCx,
                     const nsAString& aName,
                     const StringOrPerformanceMeasureOptions& aStartOrMeasureOptions,
                     const Optional<nsAString>& aEndMark,
                     ErrorResult& aRv)
{
  // Don't add the entry if the buffer is full. XXX should be removed by bug
  // 1159003.
  if (mUserEntries.Length() >= mResourceTimingBufferSize) {
    return nullptr;
  }

  const PerformanceMeasureOptions* options = nullptr;
  if (aStartOrMeasureOptions.IsPerformanceMeasureOptions()) {
    options = &aStartOrMeasureOptions.GetAsPerformanceMeasureOptions();
  }

  const bool isOptionsNotEmpty =
      (options != nullptr) &&
      (!options->mDetail.isUndefined() || options->mStart.WasPassed() ||
       options->mEnd.WasPassed() || options->mDuration.WasPassed());
  if (isOptionsNotEmpty) {
    if (aEndMark.WasPassed()) {
      aRv.ThrowTypeError<MSG_PMO_NO_SEPARATE_ENDMARK>();
      return nullptr;
    }

    if (!options->mStart.WasPassed() && !options->mEnd.WasPassed()) {
      aRv.ThrowTypeError<MSG_PMO_MISSING_STARTENDMARK>();
      return nullptr;
    }

    if (options->mStart.WasPassed() && options->mDuration.WasPassed() &&
        options->mEnd.WasPassed()) {
      aRv.ThrowTypeError<MSG_PMO_INVALID_MEMBERS>();
      return nullptr;
    }
  }

  const DOMHighResTimeStamp endTime =
    ResolveEndTimeForMeasure(aEndMark, options, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  const nsAString* startMark = nullptr;
  if (aStartOrMeasureOptions.IsString()) {
    startMark = &aStartOrMeasureOptions.GetAsString();
  }
  const DOMHighResTimeStamp startTime =
    ResolveStartTimeForMeasure(startMark, options, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  JS::Rooted<JS::Value> detail(aCx);
  if (options != nullptr && !options->mDetail.isNullOrUndefined()) {
    StructuredSerializeOptions serializeOptions;
    JS::Rooted<JS::Value> valueToClone(aCx, options->mDetail);
    nsContentUtils::StructuredClone(aCx, GetParentObject(), valueToClone,
                                    serializeOptions, &detail, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    detail.setNull();
  }

  RefPtr<PerformanceMeasure> performanceMeasure = new PerformanceMeasure(
      GetAsISupports(), aName, startTime, endTime, detail);
  InsertUserEntry(performanceMeasure);

  return performanceMeasure.forget();
}

void
Performance::ClearMeasures(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("measure"));
}

void
Performance::LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const
{
  PERFLOG("Performance Entry: %s|%s|%s|%f|%f|%" PRIu64 "\n",
          aOwner.BeginReading(),
          NS_ConvertUTF16toUTF8(aEntry->GetEntryType()).get(),
          NS_ConvertUTF16toUTF8(aEntry->GetName()).get(),
          aEntry->StartTime(),
          aEntry->Duration(),
          static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC));
}

void
Performance::TimingNotification(PerformanceEntry* aEntry,
                                const nsACString& aOwner, uint64_t aEpoch)
{
  PerformanceEntryEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mName = aEntry->GetName();
  init.mEntryType = aEntry->GetEntryType();
  init.mStartTime = aEntry->StartTime();
  init.mDuration = aEntry->Duration();
  init.mEpoch = aEpoch;
  init.mOrigin = NS_ConvertUTF8toUTF16(aOwner.BeginReading());

  RefPtr<PerformanceEntryEvent> perfEntryEvent =
    PerformanceEntryEvent::Constructor(this, NS_LITERAL_STRING("performanceentry"), init);

  nsCOMPtr<EventTarget> et = do_QueryInterface(GetOwner());
  if (et) {
    bool dummy = false;
    et->DispatchEvent(perfEntryEvent, &dummy);
  }
}

void
Performance::InsertUserEntry(PerformanceEntry* aEntry)
{
  mUserEntries.InsertElementSorted(aEntry,
                                   PerformanceEntryComparator());

  QueueEntry(aEntry);
}

void
Performance::SetResourceTimingBufferSize(uint64_t aMaxSize)
{
  mResourceTimingBufferSize = aMaxSize;
}

void
Performance::InsertResourceEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(aEntry);
  MOZ_ASSERT(mResourceEntries.Length() < mResourceTimingBufferSize);
  if (mResourceEntries.Length() >= mResourceTimingBufferSize) {
    return;
  }

  mResourceEntries.InsertElementSorted(aEntry,
                                       PerformanceEntryComparator());
  if (mResourceEntries.Length() == mResourceTimingBufferSize) {
    // call onresourcetimingbufferfull
    DispatchBufferFullEvent();
  }
  QueueEntry(aEntry);
}

void
Performance::AddObserver(PerformanceObserver* aObserver)
{
  mObservers.AppendElementUnlessExists(aObserver);
}

void
Performance::RemoveObserver(PerformanceObserver* aObserver)
{
  mObservers.RemoveElement(aObserver);
}

void
Performance::NotifyObservers()
{
  mPendingNotificationObserversTask = false;
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers,
                                           PerformanceObserver,
                                           Notify, ());
}

void
Performance::CancelNotificationObservers()
{
  mPendingNotificationObserversTask = false;
}

class NotifyObserversTask final : public CancelableRunnable
{
public:
  explicit NotifyObserversTask(Performance* aPerformance)
    : mPerformance(aPerformance)
  {
    MOZ_ASSERT(mPerformance);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mPerformance);
    mPerformance->NotifyObservers();
    return NS_OK;
  }

  nsresult Cancel() override
  {
    mPerformance->CancelNotificationObservers();
    mPerformance = nullptr;
    return NS_OK;
  }

private:
  ~NotifyObserversTask()
  {
  }

  RefPtr<Performance> mPerformance;
};

void
Performance::RunNotificationObserversTask()
{
  mPendingNotificationObserversTask = true;
  nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(this);
  nsresult rv = NS_DispatchToCurrentThread(task);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPendingNotificationObserversTask = false;
  }
}

void
Performance::QueueEntry(PerformanceEntry* aEntry)
{
  if (mObservers.IsEmpty()) {
    return;
  }
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers,
                                           PerformanceObserver,
                                           QueueEntry, (aEntry));

  if (!mPendingNotificationObserversTask) {
    RunNotificationObserversTask();
  }
}

/* static */ bool
Performance::IsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.enable_user_timing", false);
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<PrefEnabledRunnable> runnable =
    new PrefEnabledRunnable(workerPrivate,
                            NS_LITERAL_CSTRING("dom.enable_user_timing"));
  return runnable->Dispatch() && runnable->IsEnabled();
}

/* static */ bool
Performance::IsObserverEnabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.enable_performance_observer", false);
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<PrefEnabledRunnable> runnable =
    new PrefEnabledRunnable(workerPrivate,
                            NS_LITERAL_CSTRING("dom.enable_performance_observer"));

  return runnable->Dispatch() && runnable->IsEnabled();
}

} // dom namespace
} // mozilla namespace
