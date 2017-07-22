// This is the double-conversion library, put into a single header to easily
// integrate into the library (slightly modified, unused parts removed).
//
// https://github.com/google/double-conversion
//
// Original license follows:

// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef DOUBLE_CONVERSION_INLINE_H
#define DOUBLE_CONVERSION_INLINE_H 1

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//==============================================================================
// utils
//==============================================================================

#ifndef DOUBLE_CONVERSION_ASSERT
#define DOUBLE_CONVERSION_ASSERT(condition) assert(condition)
#endif
#define DOUBLE_CONVERSION_UNIMPLEMENTED() (abort())
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort())


// The expression ARRAY_SIZE(a) is a compile-time constant of type
// size_t which represents the number of elements of the given
// array. You should only use ARRAY_SIZE on statically allocated
// arrays.
#define DOUBLE_CONVERSION_ARRAY_SIZE(a)                                   \
  ((sizeof(a) / sizeof(*(a))) /                         \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

namespace double_conversion {

namespace impl {

// Returns the maximum of the two parameters.
template <typename T>
static T Max(T a, T b) {
  return a < b ? b : a;
}


// Returns the minimum of the two parameters.
template <typename T>
static T Min(T a, T b) {
  return a < b ? a : b;
}

} // namespace impl


// This is a simplified version of V8's Vector class.
template <typename T>
class Vector {
 public:
  Vector() : start_(NULL), length_(0) {}
  Vector(T* data, int len) : start_(data), length_(len) {
    DOUBLE_CONVERSION_ASSERT(len == 0 || (len > 0 && data != NULL));
  }

  // Returns a vector using the same backing storage as this one,
  // spanning from and including 'from', to but not including 'to'.
  Vector<T> SubVector(int from, int to) {
    DOUBLE_CONVERSION_ASSERT(to <= length_);
    DOUBLE_CONVERSION_ASSERT(from < to);
    DOUBLE_CONVERSION_ASSERT(0 <= from);
    return Vector<T>(start() + from, to - from);
  }

  // Returns the length of the vector.
  int length() const { return length_; }

  // Returns whether or not the vector is empty.
  bool is_empty() const { return length_ == 0; }

  // Returns the pointer to the start of the data in the vector.
  T* start() const { return start_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](int index) const {
    DOUBLE_CONVERSION_ASSERT(0 <= index && index < length_);
    return start_[index];
  }

 private:
  T* start_;
  int length_;
};

namespace impl {

// The type-based aliasing rule allows the compiler to assume that pointers of
// different types (for some definition of different) never alias each other.
// Thus the following code does not work:
//
// float f = foo();
// int fbits = *(int*)(&f);
//
// The compiler 'knows' that the int pointer can't refer to f since the types
// don't match, so the compiler may cache f in a register, leaving random data
// in fbits.  Using C++ style casts makes no difference, however a pointer to
// char data is assumed to alias any other pointer.  This is the 'memcpy
// exception'.
//
// Bit_cast uses the memcpy exception to move the bits from a variable of one
// type of a variable of another type.  Of course the end result is likely to
// be implementation dependent.  Most compilers (gcc-4.2 and MSVC 2005)
// will completely optimize BitCast away.
//
// There is an additional use for BitCast.
// Recent gccs will warn when they see casts that may result in breakage due to
// the type-based aliasing rule.  If you have checked that there is no breakage
// you can use BitCast to cast one pointer type to another.  This confuses gcc
// enough that it can no longer see that you have cast one pointer type to
// another thus avoiding the warning.
template <class Dest, class Source>
inline Dest BitCast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "incompatible types");

  Dest dest;
  memmove(&dest, &source, sizeof(dest));
  return dest;
}

template <class Dest, class Source>
inline Dest BitCast(Source* source) {
  return BitCast<Dest>(reinterpret_cast<uintptr_t>(source));
}

} // namespace impl
} // namespace double_conversion

//==============================================================================
// diy-fp
//==============================================================================

namespace double_conversion {
namespace impl {

// This "Do It Yourself Floating Point" class implements a floating-point number
// with a uint64 significand and an int exponent. Normalized DiyFp numbers will
// have the most significant bit of the significand set.
// Multiplication and Subtraction do not normalize their results.
// DiyFp are not designed to contain special doubles (NaN and Infinity).
class DiyFp {
 public:
  static const int kSignificandSize = 64;

  DiyFp() : f_(0), e_(0) {}
  DiyFp(uint64_t significand, int exponent) : f_(significand), e_(exponent) {}

  // this = this - other.
  // The exponents of both numbers must be the same and the significand of this
  // must be bigger than the significand of other.
  // The result will not be normalized.
  void Subtract(const DiyFp& other) {
    DOUBLE_CONVERSION_ASSERT(e_ == other.e_);
    DOUBLE_CONVERSION_ASSERT(f_ >= other.f_);
    f_ -= other.f_;
  }

  // Returns a - b.
  // The exponents of both numbers must be the same and this must be bigger
  // than other. The result will not be normalized.
  static DiyFp Minus(const DiyFp& a, const DiyFp& b) {
    DiyFp result = a;
    result.Subtract(b);
    return result;
  }


  // this = this * other.
  void Multiply(const DiyFp& other);

  // returns a * b;
  static DiyFp Times(const DiyFp& a, const DiyFp& b) {
    DiyFp result = a;
    result.Multiply(b);
    return result;
  }

  void Normalize() {
    DOUBLE_CONVERSION_ASSERT(f_ != 0);
    uint64_t significand = f_;
    int exponent = e_;

    // This method is mainly called for normalizing boundaries. In general
    // boundaries need to be shifted by 10 bits. We thus optimize for this case.
    const uint64_t k10MSBits = 0xFFC0000000000000;
    while ((significand & k10MSBits) == 0) {
      significand <<= 10;
      exponent -= 10;
    }
    while ((significand & kUint64MSB) == 0) {
      significand <<= 1;
      exponent--;
    }
    f_ = significand;
    e_ = exponent;
  }

  static DiyFp Normalize(const DiyFp& a) {
    DiyFp result = a;
    result.Normalize();
    return result;
  }

  uint64_t f() const { return f_; }
  int e() const { return e_; }

  void set_f(uint64_t new_value) { f_ = new_value; }
  void set_e(int new_value) { e_ = new_value; }

 private:
  static const uint64_t kUint64MSB = 0x8000000000000000;

  uint64_t f_;
  int e_;
};

inline void DiyFp::Multiply(const DiyFp& other) {
  // Simply "emulates" a 128 bit multiplication.
  // However: the resulting number only contains 64 bits. The least
  // significant 64 bits are only used for rounding the most significant 64
  // bits.
  const uint64_t kM32 = 0xFFFFFFFFU;
  uint64_t a = f_ >> 32;
  uint64_t b = f_ & kM32;
  uint64_t c = other.f_ >> 32;
  uint64_t d = other.f_ & kM32;
  uint64_t ac = a * c;
  uint64_t bc = b * c;
  uint64_t ad = a * d;
  uint64_t bd = b * d;
  uint64_t tmp = (bd >> 32) + (ad & kM32) + (bc & kM32);
  // By adding 1U << 31 to tmp we round the final result.
  // Halfway cases will be round up.
  tmp += 1U << 31;
  uint64_t result_f = ac + (ad >> 32) + (bc >> 32) + (tmp >> 32);
  e_ += other.e_ + 64;
  f_ = result_f;
}

} // namespace impl
} // namespace double_conversion

//==============================================================================
// ieee
//==============================================================================

namespace double_conversion {
namespace impl {

// We assume that doubles and uint64_t have the same endianness.
static uint64_t double_to_uint64(double d) { return BitCast<uint64_t>(d); }
static double uint64_to_double(uint64_t d64) { return BitCast<double>(d64); }

// Helper functions for doubles.
class Double {
 public:
  static const uint64_t kSignMask = 0x8000000000000000;
  static const uint64_t kExponentMask = 0x7FF0000000000000;
  static const uint64_t kSignificandMask = 0x000FFFFFFFFFFFFF;
  static const uint64_t kHiddenBit = 0x0010000000000000;
  static const int kPhysicalSignificandSize = 52;  // Excludes the hidden bit.
  static const int kSignificandSize = 53;

  Double() : d64_(0) {}
  explicit Double(double d) : d64_(double_to_uint64(d)) {}
  explicit Double(uint64_t d64) : d64_(d64) {}
  explicit Double(DiyFp diy_fp)
    : d64_(DiyFpToUint64(diy_fp)) {}

  // The value encoded by this Double must be greater or equal to +0.0.
  // It must not be special (infinity, or NaN).
  DiyFp AsDiyFp() const {
    DOUBLE_CONVERSION_ASSERT(Sign() > 0);
    DOUBLE_CONVERSION_ASSERT(!IsSpecial());
    return DiyFp(Significand(), Exponent());
  }

  // The value encoded by this Double must be strictly greater than 0.
  DiyFp AsNormalizedDiyFp() const {
    DOUBLE_CONVERSION_ASSERT(value() > 0.0);
    uint64_t f = Significand();
    int e = Exponent();

    // The current double could be a denormal.
    while ((f & kHiddenBit) == 0) {
      f <<= 1;
      e--;
    }
    // Do the final shifts in one go.
    f <<= DiyFp::kSignificandSize - kSignificandSize;
    e -= DiyFp::kSignificandSize - kSignificandSize;
    return DiyFp(f, e);
  }

  // Returns the double's bit as uint64.
  uint64_t AsUint64() const {
    return d64_;
  }

  int Exponent() const {
    if (IsDenormal()) return kDenormalExponent;

    uint64_t d64 = AsUint64();
    int biased_e =
        static_cast<int>((d64 & kExponentMask) >> kPhysicalSignificandSize);
    return biased_e - kExponentBias;
  }

  uint64_t Significand() const {
    uint64_t d64 = AsUint64();
    uint64_t significand = d64 & kSignificandMask;
    if (!IsDenormal()) {
      return significand + kHiddenBit;
    } else {
      return significand;
    }
  }

  // Returns true if the double is a denormal.
  bool IsDenormal() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == 0;
  }

  // We consider denormals not to be special.
  // Hence only Infinity and NaN are special.
  bool IsSpecial() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == kExponentMask;
  }

  bool IsNan() const {
    uint64_t d64 = AsUint64();
    return ((d64 & kExponentMask) == kExponentMask) &&
        ((d64 & kSignificandMask) != 0);
  }

  bool IsInfinite() const {
    uint64_t d64 = AsUint64();
    return ((d64 & kExponentMask) == kExponentMask) &&
        ((d64 & kSignificandMask) == 0);
  }

  int Sign() const {
    uint64_t d64 = AsUint64();
    return (d64 & kSignMask) == 0? 1: -1;
  }

  // Precondition: the value encoded by this Double must be greater or equal
  // than +0.0.
  DiyFp UpperBoundary() const {
    DOUBLE_CONVERSION_ASSERT(Sign() > 0);
    return DiyFp(Significand() * 2 + 1, Exponent() - 1);
  }

  // Computes the two boundaries of this.
  // The bigger boundary (m_plus) is normalized. The lower boundary has the same
  // exponent as m_plus.
  // Precondition: the value encoded by this Double must be greater than 0.
  void NormalizedBoundaries(DiyFp* out_m_minus, DiyFp* out_m_plus) const {
    DOUBLE_CONVERSION_ASSERT(value() > 0.0);
    DiyFp v = this->AsDiyFp();
    DiyFp m_plus = DiyFp::Normalize(DiyFp((v.f() << 1) + 1, v.e() - 1));
    DiyFp m_minus;
    if (LowerBoundaryIsCloser()) {
      m_minus = DiyFp((v.f() << 2) - 1, v.e() - 2);
    } else {
      m_minus = DiyFp((v.f() << 1) - 1, v.e() - 1);
    }
    m_minus.set_f(m_minus.f() << (m_minus.e() - m_plus.e()));
    m_minus.set_e(m_plus.e());
    *out_m_plus = m_plus;
    *out_m_minus = m_minus;
  }

  bool LowerBoundaryIsCloser() const {
    // The boundary is closer if the significand is of the form f == 2^p-1 then
    // the lower boundary is closer.
    // Think of v = 1000e10 and v- = 9999e9.
    // Then the boundary (== (v - v-)/2) is not just at a distance of 1e9 but
    // at a distance of 1e8.
    // The only exception is for the smallest normal: the largest denormal is
    // at the same distance as its successor.
    // Note: denormals have the same exponent as the smallest normals.
    bool physical_significand_is_zero = ((AsUint64() & kSignificandMask) == 0);
    return physical_significand_is_zero && (Exponent() != kDenormalExponent);
  }

  double value() const { return uint64_to_double(d64_); }

  static double Infinity() {
    return Double(kInfinity).value();
  }

  static double NaN() {
    return Double(kNaN).value();
  }

 private:
  static const int kExponentBias = 0x3FF + kPhysicalSignificandSize;
  static const int kDenormalExponent = -kExponentBias + 1;
  static const int kMaxExponent = 0x7FF - kExponentBias;
  static const uint64_t kInfinity = 0x7FF0000000000000;
  static const uint64_t kNaN = 0x7FF8000000000000;

  const uint64_t d64_;

  static uint64_t DiyFpToUint64(DiyFp diy_fp) {
    uint64_t significand = diy_fp.f();
    int exponent = diy_fp.e();
    while (significand > kHiddenBit + kSignificandMask) {
      significand >>= 1;
      exponent++;
    }
    if (exponent >= kMaxExponent) {
      return kInfinity;
    }
    if (exponent < kDenormalExponent) {
      return 0;
    }
    while (exponent > kDenormalExponent && (significand & kHiddenBit) == 0) {
      significand <<= 1;
      exponent--;
    }
    uint64_t biased_exponent;
    if (exponent == kDenormalExponent && (significand & kHiddenBit) == 0) {
      biased_exponent = 0;
    } else {
      biased_exponent = static_cast<uint64_t>(exponent + kExponentBias);
    }
    return (significand & kSignificandMask) |
        (biased_exponent << kPhysicalSignificandSize);
  }

  Double(Double const&) = delete;
  Double& operator=(Double const&) = delete;
};

} // namespace impl
} // namespace double_conversion

