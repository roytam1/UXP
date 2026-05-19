/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSNonSRGBColorSpace_h___
#define nsCSSNonSRGBColorSpace_h___

#include <algorithm>
#include <cmath>

#include "mozilla/MathAlgorithms.h"
#include "nsColor.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace css {

static constexpr float kLabLightnessMax = 100.0f;
static constexpr float kLchPercentScaleC = 150.0f;
static constexpr float kOklabPercentScaleAB = 0.4f;
static constexpr float kOklchPowerlessChromaEpsilon = 0.000004f;
static constexpr double kRadiansPerDegree = 0.01745329251994329576923690768489;
static constexpr double kDegreesPerRadian = 57.295779513082320876798154814105;

struct OklabColor {
  float mL;
  float mA;
  float mB;
};

struct OklchColor {
  float mL;
  float mChroma;
  float mHue;
};

struct LinearSRGBColor {
  float mR;
  float mG;
  float mB;
};

inline float
NormalizeHue(float aHue)
{
  float hue = std::fmod(aHue, 360.0f);
  if (hue < 0.0f) {
    hue += 360.0f;
  }
  return hue;
}

inline float
EncodedSRGBToLinear(float aValue)
{
  if (aValue <= 0.04045f) {
    return aValue / 12.92f;
  }
  return std::pow((aValue + 0.055f) / 1.055f, 2.4f);
}

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

inline LinearSRGBColor
OklabToLinearSRGB(float aL, float aA, float aB)
{
  float lRoot = aL + 0.3963377774f * aA + 0.2158037573f * aB;
  float mRoot = aL - 0.1055613458f * aA - 0.0638541728f * aB;
  float sRoot = aL - 0.0894841775f * aA - 1.2914855480f * aB;

  float l = lRoot * lRoot * lRoot;
  float m = mRoot * mRoot * mRoot;
  float s = sRoot * sRoot * sRoot;

  return {
    4.0767416361f * l - 3.3077115393f * m + 0.2309699032f * s,
   -1.2684379733f * l + 2.6097573493f * m - 0.3413193760f * s,
   -0.0041960761f * l - 0.7034186179f * m + 1.7076146941f * s
  };
}

inline OklabColor
LinearSRGBToOklabColor(float aLinearR, float aLinearG, float aLinearB)
{
  float l = std::cbrt(0.4122214695f * aLinearR +
                      0.5363325373f * aLinearG +
                      0.0514459933f * aLinearB);
  float m = std::cbrt(0.2119034958f * aLinearR +
                      0.6806995506f * aLinearG +
                      0.1073969535f * aLinearB);
  float s = std::cbrt(0.0883024592f * aLinearR +
                      0.2817188391f * aLinearG +
                      0.6299787017f * aLinearB);

  return {
    0.2104542683f * l + 0.7936177747f * m - 0.0040720430f * s,
    1.9779985324f * l - 2.4285922420f * m + 0.4505937096f * s,
    0.0259040425f * l + 0.7827717125f * m - 0.8086757549f * s
  };
}

inline OklabColor
SRGBToOklabColor(nscolor aColor)
{
  float linearR = EncodedSRGBToLinear(NS_GET_R(aColor) / 255.0f);
  float linearG = EncodedSRGBToLinear(NS_GET_G(aColor) / 255.0f);
  float linearB = EncodedSRGBToLinear(NS_GET_B(aColor) / 255.0f);

  return LinearSRGBToOklabColor(linearR, linearG, linearB);
}

inline OklchColor
OklabToOklchColor(const OklabColor& aColor)
{
  float chroma = std::sqrt(aColor.mA * aColor.mA + aColor.mB * aColor.mB);
  float hue = std::atan2(aColor.mB, aColor.mA) * kDegreesPerRadian;
  if (hue < 0.0f) {
    hue += 360.0f;
  }

  return { aColor.mL, chroma, hue };
}

