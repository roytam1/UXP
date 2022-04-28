/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DetailedPromise.h"
#include "mozilla/dom/DOMException.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal,
                                 const nsACString& aName)
  : Promise(aGlobal)
  , mName(aName)
  , mResponded(false)
  , mStartTime(TimeStamp::Now())
{
}

DetailedPromise::~DetailedPromise()
{
}

void
DetailedPromise::MaybeReject(nsresult aArg, const nsACString& aReason)
{
  nsPrintfCString msg("%s promise rejected 0x%x '%s'", mName.get(), aArg,
                      PromiseFlatCString(aReason).get());
  EME_LOG(msg.get());

  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));

  ErrorResult rv;
  rv.ThrowDOMException(aArg, aReason);
  Promise::MaybeReject(rv);
}

void
DetailedPromise::MaybeReject(ErrorResult&, const nsACString& aReason)
{
  NS_NOTREACHED("nsresult expected in MaybeReject()");
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal,
                        ErrorResult& aRv,
                        const nsACString& aName)
{
  RefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

} // namespace dom
} // namespace mozilla