//==============================================================================
// fixed-dtoa
//==============================================================================

namespace double_conversion {
namespace impl {

// Represents a 128bit type. This class should be replaced by a native type on
// platforms that support 128bit integers.
class UInt128 {
 public:
  UInt128() : high_bits_(0), low_bits_(0) { }
  UInt128(uint64_t high, uint64_t low) : high_bits_(high), low_bits_(low) { }

  void Multiply(uint32_t multiplicand) {
    uint64_t accumulator;

    accumulator = (low_bits_ & kMask32) * multiplicand;
    uint32_t part = static_cast<uint32_t>(accumulator & kMask32);
    accumulator >>= 32;
    accumulator = accumulator + (low_bits_ >> 32) * multiplicand;
    low_bits_ = (accumulator << 32) + part;
    accumulator >>= 32;
    accumulator = accumulator + (high_bits_ & kMask32) * multiplicand;
    part = static_cast<uint32_t>(accumulator & kMask32);
    accumulator >>= 32;
    accumulator = accumulator + (high_bits_ >> 32) * multiplicand;
    high_bits_ = (accumulator << 32) + part;
    DOUBLE_CONVERSION_ASSERT((accumulator >> 32) == 0);
  }

  void Shift(int shift_amount) {
    DOUBLE_CONVERSION_ASSERT(-64 <= shift_amount && shift_amount <= 64);
    if (shift_amount == 0) {
      return;
    } else if (shift_amount == -64) {
      high_bits_ = low_bits_;
      low_bits_ = 0;
    } else if (shift_amount == 64) {
      low_bits_ = high_bits_;
      high_bits_ = 0;
    } else if (shift_amount <= 0) {
      high_bits_ <<= -shift_amount;
      high_bits_ += low_bits_ >> (64 + shift_amount);
      low_bits_ <<= -shift_amount;
    } else {
      low_bits_ >>= shift_amount;
      low_bits_ += high_bits_ << (64 - shift_amount);
      high_bits_ >>= shift_amount;
    }
  }

  // Modifies *this to *this MOD (2^power).
  // Returns *this DIV (2^power).
  int DivModPowerOf2(int power) {
    if (power >= 64) {
      int result = static_cast<int>(high_bits_ >> (power - 64));
      high_bits_ -= static_cast<uint64_t>(result) << (power - 64);
      return result;
    } else {
      uint64_t part_low = low_bits_ >> power;
      uint64_t part_high = high_bits_ << (64 - power);
      int result = static_cast<int>(part_low + part_high);
      high_bits_ = 0;
      low_bits_ -= part_low << power;
      return result;
    }
  }

  bool IsZero() const {
    return high_bits_ == 0 && low_bits_ == 0;
  }

  int BitAt(int position) const {
    if (position >= 64) {
      return static_cast<int>(high_bits_ >> (position - 64)) & 1;
    } else {
      return static_cast<int>(low_bits_ >> position) & 1;
    }
  }

 private:
  static const uint64_t kMask32 = 0xFFFFFFFF;
  // Value == (high_bits_ << 64) + low_bits_
  uint64_t high_bits_;
  uint64_t low_bits_;
};


static const int kDoubleSignificandSize = 53;  // Includes the hidden bit.


static void FillDigits32FixedLength(uint32_t number, int requested_length,
                                    Vector<char> buffer, int* length) {
  for (int i = requested_length - 1; i >= 0; --i) {
    buffer[(*length) + i] = static_cast<char>('0' + number % 10);
    number /= 10;
  }
  *length += requested_length;
}


static void FillDigits32(uint32_t number, Vector<char> buffer, int* length) {
  int number_length = 0;
  // We fill the digits in reverse order and exchange them afterwards.
  while (number != 0) {
    int digit = static_cast<int>(number % 10);
    number /= 10;
    buffer[(*length) + number_length] = static_cast<char>('0' + digit);
    number_length++;
  }
  // Exchange the digits.
  int i = *length;
  int j = *length + number_length - 1;
  while (i < j) {
    char tmp = buffer[i];
    buffer[i] = buffer[j];
    buffer[j] = tmp;
    i++;
    j--;
  }
  *length += number_length;
}


static void FillDigits64FixedLength(uint64_t number,
                                    Vector<char> buffer, int* length) {
  const uint32_t kTen7 = 10000000;
  // For efficiency cut the number into 3 uint32_t parts, and print those.
  uint32_t part2 = static_cast<uint32_t>(number % kTen7);
  number /= kTen7;
  uint32_t part1 = static_cast<uint32_t>(number % kTen7);
  uint32_t part0 = static_cast<uint32_t>(number / kTen7);

  FillDigits32FixedLength(part0, 3, buffer, length);
  FillDigits32FixedLength(part1, 7, buffer, length);
  FillDigits32FixedLength(part2, 7, buffer, length);
}


static void FillDigits64(uint64_t number, Vector<char> buffer, int* length) {
  const uint32_t kTen7 = 10000000;
  // For efficiency cut the number into 3 uint32_t parts, and print those.
  uint32_t part2 = static_cast<uint32_t>(number % kTen7);
  number /= kTen7;
  uint32_t part1 = static_cast<uint32_t>(number % kTen7);
  uint32_t part0 = static_cast<uint32_t>(number / kTen7);

  if (part0 != 0) {
    FillDigits32(part0, buffer, length);
    FillDigits32FixedLength(part1, 7, buffer, length);
    FillDigits32FixedLength(part2, 7, buffer, length);
  } else if (part1 != 0) {
    FillDigits32(part1, buffer, length);
    FillDigits32FixedLength(part2, 7, buffer, length);
  } else {
    FillDigits32(part2, buffer, length);
  }
}


static void RoundUp(Vector<char> buffer, int* length, int* decimal_point) {
  // An empty buffer represents 0.
  if (*length == 0) {
    buffer[0] = '1';
    *decimal_point = 1;
    *length = 1;
    return;
  }
  // Round the last digit until we either have a digit that was not '9' or until
  // we reached the first digit.
  buffer[(*length) - 1]++;
  for (int i = (*length) - 1; i > 0; --i) {
    if (buffer[i] != '0' + 10) {
      return;
    }
    buffer[i] = '0';
    buffer[i - 1]++;
  }
  // If the first digit is now '0' + 10, we would need to set it to '0' and add
  // a '1' in front. However we reach the first digit only if all following
  // digits had been '9' before rounding up. Now all trailing digits are '0' and
  // we simply switch the first digit to '1' and update the decimal-point
  // (indicating that the point is now one digit to the right).
  if (buffer[0] == '0' + 10) {
    buffer[0] = '1';
    (*decimal_point)++;
  }
}


// The given fractionals number represents a fixed-point number with binary
// point at bit (-exponent).
// Preconditions:
//   -128 <= exponent <= 0.
//   0 <= fractionals * 2^exponent < 1
//   The buffer holds the result.
// The function will round its result. During the rounding-process digits not
// generated by this function might be updated, and the decimal-point variable
// might be updated. If this function generates the digits 99 and the buffer
// already contained "199" (thus yielding a buffer of "19999") then a
// rounding-up will change the contents of the buffer to "20000".
static void FillFractionals(uint64_t fractionals, int exponent,
                            int fractional_count, Vector<char> buffer,
                            int* length, int* decimal_point) {
  DOUBLE_CONVERSION_ASSERT(-128 <= exponent && exponent <= 0);
  // 'fractionals' is a fixed-point number, with binary point at bit
  // (-exponent). Inside the function the non-converted remainder of fractionals
  // is a fixed-point number, with binary point at bit 'point'.
  if (-exponent <= 64) {
    // One 64 bit number is sufficient.
    DOUBLE_CONVERSION_ASSERT(fractionals >> 56 == 0);
    int point = -exponent;
    for (int i = 0; i < fractional_count; ++i) {
      if (fractionals == 0) break;
      // Instead of multiplying by 10 we multiply by 5 and adjust the point
      // location. This way the fractionals variable will not overflow.
      // Invariant at the beginning of the loop: fractionals < 2^point.
      // Initially we have: point <= 64 and fractionals < 2^56
      // After each iteration the point is decremented by one.
      // Note that 5^3 = 125 < 128 = 2^7.
      // Therefore three iterations of this loop will not overflow fractionals
      // (even without the subtraction at the end of the loop body). At this
      // time point will satisfy point <= 61 and therefore fractionals < 2^point
      // and any further multiplication of fractionals by 5 will not overflow.
      fractionals *= 5;
      point--;
      int digit = static_cast<int>(fractionals >> point);
      DOUBLE_CONVERSION_ASSERT(digit <= 9);
      buffer[*length] = static_cast<char>('0' + digit);
      (*length)++;
      fractionals -= static_cast<uint64_t>(digit) << point;
    }
    // If the first bit after the point is set we have to round up.
    DOUBLE_CONVERSION_ASSERT(fractionals == 0 || point - 1 >= 0);
    if ((fractionals != 0) && ((fractionals >> (point - 1)) & 1) == 1) {
      RoundUp(buffer, length, decimal_point);
    }
  } else {  // We need 128 bits.
    DOUBLE_CONVERSION_ASSERT(64 < -exponent && -exponent <= 128);
    UInt128 fractionals128 = UInt128(fractionals, 0);
    fractionals128.Shift(-exponent - 64);
    int point = 128;
    for (int i = 0; i < fractional_count; ++i) {
      if (fractionals128.IsZero()) break;
      // As before: instead of multiplying by 10 we multiply by 5 and adjust the
      // point location.
      // This multiplication will not overflow for the same reasons as before.
      fractionals128.Multiply(5);
      point--;
      int digit = fractionals128.DivModPowerOf2(point);
      DOUBLE_CONVERSION_ASSERT(digit <= 9);
      buffer[*length] = static_cast<char>('0' + digit);
      (*length)++;
    }
    if (fractionals128.BitAt(point - 1) == 1) {
      RoundUp(buffer, length, decimal_point);
    }
  }
}


// Removes leading and trailing zeros.
// If leading zeros are removed then the decimal point position is adjusted.
static void TrimZeros(Vector<char> buffer, int* length, int* decimal_point) {
  while (*length > 0 && buffer[(*length) - 1] == '0') {
    (*length)--;
  }
  int first_non_zero = 0;
  while (first_non_zero < *length && buffer[first_non_zero] == '0') {
    first_non_zero++;
  }
  if (first_non_zero != 0) {
    for (int i = first_non_zero; i < *length; ++i) {
      buffer[i - first_non_zero] = buffer[i];
    }
    *length -= first_non_zero;
    *decimal_point -= first_non_zero;
  }
}

} // namespace impl


static bool FastFixedDtoa(double v,
                   int fractional_count,
                   Vector<char> buffer,
                   int* length,
                   int* decimal_point) {
  const uint32_t kMaxUInt32 = 0xFFFFFFFF;
  uint64_t significand = impl::Double(v).Significand();
  int exponent = impl::Double(v).Exponent();
  // v = significand * 2^exponent (with significand a 53bit integer).
  // If the exponent is larger than 20 (i.e. we may have a 73bit number) then we
  // don't know how to compute the representation. 2^73 ~= 9.5*10^21.
  // If necessary this limit could probably be increased, but we don't need
  // more.
  if (exponent > 20) return false;
  if (fractional_count > 20) return false;
  *length = 0;
  // At most kDoubleSignificandSize bits of the significand are non-zero.
  // Given a 64 bit integer we have 11 0s followed by 53 potentially non-zero
  // bits:  0..11*..0xxx..53*..xx
  if (exponent + impl::kDoubleSignificandSize > 64) {
    // The exponent must be > 11.
    //
    // We know that v = significand * 2^exponent.
    // And the exponent > 11.
    // We simplify the task by dividing v by 10^17.
    // The quotient delivers the first digits, and the remainder fits into a 64
    // bit number.
    // Dividing by 10^17 is equivalent to dividing by 5^17*2^17.
    const uint64_t kFive17 = 0xB1A2BC2EC5;  // 5^17
    uint64_t divisor = kFive17;
    int divisor_power = 17;
    uint64_t dividend = significand;
    uint32_t quotient;
    uint64_t remainder;
    // Let v = f * 2^e with f == significand and e == exponent.
    // Then need q (quotient) and r (remainder) as follows:
    //   v            = q * 10^17       + r
    //   f * 2^e      = q * 10^17       + r
    //   f * 2^e      = q * 5^17 * 2^17 + r
    // If e > 17 then
    //   f * 2^(e-17) = q * 5^17        + r/2^17
    // else
    //   f  = q * 5^17 * 2^(17-e) + r/2^e
    if (exponent > divisor_power) {
      // We only allow exponents of up to 20 and therefore (17 - e) <= 3
      dividend <<= exponent - divisor_power;
      quotient = static_cast<uint32_t>(dividend / divisor);
      remainder = (dividend % divisor) << divisor_power;
    } else {
      divisor <<= divisor_power - exponent;
      quotient = static_cast<uint32_t>(dividend / divisor);
      remainder = (dividend % divisor) << exponent;
    }
    impl::FillDigits32(quotient, buffer, length);
    impl::FillDigits64FixedLength(remainder, buffer, length);
    *decimal_point = *length;
  } else if (exponent >= 0) {
    // 0 <= exponent <= 11
    significand <<= exponent;
    impl::FillDigits64(significand, buffer, length);
    *decimal_point = *length;
  } else if (exponent > -impl::kDoubleSignificandSize) {
    // We have to cut the number.
    uint64_t integrals = significand >> -exponent;
    uint64_t fractionals = significand - (integrals << -exponent);
    if (integrals > kMaxUInt32) {
      impl::FillDigits64(integrals, buffer, length);
    } else {
      impl::FillDigits32(static_cast<uint32_t>(integrals), buffer, length);
    }
    *decimal_point = *length;
    impl::FillFractionals(fractionals, exponent, fractional_count,
                    buffer, length, decimal_point);
  } else if (exponent < -128) {
    // This configuration (with at most 20 digits) means that all digits must be
    // 0.
    DOUBLE_CONVERSION_ASSERT(fractional_count <= 20);
    buffer[0] = '\0';
    *length = 0;
    *decimal_point = -fractional_count;
  } else {
    *decimal_point = 0;
    impl::FillFractionals(significand, exponent, fractional_count,
                    buffer, length, decimal_point);
  }
  impl::TrimZeros(buffer, length, decimal_point);
  buffer[*length] = '\0';
  if ((*length) == 0) {
    // The string is empty and the decimal_point thus has no importance. Mimick
    // Gay's dtoa and and set it to -fractional_count.
    *decimal_point = -fractional_count;
  }
  return true;
}

} // namespace double_conversion