inline nscolor
OklabToSRGBColor(float aL, float aA, float aB, float aAlpha)
{
  // Per CSS Color, the lightness component for Oklab/Oklch is clamped at
  // parsed-value time.
  float lightness = mozilla::clamped(aL, 0.0f, 1.0f);
  uint8_t alpha =
    nsStyleUtil::FloatToColorComponent(mozilla::clamped(aAlpha, 0.0f, 1.0f));

  OklabColor mapped = { lightness, aA, aB };
  auto isInSRGBGamut = [](const OklabColor& aColor) {
    LinearSRGBColor linear =
      OklabToLinearSRGB(aColor.mL, aColor.mA, aColor.mB);
    return linear.mR >= 0.0f && linear.mR <= 1.0f &&
           linear.mG >= 0.0f && linear.mG <= 1.0f &&
           linear.mB >= 0.0f && linear.mB <= 1.0f;
  };
  auto clippedOklab = [](const OklabColor& aColor) {
    LinearSRGBColor linear =
      OklabToLinearSRGB(aColor.mL, aColor.mA, aColor.mB);
    return LinearSRGBToOklabColor(
      mozilla::clamped(linear.mR, 0.0f, 1.0f),
      mozilla::clamped(linear.mG, 0.0f, 1.0f),
      mozilla::clamped(linear.mB, 0.0f, 1.0f));
  };
  auto deltaEOK = [](const OklabColor& aColor1, const OklabColor& aColor2) {
    float deltaL = aColor1.mL - aColor2.mL;
    float deltaA = aColor1.mA - aColor2.mA;
    float deltaB = aColor1.mB - aColor2.mB;
    return std::sqrt(deltaL * deltaL + deltaA * deltaA + deltaB * deltaB);
  };

  if (lightness <= 0.0f) {
    mapped = { 0.0f, 0.0f, 0.0f };
  } else if (lightness >= 1.0f) {
    mapped = { 1.0f, 0.0f, 0.0f };
  } else if (!isInSRGBGamut(mapped)) {
    // CSS Color 4 binary search gamut mapping with local MINDE, targeting sRGB.
    static constexpr float kJND = 0.02f;
    static constexpr float kGamutMapEpsilon = 0.0001f;

    OklchColor origin = OklabToOklchColor(mapped);
    OklabColor clipped = clippedOklab(mapped);
    float delta = deltaEOK(clipped, mapped);

    if (delta < kJND) {
      mapped = clipped;
    } else {
      float min = 0.0f;
      float max = origin.mChroma;
      bool minInGamut = true;
      OklabColor current = mapped;

      while (max - min > kGamutMapEpsilon) {
        float chroma = (min + max) / 2.0f;
        current.mL = origin.mL;
        current.mA = chroma * std::cos(origin.mHue * kRadiansPerDegree);
        current.mB = chroma * std::sin(origin.mHue * kRadiansPerDegree);

        if (minInGamut && isInSRGBGamut(current)) {
          min = chroma;
          continue;
        }

        clipped = clippedOklab(current);
        delta = deltaEOK(clipped, current);
        if (delta < kJND) {
          if (kJND - delta < kGamutMapEpsilon) {
            mapped = clipped;
            break;
          }
          minInGamut = false;
          min = chroma;
        } else {
          max = chroma;
        }
        mapped = clipped;
      }
    }
  }

  LinearSRGBColor linear =
    OklabToLinearSRGB(mapped.mL, mapped.mA, mapped.mB);
  return LinearSRGBToColor(linear.mR, linear.mG, linear.mB, alpha);
}

inline nscolor
OklchToSRGBColor(float aL, float aChroma, float aHue, float aAlpha)
{
  float chroma = std::max(aChroma, 0.0f);
  double hueRadians = NormalizeHue(aHue) * kRadiansPerDegree;
  float a = chroma * std::cos(hueRadians);
  float b = chroma * std::sin(hueRadians);
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
