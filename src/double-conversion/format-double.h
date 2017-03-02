#pragma once

#include <cstdint>

namespace fmtxx {

struct Double
{
    static const uint64_t kSignMask        = 0x8000000000000000;
    static const uint64_t kExponentMask    = 0x7FF0000000000000;
    static const uint64_t kSignificandMask = 0x000FFFFFFFFFFFFF;
    static const uint64_t kHiddenBit       = 0x0010000000000000;

    static const int kExponentBias = 0x3FF;

    union {
        const double d;
        const uint64_t bits;
    };

    explicit Double(double d) : d(d) {}
    explicit Double(uint64_t bits) : bits(bits) {}

    uint64_t Sign()        const { return (bits & kSignMask       ) >> 63; }
    uint64_t Exponent()    const { return (bits & kExponentMask   ) >> 52; }
    uint64_t Significand() const { return (bits & kSignificandMask);       }

    int UnbiasedExponent() const {
        return static_cast<int>(Exponent()) - kExponentBias;
    }

    bool IsZero() const {
        return (bits & ~kSignMask) == 0;
    }

    bool IsNegative() const {
        return Sign() != 0;
    }

    bool IsDenormal() const {
        return (bits & kExponentMask) == 0;
    }

    bool IsSpecial() const {
        return (bits & kExponentMask) == kExponentMask;
    }

    bool IsInf() const {
        return IsSpecial() && (bits & kSignificandMask) == 0;
    }

    bool IsNaN() const {
        return IsSpecial() && (bits & kSignificandMask) != 0;
    }

    double Abs() const {
        return Double { bits & ~kSignMask }.d;
    }
};

enum struct FormatStyle {
    fixed,
    exponential,
    general,
    hex,
};

struct FormatResult {
    char* next;
    int ec;
};

struct FormatOptions {
    bool use_upper_case_digits       = true;  //       A
    bool normalize                   = true;  //       A
    char grouping_char               = '\0';  // F   G   S
    char decimal_point_char          = '.';   // F E G A S
    bool emit_trailing_dot           = false; // F E G A S (not implemented)
    bool emit_trailing_zero          = false; // F E G A S (not implemented)
    int  min_exponent_digits         = 2;     //   E G A S
    char exponent_char               = 'e';   //   E G A S
    bool emit_positive_exponent_sign = true;  //   E G A S
};

// %f
//
// http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf (p. 312)
//
// A double argument representing a floating-point number is converted to
// decimal notation in the style [-]ddd.ddd, where the number of digits after
// the decimal-point character is equal to the precision specification. If the
// precision is missing, it is taken as 6; if the precision is zero and the #
// flag is not specified, no decimal-point character appears. If a decimal-point
// character appears, at least one digit appears before it. The value is rounded
// to the appropriate number of digits.
//
// A double argument representing an infinity is converted in one of the styles
// [-]inf or [-]infinity -- which style is implementation-defined. A double
// argument representing a NaN is converted in one of the styles [-]nan or
// [-]nan(n-char-sequence) -- which style, and the meaning of any
// n-char-sequence, is implementation-defined. The F conversion specifier
// produces INF, INFINITY, or NAN instead of inf, infinity, or nan,
// respectively.[277]
//
// [277]
//  When applied to infinite and NaN values, the -, +, and space flag characters
//  have their usual meaning; the # and 0 flag characters have no effect.
FormatResult Format_f_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const FormatOptions& options);

// %e
//
// http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf (pp. 312)
//
// A double argument representing a floating-point number is converted in the
// style [-]d.ddde+-dd, where there is one digit (which is nonzero if the
// argument is nonzero) before the decimal-point character and the number of
// digits after it is equal to the precision; if the precision is missing, it is
// taken as 6; if the precision is zero and the # flag is not specified, no
// decimal-point character appears. The value is rounded to the appropriate
// number of digits. The E conversion specifier produces a number with E instead
// of e introducing the exponent. The exponent always contains at least two
// digits, and only as many more digits as necessary to represent the exponent.
// If the value is zero, the exponent is zero.
//
// A double argument representing an infinity or NaN is converted in the style
// of an f or F conversion specifier.
FormatResult Format_e_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const FormatOptions& options);

// %g
//
// http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf (p. 313)
//
// A double argument representing a floating-point number is converted in
// style f or e (or in style F or E in the case of a G conversion specifier),
// depending on the value converted and the precision. Let P equal the
// precision if nonzero, 6 if the precision is omitted, or 1 if the precision is
// zero. Then, if a conversion with style E would have an exponent of X:
//
//  - if P > X >= -4, the conversion is with style f (or F) and precision
//      P - (X + 1).
//  - otherwise, the conversion is with style e (or E) and precision P - 1.
//
// Finally, unless the # flag is used, any trailing zeros are removed from the
// fractional portion of the result and the decimal-point character is removed
// if there is no fractional portion remaining.
//
// A double argument representing an infinity or NaN is converted in the style
// of an f or F conversion specifier.
FormatResult Format_g_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const FormatOptions& options);

// %a
//
// http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf (pp. 314)
//
// A double argument representing a floating-point number is converted in the
// style [-]0xh.hhhhp+-d, where there is one hexadecimal digit (which is
// nonzero if the argument is a normalized floating-point number and is
// otherwise unspecified) before the decimal-point character and the number
// of hexadecimal digits after it is equal to the precision; if the precision is
// missing and FLT_RADIX is a power of 2, then the precision is sufficient for
// an exact representation of the value; if the precision is missing and
// FLT_RADIX is not a power of 2, then the precision is sufficient to
// distinguish [279] values of type double, except that trailing zeros may be
// omitted; if the precision is zero and the # flag is not specified, no
// decimalpoint character appears. The letters abcdef are used for a conversion
// and the letters ABCDEF for A conversion. The A conversion specifier produces
// a number with X and P instead of x and p. The exponent always contains at
// least one digit, and only as many more digits as necessary to represent the
// decimal exponent of 2. If the value is zero, the exponent is zero.
//
// A double argument representing an infinity or NaN is converted in the style
// of an f or F conversion specifier.
FormatResult Format_a_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const FormatOptions& options);

// %s
//
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0067r5.html
//
// The functions that take a floating-point value but not a precision parameter
// ensure that the string representation consists of the smallest number of
// characters such that there is at least one digit before the radix point (if
// present) and parsing the representation using the corresponding from_chars
// function recovers value exactly. [ Note: This guarantee applies only if
// to_chars and from_chars are executed on the same implementation. ]
//
// The functions taking a chars_format parameter determine the conversion
// specifier for printf as follows: The conversion specifier is f if fmt is
// chars_format::fixed, e if fmt is chars_format::scientific, a (without leading
// "0x" in the result) if fmt is chars_format::hex, and g if fmt is
// chars_format::general.
FormatResult Format_s_non_negative(
    char* first,
    char* last,
    const double d,
    const FormatStyle style,
    const FormatOptions& options);

FormatResult Printf_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const char grouping_char,
    const bool alt,
    const char conversion_specifier);

} // namespace fmtxx
