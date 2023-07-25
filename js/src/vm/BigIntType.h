/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BigIntType_h
#define vm_BigIntType_h

#include "mozilla/Range.h"
#include "mozilla/Span.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Heap.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/TraceKind.h"
#include "js/TypeDecls.h"
#include "vm/String.h"
#include "vm/Xdr.h"

// Handle future js::gc::Cell::ReservedBits, we have no reserved bits...
#define js_gc_Cell_ReservedBits 0
// Handle future js::gc::MinCellSize, 16 bytes, twice our js:gc:CellSize
#define js_gc_MinCellSize (js::gc::CellSize*2)

namespace JS {

class BigInt;

}  // namespace JS

namespace js {

template <XDRMode mode>
bool XDRBigInt(XDRState<mode>* xdr, MutableHandle<JS::BigInt*> bi);

}  // namespace js

namespace JS {

class BigInt final : public js::gc::TenuredCell {
 public:
  using Digit = uintptr_t;

 private:
  // The low js::gc::Cell::ReservedBits are reserved.
  static constexpr uintptr_t SignBit = JS_BIT(js_gc_Cell_ReservedBits);
  static constexpr uintptr_t LengthShift = js_gc_Cell_ReservedBits + 1;
  static constexpr size_t InlineDigitsLength =
      (js_gc_MinCellSize - sizeof(uintptr_t)) / sizeof(Digit);

  uintptr_t lengthSignAndReservedBits_;

  // The digit storage starts with the least significant digit (little-endian
  // digit order).  Byte order within a digit is of course native endian.
  union {
    Digit* heapDigits_;
    Digit inlineDigits_[InlineDigitsLength];
  };

 public:
  static const JS::TraceKind TraceKind = JS::TraceKind::BigInt;

  size_t digitLength() const {
    return lengthSignAndReservedBits_ >> LengthShift;
  }

  bool hasInlineDigits() const { return digitLength() <= InlineDigitsLength; }
  bool hasHeapDigits() const { return !hasInlineDigits(); }

  using Digits = mozilla::Span<Digit>;
  Digits digits() {
    return Digits(hasInlineDigits() ? inlineDigits_ : heapDigits_,
                  digitLength());
  }
  Digit digit(size_t idx) { return digits()[idx]; }
  void setDigit(size_t idx, Digit digit) { digits()[idx] = digit; }

  bool isZero() const { return digitLength() == 0; }
  bool isNegative() const { return lengthSignAndReservedBits_ & SignBit; }

  // Offset for direct access from JIT code.
  static constexpr size_t offsetOfLengthSignAndReservedBits() {
    return offsetof(BigInt, lengthSignAndReservedBits_);
  }

  void initializeDigitsToZero();

  void traceChildren(JSTracer* trc);
  void finalize(js::FreeOp* fop);
  js::HashNumber hash();
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  static BigInt* createUninitialized(js::ExclusiveContext* cx, size_t length,
                                     bool isNegative);
  static BigInt* createFromDouble(js::ExclusiveContext* cx, double d);
  static BigInt* createFromUint64(js::ExclusiveContext* cx, uint64_t n);
  static BigInt* createFromInt64(js::ExclusiveContext* cx, int64_t n);
  // FIXME: Cache these values.
  static BigInt* zero(js::ExclusiveContext* cx);
  static BigInt* one(js::ExclusiveContext* cx);