//==============================================================================
// cached-powers
//==============================================================================

namespace double_conversion {
namespace impl {

class PowersOfTenCache {
 public:

  // Not all powers of ten are cached. The decimal exponent of two neighboring
  // cached numbers will differ by kDecimalExponentDistance.
  static const int kDecimalExponentDistance;

  static const int kMinDecimalExponent;
  static const int kMaxDecimalExponent;

  // Returns a cached power-of-ten with a binary exponent in the range
  // [min_exponent; max_exponent] (boundaries included).
  static void GetCachedPowerForBinaryExponentRange(int min_exponent,
                                                   int max_exponent,
                                                   DiyFp* power,
                                                   int* decimal_exponent);

  // Returns a cached power of ten x ~= 10^k such that
  //   k <= decimal_exponent < k + kCachedPowersDecimalDistance.
  // The given decimal_exponent must satisfy
  //   kMinDecimalExponent <= requested_exponent, and
  //   requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance.
  static void GetCachedPowerForDecimalExponent(int requested_exponent,
                                               DiyFp* power,
                                               int* found_exponent);
};

struct CachedPower {
  uint64_t significand;
  int16_t binary_exponent;
  int16_t decimal_exponent;
};

static const CachedPower kCachedPowers[] = {
  // clang-format off
  {0xfa8fd5a0081c0288, -1220, -348},
  {0xbaaee17fa23ebf76, -1193, -340},
  {0x8b16fb203055ac76, -1166, -332},
  {0xcf42894a5dce35ea, -1140, -324},
  {0x9a6bb0aa55653b2d, -1113, -316},
  {0xe61acf033d1a45df, -1087, -308},
  {0xab70fe17c79ac6ca, -1060, -300},
  {0xff77b1fcbebcdc4f, -1034, -292},
  {0xbe5691ef416bd60c, -1007, -284},
  {0x8dd01fad907ffc3c,  -980, -276},
  {0xd3515c2831559a83,  -954, -268},
  {0x9d71ac8fada6c9b5,  -927, -260},
  {0xea9c227723ee8bcb,  -901, -252},
  {0xaecc49914078536d,  -874, -244},
  {0x823c12795db6ce57,  -847, -236},
  {0xc21094364dfb5637,  -821, -228},
  {0x9096ea6f3848984f,  -794, -220},
  {0xd77485cb25823ac7,  -768, -212},
  {0xa086cfcd97bf97f4,  -741, -204},
  {0xef340a98172aace5,  -715, -196},
  {0xb23867fb2a35b28e,  -688, -188},
  {0x84c8d4dfd2c63f3b,  -661, -180},
  {0xc5dd44271ad3cdba,  -635, -172},
  {0x936b9fcebb25c996,  -608, -164},
  {0xdbac6c247d62a584,  -582, -156},
  {0xa3ab66580d5fdaf6,  -555, -148},
  {0xf3e2f893dec3f126,  -529, -140},
  {0xb5b5ada8aaff80b8,  -502, -132},
  {0x87625f056c7c4a8b,  -475, -124},
  {0xc9bcff6034c13053,  -449, -116},
  {0x964e858c91ba2655,  -422, -108},
  {0xdff9772470297ebd,  -396, -100},
  {0xa6dfbd9fb8e5b88f,  -369,  -92},
  {0xf8a95fcf88747d94,  -343,  -84},
  {0xb94470938fa89bcf,  -316,  -76},
  {0x8a08f0f8bf0f156b,  -289,  -68},
  {0xcdb02555653131b6,  -263,  -60},
  {0x993fe2c6d07b7fac,  -236,  -52},
  {0xe45c10c42a2b3b06,  -210,  -44},
  {0xaa242499697392d3,  -183,  -36},
  {0xfd87b5f28300ca0e,  -157,  -28},
  {0xbce5086492111aeb,  -130,  -20},
  {0x8cbccc096f5088cc,  -103,  -12},
  {0xd1b71758e219652c,   -77,   -4},
  {0x9c40000000000000,   -50,    4},
  {0xe8d4a51000000000,   -24,   12},
  {0xad78ebc5ac620000,     3,   20},
  {0x813f3978f8940984,    30,   28},
  {0xc097ce7bc90715b3,    56,   36},
  {0x8f7e32ce7bea5c70,    83,   44},
  {0xd5d238a4abe98068,   109,   52},
  {0x9f4f2726179a2245,   136,   60},
  {0xed63a231d4c4fb27,   162,   68},
  {0xb0de65388cc8ada8,   189,   76},
  {0x83c7088e1aab65db,   216,   84},
  {0xc45d1df942711d9a,   242,   92},
  {0x924d692ca61be758,   269,  100},
  {0xda01ee641a708dea,   295,  108},
  {0xa26da3999aef774a,   322,  116},
  {0xf209787bb47d6b85,   348,  124},
  {0xb454e4a179dd1877,   375,  132},
  {0x865b86925b9bc5c2,   402,  140},
  {0xc83553c5c8965d3d,   428,  148},
  {0x952ab45cfa97a0b3,   455,  156},
  {0xde469fbd99a05fe3,   481,  164},
  {0xa59bc234db398c25,   508,  172},
  {0xf6c69a72a3989f5c,   534,  180},
  {0xb7dcbf5354e9bece,   561,  188},
  {0x88fcf317f22241e2,   588,  196},
  {0xcc20ce9bd35c78a5,   614,  204},
  {0x98165af37b2153df,   641,  212},
  {0xe2a0b5dc971f303a,   667,  220},
  {0xa8d9d1535ce3b396,   694,  228},
  {0xfb9b7cd9a4a7443c,   720,  236},
  {0xbb764c4ca7a44410,   747,  244},
  {0x8bab8eefb6409c1a,   774,  252},
  {0xd01fef10a657842c,   800,  260},
  {0x9b10a4e5e9913129,   827,  268},
  {0xe7109bfba19c0c9d,   853,  276},
  {0xac2820d9623bf429,   880,  284},
  {0x80444b5e7aa7cf85,   907,  292},
  {0xbf21e44003acdd2d,   933,  300},
  {0x8e679c2f5e44ff8f,   960,  308},
  {0xd433179d9c8cb841,   986,  316},
  {0x9e19db92b4e31ba9,  1013,  324},
  {0xeb96bf6ebadf77d9,  1039,  332},
  {0xaf87023b9bf0ee6b,  1066,  340},
  // clang-format on
};

static const int kCachedPowersOffset = 348;  // -1 * the first decimal_exponent.
static const double kD_1_LOG2_10 = 0.30102999566398114;  //  1 / lg(10)
// Difference between the decimal exponents in the table above.
const int PowersOfTenCache::kDecimalExponentDistance = 8;
const int PowersOfTenCache::kMinDecimalExponent = -348;
const int PowersOfTenCache::kMaxDecimalExponent = 340;

inline void PowersOfTenCache::GetCachedPowerForBinaryExponentRange(
    int min_exponent,
    int max_exponent,
    DiyFp* power,
    int* decimal_exponent) {
  int kQ = DiyFp::kSignificandSize;
  double k = ceil((min_exponent + kQ - 1) * kD_1_LOG2_10);
  int foo = kCachedPowersOffset;
  int index =
      (foo + static_cast<int>(k) - 1) / kDecimalExponentDistance + 1;
  DOUBLE_CONVERSION_ASSERT(0 <= index && index < static_cast<int>(DOUBLE_CONVERSION_ARRAY_SIZE(kCachedPowers)));
  CachedPower cached_power = kCachedPowers[index];
  DOUBLE_CONVERSION_ASSERT(min_exponent <= cached_power.binary_exponent);
  (void) max_exponent;  // Mark variable as used.
  DOUBLE_CONVERSION_ASSERT(cached_power.binary_exponent <= max_exponent);
  *decimal_exponent = cached_power.decimal_exponent;
  *power = DiyFp(cached_power.significand, cached_power.binary_exponent);
}


inline void PowersOfTenCache::GetCachedPowerForDecimalExponent(int requested_exponent,
                                                        DiyFp* power,
                                                        int* found_exponent) {
  DOUBLE_CONVERSION_ASSERT(kMinDecimalExponent <= requested_exponent);
  DOUBLE_CONVERSION_ASSERT(requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance);
  int index =
      (requested_exponent + kCachedPowersOffset) / kDecimalExponentDistance;
  CachedPower cached_power = kCachedPowers[index];
  *power = DiyFp(cached_power.significand, cached_power.binary_exponent);
  *found_exponent = cached_power.decimal_exponent;
  DOUBLE_CONVERSION_ASSERT(*found_exponent <= requested_exponent);
  DOUBLE_CONVERSION_ASSERT(requested_exponent < *found_exponent + kDecimalExponentDistance);
}

} // namespace impl
} // namespace double_conversion

//==============================================================================
// fast-dtoa
//==============================================================================

