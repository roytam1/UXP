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

static constexpr float kLabLightnessMax = 100.0f;
static constexpr float kLchPercentScaleC = 150.0f;
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
LinearSRGBToColor(float aLinearR, float aLinearG, float aLinearB,
                  uint8_t aAlpha)
{
  float r = mozilla::clamped(LinearSRGBToEncoded(aLinearR), 0.0f, 1.0f);
  float g = mozilla::clamped(LinearSRGBToEncoded(aLinearG), 0.0f, 1.0f);
  float b = mozilla::clamped(LinearSRGBToEncoded(aLinearB), 0.0f, 1.0f);

  return NS_RGBA(
    NSToIntRound(r * 255.0f),
    NSToIntRound(g * 255.0f),
    NSToIntRound(b * 255.0f),
    aAlpha);
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

  return LinearSRGBToColor(linearR, linearG, linearB, alpha);
}

inline nscolor
OklchToSRGBColor(float aL, float aChroma, float aHue, float aAlpha)
{
  double hueRadians = aHue * kRadiansPerDegree;
  float a = aChroma * std::cos(hueRadians);
  float b = aChroma * std::sin(hueRadians);
  return OklabToSRGBColor(aL, a, b, aAlpha);
}

inline nscolor
LabToSRGBColor(float aL, float aA, float aB, float aAlpha)
{
  float lightness = mozilla::clamped(aL, 0.0f, kLabLightnessMax);
  uint8_t alpha =
    nsStyleUtil::FloatToColorComponent(mozilla::clamped(aAlpha, 0.0f, 1.0f));

  if (lightness <= 0.0f) {
    return NS_RGBA(0, 0, 0, alpha);
  }
  if (lightness >= kLabLightnessMax) {
    return NS_RGBA(255, 255, 255, alpha);
  }

  static constexpr float kLabEpsilon = 216.0f / 24389.0f;
  static constexpr float kLabKappa = 24389.0f / 27.0f;
  static constexpr float kD50WhitePointX = 0.9642956764295677f;
  static constexpr float kD50WhitePointZ = 0.8251046025104602f;

  float fy = (lightness + 16.0f) / 116.0f;
  float fx = aA / 500.0f + fy;
  float fz = fy - aB / 200.0f;

  float fx3 = fx * fx * fx;
  float fy3 = fy * fy * fy;
  float fz3 = fz * fz * fz;

  float x = fx3 > kLabEpsilon ? fx3 : (116.0f * fx - 16.0f) / kLabKappa;
  float y = lightness > kLabKappa * kLabEpsilon ? fy3 : lightness / kLabKappa;
  float z = fz3 > kLabEpsilon ? fz3 : (116.0f * fz - 16.0f) / kLabKappa;

  x *= kD50WhitePointX;
  z *= kD50WhitePointZ;

  float adaptedX = 0.955473421488075f * x - 0.02309845494876471f * y +
                   0.06325924320057072f * z;
  float adaptedY = -0.0283697093338637f * x + 1.0099953980813041f * y +
                   0.021041441191917323f * z;
  float adaptedZ = 0.012314014864481998f * x - 0.020507649298898964f * y +
                   1.330365926242124f * z;

  float linearR = (12831.0f / 3959.0f) * adaptedX +
                  (-329.0f / 214.0f) * adaptedY +
                  (-1974.0f / 3959.0f) * adaptedZ;
  float linearG = (-851781.0f / 878810.0f) * adaptedX +
                  (1648619.0f / 878810.0f) * adaptedY +
                  (36519.0f / 878810.0f) * adaptedZ;
  float linearB = (705.0f / 12673.0f) * adaptedX +
                  (-2585.0f / 12673.0f) * adaptedY +
                  (705.0f / 667.0f) * adaptedZ;

  return LinearSRGBToColor(linearR, linearG, linearB, alpha);
}

inline nscolor
LchToSRGBColor(float aL, float aChroma, float aHue, float aAlpha)
{
  float chroma = aChroma < 0.0f ? 0.0f : aChroma;
  double hueRadians = aHue * kRadiansPerDegree;
  float a = chroma * std::cos(hueRadians);
  float b = chroma * std::sin(hueRadians);
  return LabToSRGBColor(aL, a, b, aAlpha);
}

} // namespace css
} // namespace mozilla

#endif /* nsCSSNonSRGBColorSpace_h___ */
