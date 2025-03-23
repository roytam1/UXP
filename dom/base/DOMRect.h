/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMRECT_H_
#define MOZILLA_DOMRECT_H_

#include "js/StructuredClone.h"
#include "nsIDOMClientRect.h"
#include "nsIDOMClientRectList.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"

struct nsRect;

namespace mozilla {
namespace dom {

struct DOMRectInit;

class DOMRectReadOnly : public nsISupports
                      , public nsWrapperCache
{
protected:
  virtual ~DOMRectReadOnly() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRectReadOnly)

  explicit DOMRectReadOnly(nsISupports* aParent, double aX = 0, double aY = 0,
                           double aWidth = 0, double aHeight = 0)
    : mParent(aParent)
    , mX(aX)
    , mY(aY)
    , mWidth(aWidth)
    , mHeight(aHeight)
  {
  }

  nsISupports* GetParentObject() const
  {
    MOZ_ASSERT(mParent);
    return mParent;
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMRectReadOnly>
  FromRect(const GlobalObject& aGlobal, const DOMRectInit& aInit);

  static already_AddRefed<DOMRectReadOnly>
  Constructor(const GlobalObject& aGlobal, double aX, double aY,
              double aWidth, double aHeight, ErrorResult& aRv);

  double X() const
  {
    return mX;
  }
  double Y() const
  {
    return mY;
  }
  double Width() const
  {
    return mWidth;
  }
  double Height() const
  {
    return mHeight;
  }

  double Left() const
  {
    double x = X(), w = Width();
    return NaNSafeMin(x, x + w);
  }
  double Top() const
  {
    double y = Y(), h = Height();
    return NaNSafeMin(y, y + h);
  }
  double Right() const
  {
    double x = X(), w = Width();
    return NaNSafeMax(x, x + w);
  }
  double Bottom() const
  {
    double y = Y(), h = Height();
    return NaNSafeMax(y, y + h);
  }

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;

  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

protected:
  nsCOMPtr<nsISupports> mParent;
  double mX, mY, mWidth, mHeight;
};

class DOMRect final : public DOMRectReadOnly
                    , public nsIDOMClientRect
{
public:
  explicit DOMRect(nsISupports* aParent, double aX = 0, double aY = 0,
                   double aWidth = 0, double aHeight = 0)
    : DOMRectReadOnly(aParent, aX, aY, aWidth, aHeight)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCLIENTRECT

  static already_AddRefed<DOMRect>
  FromRect(const GlobalObject& aGlobal, const DOMRectInit& aInit);

  static already_AddRefed<DOMRect>
  Constructor(const GlobalObject& aGlobal, double aX, double aY,
              double aWidth, double aHeight, ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX; mY = aY; mWidth = aWidth; mHeight = aHeight;
  }
  void SetLayoutRect(const nsRect& aLayoutRect);

  void SetX(double aX)
  {
    mX = aX;
  }
  void SetY(double aY)
  {
    mY = aY;
  }
  void SetWidth(double aWidth)
  {
    mWidth = aWidth;
  }
  void SetHeight(double aHeight)
  {
    mHeight = aHeight;
  }

private:
  ~DOMRect() {}
};

class DOMRectList final : public nsIDOMClientRectList,
                          public nsWrapperCache
{
  ~DOMRectList() {}

public:
  explicit DOMRectList(nsISupports *aParent) : mParent(aParent)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRectList)

  NS_DECL_NSIDOMCLIENTRECTLIST
  
  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(DOMRect* aElement) { mArray.AppendElement(aElement); }

  static DOMRectList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMClientRectList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMClientRectList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMClientRectList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<DOMRectList*>(aSupports);
  }

  uint32_t Length()
  {
    return mArray.Length();
  }
  DOMRect* Item(uint32_t aIndex)
  {
    return mArray.SafeElementAt(aIndex);
  }
  DOMRect* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < mArray.Length();
    if (!aFound) {
      return nullptr;
    }
    return mArray[aIndex];
  }

protected:
  nsTArray<RefPtr<DOMRect> > mArray;
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

#endif /*MOZILLA_DOMRECT_H_*/