namespace double_conversion {

enum FastDtoaMode {
  // Computes the shortest representation of the given input. The returned
  // result will be the most accurate number of this length. Longer
  // representations might be more accurate.
  FAST_DTOA_SHORTEST,
  // Computes a representation where the precision (number of digits) is
  // given as input. The precision is independent of the decimal point.
  FAST_DTOA_PRECISION
};

// FastDtoa will produce at most kFastDtoaMaximalLength digits. This does not
// include the terminating '\0' character.
static const int kFastDtoaMaximalLength = 17;
// Same for single-precision numbers.
static const int kFastDtoaMaximalSingleLength = 9;

namespace impl {

// The minimal and maximal target exponent define the range of w's binary
// exponent, where 'w' is the result of multiplying the input by a cached power
// of ten.
//
// A different range might be chosen on a different platform, to optimize digit
// generation, but a smaller range requires more powers of ten to be cached.
static const int kMinimalTargetExponent = -60;
static const int kMaximalTargetExponent = -32;


// Adjusts the last digit of the generated number, and screens out generated
// solutions that may be inaccurate. A solution may be inaccurate if it is
// outside the safe interval, or if we cannot prove that it is closer to the
// input than a neighboring representation of the same length.
//
// Input: * buffer containing the digits of too_high / 10^kappa
//        * the buffer's length
//        * distance_too_high_w == (too_high - w).f() * unit
//        * unsafe_interval == (too_high - too_low).f() * unit
//        * rest = (too_high - buffer * 10^kappa).f() * unit
//        * ten_kappa = 10^kappa * unit
//        * unit = the common multiplier
// Output: returns true if the buffer is guaranteed to contain the closest
//    representable number to the input.
//  Modifies the generated digits in the buffer to approach (round towards) w.
static bool RoundWeed(Vector<char> buffer,
                      int length,
                      uint64_t distance_too_high_w,
                      uint64_t unsafe_interval,
                      uint64_t rest,
                      uint64_t ten_kappa,
                      uint64_t unit) {
  uint64_t small_distance = distance_too_high_w - unit;
  uint64_t big_distance = distance_too_high_w + unit;
  // Let w_low  = too_high - big_distance, and
  //     w_high = too_high - small_distance.
  // Note: w_low < w < w_high
  //
  // The real w (* unit) must lie somewhere inside the interval
  // ]w_low; w_high[ (often written as "(w_low; w_high)")

  // Basically the buffer currently contains a number in the unsafe interval
  // ]too_low; too_high[ with too_low < w < too_high
  //
  //  too_high - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //                     ^v 1 unit            ^      ^                 ^      ^
  //  boundary_high ---------------------     .      .                 .      .
  //                     ^v 1 unit            .      .                 .      .
  //   - - - - - - - - - - - - - - - - - - -  +  - - + - - - - - -     .      .
  //                                          .      .         ^       .      .
  //                                          .  big_distance  .       .      .
  //                                          .      .         .       .    rest
  //                              small_distance     .         .       .      .
  //                                          v      .         .       .      .
  //  w_high - - - - - - - - - - - - - - - - - -     .         .       .      .
  //                     ^v 1 unit                   .         .       .      .
  //  w ----------------------------------------     .         .       .      .
  //                     ^v 1 unit                   v         .       .      .
  //  w_low  - - - - - - - - - - - - - - - - - - - - -         .       .      .
  //                                                           .       .      v
  //  buffer --------------------------------------------------+-------+--------
  //                                                           .       .
  //                                                  safe_interval    .
  //                                                           v       .
  //   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -     .
  //                     ^v 1 unit                                     .
  //  boundary_low -------------------------                     unsafe_interval
  //                     ^v 1 unit                                     v
  //  too_low  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //
  //
  // Note that the value of buffer could lie anywhere inside the range too_low
  // to too_high.
  //
  // boundary_low, boundary_high and w are approximations of the real boundaries
  // and v (the input number). They are guaranteed to be precise up to one unit.
  // In fact the error is guaranteed to be strictly less than one unit.
  //
  // Anything that lies outside the unsafe interval is guaranteed not to round
  // to v when read again.
  // Anything that lies inside the safe interval is guaranteed to round to v
  // when read again.
  // If the number inside the buffer lies inside the unsafe interval but not
  // inside the safe interval then we simply do not know and bail out (returning
  // false).
  //
  // Similarly we have to take into account the imprecision of 'w' when finding
  // the closest representation of 'w'. If we have two potential
  // representations, and one is closer to both w_low and w_high, then we know
  // it is closer to the actual value v.
  //
  // By generating the digits of too_high we got the largest (closest to
  // too_high) buffer that is still in the unsafe interval. In the case where
  // w_high < buffer < too_high we try to decrement the buffer.
  // This way the buffer approaches (rounds towards) w.
  // There are 3 conditions that stop the decrementation process:
  //   1) the buffer is already below w_high
  //   2) decrementing the buffer would make it leave the unsafe interval
  //   3) decrementing the buffer would yield a number below w_high and farther
  //      away than the current number. In other words:
  //              (buffer{-1} < w_high) && w_high - buffer{-1} > buffer - w_high
  // Instead of using the buffer directly we use its distance to too_high.
  // Conceptually rest ~= too_high - buffer
  // We need to do the following tests in this order to avoid over- and
  // underflows.
  DOUBLE_CONVERSION_ASSERT(rest <= unsafe_interval);
  while (rest < small_distance &&  // Negated condition 1
         unsafe_interval - rest >= ten_kappa &&  // Negated condition 2
         (rest + ten_kappa < small_distance ||  // buffer{-1} > w_high
          small_distance - rest >= rest + ten_kappa - small_distance)) {
    buffer[length - 1]--;
    rest += ten_kappa;
  }

  // We have approached w+ as much as possible. We now test if approaching w-
  // would require changing the buffer. If yes, then we have two possible
  // representations close to w, but we cannot decide which one is closer.
  if (rest < big_distance &&
      unsafe_interval - rest >= ten_kappa &&
      (rest + ten_kappa < big_distance ||
       big_distance - rest > rest + ten_kappa - big_distance)) {
    return false;
  }

  // Weeding test.
  //   The safe interval is [too_low + 2 ulp; too_high - 2 ulp]
  //   Since too_low = too_high - unsafe_interval this is equivalent to
  //      [too_high - unsafe_interval + 4 ulp; too_high - 2 ulp]
  //   Conceptually we have: rest ~= too_high - buffer
  return (2 * unit <= rest) && (rest <= unsafe_interval - 4 * unit);
}


// Rounds the buffer upwards if the result is closer to v by possibly adding
// 1 to the buffer. If the precision of the calculation is not sufficient to
// round correctly, return false.
// The rounding might shift the whole buffer in which case the kappa is
// adjusted. For example "99", kappa = 3 might become "10", kappa = 4.
//
// If 2*rest > ten_kappa then the buffer needs to be round up.
// rest can have an error of +/- 1 unit. This function accounts for the
// imprecision and returns false, if the rounding direction cannot be
// unambiguously determined.
//
// Precondition: rest < ten_kappa.
static bool RoundWeedCounted(Vector<char> buffer,
                             int length,
                             uint64_t rest,
                             uint64_t ten_kappa,
                             uint64_t unit,
                             int* kappa) {
  DOUBLE_CONVERSION_ASSERT(rest < ten_kappa);
  // The following tests are done in a specific order to avoid overflows. They
  // will work correctly with any uint64 values of rest < ten_kappa and unit.
  //
  // If the unit is too big, then we don't know which way to round. For example
  // a unit of 50 means that the real number lies within rest +/- 50. If
  // 10^kappa == 40 then there is no way to tell which way to round.
  if (unit >= ten_kappa) return false;
  // Even if unit is just half the size of 10^kappa we are already completely
  // lost. (And after the previous test we know that the expression will not
  // over/underflow.)
  if (ten_kappa - unit <= unit) return false;
  // If 2 * (rest + unit) <= 10^kappa we can safely round down.
  if ((ten_kappa - rest > rest) && (ten_kappa - 2 * rest >= 2 * unit)) {
    return true;
  }
  // If 2 * (rest - unit) >= 10^kappa, then we can safely round up.
  if ((rest > unit) && (ten_kappa - (rest - unit) <= (rest - unit))) {
    // Increment the last digit recursively until we find a non '9' digit.
    buffer[length - 1]++;
    for (int i = length - 1; i > 0; --i) {
      if (buffer[i] != '0' + 10) break;
      buffer[i] = '0';
      buffer[i - 1]++;
    }
    // If the first digit is now '0'+ 10 we had a buffer with all '9's. With the
    // exception of the first digit all digits are now '0'. Simply switch the
    // first digit to '1' and adjust the kappa. Example: "99" becomes "10" and
    // the power (the kappa) is increased.
    if (buffer[0] == '0' + 10) {
      buffer[0] = '1';
      (*kappa) += 1;
    }
    return true;
  }
  return false;
}

// Returns the biggest power of ten that is less than or equal to the given
// number. We furthermore receive the maximum number of bits 'number' has.
//
// Returns power == 10^(exponent_plus_one-1) such that
//    power <= number < power * 10.
// If number_bits == 0 then 0^(0-1) is returned.
// The number of bits must be <= 32.
// Precondition: number < (1 << (number_bits + 1)).

// Inspired by the method for finding an integer log base 10 from here:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
static unsigned int const kSmallPowersOfTen[] =
    {0, 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
     1000000000};

static void BiggestPowerTen(uint32_t number,
                            int number_bits,
                            uint32_t* power,
                            int* exponent_plus_one) {
  DOUBLE_CONVERSION_ASSERT(number < (1u << (number_bits + 1)));
  // 1233/4096 is approximately 1/lg(10).
  int exponent_plus_one_guess = ((number_bits + 1) * 1233 >> 12);
  // We increment to skip over the first entry in the kPowersOf10 table.
  // Note: kPowersOf10[i] == 10^(i-1).
  exponent_plus_one_guess++;
  // We don't have any guarantees that 2^number_bits <= number.
  if (number < kSmallPowersOfTen[exponent_plus_one_guess]) {
    exponent_plus_one_guess--;
  }
  *power = kSmallPowersOfTen[exponent_plus_one_guess];
  *exponent_plus_one = exponent_plus_one_guess;
}

// Generates the digits of input number w.
// w is a floating-point number (DiyFp), consisting of a significand and an
// exponent. Its exponent is bounded by kMinimalTargetExponent and
// kMaximalTargetExponent.
//       Hence -60 <= w.e() <= -32.
//
// Returns false if it fails, in which case the generated digits in the buffer
// should not be used.
// Preconditions:
//  * low, w and high are correct up to 1 ulp (unit in the last place). That
//    is, their error must be less than a unit of their last digits.
//  * low.e() == w.e() == high.e()
//  * low < w < high, and taking into account their error: low~ <= high~
//  * kMinimalTargetExponent <= w.e() <= kMaximalTargetExponent
// Postconditions: returns false if procedure fails.
//   otherwise:
//     * buffer is not null-terminated, but len contains the number of digits.
//     * buffer contains the shortest possible decimal digit-sequence
//       such that LOW < buffer * 10^kappa < HIGH, where LOW and HIGH are the
//       correct values of low and high (without their error).
//     * if more than one decimal representation gives the minimal number of
//       decimal digits then the one closest to W (where W is the correct value
//       of w) is chosen.
// Remark: this procedure takes into account the imprecision of its input
//   numbers. If the precision is not enough to guarantee all the postconditions
//   then false is returned. This usually happens rarely (~0.5%).
//
// Say, for the sake of example, that
//   w.e() == -48, and w.f() == 0x1234567890abcdef
// w's value can be computed by w.f() * 2^w.e()
// We can obtain w's integral digits by simply shifting w.f() by -w.e().
//  -> w's integral part is 0x1234
//  w's fractional part is therefore 0x567890abcdef.
// Printing w's integral part is easy (simply print 0x1234 in decimal).
// In order to print its fraction we repeatedly multiply the fraction by 10 and
// get each digit. Example the first digit after the point would be computed by
//   (0x567890abcdef * 10) >> 48. -> 3
// The whole thing becomes slightly more complicated because we want to stop
// once we have enough digits. That is, once the digits inside the buffer
// represent 'w' we can stop. Everything inside the interval low - high
// represents w. However we have to pay attention to low, high and w's
// imprecision.
static bool DigitGen(DiyFp low,
                     DiyFp w,
                     DiyFp high,
                     Vector<char> buffer,
                     int* length,
                     int* kappa) {
  DOUBLE_CONVERSION_ASSERT(low.e() == w.e() && w.e() == high.e());
  DOUBLE_CONVERSION_ASSERT(low.f() + 1 <= high.f() - 1);
  DOUBLE_CONVERSION_ASSERT(kMinimalTargetExponent <= w.e() && w.e() <= kMaximalTargetExponent);
  // low, w and high are imprecise, but by less than one ulp (unit in the last
  // place).
  // If we remove (resp. add) 1 ulp from low (resp. high) we are certain that
  // the new numbers are outside of the interval we want the final
  // representation to lie in.
  // Inversely adding (resp. removing) 1 ulp from low (resp. high) would yield
  // numbers that are certain to lie in the interval. We will use this fact
  // later on.
  // We will now start by generating the digits within the uncertain
  // interval. Later we will weed out representations that lie outside the safe
  // interval and thus _might_ lie outside the correct interval.
  uint64_t unit = 1;
  DiyFp too_low = DiyFp(low.f() - unit, low.e());
  DiyFp too_high = DiyFp(high.f() + unit, high.e());
  // too_low and too_high are guaranteed to lie outside the interval we want the
  // generated number in.
  DiyFp unsafe_interval = DiyFp::Minus(too_high, too_low);
  // We now cut the input number into two parts: the integral digits and the
  // fractionals. We will not write any decimal separator though, but adapt
  // kappa instead.
  // Reminder: we are currently computing the digits (stored inside the buffer)
  // such that:   too_low < buffer * 10^kappa < too_high
  // We use too_high for the digit_generation and stop as soon as possible.
  // If we stop early we effectively round down.
  DiyFp one = DiyFp(static_cast<uint64_t>(1) << -w.e(), w.e());
  // Division by one is a shift.
  uint32_t integrals = static_cast<uint32_t>(too_high.f() >> -one.e());
  // Modulo by one is an and.
  uint64_t fractionals = too_high.f() & (one.f() - 1);
  uint32_t divisor;
  int divisor_exponent_plus_one;
  BiggestPowerTen(integrals, DiyFp::kSignificandSize - (-one.e()),
                  &divisor, &divisor_exponent_plus_one);
  *kappa = divisor_exponent_plus_one;
  *length = 0;
  // Loop invariant: buffer = too_high / 10^kappa  (integer division)
  // The invariant holds for the first iteration: kappa has been initialized
  // with the divisor exponent + 1. And the divisor is the biggest power of ten
  // that is smaller than integrals.
  while (*kappa > 0) {
    int digit = static_cast<int>(integrals / divisor);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);
    buffer[*length] = static_cast<char>('0' + digit);
    (*length)++;
    integrals %= divisor;
    (*kappa)--;
    // Note that kappa now equals the exponent of the divisor and that the
    // invariant thus holds again.
    uint64_t rest =
        (static_cast<uint64_t>(integrals) << -one.e()) + fractionals;
    // Invariant: too_high = buffer * 10^kappa + DiyFp(rest, one.e())
    // Reminder: unsafe_interval.e() == one.e()
    if (rest < unsafe_interval.f()) {
      // Rounding down (by not emitting the remaining digits) yields a number
      // that lies within the unsafe interval.
      return RoundWeed(buffer, *length, DiyFp::Minus(too_high, w).f(),
                       unsafe_interval.f(), rest,
                       static_cast<uint64_t>(divisor) << -one.e(), unit);
    }
    divisor /= 10;
  }

  // The integrals have been generated. We are at the point of the decimal
  // separator. In the following loop we simply multiply the remaining digits by
  // 10 and divide by one. We just need to pay attention to multiply associated
  // data (like the interval or 'unit'), too.
  // Note that the multiplication by 10 does not overflow, because w.e >= -60
  // and thus one.e >= -60.
  DOUBLE_CONVERSION_ASSERT(one.e() >= -60);
  DOUBLE_CONVERSION_ASSERT(fractionals < one.f());
  DOUBLE_CONVERSION_ASSERT(0xFFFFFFFFFFFFFFFF / 10 >= one.f());
  for (;;) {
    fractionals *= 10;
    unit *= 10;
    unsafe_interval.set_f(unsafe_interval.f() * 10);
    // Integer division by one.
    int digit = static_cast<int>(fractionals >> -one.e());
    DOUBLE_CONVERSION_ASSERT(digit <= 9);
    buffer[*length] = static_cast<char>('0' + digit);
    (*length)++;
    fractionals &= one.f() - 1;  // Modulo by one.
    (*kappa)--;
    if (fractionals < unsafe_interval.f()) {
      return RoundWeed(buffer, *length, DiyFp::Minus(too_high, w).f() * unit,
                       unsafe_interval.f(), fractionals, one.f(), unit);
    }
  }
}



// Generates (at most) requested_digits digits of input number w.
// w is a floating-point number (DiyFp), consisting of a significand and an
// exponent. Its exponent is bounded by kMinimalTargetExponent and
// kMaximalTargetExponent.
//       Hence -60 <= w.e() <= -32.
//
// Returns false if it fails, in which case the generated digits in the buffer
// should not be used.
// Preconditions:
//  * w is correct up to 1 ulp (unit in the last place). That
//    is, its error must be strictly less than a unit of its last digit.
//  * kMinimalTargetExponent <= w.e() <= kMaximalTargetExponent
//
// Postconditions: returns false if procedure fails.
//   otherwise:
//     * buffer is not null-terminated, but length contains the number of
//       digits.
//     * the representation in buffer is the most precise representation of
//       requested_digits digits.
//     * buffer contains at most requested_digits digits of w. If there are less
//       than requested_digits digits then some trailing '0's have been removed.
//     * kappa is such that
//            w = buffer * 10^kappa + eps with |eps| < 10^kappa / 2.
//
// Remark: This procedure takes into account the imprecision of its input
//   numbers. If the precision is not enough to guarantee all the postconditions
//   then false is returned. This usually happens rarely, but the failure-rate
//   increases with higher requested_digits.
static bool DigitGenCounted(DiyFp w,
                            int requested_digits,
                            Vector<char> buffer,
                            int* length,
                            int* kappa) {
  DOUBLE_CONVERSION_ASSERT(kMinimalTargetExponent <= w.e() && w.e() <= kMaximalTargetExponent);
  DOUBLE_CONVERSION_ASSERT(kMinimalTargetExponent >= -60);
  DOUBLE_CONVERSION_ASSERT(kMaximalTargetExponent <= -32);
  // w is assumed to have an error less than 1 unit. Whenever w is scaled we
  // also scale its error.
  uint64_t w_error = 1;
  // We cut the input number into two parts: the integral digits and the
  // fractional digits. We don't emit any decimal separator, but adapt kappa
  // instead. Example: instead of writing "1.2" we put "12" into the buffer and
  // increase kappa by 1.
  DiyFp one = DiyFp(static_cast<uint64_t>(1) << -w.e(), w.e());
  // Division by one is a shift.
  uint32_t integrals = static_cast<uint32_t>(w.f() >> -one.e());
  // Modulo by one is an and.
  uint64_t fractionals = w.f() & (one.f() - 1);
  uint32_t divisor;
  int divisor_exponent_plus_one;
  BiggestPowerTen(integrals, DiyFp::kSignificandSize - (-one.e()),
                  &divisor, &divisor_exponent_plus_one);
  *kappa = divisor_exponent_plus_one;
  *length = 0;

  // Loop invariant: buffer = w / 10^kappa  (integer division)
  // The invariant holds for the first iteration: kappa has been initialized
  // with the divisor exponent + 1. And the divisor is the biggest power of ten
  // that is smaller than 'integrals'.
  while (*kappa > 0) {
    int digit = static_cast<int>(integrals / divisor);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);
    buffer[*length] = static_cast<char>('0' + digit);
    (*length)++;
    requested_digits--;
    integrals %= divisor;
    (*kappa)--;
    // Note that kappa now equals the exponent of the divisor and that the
    // invariant thus holds again.
    if (requested_digits == 0) break;
    divisor /= 10;
  }

