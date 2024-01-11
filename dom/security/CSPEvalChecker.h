/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSPEvalChecker_h
#define mozilla_dom_CSPEvalChecker_h

#include "nsString.h"

struct JSContext;
class nsGlobalWindow;

namespace mozilla {
namespace dom {
namespace workers {
class WorkerPrivate;
}

using namespace mozilla::dom::workers;

class CSPEvalChecker final
{
public:
  static nsresult
  CheckForWindow(JSContext* aCx, nsGlobalWindow* aWindow,
                 const nsAString& aExpression, bool* aAllowEval);

  static nsresult
  CheckForWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 const nsAString& aExpression, bool* aAllowEval);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_CSPEvalChecker_h
