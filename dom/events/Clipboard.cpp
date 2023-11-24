/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"
#include "mozilla/dom/Clipboard.h"
#include "mozilla/dom/ClipboardBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/DataTransferItem.h"
#include "mozilla/dom/ContentChild.h"
#include "nsIClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsITransferable.h"
#include "nsArrayUtils.h"


static mozilla::LazyLogModule gClipboardLog("Clipboard");

namespace mozilla {
namespace dom {

Clipboard::Clipboard(nsPIDOMWindowInner* aWindow)
: DOMEventTargetHelper(aWindow)
{
}

Clipboard::~Clipboard()
{
}

already_AddRefed<Promise>
Clipboard::ReadHelper(JSContext* aCx, nsIPrincipal& aSubjectPrincipal,
                      ClipboardReadType aClipboardReadType, ErrorResult& aRv)
{
  // Create a new promise
  RefPtr<Promise> p = dom::Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // We always reject reading from clipboard for sec reasons.
  MOZ_LOG(GetClipboardLog(), LogLevel::Debug, ("Clipboard, ReadHelper, "
          "Don't have permissions for reading\n"));
  p->MaybeRejectWithUndefined();
  return p.forget();
}

already_AddRefed<Promise>
Clipboard::Read(JSContext* aCx, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv)
{
  return ReadHelper(aCx, aSubjectPrincipal, eRead, aRv);
}

already_AddRefed<Promise>
Clipboard::ReadText(JSContext* aCx, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv)
{
  return ReadHelper(aCx, aSubjectPrincipal, eReadText, aRv);
}

already_AddRefed<Promise>
Clipboard::Write(JSContext* aCx, DataTransfer& aData, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aRv)
{
  // Create a promise
  RefPtr<Promise> p = dom::Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // We want to disable this if dom.allow_cut_copy is false, or
  // it doesn't meet the other requirements for copying to clipboard
  if (!nsContentUtils::IsCutCopyAllowed()) {
    MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
            ("Clipboard, Write, Not allowed to write to clipboard\n"));
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  // Get the clipboard service
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1"));
  if (!clipboard) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  nsPIDOMWindowInner* owner = GetOwner();
  nsIDocument* doc          = owner ? owner->GetDoc() : nullptr;
  nsILoadContext* context   = doc ? doc->GetLoadContext() : nullptr;
  if (!context) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  // Get the transferable
  RefPtr<nsITransferable> transferable = aData.GetTransferable(0, context);
  if (!transferable) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  // Create a runnable
  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
    [transferable, p, clipboard]() {
      nsresult rv = clipboard->SetData(transferable,
                                       /* owner of the transferable */ nullptr,
                                       nsIClipboard::kGlobalClipboard);
      if (NS_FAILED(rv)) {
        p->MaybeRejectWithUndefined();
        return;
      }
      p->MaybeResolveWithUndefined();
      return;
    });
  // Dispatch the runnable
  NS_DispatchToCurrentThread(r.forget());
  return p.forget();
}

already_AddRefed<Promise>
Clipboard::WriteText(JSContext* aCx, const nsAString& aData,
                     nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv)
{
  // We create a data transfer with text/plain format so that
  // we can reuse the Clipboard::Write(...) member function
  // We want isExternal = true in order to use the data transfer object to perform a write
  RefPtr<DataTransfer> dataTransfer = new DataTransfer(this, eCopy,
                                                      /* is external */ true,
                                                      /* clipboard type */ -1);
  dataTransfer->SetData(NS_LITERAL_STRING(kTextMime), aData, aSubjectPrincipal, aRv);
  return Write(aCx, *dataTransfer, aSubjectPrincipal, aRv);
}

JSObject*
Clipboard::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ClipboardBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ LogModule*
Clipboard::GetClipboardLog()
{
  return gClipboardLog;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Clipboard)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Clipboard,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Clipboard,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Clipboard)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Clipboard, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Clipboard, DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