  if (requested_digits == 0) {
    uint64_t rest =
        (static_cast<uint64_t>(integrals) << -one.e()) + fractionals;
    return RoundWeedCounted(buffer, *length, rest,
                            static_cast<uint64_t>(divisor) << -one.e(), w_error,
                            kappa);
  }

  // The integrals have been generated. We are at the point of the decimal
  // separator. In the following loop we simply multiply the remaining digits by
  // 10 and divide by one. We just need to pay attention to multiply associated
  // data (the 'unit'), too.
  // Note that the multiplication by 10 does not overflow, because w.e >= -60
  // and thus one.e >= -60.
  DOUBLE_CONVERSION_ASSERT(one.e() >= -60);
  DOUBLE_CONVERSION_ASSERT(fractionals < one.f());
  DOUBLE_CONVERSION_ASSERT(0xFFFFFFFFFFFFFFFF / 10 >= one.f());
  while (requested_digits > 0 && fractionals > w_error) {
    fractionals *= 10;
    w_error *= 10;
    // Integer division by one.
    int digit = static_cast<int>(fractionals >> -one.e());
    DOUBLE_CONVERSION_ASSERT(digit <= 9);
    buffer[*length] = static_cast<char>('0' + digit);
    (*length)++;
    requested_digits--;
    fractionals &= one.f() - 1;  // Modulo by one.
    (*kappa)--;
  }
  if (requested_digits != 0) return false;
  return RoundWeedCounted(buffer, *length, fractionals, one.f(), w_error,
                          kappa);
}


// Provides a decimal representation of v.
// Returns true if it succeeds, otherwise the result cannot be trusted.
// There will be *length digits inside the buffer (not null-terminated).
// If the function returns true then
//        v == (double) (buffer * 10^decimal_exponent).
// The digits in the buffer are the shortest representation possible: no
// 0.09999999999999999 instead of 0.1. The shorter representation will even be
// chosen even if the longer one would be closer to v.
// The last digit will be closest to the actual v. That is, even if several
// digits might correctly yield 'v' when read again, the closest will be
// computed.
static bool Grisu3(double v,
                   FastDtoaMode /*mode*/,
                   Vector<char> buffer,
                   int* length,
                   int* decimal_exponent) {
  DiyFp w = Double(v).AsNormalizedDiyFp();
  // boundary_minus and boundary_plus are the boundaries between v and its
  // closest floating-point neighbors. Any number strictly between
  // boundary_minus and boundary_plus will round to v when convert to a double.
  // Grisu3 will never output representations that lie exactly on a boundary.
  DiyFp boundary_minus, boundary_plus;
  Double(v).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  DOUBLE_CONVERSION_ASSERT(boundary_plus.e() == w.e());
  DiyFp ten_mk;  // Cached power of ten: 10^-k
  int mk;        // -k
  int ten_mk_minimal_binary_exponent =
     kMinimalTargetExponent - (w.e() + DiyFp::kSignificandSize);
  int ten_mk_maximal_binary_exponent =
     kMaximalTargetExponent - (w.e() + DiyFp::kSignificandSize);
  PowersOfTenCache::GetCachedPowerForBinaryExponentRange(
      ten_mk_minimal_binary_exponent,
      ten_mk_maximal_binary_exponent,
      &ten_mk, &mk);
  DOUBLE_CONVERSION_ASSERT((kMinimalTargetExponent <= w.e() + ten_mk.e() +
          DiyFp::kSignificandSize) &&
         (kMaximalTargetExponent >= w.e() + ten_mk.e() +
          DiyFp::kSignificandSize));
  // Note that ten_mk is only an approximation of 10^-k. A DiyFp only contains a
  // 64 bit significand and ten_mk is thus only precise up to 64 bits.

  // The DiyFp::Times procedure rounds its result, and ten_mk is approximated
  // too. The variable scaled_w (as well as scaled_boundary_minus/plus) are now
  // off by a small amount.
  // In fact: scaled_w - w*10^k < 1ulp (unit in the last place) of scaled_w.
  // In other words: let f = scaled_w.f() and e = scaled_w.e(), then
  //           (f-1) * 2^e < w*10^k < (f+1) * 2^e
  DiyFp scaled_w = DiyFp::Times(w, ten_mk);
  DOUBLE_CONVERSION_ASSERT(scaled_w.e() ==
         boundary_plus.e() + ten_mk.e() + DiyFp::kSignificandSize);
  // In theory it would be possible to avoid some recomputations by computing
  // the difference between w and boundary_minus/plus (a power of 2) and to
  // compute scaled_boundary_minus/plus by subtracting/adding from
  // scaled_w. However the code becomes much less readable and the speed
  // enhancements are not terriffic.
  DiyFp scaled_boundary_minus = DiyFp::Times(boundary_minus, ten_mk);
  DiyFp scaled_boundary_plus  = DiyFp::Times(boundary_plus,  ten_mk);

  // DigitGen will generate the digits of scaled_w. Therefore we have
  // v == (double) (scaled_w * 10^-mk).
  // Set decimal_exponent == -mk and pass it to DigitGen. If scaled_w is not an
  // integer than it will be updated. For instance if scaled_w == 1.23 then
  // the buffer will be filled with "123" und the decimal_exponent will be
  // decreased by 2.
  int kappa;
  bool result = DigitGen(scaled_boundary_minus, scaled_w, scaled_boundary_plus,
                         buffer, length, &kappa);
  *decimal_exponent = -mk + kappa;
  return result;
}


// The "counted" version of grisu3 (see above) only generates requested_digits
// number of digits. This version does not generate the shortest representation,
// and with enough requested digits 0.1 will at some point print as 0.9999999...
// Grisu3 is too imprecise for real halfway cases (1.5 will not work) and
// therefore the rounding strategy for halfway cases is irrelevant.
static bool Grisu3Counted(double v,
                          int requested_digits,
                          Vector<char> buffer,
                          int* length,
                          int* decimal_exponent) {
  DiyFp w = Double(v).AsNormalizedDiyFp();
  DiyFp ten_mk;  // Cached power of ten: 10^-k
  int mk;        // -k
  int ten_mk_minimal_binary_exponent =
     kMinimalTargetExponent - (w.e() + DiyFp::kSignificandSize);
  int ten_mk_maximal_binary_exponent =
     kMaximalTargetExponent - (w.e() + DiyFp::kSignificandSize);
  PowersOfTenCache::GetCachedPowerForBinaryExponentRange(
      ten_mk_minimal_binary_exponent,
      ten_mk_maximal_binary_exponent,
      &ten_mk, &mk);
  DOUBLE_CONVERSION_ASSERT((kMinimalTargetExponent <= w.e() + ten_mk.e() +
          DiyFp::kSignificandSize) &&
         (kMaximalTargetExponent >= w.e() + ten_mk.e() +
          DiyFp::kSignificandSize));
  // Note that ten_mk is only an approximation of 10^-k. A DiyFp only contains a
  // 64 bit significand and ten_mk is thus only precise up to 64 bits.

  // The DiyFp::Times procedure rounds its result, and ten_mk is approximated
  // too. The variable scaled_w (as well as scaled_boundary_minus/plus) are now
  // off by a small amount.
  // In fact: scaled_w - w*10^k < 1ulp (unit in the last place) of scaled_w.
  // In other words: let f = scaled_w.f() and e = scaled_w.e(), then
  //           (f-1) * 2^e < w*10^k < (f+1) * 2^e
  DiyFp scaled_w = DiyFp::Times(w, ten_mk);

  // We now have (double) (scaled_w * 10^-mk).
  // DigitGen will generate the first requested_digits digits of scaled_w and
  // return together with a kappa such that scaled_w ~= buffer * 10^kappa. (It
  // will not always be exactly the same since DigitGenCounted only produces a
  // limited number of digits.)
  int kappa;
  bool result = DigitGenCounted(scaled_w, requested_digits,
                                buffer, length, &kappa);
  *decimal_exponent = -mk + kappa;
  return result;
}

} // namespace impl


static bool FastDtoa(double v,
              FastDtoaMode mode,
              int requested_digits,
              Vector<char> buffer,
              int* length,
              int* decimal_point) {
  DOUBLE_CONVERSION_ASSERT(v > 0);
  DOUBLE_CONVERSION_ASSERT(!impl::Double(v).IsSpecial());

  bool result = false;
  int decimal_exponent = 0;
  switch (mode) {
    case FAST_DTOA_SHORTEST:
      result = impl::Grisu3(v, mode, buffer, length, &decimal_exponent);
      break;
    case FAST_DTOA_PRECISION:
      result = impl::Grisu3Counted(v, requested_digits,
                             buffer, length, &decimal_exponent);
      break;
    default:
      DOUBLE_CONVERSION_UNREACHABLE();
  }
  if (result) {
    *decimal_point = *length + decimal_exponent;
    buffer[*length] = '\0';
  }
  return result;
}

} // namespace double_conversion

//==============================================================================
// bignum
//==============================================================================

namespace double_conversion {
namespace impl {

class Bignum {
 public:
  // 3584 = 128 * 28. We can represent 2^3584 > 10^1000 accurately.
  // This bignum can encode much bigger numbers, since it contains an
  // exponent.
  static const int kMaxSignificantBits = 3584;

  Bignum();
  void AssignUInt16(uint16_t value);
  void AssignUInt64(uint64_t value);
  void AssignBignum(const Bignum& other);

  void AssignPowerUInt16(uint16_t base, int exponent);

  // Precondition: this >= other.
  void SubtractBignum(const Bignum& other);

  void Square();
  void ShiftLeft(int shift_amount);
  void MultiplyByUInt32(uint32_t factor);
  void MultiplyByUInt64(uint64_t factor);
  void Times10() { return MultiplyByUInt32(10); }
  // Pseudocode:
  //  int result = this / other;
  //  this = this % other;
  // In the worst case this function is in O(this/other).
  uint16_t DivideModuloIntBignum(const Bignum& other);

  // Returns
  //  -1 if a < b,
  //   0 if a == b, and
  //  +1 if a > b.
  static int Compare(const Bignum& a, const Bignum& b);
  static bool Equal(const Bignum& a, const Bignum& b) {
    return Compare(a, b) == 0;
  }
  static bool LessEqual(const Bignum& a, const Bignum& b) {
    return Compare(a, b) <= 0;
  }
  static bool Less(const Bignum& a, const Bignum& b) {
    return Compare(a, b) < 0;
  }
  // Returns Compare(a + b, c);
  static int PlusCompare(const Bignum& a, const Bignum& b, const Bignum& c);
  // Returns a + b == c
  static bool PlusEqual(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) == 0;
  }
  // Returns a + b <= c
  static bool PlusLessEqual(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) <= 0;
  }
  // Returns a + b < c
  static bool PlusLess(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) < 0;
  }
 private:
  typedef uint32_t Chunk;
  typedef uint64_t DoubleChunk;

  static const int kChunkSize = sizeof(Chunk) * 8;
  static const int kDoubleChunkSize = sizeof(DoubleChunk) * 8;
  // With bigit size of 28 we loose some bits, but a double still fits easily
  // into two chunks, and more importantly we can use the Comba multiplication.
  static const int kBigitSize = 28;
  static const Chunk kBigitMask = (1 << kBigitSize) - 1;
  // Every instance allocates kBigitLength chunks on the stack. Bignums cannot
  // grow. There are no checks if the stack-allocated space is sufficient.
  static const int kBigitCapacity = kMaxSignificantBits / kBigitSize;

  void EnsureCapacity(int size) {
    if (size > kBigitCapacity) {
      DOUBLE_CONVERSION_UNREACHABLE();
    }
  }
  void Align(const Bignum& other);
  void Clamp();
  bool IsClamped() const;
  void Zero();
  // Requires this to have enough capacity (no tests done).
  // Updates used_digits_ if necessary.
  // shift_amount must be < kBigitSize.
  void BigitsShiftLeft(int shift_amount);
  // BigitLength includes the "hidden" digits encoded in the exponent.
  int BigitLength() const { return used_digits_ + exponent_; }
  Chunk BigitAt(int index) const;
  void SubtractTimes(const Bignum& other, int factor);

  Chunk bigits_buffer_[kBigitCapacity];
  // A vector backed by bigits_buffer_. This way accesses to the array are
  // checked for out-of-bounds errors.
  Vector<Chunk> bigits_;
  int used_digits_;
  // The Bignum's value equals value(bigits_) * 2^(exponent_ * kBigitSize).
  int exponent_;

  Bignum(Bignum const&) = delete;
  Bignum& operator=(Bignum const&) = delete;
};


inline Bignum::Bignum()
    : bigits_(bigits_buffer_, kBigitCapacity), used_digits_(0), exponent_(0) {
  for (int i = 0; i < kBigitCapacity; ++i) {
    bigits_[i] = 0;
  }
}


