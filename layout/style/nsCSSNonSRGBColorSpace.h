/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSNonSRGBColorSpace_h___
#define nsCSSNonSRGBColorSpace_h___

#include <cmath>

#include "mozilla/MathAlgorithms.h"
#include "nsColor.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace css {

static constexpr float kOklabPercentScaleAB = 0.4f;
static constexpr double kRadiansPerDegree = 0.01745329251994329576923690768489;

inline float
LinearSRGBToEncoded(float aValue)
{
  if (aValue <= 0.0031308f) {
    return 12.92f * aValue;
  }
  return 1.055f * std::pow(aValue, 1.0f / 2.4f) - 0.055f;
}

inline nscolor
OklabToSRGBColor(float aL, float aA, float aB, float aAlpha)
{
  // Per CSS Color, the lightness component for Oklab/Oklch is clamped.
  float lightness = mozilla::clamped(aL, 0.0f, 1.0f);
  uint8_t alpha =
    nsStyleUtil::FloatToColorComponent(mozilla::clamped(aAlpha, 0.0f, 1.0f));

  // Treat values extremely close to zero as zero to avoid tiny floating-point
  // representation differences for percentage inputs.
  static constexpr float kLightnessEndpointEpsilon = 0.000002f;

  if (lightness <= kLightnessEndpointEpsilon) {
    return NS_RGBA(0, 0, 0, alpha);
  }

  float lRoot = lightness + 0.3963377774f * aA + 0.2158037573f * aB;
  float mRoot = lightness - 0.1055613458f * aA - 0.0638541728f * aB;
  float sRoot = lightness - 0.0894841775f * aA - 1.2914855480f * aB;

  float l = lRoot * lRoot * lRoot;
  float m = mRoot * mRoot * mRoot;
  float s = sRoot * sRoot * sRoot;

  float linearR =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
  float linearG = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
  float linearB = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

  float r = mozilla::clamped(LinearSRGBToEncoded(linearR), 0.0f, 1.0f);
  float g = mozilla::clamped(LinearSRGBToEncoded(linearG), 0.0f, 1.0f);
  float b = mozilla::clamped(LinearSRGBToEncoded(linearB), 0.0f, 1.0f);

  return NS_RGBA(
    NSToIntRound(r * 255.0f),
    NSToIntRound(g * 255.0f),
    NSToIntRound(b * 255.0f),
    alpha);
}

inline nscolor
OklchToSRGBColor(float aL, float aChroma, float aHue, float aAlpha)
{
  double hueRadians = aHue * kRadiansPerDegree;
  float a = aChroma * std::cos(hueRadians);
  float b = aChroma * std::sin(hueRadians);
  return OklabToSRGBColor(aL, a, b, aAlpha);
}

} // namespace css
} // namespace mozilla

#endif /* nsCSSNonSRGBColorSpace_h___ */
