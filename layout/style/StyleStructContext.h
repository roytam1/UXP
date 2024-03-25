/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleStructContext_h
#define mozilla_StyleStructContext_h

#include "CounterStyleManager.h"
#include "mozilla/LookAndFeel.h"
#include "nsPresContext.h"

class nsDeviceContext;

/**
 * Construction context for style structs.
 * TODO: marked for removal.
 */
class StyleStructContext {
public:
  MOZ_IMPLICIT StyleStructContext(nsPresContext* aPresContext)
    : mPresContext(aPresContext) { MOZ_ASSERT(aPresContext); }
  static StyleStructContext ServoContext() { return StyleStructContext(); }

  float TextZoom()
  {
    return mPresContext->TextZoom();
  }

  const nsFont* GetDefaultFont(uint8_t aFontID)
  {
    return mPresContext->GetDefaultFont(aFontID, GetLanguageFromCharset());
  }

  uint32_t GetBidi()
  {
    return mPresContext->GetBidi();
  }

  int32_t AppUnitsPerDevPixel();

  nscoord DevPixelsToAppUnits(int32_t aPixels)
  {
    return NSIntPixelsToAppUnits(aPixels, AppUnitsPerDevPixel());
  }

  typedef mozilla::LookAndFeel LookAndFeel;
  nscolor DefaultColor()
  {
    return mPresContext->DefaultColor();
  }

  mozilla::CounterStyle* BuildCounterStyle(const nsSubstring& aName)
  {
    return mPresContext->CounterStyleManager()->BuildCounterStyle(aName);
  }

  nsIAtom* GetLanguageFromCharset() const
  {
    return mPresContext->GetLanguageFromCharset();
  }

  already_AddRefed<nsIAtom> GetContentLanguage() const
  {
    return mPresContext->GetContentLanguage();
  }

private:
  nsDeviceContext* DeviceContext()
  {
    return mPresContext->DeviceContext();
  }

  nsDeviceContext* HackilyFindSomeDeviceContext();

  StyleStructContext() : mPresContext(nullptr) {}
  nsPresContext* mPresContext;
};

#endif // mozilla_StyleStructContext_h