template<typename S>
static int BitSize(S value) {
  (void) value;  // Mark variable as used.
  return 8 * sizeof(value);
}

// Guaranteed to lie in one Bigit.
inline void Bignum::AssignUInt16(uint16_t value) {
  DOUBLE_CONVERSION_ASSERT(kBigitSize >= BitSize(value));
  Zero();
  if (value == 0) return;

  EnsureCapacity(1);
  bigits_[0] = value;
  used_digits_ = 1;
}


inline void Bignum::AssignUInt64(uint64_t value) {
  const int kUInt64Size = 64;

  Zero();
  if (value == 0) return;

  int needed_bigits = kUInt64Size / kBigitSize + 1;
  EnsureCapacity(needed_bigits);
  for (int i = 0; i < needed_bigits; ++i) {
    bigits_[i] = value & kBigitMask;
    value = value >> kBigitSize;
  }
  used_digits_ = needed_bigits;
  Clamp();
}


inline void Bignum::AssignBignum(const Bignum& other) {
  exponent_ = other.exponent_;
  for (int i = 0; i < other.used_digits_; ++i) {
    bigits_[i] = other.bigits_[i];
  }
  // Clear the excess digits (if there were any).
  for (int i = other.used_digits_; i < used_digits_; ++i) {
    bigits_[i] = 0;
  }
  used_digits_ = other.used_digits_;
}


inline void Bignum::SubtractBignum(const Bignum& other) {
  DOUBLE_CONVERSION_ASSERT(IsClamped());
  DOUBLE_CONVERSION_ASSERT(other.IsClamped());
  // We require this to be bigger than other.
  DOUBLE_CONVERSION_ASSERT(LessEqual(other, *this));

  Align(other);

  int offset = other.exponent_ - exponent_;
  Chunk borrow = 0;
  int i;
  for (i = 0; i < other.used_digits_; ++i) {
    DOUBLE_CONVERSION_ASSERT((borrow == 0) || (borrow == 1));
    Chunk difference = bigits_[i + offset] - other.bigits_[i] - borrow;
    bigits_[i + offset] = difference & kBigitMask;
    borrow = difference >> (kChunkSize - 1);
  }
  while (borrow != 0) {
    Chunk difference = bigits_[i + offset] - borrow;
    bigits_[i + offset] = difference & kBigitMask;
    borrow = difference >> (kChunkSize - 1);
    ++i;
  }
  Clamp();
}


inline void Bignum::ShiftLeft(int shift_amount) {
  if (used_digits_ == 0) return;
  exponent_ += shift_amount / kBigitSize;
  int local_shift = shift_amount % kBigitSize;
  EnsureCapacity(used_digits_ + 1);
  BigitsShiftLeft(local_shift);
}


inline void Bignum::MultiplyByUInt32(uint32_t factor) {
  if (factor == 1) return;
  if (factor == 0) {
    Zero();
    return;
  }
  if (used_digits_ == 0) return;

  // The product of a bigit with the factor is of size kBigitSize + 32.
  // Assert that this number + 1 (for the carry) fits into double chunk.
  DOUBLE_CONVERSION_ASSERT(kDoubleChunkSize >= kBigitSize + 32 + 1);
  DoubleChunk carry = 0;
  for (int i = 0; i < used_digits_; ++i) {
    DoubleChunk product = static_cast<DoubleChunk>(factor) * bigits_[i] + carry;
    bigits_[i] = static_cast<Chunk>(product & kBigitMask);
    carry = (product >> kBigitSize);
  }
  while (carry != 0) {
    EnsureCapacity(used_digits_ + 1);
    bigits_[used_digits_] = carry & kBigitMask;
    used_digits_++;
    carry >>= kBigitSize;
  }
}


inline void Bignum::MultiplyByUInt64(uint64_t factor) {
  if (factor == 1) return;
  if (factor == 0) {
    Zero();
    return;
  }
  DOUBLE_CONVERSION_ASSERT(kBigitSize < 32);
  uint64_t carry = 0;
  uint64_t low = factor & 0xFFFFFFFF;
  uint64_t high = factor >> 32;
  for (int i = 0; i < used_digits_; ++i) {
    uint64_t product_low = low * bigits_[i];
    uint64_t product_high = high * bigits_[i];
    uint64_t tmp = (carry & kBigitMask) + product_low;
    bigits_[i] = tmp & kBigitMask;
    carry = (carry >> kBigitSize) + (tmp >> kBigitSize) +
        (product_high << (32 - kBigitSize));
  }
  while (carry != 0) {
    EnsureCapacity(used_digits_ + 1);
    bigits_[used_digits_] = carry & kBigitMask;
    used_digits_++;
    carry >>= kBigitSize;
  }
}


inline void Bignum::Square() {
  DOUBLE_CONVERSION_ASSERT(IsClamped());
  int product_length = 2 * used_digits_;
  EnsureCapacity(product_length);

  // Comba multiplication: compute each column separately.
  // Example: r = a2a1a0 * b2b1b0.
  //    r =  1    * a0b0 +
  //        10    * (a1b0 + a0b1) +
  //        100   * (a2b0 + a1b1 + a0b2) +
  //        1000  * (a2b1 + a1b2) +
  //        10000 * a2b2
  //
  // In the worst case we have to accumulate nb-digits products of digit*digit.
  //
  // Assert that the additional number of bits in a DoubleChunk are enough to
  // sum up used_digits of Bigit*Bigit.
  if ((1 << (2 * (kChunkSize - kBigitSize))) <= used_digits_) {
    DOUBLE_CONVERSION_UNIMPLEMENTED();
  }
  DoubleChunk accumulator = 0;
  // First shift the digits so we don't overwrite them.
  int copy_offset = used_digits_;
  for (int i = 0; i < used_digits_; ++i) {
    bigits_[copy_offset + i] = bigits_[i];
  }
  // We have two loops to avoid some 'if's in the loop.
  for (int i = 0; i < used_digits_; ++i) {
    // Process temporary digit i with power i.
    // The sum of the two indices must be equal to i.
    int bigit_index1 = i;
    int bigit_index2 = 0;
    // Sum all of the sub-products.
    while (bigit_index1 >= 0) {
      Chunk chunk1 = bigits_[copy_offset + bigit_index1];
      Chunk chunk2 = bigits_[copy_offset + bigit_index2];
      accumulator += static_cast<DoubleChunk>(chunk1) * chunk2;
      bigit_index1--;
      bigit_index2++;
    }
    bigits_[i] = static_cast<Chunk>(accumulator) & kBigitMask;
    accumulator >>= kBigitSize;
  }
  for (int i = used_digits_; i < product_length; ++i) {
    int bigit_index1 = used_digits_ - 1;
    int bigit_index2 = i - bigit_index1;
    // Invariant: sum of both indices is again equal to i.
    // Inner loop runs 0 times on last iteration, emptying accumulator.
    while (bigit_index2 < used_digits_) {
      Chunk chunk1 = bigits_[copy_offset + bigit_index1];
      Chunk chunk2 = bigits_[copy_offset + bigit_index2];
      accumulator += static_cast<DoubleChunk>(chunk1) * chunk2;
      bigit_index1--;
      bigit_index2++;
    }
    // The overwritten bigits_[i] will never be read in further loop iterations,
    // because bigit_index1 and bigit_index2 are always greater
    // than i - used_digits_.
    bigits_[i] = static_cast<Chunk>(accumulator) & kBigitMask;
    accumulator >>= kBigitSize;
  }
  // Since the result was guaranteed to lie inside the number the
  // accumulator must be 0 now.
  DOUBLE_CONVERSION_ASSERT(accumulator == 0);

  // Don't forget to update the used_digits and the exponent.
  used_digits_ = product_length;
  exponent_ *= 2;
  Clamp();
}


inline void Bignum::AssignPowerUInt16(uint16_t base, int power_exponent) {
  DOUBLE_CONVERSION_ASSERT(base != 0);
  DOUBLE_CONVERSION_ASSERT(power_exponent >= 0);
  if (power_exponent == 0) {
    AssignUInt16(1);
    return;
  }
  Zero();
  int shifts = 0;
  // We expect base to be in range 2-32, and most often to be 10.
  // It does not make much sense to implement different algorithms for counting
  // the bits.
  while ((base & 1) == 0) {
    base = static_cast<uint16_t>(base >> 1);
    shifts++;
  }
  int bit_size = 0;
  int tmp_base = base;
  while (tmp_base != 0) {
    tmp_base >>= 1;
    bit_size++;
  }
  int final_size = bit_size * power_exponent;
  // 1 extra bigit for the shifting, and one for rounded final_size.
  EnsureCapacity(final_size / kBigitSize + 2);

  // Left to Right exponentiation.
  int mask = 1;
  while (power_exponent >= mask) mask <<= 1;

  // The mask is now pointing to the bit above the most significant 1-bit of
  // power_exponent.
  // Get rid of first 1-bit;
  mask >>= 2;
  uint64_t this_value = base;

  bool delayed_multipliciation = false;
  const uint64_t max_32bits = 0xFFFFFFFF;
  while (mask != 0 && this_value <= max_32bits) {
    this_value = this_value * this_value;
    // Verify that there is enough space in this_value to perform the
    // multiplication.  The first bit_size bits must be 0.
    if ((power_exponent & mask) != 0) {
      uint64_t base_bits_mask =
          ~((static_cast<uint64_t>(1) << (64 - bit_size)) - 1);
      bool high_bits_zero = (this_value & base_bits_mask) == 0;
      if (high_bits_zero) {
        this_value *= base;
      } else {
        delayed_multipliciation = true;
      }
    }
    mask >>= 1;
  }
  AssignUInt64(this_value);
  if (delayed_multipliciation) {
    MultiplyByUInt32(base);
  }

  // Now do the same thing as a bignum.
  while (mask != 0) {
    Square();
    if ((power_exponent & mask) != 0) {
      MultiplyByUInt32(base);
    }
    mask >>= 1;
  }

  // And finally add the saved shifts.
  ShiftLeft(shifts * power_exponent);
}


// Precondition: this/other < 16bit.
inline uint16_t Bignum::DivideModuloIntBignum(const Bignum& other) {
  DOUBLE_CONVERSION_ASSERT(IsClamped());
  DOUBLE_CONVERSION_ASSERT(other.IsClamped());
  DOUBLE_CONVERSION_ASSERT(other.used_digits_ > 0);

  // Easy case: if we have less digits than the divisor than the result is 0.
  // Note: this handles the case where this == 0, too.
  if (BigitLength() < other.BigitLength()) {
    return 0;
  }

  Align(other);

  uint16_t result = 0;

  // Start by removing multiples of 'other' until both numbers have the same
  // number of digits.
  while (BigitLength() > other.BigitLength()) {
    // This naive approach is extremely inefficient if `this` divided by other
    // is big. This function is implemented for doubleToString where
    // the result should be small (less than 10).
    DOUBLE_CONVERSION_ASSERT(other.bigits_[other.used_digits_ - 1] >= ((1 << kBigitSize) / 16));
    DOUBLE_CONVERSION_ASSERT(bigits_[used_digits_ - 1] < 0x10000);
    // Remove the multiples of the first digit.
    // Example this = 23 and other equals 9. -> Remove 2 multiples.
    result = static_cast<uint16_t>(result + bigits_[used_digits_ - 1]);
    SubtractTimes(other, static_cast<int>(bigits_[used_digits_ - 1]));
  }

  DOUBLE_CONVERSION_ASSERT(BigitLength() == other.BigitLength());

  // Both bignums are at the same length now.
  // Since other has more than 0 digits we know that the access to
  // bigits_[used_digits_ - 1] is safe.
  Chunk this_bigit = bigits_[used_digits_ - 1];
  Chunk other_bigit = other.bigits_[other.used_digits_ - 1];

  if (other.used_digits_ == 1) {
    // Shortcut for easy (and common) case.
    Chunk quotient = this_bigit / other_bigit;
    bigits_[used_digits_ - 1] = this_bigit - other_bigit * quotient;
    DOUBLE_CONVERSION_ASSERT(quotient < 0x10000);
    result = static_cast<uint16_t>(result + quotient);
    Clamp();
    return result;
  }

  Chunk division_estimate = this_bigit / (other_bigit + 1);
  DOUBLE_CONVERSION_ASSERT(division_estimate < 0x10000);
  result = static_cast<uint16_t>(result + division_estimate);
  SubtractTimes(other, static_cast<int>(division_estimate));

  if (other_bigit * (division_estimate + 1) > this_bigit) {
    // No need to even try to subtract. Even if other's remaining digits were 0
    // another subtraction would be too much.
    return result;
  }

  while (LessEqual(other, *this)) {
    SubtractBignum(other);
    result++;
  }
  return result;
}


inline Bignum::Chunk Bignum::BigitAt(int index) const {
  if (index >= BigitLength()) return 0;
  if (index < exponent_) return 0;
  return bigits_[index - exponent_];
}


inline int Bignum::Compare(const Bignum& a, const Bignum& b) {
  DOUBLE_CONVERSION_ASSERT(a.IsClamped());
  DOUBLE_CONVERSION_ASSERT(b.IsClamped());
  int bigit_length_a = a.BigitLength();
  int bigit_length_b = b.BigitLength();
  if (bigit_length_a < bigit_length_b) return -1;
  if (bigit_length_a > bigit_length_b) return +1;
  for (int i = bigit_length_a - 1; i >= Min(a.exponent_, b.exponent_); --i) {
    Chunk bigit_a = a.BigitAt(i);
    Chunk bigit_b = b.BigitAt(i);
    if (bigit_a < bigit_b) return -1;
    if (bigit_a > bigit_b) return +1;
    // Otherwise they are equal up to this digit. Try the next digit.
  }
  return 0;
}


