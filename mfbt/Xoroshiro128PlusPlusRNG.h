/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The xoroshiro128++ pseudo-random number generator. */

#ifndef mozilla_Xoroshiro128PlusPlus_h
#define mozilla_Xoroshiro128PlusPlus_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"

#include <inttypes.h>

namespace mozilla {
namespace non_crypto {

/*
 * A stream of pseudo-random numbers generated using the xoroshiro128++ technique described here:
 *
 * Blackman, David and Vigna, Sebastiano (2019). "A PRNG Shootout"
 * https://prng.di.unimi.it/
 *
 * That paper says:
 *
 *     xoshiro256++/xoshiro256** (XOR/shift/rotate) are our all-purpose generators (not cryptographically secure
 *     generators, though, like all PRNGs in these pages). They have excellent (sub-ns) speed, a state space
 *     (256 bits) that is large enough for any parallel application, and they pass all tests we are aware of. 
 *     If you are tight on space, xoroshiro128++/xoroshiro128** (XOR/rotate/shift/rotate) and xoroshiro128+ have
 *     the same speed and use half of the space; the same comments apply.
 *     They are suitable only for low-scale parallel applications.
 *
 * The stream of numbers produced by this method repeats every 2^128 - 1 calls (i.e. never, for all practical
 * purposes).
 *
 */
class Xoroshiro128PlusPlusRNG {
  /*
   * mState[0] and mState[1] are as-described in the Xoroshiro128++ paper.
   * mState[2] is used for temporary storage of the result in JIT code.
   */
  uint64_t mState[3];

 public:
  /*
   * Construct a xoroshiro128++ pseudo-random number stream using |aInitial0| and |aInitial1| as the initial state.
   * These MUST NOT both be zero.
   *
   * If the initial states contain many zeros, for a few iterations you'll see many zeroes in the generated numbers.
   * It's suggested to seed a SplitMix64 generator <http://xorshift.di.unimi.it/splitmix64.c> and use its first two
   * outputs to seed xoroshiro128++.
   */
  Xoroshiro128PlusPlusRNG(uint64_t aInitial0, uint64_t aInitial1) {
    setState(aInitial0, aInitial1);
  }

  /**
   * Helper function:
   * Rotate a 64-bit number left by a specified number of positions
   */
  static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }

  /**
   * Return a pseudo-random 64-bit number.
   */
  uint64_t next() {
    /*
     * The offsetOfState*() methods below are provided so that exceedingly-rare callers (like our JIT...) that want to
     * observe or poke at RNG state in C++ type-system-ignoring means can do so. Don't change the next() or nextDouble()
     * algorithms without altering code that uses offsetOfState*()!
     */
    const uint64_t s0 = mState[0];
    uint64_t s1 = mState[1];
    const uint64_t result = rotl(s0 + s1, 17) + s0;
    
    s1 ^= s0;
    mState[0] = rotl(s0, 49) ^ s1 ^ (s1 << 21); // a, b
    mState[1] = rotl(s1, 28); // c
    
    return result;
  }

  /*
   * Return a pseudo-random floating-point value in the range [0, 1). More
   * precisely, choose an integer in the range [0, 2^53) and divide it by
   * 2^53. Given the 2^128 - 1 period noted above, the produced doubles are
   * all but uniformly distributed in this range.
   */
  double nextDouble() {
    /*
     * Because the IEEE 64-bit floating point format stores the leading '1' bit
     * of the mantissa implicitly, it effectively represents a mantissa in the
     * range [0, 2^53) in only 52 bits. FloatingPoint<double>::kExponentShift
     * is the width of the bitfield in the in-memory format, so we must add one
     * to get the mantissa's range.
     */
    static constexpr int kMantissaBits =
      mozilla::FloatingPoint<double>::kExponentShift + 1;
    uint64_t mantissa = next() & ((UINT64_C(1) << kMantissaBits) - 1);
    return double(mantissa) / (UINT64_C(1) << kMantissaBits);
  }

  /*
   * Set the stream's current state to |aState0| and |aState1|. These must not
   * both be zero; ideally, they should have an almost even mix of zero and one
   * bits.
   */
  void setState(uint64_t aState0, uint64_t aState1) {
    MOZ_ASSERT(aState0 || aState1);
    mState[0] = aState0;
    mState[1] = aState1;
    mState[2] = 0; // Could be left uninitialized, but we do this just-in-case.
  }

  static size_t offsetOfState0() {
    return offsetof(Xoroshiro128PlusPlusRNG, mState[0]);
  }
  static size_t offsetOfState1() {
    return offsetof(Xoroshiro128PlusPlusRNG, mState[1]);
  }
  static size_t offsetOfState2() {
    return offsetof(Xoroshiro128PlusPlusRNG, mState[2]);
  }
};

} // namespace non_crypto
} // namespace mozilla

#endif // mozilla_Xoroshiro128PlusPlus_h