  static BigInt* copy(js::ExclusiveContext* cx, Handle<BigInt*> x);
  static BigInt* add(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* sub(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* mul(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* div(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* mod(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* pow(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* neg(js::ExclusiveContext* cx, Handle<BigInt*> x);
  static BigInt* lsh(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* rsh(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitAnd(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitXor(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitOr(js::ExclusiveContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitNot(js::ExclusiveContext* cx, Handle<BigInt*> x);

  static int64_t toInt64(BigInt* x);
  static uint64_t toUint64(BigInt* x);

  // Return true if the BigInt is without loss of precision representable as an
  // int64 and store the int64 value in the output. Otherwise return false and
  // leave the value of the output parameter unspecified.
  static bool isInt64(BigInt* x, int64_t* result);

  static BigInt* asIntN(js::ExclusiveContext* cx, Handle<BigInt*> x, uint64_t bits);
  static BigInt* asUintN(js::ExclusiveContext* cx, Handle<BigInt*> x, uint64_t bits);

  // Type-checking versions of arithmetic operations. These methods
  // must be called with at least one BigInt operand. Binary
  // operations will throw a TypeError if one of the operands is not a
  // BigInt value.
  static bool add(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool sub(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool mul(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool div(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool mod(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool pow(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool neg(js::ExclusiveContext* cx, Handle<Value> operand,
                  MutableHandle<Value> res);
  static bool lsh(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool rsh(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool bitAnd(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                     MutableHandle<Value> res);
  static bool bitXor(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                     MutableHandle<Value> res);
  static bool bitOr(js::ExclusiveContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                    MutableHandle<Value> res);
  static bool bitNot(js::ExclusiveContext* cx, Handle<Value> operand,
                     MutableHandle<Value> res);

  static double numberValue(BigInt* x);

  static JSLinearString* toString(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                  uint8_t radix);
  template <typename CharT>
  static BigInt* parseLiteral(js::ExclusiveContext* cx,
                              const mozilla::Range<const CharT> chars,
                              bool* haveParseError);
  template <typename CharT>
  static BigInt* parseLiteralDigits(js::ExclusiveContext* cx,
                                    const mozilla::Range<const CharT> chars,
                                    unsigned radix, bool isNegative,
                                    bool* haveParseError);

  static int8_t compare(BigInt* lhs, BigInt* rhs);
  static bool equal(BigInt* lhs, BigInt* rhs);
  static JS::Result<bool> looselyEqual(js::ExclusiveContext* cx, Handle<BigInt*> lhs,
                                       HandleValue rhs);

  static bool lessThan(BigInt* x, BigInt* y);
  // These methods return Nothing when the non-BigInt operand is NaN
  // or a string that can't be interpreted as a BigInt.
  static mozilla::Maybe<bool> lessThan(BigInt* lhs, double rhs);
  static mozilla::Maybe<bool> lessThan(double lhs, BigInt* rhs);
  static bool lessThan(js::ExclusiveContext* cx, Handle<BigInt*> lhs, HandleString rhs,
                       mozilla::Maybe<bool>& res);
  static bool lessThan(js::ExclusiveContext* cx, HandleString lhs, Handle<BigInt*> rhs,
                       mozilla::Maybe<bool>& res);
  static bool lessThan(js::ExclusiveContext* cx, HandleValue lhs, HandleValue rhs,
                       mozilla::Maybe<bool>& res);

 private:
  static constexpr size_t DigitBits = sizeof(Digit) * CHAR_BIT;
  static constexpr size_t HalfDigitBits = DigitBits / 2;
  static constexpr Digit HalfDigitMask = (1ull << HalfDigitBits) - 1;

  static_assert(DigitBits == 32 || DigitBits == 64,
                "Unexpected BigInt Digit size");

  // The maximum number of digits that the current implementation supports
  // would be 0x7fffffff / DigitBits. However, we use a lower limit for now,
  // because raising it later is easier than lowering it.  Support up to 1
  // million bits.
  static constexpr size_t MaxBitLength = 1024 * 1024;
  static constexpr size_t MaxDigitLength = MaxBitLength / DigitBits;

  // BigInts can be serialized to strings of radix between 2 and 36.  For a
  // given bigint, radix 2 will take the most characters (one per bit).
  // Ensure that the max bigint size is small enough so that we can fit the
  // corresponding character count into a size_t, with space for a possible
  // sign prefix.
  static_assert(MaxBitLength <= std::numeric_limits<size_t>::max() - 1,
                "BigInt max length must be small enough to be serialized as a "
                "binary string");

  static size_t calculateMaximumCharactersRequired(HandleBigInt x,
                                                   unsigned radix);
  static MOZ_MUST_USE bool calculateMaximumDigitsRequired(js::ExclusiveContext* cx,
                                                          uint8_t radix,
                                                          size_t charCount,
                                                          size_t* result);

  static bool absoluteDivWithDigitDivisor(
      js::ExclusiveContext* cx, Handle<BigInt*> x, Digit divisor,
      const mozilla::Maybe<MutableHandle<BigInt*>>& quotient, Digit* remainder,
      bool quotientNegative);
  static void internalMultiplyAdd(BigInt* source, Digit factor, Digit summand,
                                  unsigned, BigInt* result);
  static void multiplyAccumulate(BigInt* multiplicand, Digit multiplier,
                                 BigInt* accumulator,
                                 unsigned accumulatorIndex);
  static bool absoluteDivWithBigIntDivisor(
      js::ExclusiveContext* cx, Handle<BigInt*> dividend, Handle<BigInt*> divisor,
      const mozilla::Maybe<MutableHandle<BigInt*>>& quotient,
      const mozilla::Maybe<MutableHandle<BigInt*>>& remainder,
      bool quotientNegative);

  enum class LeftShiftMode { SameSizeResult, AlwaysAddOneDigit };

  static BigInt* absoluteLeftShiftAlwaysCopy(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                             unsigned shift, LeftShiftMode);
  static bool productGreaterThan(Digit factor1, Digit factor2, Digit high,
                                 Digit low);
  static BigInt* lshByAbsolute(js::ExclusiveContext* cx, HandleBigInt x, HandleBigInt y);
  static BigInt* rshByAbsolute(js::ExclusiveContext* cx, HandleBigInt x, HandleBigInt y);
  static BigInt* rshByMaximum(js::ExclusiveContext* cx, bool isNegative);
  static BigInt* truncateAndSubFromPowerOfTwo(js::ExclusiveContext* cx, HandleBigInt x,
                                              uint64_t bits,
                                              bool resultNegative);

  Digit absoluteInplaceAdd(BigInt* summand, unsigned startIndex);
  Digit absoluteInplaceSub(BigInt* subtrahend, unsigned startIndex);
  void inplaceRightShiftLowZeroBits(unsigned shift);
  void inplaceMultiplyAdd(Digit multiplier, Digit part);

  // The result of an SymmetricTrim bitwise op has as many digits as the
  // smaller operand.  A SymmetricFill bitwise op result has as many digits as
  // the larger operand, with high digits (if any) copied from the larger
  // operand.  AsymmetricFill is like SymmetricFill, except the result has as
  // many digits as the first operand; this kind is used for the and-not
  // operation.
  enum class BitwiseOpKind { SymmetricTrim, SymmetricFill, AsymmetricFill };

  template <BitwiseOpKind kind, typename BitwiseOp>
  static BigInt* absoluteBitwiseOp(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                   Handle<BigInt*> y, BitwiseOp&& op);

  // Return `|x| & |y|`.
  static BigInt* absoluteAnd(js::ExclusiveContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y);

  // Return `|x| | |y|`.
  static BigInt* absoluteOr(js::ExclusiveContext* cx, Handle<BigInt*> x,
                            Handle<BigInt*> y);

  // Return `|x| & ~|y|`.
  static BigInt* absoluteAndNot(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                Handle<BigInt*> y);

  // Return `|x| ^ |y|`.
  static BigInt* absoluteXor(js::ExclusiveContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y);

  // Return `(|x| + 1) * (resultNegative ? -1 : +1)`.
  static BigInt* absoluteAddOne(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                bool resultNegative);

  // Return `(|x| - 1) * (resultNegative ? -1 : +1)`, with the precondition that
  // |x| != 0.
  static BigInt* absoluteSubOne(js::ExclusiveContext* cx, Handle<BigInt*> x,
                                unsigned resultLength);

  // Return `a + b`, incrementing `*carry` if the addition overflows.
  static inline Digit digitAdd(Digit a, Digit b, Digit* carry) {
    Digit result = a + b;
    *carry += static_cast<Digit>(result < a);
    return result;
  }

  // Return `left - right`, incrementing `*borrow` if the addition overflows.
  static inline Digit digitSub(Digit left, Digit right, Digit* borrow) {
    Digit result = left - right;
    *borrow += static_cast<Digit>(result > left);
    return result;
  }

  // Compute `a * b`, returning the low half of the result and putting the
  // high half in `*high`.
  static Digit digitMul(Digit a, Digit b, Digit* high);

  // Divide `(high << DigitBits) + low` by `divisor`, returning the quotient
  // and storing the remainder in `*remainder`, with the precondition that
  // `high < divisor` so that the result fits in a Digit.
  static Digit digitDiv(Digit high, Digit low, Digit divisor, Digit* remainder);

  // Return `(|x| + |y|) * (resultNegative ? -1 : +1)`.
  static BigInt* absoluteAdd(js::ExclusiveContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y, bool resultNegative);

  // Return `(|x| - |y|) * (resultNegative ? -1 : +1)`, with the precondition
  // that |x| >= |y|.
  static BigInt* absoluteSub(js::ExclusiveContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y, bool resultNegative);

  // If `|x| < |y|` return -1; if `|x| == |y|` return 0; otherwise return 1.
  static int8_t absoluteCompare(BigInt* lhs, BigInt* rhs);

  static int8_t compare(BigInt* lhs, double rhs);

  static bool equal(BigInt* lhs, double rhs);

  static JSLinearString* toStringBasePowerOfTwo(js::ExclusiveContext* cx, Handle<BigInt*>,
                                                unsigned radix);
  static JSLinearString* toStringGeneric(js::ExclusiveContext* cx, Handle<BigInt*>,
                                         unsigned radix);

  static BigInt* trimHighZeroDigits(js::ExclusiveContext* cx, Handle<BigInt*> x);
  static BigInt* destructivelyTrimHighZeroDigits(js::ExclusiveContext* cx,
                                                 Handle<BigInt*> x);

  friend struct JSStructuredCloneReader;
  friend struct JSStructuredCloneWriter;
  template <js::XDRMode mode>
  friend bool js::XDRBigInt(js::XDRState<mode>* xdr, MutableHandle<BigInt*> bi);

  BigInt() = delete;
  BigInt(const BigInt& other) = delete;
  void operator=(const BigInt& other) = delete;
};

static_assert(
    sizeof(BigInt) >= js_gc_MinCellSize,
    "sizeof(BigInt) must be greater than the minimum allocation size");

static_assert(
    sizeof(BigInt) == js_gc_MinCellSize,
    "sizeof(BigInt) intended to be the same as the minimum allocation size");

}  // namespace JS

namespace js {

extern JSAtom* BigIntToAtom(js::ExclusiveContext* cx, JS::HandleBigInt bi);

extern JS::BigInt* NumberToBigInt(js::ExclusiveContext* cx, double d);
extern JS::Result<int64_t> ToBigInt64(JSContext* cx, JS::Handle<JS::Value> v);
extern JS::Result<uint64_t> ToBigUint64(JSContext* cx, JS::Handle<JS::Value> v);

// Parse a BigInt from a string, using the method specified for StringToBigInt.
// Used by the BigInt constructor among other places.
extern JS::Result<JS::BigInt*, JS::OOM&> StringToBigInt(
    js::ExclusiveContext* cx, JS::Handle<JSString*> str);

// Parse a BigInt from an already-validated numeric literal.  Used by the
// parser.  Can only fail in out-of-memory situations.
extern JS::BigInt* ParseBigIntLiteral(
    js::ExclusiveContext* cx, const mozilla::Range<const char16_t>& chars);

extern JS::BigInt* ToBigInt(js::ExclusiveContext* cx, JS::Handle<JS::Value> v);

}  // namespace js

#undef js_gc_MinCellSize
#undef js_gc_Cell_ReservedBits

#endif