inline int Bignum::PlusCompare(const Bignum& a, const Bignum& b, const Bignum& c) {
  DOUBLE_CONVERSION_ASSERT(a.IsClamped());
  DOUBLE_CONVERSION_ASSERT(b.IsClamped());
  DOUBLE_CONVERSION_ASSERT(c.IsClamped());
  if (a.BigitLength() < b.BigitLength()) {
    return PlusCompare(b, a, c);
  }
  if (a.BigitLength() + 1 < c.BigitLength()) return -1;
  if (a.BigitLength() > c.BigitLength()) return +1;
  // The exponent encodes 0-bigits. So if there are more 0-digits in 'a' than
  // 'b' has digits, then the bigit-length of 'a'+'b' must be equal to the one
  // of 'a'.
  if (a.exponent_ >= b.BigitLength() && a.BigitLength() < c.BigitLength()) {
    return -1;
  }

  Chunk borrow = 0;
  // Starting at min_exponent all digits are == 0. So no need to compare them.
  int min_exponent = Min(Min(a.exponent_, b.exponent_), c.exponent_);
  for (int i = c.BigitLength() - 1; i >= min_exponent; --i) {
    Chunk chunk_a = a.BigitAt(i);
    Chunk chunk_b = b.BigitAt(i);
    Chunk chunk_c = c.BigitAt(i);
    Chunk sum = chunk_a + chunk_b;
    if (sum > chunk_c + borrow) {
      return +1;
    } else {
      borrow = chunk_c + borrow - sum;
      if (borrow > 1) return -1;
      borrow <<= kBigitSize;
    }
  }
  if (borrow == 0) return 0;
  return -1;
}


inline void Bignum::Clamp() {
  while (used_digits_ > 0 && bigits_[used_digits_ - 1] == 0) {
    used_digits_--;
  }
  if (used_digits_ == 0) {
    // Zero.
    exponent_ = 0;
  }
}


inline bool Bignum::IsClamped() const {
  return used_digits_ == 0 || bigits_[used_digits_ - 1] != 0;
}


inline void Bignum::Zero() {
  for (int i = 0; i < used_digits_; ++i) {
    bigits_[i] = 0;
  }
  used_digits_ = 0;
  exponent_ = 0;
}


inline void Bignum::Align(const Bignum& other) {
  if (exponent_ > other.exponent_) {
    // If "X" represents a "hidden" digit (by the exponent) then we are in the
    // following case (a == this, b == other):
    // a:  aaaaaaXXXX   or a:   aaaaaXXX
    // b:     bbbbbbX      b: bbbbbbbbXX
    // We replace some of the hidden digits (X) of a with 0 digits.
    // a:  aaaaaa000X   or a:   aaaaa0XX
    int zero_digits = exponent_ - other.exponent_;
    EnsureCapacity(used_digits_ + zero_digits);
    for (int i = used_digits_ - 1; i >= 0; --i) {
      bigits_[i + zero_digits] = bigits_[i];
    }
    for (int i = 0; i < zero_digits; ++i) {
      bigits_[i] = 0;
    }
    used_digits_ += zero_digits;
    exponent_ -= zero_digits;
    DOUBLE_CONVERSION_ASSERT(used_digits_ >= 0);
    DOUBLE_CONVERSION_ASSERT(exponent_ >= 0);
  }
}


inline void Bignum::BigitsShiftLeft(int shift_amount) {
  DOUBLE_CONVERSION_ASSERT(shift_amount < kBigitSize);
  DOUBLE_CONVERSION_ASSERT(shift_amount >= 0);
  Chunk carry = 0;
  for (int i = 0; i < used_digits_; ++i) {
    Chunk new_carry = bigits_[i] >> (kBigitSize - shift_amount);
    bigits_[i] = ((bigits_[i] << shift_amount) + carry) & kBigitMask;
    carry = new_carry;
  }
  if (carry != 0) {
    bigits_[used_digits_] = carry;
    used_digits_++;
  }
}


inline void Bignum::SubtractTimes(const Bignum& other, int factor) {
  DOUBLE_CONVERSION_ASSERT(exponent_ <= other.exponent_);
  if (factor < 3) {
    for (int i = 0; i < factor; ++i) {
      SubtractBignum(other);
    }
    return;
  }
  Chunk borrow = 0;
  int exponent_diff = other.exponent_ - exponent_;
  for (int i = 0; i < other.used_digits_; ++i) {
    DoubleChunk product = static_cast<DoubleChunk>(factor) * other.bigits_[i];
    DoubleChunk remove = borrow + product;
    Chunk difference = static_cast<Chunk>(bigits_[i + exponent_diff] - (remove & kBigitMask));
    bigits_[i + exponent_diff] = difference & kBigitMask;
    borrow = static_cast<Chunk>((difference >> (kChunkSize - 1)) +
                                (remove >> kBigitSize));
  }
  for (int i = other.used_digits_ + exponent_diff; i < used_digits_; ++i) {
    if (borrow == 0) return;
    Chunk difference = bigits_[i] - borrow;
    bigits_[i] = difference & kBigitMask;
    borrow = difference >> (kChunkSize - 1);
  }
  Clamp();
}


} // namespace impl
} // namespace double_conversion

//==============================================================================
// bignum-dtoa
//==============================================================================

namespace double_conversion {

enum BignumDtoaMode {
  // Return the shortest correct representation.
  // For example the output of 0.299999999999999988897 is (the less accurate but
  // correct) 0.3.
  BIGNUM_DTOA_SHORTEST,
  // Return a fixed number of digits after the decimal point.
  // For instance fixed(0.1, 4) becomes 0.1000
  // If the input number is big, the output will be big.
  BIGNUM_DTOA_FIXED,
  // Return a fixed number of digits, no matter what the exponent is.
  BIGNUM_DTOA_PRECISION
};

namespace impl {

static int NormalizedExponent(uint64_t significand, int exponent) {
  DOUBLE_CONVERSION_ASSERT(significand != 0);
  while ((significand & Double::kHiddenBit) == 0) {
    significand = significand << 1;
    exponent = exponent - 1;
  }
  return exponent;
}


// Forward declarations:
// Returns an estimation of k such that 10^(k-1) <= v < 10^k.
static int EstimatePower(int exponent);
// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator.
static void InitialScaledStartValues(uint64_t significand,
                                     int exponent,
                                     bool lower_boundary_is_closer,
                                     int estimated_power,
                                     bool need_boundary_deltas,
                                     Bignum* numerator,
                                     Bignum* denominator,
                                     Bignum* delta_minus,
                                     Bignum* delta_plus);
// Multiplies numerator/denominator so that its values lies in the range 1-10.
// Returns decimal_point s.t.
//  v = numerator'/denominator' * 10^(decimal_point-1)
//     where numerator' and denominator' are the values of numerator and
//     denominator after the call to this function.
static void FixupMultiply10(int estimated_power, bool is_even,
                            int* decimal_point,
                            Bignum* numerator, Bignum* denominator,
                            Bignum* delta_minus, Bignum* delta_plus);
// Generates digits from the left to the right and stops when the generated
// digits yield the shortest decimal representation of v.
static void GenerateShortestDigits(Bignum* numerator, Bignum* denominator,
                                   Bignum* delta_minus, Bignum* delta_plus,
                                   bool is_even,
                                   Vector<char> buffer, int* length);
// Generates 'requested_digits' after the decimal point.
static void BignumToFixed(int requested_digits, int* decimal_point,
                          Bignum* numerator, Bignum* denominator,
                          Vector<char>(buffer), int* length);
// Generates 'count' digits of numerator/denominator.
// Once 'count' digits have been produced rounds the result depending on the
// remainder (remainders of exactly .5 round upwards). Might update the
// decimal_point when rounding up (for example for 0.9999).
static void GenerateCountedDigits(int count, int* decimal_point,
                                  Bignum* numerator, Bignum* denominator,
                                  Vector<char> buffer, int* length);

} // namespace impl


static void BignumDtoa(double v, BignumDtoaMode mode, int requested_digits,
                Vector<char> buffer, int* length, int* decimal_point) {
  DOUBLE_CONVERSION_ASSERT(v > 0);
  DOUBLE_CONVERSION_ASSERT(!impl::Double(v).IsSpecial());
  uint64_t significand;
  int exponent;
  bool lower_boundary_is_closer;
  significand = impl::Double(v).Significand();
  exponent = impl::Double(v).Exponent();
  lower_boundary_is_closer = impl::Double(v).LowerBoundaryIsCloser();
  bool need_boundary_deltas =
      (mode == BIGNUM_DTOA_SHORTEST);

  bool is_even = (significand & 1) == 0;
  int normalized_exponent = impl::NormalizedExponent(significand, exponent);
  // estimated_power might be too low by 1.
  int estimated_power = impl::EstimatePower(normalized_exponent);

  // Shortcut for Fixed.
  // The requested digits correspond to the digits after the point. If the
  // number is much too small, then there is no need in trying to get any
  // digits.
  if (mode == BIGNUM_DTOA_FIXED && -estimated_power - 1 > requested_digits) {
    buffer[0] = '\0';
    *length = 0;
    // Set decimal-point to -requested_digits. This is what Gay does.
    // Note that it should not have any effect anyways since the string is
    // empty.
    *decimal_point = -requested_digits;
    return;
  }

  impl::Bignum numerator;
  impl::Bignum denominator;
  impl::Bignum delta_minus;
  impl::Bignum delta_plus;
  // Make sure the bignum can grow large enough. The smallest double equals
  // 4e-324. In this case the denominator needs fewer than 324*4 binary digits.
  // The maximum double is 1.7976931348623157e308 which needs fewer than
  // 308*4 binary digits.
  DOUBLE_CONVERSION_ASSERT(impl::Bignum::kMaxSignificantBits >= 324*4);
  impl::InitialScaledStartValues(significand, exponent, lower_boundary_is_closer,
                           estimated_power, need_boundary_deltas,
                           &numerator, &denominator,
                           &delta_minus, &delta_plus);
  // We now have v = (numerator / denominator) * 10^estimated_power.
  impl::FixupMultiply10(estimated_power, is_even, decimal_point,
                  &numerator, &denominator,
                  &delta_minus, &delta_plus);
  // We now have v = (numerator / denominator) * 10^(decimal_point-1), and
  //  1 <= (numerator + delta_plus) / denominator < 10
  switch (mode) {
    case BIGNUM_DTOA_SHORTEST:
      impl::GenerateShortestDigits(&numerator, &denominator,
                             &delta_minus, &delta_plus,
                             is_even, buffer, length);
      break;
    case BIGNUM_DTOA_FIXED:
      impl::BignumToFixed(requested_digits, decimal_point,
                    &numerator, &denominator,
                    buffer, length);
      break;
    case BIGNUM_DTOA_PRECISION:
      impl::GenerateCountedDigits(requested_digits, decimal_point,
                            &numerator, &denominator,
                            buffer, length);
      break;
    default:
      DOUBLE_CONVERSION_UNREACHABLE();
  }
  buffer[*length] = '\0';
}

namespace impl {

// The procedure starts generating digits from the left to the right and stops
// when the generated digits yield the shortest decimal representation of v. A
// decimal representation of v is a number lying closer to v than to any other
// double, so it converts to v when read.
//
// This is true if d, the decimal representation, is between m- and m+, the
// upper and lower boundaries. d must be strictly between them if !is_even.
//           m- := (numerator - delta_minus) / denominator
//           m+ := (numerator + delta_plus) / denominator
//
// Precondition: 0 <= (numerator+delta_plus) / denominator < 10.
//   If 1 <= (numerator+delta_plus) / denominator < 10 then no leading 0 digit
//   will be produced. This should be the standard precondition.
static void GenerateShortestDigits(Bignum* numerator, Bignum* denominator,
                                   Bignum* delta_minus, Bignum* delta_plus,
                                   bool is_even,
                                   Vector<char> buffer, int* length) {
  // Small optimization: if delta_minus and delta_plus are the same just reuse
  // one of the two bignums.
  if (Bignum::Equal(*delta_minus, *delta_plus)) {
    delta_plus = delta_minus;
  }
  *length = 0;
  for (;;) {
    uint16_t digit;
    digit = numerator->DivideModuloIntBignum(*denominator);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always positive.
    // digit = numerator / denominator (integer division).
    // numerator = numerator % denominator.
    buffer[(*length)++] = static_cast<char>(digit + '0');

    // Can we stop already?
    // If the remainder of the division is less than the distance to the lower
    // boundary we can stop. In this case we simply round down (discarding the
    // remainder).
    // Similarly we test if we can round up (using the upper boundary).
    bool in_delta_room_minus;
    bool in_delta_room_plus;
    if (is_even) {
      in_delta_room_minus = Bignum::LessEqual(*numerator, *delta_minus);
    } else {
      in_delta_room_minus = Bignum::Less(*numerator, *delta_minus);
    }
    if (is_even) {
      in_delta_room_plus =
          Bignum::PlusCompare(*numerator, *delta_plus, *denominator) >= 0;
    } else {
      in_delta_room_plus =
          Bignum::PlusCompare(*numerator, *delta_plus, *denominator) > 0;
    }
    if (!in_delta_room_minus && !in_delta_room_plus) {
      // Prepare for next iteration.
      numerator->Times10();
      delta_minus->Times10();
      // We optimized delta_plus to be equal to delta_minus (if they share the
      // same value). So don't multiply delta_plus if they point to the same
      // object.
      if (delta_minus != delta_plus) {
        delta_plus->Times10();
      }
    } else if (in_delta_room_minus && in_delta_room_plus) {
      // Let's see if 2*numerator < denominator.
      // If yes, then the next digit would be < 5 and we can round down.
      int compare = Bignum::PlusCompare(*numerator, *numerator, *denominator);
      if (compare < 0) {
        // Remaining digits are less than .5. -> Round down (== do nothing).
      } else if (compare > 0) {
        // Remaining digits are more than .5 of denominator. -> Round up.
        // Note that the last digit could not be a '9' as otherwise the whole
        // loop would have stopped earlier.
        // We still have an assert here in case the preconditions were not
        // satisfied.
        DOUBLE_CONVERSION_ASSERT(buffer[(*length) - 1] != '9');
        buffer[(*length) - 1]++;
      } else {
        // Halfway case.
        // TODO(floitsch): need a way to solve half-way cases.
        //   For now let's round towards even (since this is what Gay seems to
        //   do).

        if ((buffer[(*length) - 1] - '0') % 2 == 0) {
          // Round down => Do nothing.
        } else {
          DOUBLE_CONVERSION_ASSERT(buffer[(*length) - 1] != '9');
          buffer[(*length) - 1]++;
        }
      }
      return;
    } else if (in_delta_room_minus) {
      // Round down (== do nothing).
      return;
    } else {  // in_delta_room_plus
      // Round up.
      // Note again that the last digit could not be '9' since this would have
      // stopped the loop earlier.
      // We still have an ASSERT here, in case the preconditions were not
      // satisfied.
      DOUBLE_CONVERSION_ASSERT(buffer[(*length) -1] != '9');
      buffer[(*length) - 1]++;
      return;
    }
  }
}


// Let v = numerator / denominator < 10.
// Then we generate 'count' digits of d = x.xxxxx... (without the decimal point)
// from left to right. Once 'count' digits have been produced we decide wether
// to round up or down. Remainders of exactly .5 round upwards. Numbers such
// as 9.999999 propagate a carry all the way, and change the
// exponent (decimal_point), when rounding upwards.
static void GenerateCountedDigits(int count, int* decimal_point,
                                  Bignum* numerator, Bignum* denominator,
                                  Vector<char> buffer, int* length) {
  DOUBLE_CONVERSION_ASSERT(count >= 0);
  for (int i = 0; i < count - 1; ++i) {
    uint16_t digit;
    digit = numerator->DivideModuloIntBignum(*denominator);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always positive.
    // digit = numerator / denominator (integer division).
    // numerator = numerator % denominator.
    buffer[i] = static_cast<char>(digit + '0');
    // Prepare for next iteration.
    numerator->Times10();
  }
  // Generate the last digit.
  uint16_t digit;
  digit = numerator->DivideModuloIntBignum(*denominator);
  if (Bignum::PlusCompare(*numerator, *numerator, *denominator) >= 0) {
    digit++;
  }
  DOUBLE_CONVERSION_ASSERT(digit <= 10);
  buffer[count - 1] = static_cast<char>(digit + '0');
  // Correct bad digits (in case we had a sequence of '9's). Propagate the
  // carry until we hat a non-'9' or til we reach the first digit.
  for (int i = count - 1; i > 0; --i) {
    if (buffer[i] != '0' + 10) break;
    buffer[i] = '0';
    buffer[i - 1]++;
  }
  if (buffer[0] == '0' + 10) {
    // Propagate a carry past the top place.
    buffer[0] = '1';
    (*decimal_point)++;
  }
  *length = count;
}


// Generates 'requested_digits' after the decimal point. It might omit
// trailing '0's. If the input number is too small then no digits at all are
// generated (ex.: 2 fixed digits for 0.00001).
//
// Input verifies:  1 <= (numerator + delta) / denominator < 10.
static void BignumToFixed(int requested_digits, int* decimal_point,
                          Bignum* numerator, Bignum* denominator,
                          Vector<char>(buffer), int* length) {
  // Note that we have to look at more than just the requested_digits, since
  // a number could be rounded up. Example: v=0.5 with requested_digits=0.
  // Even though the power of v equals 0 we can't just stop here.
  if (-(*decimal_point) > requested_digits) {
    // The number is definitively too small.
    // Ex: 0.001 with requested_digits == 1.
    // Set decimal-point to -requested_digits. This is what Gay does.
    // Note that it should not have any effect anyways since the string is
    // empty.
    *decimal_point = -requested_digits;
    *length = 0;
    return;
  } else if (-(*decimal_point) == requested_digits) {
    // We only need to verify if the number rounds down or up.
    // Ex: 0.04 and 0.06 with requested_digits == 1.
    DOUBLE_CONVERSION_ASSERT(*decimal_point == -requested_digits);
    // Initially the fraction lies in range (1, 10]. Multiply the denominator
    // by 10 so that we can compare more easily.
    denominator->Times10();
    if (Bignum::PlusCompare(*numerator, *numerator, *denominator) >= 0) {
      // If the fraction is >= 0.5 then we have to include the rounded
      // digit.
      buffer[0] = '1';
      *length = 1;
      (*decimal_point)++;
    } else {
      // Note that we caught most of similar cases earlier.
      *length = 0;
    }
    return;
  } else {
    // The requested digits correspond to the digits after the point.
    // The variable 'needed_digits' includes the digits before the point.
    int needed_digits = (*decimal_point) + requested_digits;
    GenerateCountedDigits(needed_digits, decimal_point,
                          numerator, denominator,
                          buffer, length);
  }
}


// Returns an estimation of k such that 10^(k-1) <= v < 10^k where
// v = f * 2^exponent and 2^52 <= f < 2^53.
// v is hence a normalized double with the given exponent. The output is an
// approximation for the exponent of the decimal approimation .digits * 10^k.
//
// The result might undershoot by 1 in which case 10^k <= v < 10^k+1.
// Note: this property holds for v's upper boundary m+ too.
//    10^k <= m+ < 10^k+1.
//   (see explanation below).
//
// Examples:
//  EstimatePower(0)   => 16
//  EstimatePower(-52) => 0
//
// Note: e >= 0 => EstimatedPower(e) > 0. No similar claim can be made for e<0.
static int EstimatePower(int exponent) {
  // This function estimates log10 of v where v = f*2^e (with e == exponent).
  // Note that 10^floor(log10(v)) <= v, but v <= 10^ceil(log10(v)).
  // Note that f is bounded by its container size. Let p = 53 (the double's
  // significand size). Then 2^(p-1) <= f < 2^p.
  //
  // Given that log10(v) == log2(v)/log2(10) and e+(len(f)-1) is quite close
  // to log2(v) the function is simplified to (e+(len(f)-1)/log2(10)).
  // The computed number undershoots by less than 0.631 (when we compute log3
  // and not log10).
  //
  // Optimization: since we only need an approximated result this computation
  // can be performed on 64 bit integers. On x86/x64 architecture the speedup is
  // not really measurable, though.
  //
  // Since we want to avoid overshooting we decrement by 1e10 so that
  // floating-point imprecisions don't affect us.
  //
  // Explanation for v's boundary m+: the computation takes advantage of
  // the fact that 2^(p-1) <= f < 2^p. Boundaries still satisfy this requirement
  // (even for denormals where the delta can be much more important).

  const double k1Log10 = 0.30102999566398114;  // 1/lg(10)

  // For doubles len(f) == 53 (don't forget the hidden bit).
  const int kSignificandSize = Double::kSignificandSize;
  double estimate = ceil((exponent + kSignificandSize - 1) * k1Log10 - 1e-10);
  return static_cast<int>(estimate);
}


// See comments for InitialScaledStartValues.
static void InitialScaledStartValuesPositiveExponent(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // A positive exponent implies a positive power.
  DOUBLE_CONVERSION_ASSERT(estimated_power >= 0);
  // Since the estimated_power is positive we simply multiply the denominator
  // by 10^estimated_power.

  // numerator = v.
  numerator->AssignUInt64(significand);
  numerator->ShiftLeft(exponent);
  // denominator = 10^estimated_power.
  denominator->AssignPowerUInt16(10, estimated_power);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    denominator->ShiftLeft(1);
    numerator->ShiftLeft(1);
    // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
    // denominator (of 2) delta_plus equals 2^e.
    delta_plus->AssignUInt16(1);
    delta_plus->ShiftLeft(exponent);
    // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
    delta_minus->AssignUInt16(1);
    delta_minus->ShiftLeft(exponent);
  }
}


// See comments for InitialScaledStartValues
static void InitialScaledStartValuesNegativeExponentPositivePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // v = f * 2^e with e < 0, and with estimated_power >= 0.
  // This means that e is close to 0 (have a look at how estimated_power is
  // computed).

