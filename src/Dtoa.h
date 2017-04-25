// Distributed under the MIT license. See the end of the file for details.

#pragma once

//------------------------------------------------------------------------------
// NOTE:
//
// All the conversion functions defined here require a non-negative finite
// IEEE-754 double precision value!!!
//
// No infinity, no NaN, no negative zero!!!
//------------------------------------------------------------------------------

namespace dtoa {

enum struct Style {
    fixed,
    scientific,
    general,
    hex,
};

struct Result {
    bool success;
    // If success == false, this is a size hint: retry with a buffer of at least this size.
    // Otherwise this is equal to length of converted float.
    int size;
};

struct Options {
    bool use_upper_case_digits       = true;  //       A
    bool normalize                   = true;  //       A
    char thousands_sep               = '\0';  // F   G
    char decimal_point               = '.';   // F E G A J
    bool use_alternative_form        = false; // F E G A
    int  min_exponent_digits         = 2;     //   E G A
    char exponent_char               = 'e';   //   E G A J
    bool emit_positive_exponent_sign = true;  //   E G A
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

// PRE: last - first >= 27
//  (Even if the actual result is shorter!)
Result ToFixed(
    char*          first,
    char*          last,
    double         d,
    int            precision,
    Options const& options);

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

// PRE: last - first >= 1
Result ToExponential(
    char*          first,
    char*          last,
    double         d,
    int            precision,
    Options const& options);

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

// PRE: last - first >= 1
Result ToGeneral(
    char*          first,
    char*          last,
    double         d,
    int            precision,
    Options const& options);

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
// distinguish values of type double, except that trailing zeros may be omitted;
// if the precision is zero and the # flag is not specified, no decimalpoint
// character appears. The letters abcdef are used for a conversion and the
// letters ABCDEF for A conversion. The A conversion specifier produces a number
// with X and P instead of x and p. The exponent always contains at least one
// digit, and only as many more digits as necessary to represent the decimal
// exponent of 2. If the value is zero, the exponent is zero.

// NOTE: This function *never* adds any prefix!
//
// PRE: last - first >= 14
//  (Even if the actual result is shorter!)
Result ToHex(
    char*          first,
    char*          last,
    double         d,
    int            precision,
    Options const& options);

// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0067r5.html
//
// 20.2.7 Output functions
//
// All functions named to_chars convert value into a character string by
// successively filling the range [first, last), where [first, last) is required
// to be a valid range. If the member ec of the return value is such that the
// value, when converted to bool, is false, the conversion was successful and
// the member ptr is the one-past-the-end pointer of the characters written.
// Otherwise, the member ec has the value errc::value_too_large, the member ptr
// has the value last, and the contents of the range [first, last) are
// unspecified.
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

// PRE: last - first >= 14 (style == hex)
//      last - first >= 18 (otherwise)
//  (Even if the actual result is shorter!)
Result ToShortest(
    char*          first,
    char*          last,
    double         d,
    Style          style,
    Options const& options);

// https://www.ecma-international.org/ecma-262/5.1/#sec-9.8.1
//
// 9.8.1 ToString Applied to the Number Type
//
// The abstract operation ToString converts a Number m to String format as
// follows:
//
//  1. If m is NaN, return the String "NaN".
//  2. If m is +0 or -0, return the String "0".
//  3. If m is less than zero, return the String concatenation of the String "-"
//     and ToString(-m).
//  4. If m is infinity, return the String "Infinity".
//  5. Otherwise, let n, k, and s be integers such that k >= 1,
//     10^(k-1) <= s < 10^k, the Number value for s * 10^(n-k) is m, and k is as
//     small as possible. Note that k is the number of digits in the decimal
//     representation of s, that s is not divisible by 10, and that the least
//     significant digit of s is not necessarily uniquely determined by these
//     criteria.
//  6. If k <= n <= 21, return the String consisting of the k digits of the
//     decimal representation of s (in order, with no leading zeroes), followed
//     by n-k occurrences of the character '0'.
//  7. If 0 < n <= 21, return the String consisting of the most significant n
//     digits of the decimal representation of s, followed by a decimal point
//     '.', followed by the remaining k-n digits of the decimal representation
//     of s.
//  8. If -6 < n <= 0, return the String consisting of the character '0',
//     followed by a decimal point '.', followed by -n occurrences of the
//     character '0', followed by the k digits of the decimal representation of
//     s.
//  9. Otherwise, if k = 1, return the String consisting of the single digit of
//     s, followed by lowercase character 'e', followed by a plus sign '+' or
//     minus sign '-' according to whether n-1 is positive or negative, followed
//     by the decimal representation of the integer abs(n-1) (with no leading
//     zeros).
// 10. Return the String consisting of the most significant digit of the decimal
//     representation of s, followed by a decimal point '.', followed by the
//     remaining k-1 digits of the decimal representation of s, followed by the
//     lowercase character 'e', followed by a plus sign '+' or minus sign '-'
//     according to whether n-1 is positive or negative, followed by the decimal
//     representation of the integer abs(n-1) (with no leading zeros).

// PRE: last - first >= 24.
//  (Even if the actual result is shorter!)
Result ToECMAScript(
    char*  first,
    char*  last,
    double d,
    char   decimal_point = '.',
    char   exponent_char = 'e');

} // namespace dtoa

//------------------------------------------------------------------------------
// Copyright (c) 2017 A. Bolz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