  // numerator = significand
  //  since v = significand * 2^exponent this is equivalent to
  //  numerator = v * / 2^-exponent
  numerator->AssignUInt64(significand);
  // denominator = 10^estimated_power * 2^-exponent (with exponent < 0)
  denominator->AssignPowerUInt16(10, estimated_power);
  denominator->ShiftLeft(-exponent);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    denominator->ShiftLeft(1);
    numerator->ShiftLeft(1);
    // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
    // denominator (of 2) delta_plus equals 2^e.
    // Given that the denominator already includes v's exponent the distance
    // to the boundaries is simply 1.
    delta_plus->AssignUInt16(1);
    // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
    delta_minus->AssignUInt16(1);
  }
}


// See comments for InitialScaledStartValues
static void InitialScaledStartValuesNegativeExponentNegativePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // Instead of multiplying the denominator with 10^estimated_power we
  // multiply all values (numerator and deltas) by 10^-estimated_power.

  // Use numerator as temporary container for power_ten.
  Bignum* power_ten = numerator;
  power_ten->AssignPowerUInt16(10, -estimated_power);

  if (need_boundary_deltas) {
    // Since power_ten == numerator we must make a copy of 10^estimated_power
    // before we complete the computation of the numerator.
    // delta_plus = delta_minus = 10^estimated_power
    delta_plus->AssignBignum(*power_ten);
    delta_minus->AssignBignum(*power_ten);
  }

  // numerator = significand * 2 * 10^-estimated_power
  //  since v = significand * 2^exponent this is equivalent to
  // numerator = v * 10^-estimated_power * 2 * 2^-exponent.
  // Remember: numerator has been abused as power_ten. So no need to assign it
  //  to itself.
  DOUBLE_CONVERSION_ASSERT(numerator == power_ten);
  numerator->MultiplyByUInt64(significand);

  // denominator = 2 * 2^-exponent with exponent < 0.
  denominator->AssignUInt16(1);
  denominator->ShiftLeft(-exponent);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    numerator->ShiftLeft(1);
    denominator->ShiftLeft(1);
    // With this shift the boundaries have their correct value, since
    // delta_plus = 10^-estimated_power, and
    // delta_minus = 10^-estimated_power.
    // These assignments have been done earlier.
    // The adjustments if f == 2^p-1 (lower boundary is closer) are done later.
  }
}


// Let v = significand * 2^exponent.
// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator. The functions GenerateShortestDigits and
// GenerateCountedDigits will then convert this ratio to its decimal
// representation d, with the required accuracy.
// Then d * 10^estimated_power is the representation of v.
// (Note: the fraction and the estimated_power might get adjusted before
// generating the decimal representation.)
//
// The initial start values consist of:
//  - a scaled numerator: s.t. numerator/denominator == v / 10^estimated_power.
//  - a scaled (common) denominator.
//  optionally (used by GenerateShortestDigits to decide if it has the shortest
//  decimal converting back to v):
//  - v - m-: the distance to the lower boundary.
//  - m+ - v: the distance to the upper boundary.
//
// v, m+, m-, and therefore v - m- and m+ - v all share the same denominator.
//
// Let ep == estimated_power, then the returned values will satisfy:
//  v / 10^ep = numerator / denominator.
//  v's boundarys m- and m+:
//    m- / 10^ep == v / 10^ep - delta_minus / denominator
//    m+ / 10^ep == v / 10^ep + delta_plus / denominator
//  Or in other words:
//    m- == v - delta_minus * 10^ep / denominator;
//    m+ == v + delta_plus * 10^ep / denominator;
//
// Since 10^(k-1) <= v < 10^k    (with k == estimated_power)
//  or       10^k <= v < 10^(k+1)
//  we then have 0.1 <= numerator/denominator < 1
//           or    1 <= numerator/denominator < 10
//
// It is then easy to kickstart the digit-generation routine.
//
// The boundary-deltas are only filled if the mode equals BIGNUM_DTOA_SHORTEST
// or BIGNUM_DTOA_SHORTEST_SINGLE.

static void InitialScaledStartValues(uint64_t significand,
                                     int exponent,
                                     bool lower_boundary_is_closer,
                                     int estimated_power,
                                     bool need_boundary_deltas,
                                     Bignum* numerator,
                                     Bignum* denominator,
                                     Bignum* delta_minus,
                                     Bignum* delta_plus) {
  if (exponent >= 0) {
    InitialScaledStartValuesPositiveExponent(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  } else if (estimated_power >= 0) {
    InitialScaledStartValuesNegativeExponentPositivePower(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  } else {
    InitialScaledStartValuesNegativeExponentNegativePower(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  }

  if (need_boundary_deltas && lower_boundary_is_closer) {
    // The lower boundary is closer at half the distance of "normal" numbers.
    // Increase the common denominator and adapt all but the delta_minus.
    denominator->ShiftLeft(1);  // *2
    numerator->ShiftLeft(1);    // *2
    delta_plus->ShiftLeft(1);   // *2
  }
}


// This routine multiplies numerator/denominator so that its values lies in the
// range 1-10. That is after a call to this function we have:
//    1 <= (numerator + delta_plus) /denominator < 10.
// Let numerator the input before modification and numerator' the argument
// after modification, then the output-parameter decimal_point is such that
//  numerator / denominator * 10^estimated_power ==
//    numerator' / denominator' * 10^(decimal_point - 1)
// In some cases estimated_power was too low, and this is already the case. We
// then simply adjust the power so that 10^(k-1) <= v < 10^k (with k ==
// estimated_power) but do not touch the numerator or denominator.
// Otherwise the routine multiplies the numerator and the deltas by 10.
static void FixupMultiply10(int estimated_power, bool is_even,
                            int* decimal_point,
                            Bignum* numerator, Bignum* denominator,
                            Bignum* delta_minus, Bignum* delta_plus) {
  bool in_range;
  if (is_even) {
    // For IEEE doubles half-way cases (in decimal system numbers ending with 5)
    // are rounded to the closest floating-point number with even significand.
    in_range = Bignum::PlusCompare(*numerator, *delta_plus, *denominator) >= 0;
  } else {
    in_range = Bignum::PlusCompare(*numerator, *delta_plus, *denominator) > 0;
  }
  if (in_range) {
    // Since numerator + delta_plus >= denominator we already have
    // 1 <= numerator/denominator < 10. Simply update the estimated_power.
    *decimal_point = estimated_power + 1;
  } else {
    *decimal_point = estimated_power;
    numerator->Times10();
    if (Bignum::Equal(*delta_minus, *delta_plus)) {
      delta_minus->Times10();
      delta_plus->AssignBignum(*delta_minus);
    } else {
      delta_minus->Times10();
      delta_plus->Times10();
    }
  }
}

} // namespace impl
} // namespace double_conversion

#endif // DOUBLE_CONVERSION_INLINE_H
