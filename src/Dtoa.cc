// Distributed under the MIT license. See the end of the file for details.

#include "Dtoa.h"

#include "double-conversion/bignum-dtoa.h"
#include "double-conversion/fast-dtoa.h"
#include "double-conversion/fixed-dtoa.h"

#include <algorithm>
#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace dtoa;
using Vector = double_conversion::Vector<char>;

struct IEEEDouble // FIXME: Use double_conversion::Double
{
    static uint64_t const kSignMask        = 0x8000000000000000;
    static uint64_t const kExponentMask    = 0x7FF0000000000000;
    static uint64_t const kSignificandMask = 0x000FFFFFFFFFFFFF;
    static uint64_t const kHiddenBit       = 0x0010000000000000;

    static int const kExponentBias = 0x3FF;

    union {
        double const d;
        uint64_t const bits;
    };

    explicit IEEEDouble(double d) : d(d) {}
    explicit IEEEDouble(uint64_t bits) : bits(bits) {}

    uint64_t Sign()        const { return (bits & kSignMask       ) >> 63; }
    uint64_t Exponent()    const { return (bits & kExponentMask   ) >> 52; }
    uint64_t Significand() const { return (bits & kSignificandMask);       }

    bool IsZero() const {
        return (bits & ~kSignMask) == 0;
    }

    bool IsSpecial() const {
        return (bits & kExponentMask) == kExponentMask;
    }

    double Abs() const {
        return IEEEDouble{bits & ~kSignMask}.d;
    }
};

static int InsertThousandsSep(Vector buf, int pt, int last, char sep)
{
    assert(pt >= 0);
    assert(pt <= last);
    assert(sep != '\0');

    int const nsep = (pt - 1) / 3;

    if (nsep <= 0)
        return 0;

    std::copy_backward(buf.start() + pt, buf.start() + last, buf.start() + (last + nsep));

    for (int l = pt - 1, shift = nsep; shift > 0; --shift, l -= 3)
    {
        buf[l - 0 + shift] = buf[l - 0];
        buf[l - 1 + shift] = buf[l - 1];
        buf[l - 2 + shift] = buf[l - 2];
        buf[l - 3 + shift] = sep;
    }

    return nsep;
}

static void CreateFixedRepresentation(Vector buf, int num_digits, int decpt, int precision, Options const& options)
{
    assert(options.decimal_point_char != '\0');

    if (decpt <= 0)
    {
        // 0.[000]digits[000]

        assert(precision == 0 || precision >= -decpt + num_digits);

        if (precision > 0)
        {
            // digits --> digits0.[000][000]

            int const nextra = 2 + (precision - num_digits);
            // nextra includes the decimal point.
            std::fill_n(buf.start() + num_digits, nextra, '0');
            buf[num_digits + 1] = options.decimal_point_char;

            // digits0.[000][000] --> 0.[000]digits[000]
            std::rotate(buf.start(), buf.start() + num_digits, buf.start() + (num_digits + 2 + -decpt));
        }
        else
        {
            buf[0] = '0';
            if (options.use_alternative_form)
                buf[1] = options.decimal_point_char;
        }

        return;
    }

    int last = 0;
    if (decpt >= num_digits)
    {
        // digits[000][.000]

        int const nzeros = decpt - num_digits;
        int const nextra = precision > 0 ? 1 + precision : (options.use_alternative_form ? 1 : 0);
        // nextra includes the decimal point -- if any.

        std::fill_n(buf.start() + num_digits, nzeros + nextra, '0');
        if (nextra > 0)
        {
            buf[decpt] = options.decimal_point_char;
        }

        last = decpt + nextra;
    }
    else
    {
        // dig.its[000]
        assert(precision >= num_digits - decpt); // >= 1

        // digits --> dig.its
        std::copy_backward(buf.start() + decpt, buf.start() + num_digits, buf.start() + (num_digits + 1));
        buf[decpt] = options.decimal_point_char;
        // dig.its --> dig.its[000]
        std::fill_n(buf.start() + (num_digits + 1), precision - (num_digits - decpt), '0');

        last = decpt + 1 + precision;
    }

    if (options.thousands_sep != '\0')
    {
        InsertThousandsSep(buf, decpt, last, options.thousands_sep);
    }
}

static int ComputeFixedRepresentationLength(int num_digits, int decpt, int precision, Options const& options)
{
    assert(num_digits >= 0);

    if (decpt <= 0)
    {
        // 0.[000]digits[000]

        if (precision > 0)
            return 2 + precision;
        else
            return 1 + (options.use_alternative_form ? 1 : 0);
    }

    int const nseps = options.thousands_sep != '\0' ? (decpt - 1) / 3 : 0;
    int const num_digits_before_decpt = decpt + nseps;

    if (decpt >= num_digits)
    {
        // digits[000][.000]

        if (precision > 0)
            return num_digits_before_decpt + 1 + precision;
        else
            return num_digits_before_decpt + (options.use_alternative_form ? 1 : 0);
    }
    else
    {
        // dig.its[000]
        assert(precision >= num_digits - decpt);
        return num_digits_before_decpt + 1 + precision;
    }
}

static bool GenerateFixedDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 1);
    assert(vec.length() >= 40); // For FastFixedDtoa

    using namespace double_conversion;

    IEEEDouble const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);
    assert(requested_digits >= 0);

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return true;
    }

    bool const fast_worked = FastFixedDtoa(v, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        bool const slow_worked = BignumDtoa(v, BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decpt);
        if (!slow_worked)
            return false; // buffer too small.
    }

    return true;
}

Result dtoa::ToFixed(char* first, char* last, double d, int precision, Options const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    if (!GenerateFixedDigits(d, precision, buf, &num_digits, &decpt))
        return {last, -1};

    assert(num_digits >= 0);

    int const fixed_len = ComputeFixedRepresentationLength(num_digits, decpt, precision, options);

    if (last - first < fixed_len)
        return {last, -1};

    CreateFixedRepresentation(Vector(first, fixed_len), num_digits, decpt, precision, options);
    return {first + fixed_len, 0};
}

static int ComputeExponentLength(int exponent, Options const& options)
{
    int len = 1; // exponent_char

    if (exponent < 0)
    {
        len += 1;
        exponent = -exponent;
    }
    else if (options.emit_positive_exponent_sign)
    {
        len += 1;
    }

    if (exponent >= 1000 || options.min_exponent_digits >= 4) return len + 4;
    if (exponent >=  100 || options.min_exponent_digits >= 3) return len + 3;
    if (exponent >=   10 || options.min_exponent_digits >= 2) return len + 2;
    return len + 1;
}

static int AppendExponent(Vector buf, int pos, int exponent, Options const& options)
{
    assert(exponent > -10000);
    assert(exponent <  10000);
    assert(options.exponent_char != '\0');
    assert(options.min_exponent_digits >= 1);
    assert(options.min_exponent_digits <= 4);

    buf[pos++] = options.exponent_char;

    if (exponent < 0)
    {
        buf[pos++] = '-';
        exponent = -exponent;
    }
    else if (options.emit_positive_exponent_sign)
    {
        buf[pos++] = '+';
    }

    int const k = exponent;

    if (k >= 1000 || options.min_exponent_digits >= 4) { buf[pos++] = static_cast<char>('0' + exponent / 1000); exponent %= 1000; }
    if (k >=  100 || options.min_exponent_digits >= 3) { buf[pos++] = static_cast<char>('0' + exponent /  100); exponent %=  100; }
    if (k >=   10 || options.min_exponent_digits >= 2) { buf[pos++] = static_cast<char>('0' + exponent /   10); exponent %=   10; }
    buf[pos++] = static_cast<char>('0' + exponent % 10);

    return pos;
}

static int CreateExponentialRepresentation(Vector buf, int num_digits, int exponent, int precision, Options const& options)
{
    assert(options.decimal_point_char != '\0');

    int pos = 0;

    pos += 1; // leading digit
    if (num_digits > 1)
    {
        // d.igits[000]e+123

        std::copy_backward(buf.start() + pos, buf.start() + (pos + num_digits - 1), buf.start() + (pos + num_digits));
        buf[pos] = options.decimal_point_char;
        pos += 1 + (num_digits - 1);

        if (precision > num_digits - 1)
        {
            int const nzeros = precision - (num_digits - 1);
            std::fill_n(buf.start() + pos, nzeros, '0');
            pos += nzeros;
        }
    }
    else if (precision > 0)
    {
        // d.0[000]e+123

        std::fill_n(buf.start() + pos, 1 + precision, '0');
        buf[pos] = options.decimal_point_char;
        pos += 1 + precision;
    }
    else
    {
        // d[.]e+123

        if (options.use_alternative_form)
            buf[pos++] = options.decimal_point_char;
    }

    return AppendExponent(buf, pos, exponent, options);
}

static int ComputeExponentialRepresentationLength(int num_digits, int exponent, int precision, Options const& options)
{
    assert(num_digits > 0);
    assert(exponent > -10000);
    assert(exponent < 10000);
    assert(precision < 0 || precision >= num_digits - 1);

    int len = 0;

    len += num_digits;
    if (num_digits > 1)
    {
        len += 1;
        if (precision > num_digits - 1)
            len += precision - (num_digits - 1);
    }
    else if (precision > 0)
    {
        len += 1 + precision;
    }
    else
    {
        if (options.use_alternative_form)
            len += 1;
    }

    return len + ComputeExponentLength(exponent, options);
}

static bool GeneratePrecisionDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    using namespace double_conversion;

    assert(vec.length() >= 1);
    assert(requested_digits >= 0);

    IEEEDouble const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (requested_digits == 0)
    {
        *num_digits = 0;
        *decpt = 0;
        return true;
    }

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return true;
    }

    if (vec.length() < requested_digits + 1 /*null*/)
        return false;

    bool const fast_worked = FastDtoa(v, FAST_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, BIGNUM_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    }

    return true;
}

Result dtoa::ToExponential(char* first, char* last, double d, int precision, Options const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    if (!GeneratePrecisionDigits(d, precision + 1, buf, &num_digits, &decpt))
        return {last, -1};

    assert(num_digits > 0);

    int const exponent = decpt - 1;
    int const exponential_len = ComputeExponentialRepresentationLength(num_digits, exponent, precision, options);

    if (last - first < exponential_len)
        return {last, -1};

    CreateExponentialRepresentation(Vector(first, exponential_len), num_digits, exponent, precision, options);
    return {first + exponential_len, 0};
}

Result dtoa::ToGeneral(char* first, char* last, double d, int precision, Options const& options)
{
    assert(precision >= 0);

    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    int const P = precision == 0 ? 1 : precision;

    if (!GeneratePrecisionDigits(d, P, buf, &num_digits, &decpt))
        return {last, -1};

    assert(num_digits > 0);
    assert(num_digits == P);

    int const X = decpt - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && first[num_digits - 1] == '0')
    {
        --num_digits;
    }

    if (-4 <= X && X < P)
    {
        int prec = P - (X + 1);
        if (!options.use_alternative_form)
        {
            if (prec > num_digits - decpt)
                prec = num_digits - decpt;
        }

        int const output_len = ComputeFixedRepresentationLength(num_digits, decpt, prec, options);
        if (last - first >= output_len)
        {
            CreateFixedRepresentation(Vector(first, output_len), num_digits, decpt, prec, options);
            return {first + output_len, 0};
        }
    }
    else
    {
        int prec = P - 1;
        if (!options.use_alternative_form)
        {
            if (prec > num_digits - 1)
                prec = num_digits - 1;
        }

        int const output_len = ComputeExponentialRepresentationLength(num_digits, X, prec, options);
        if (last - first >= output_len)
        {
            CreateExponentialRepresentation(Vector(first, output_len), num_digits, X, prec, options);
            return {first + output_len, 0};
        }
    }

    return {last, -1};
}

static int CountLeadingZeros64(uint64_t n)
{
    assert(n != 0);

#if defined(_MSC_VER) && (defined(_M_X64) /* || defined(_M_ARM64) */)

    unsigned long high = 0; // index of most significant 1-bit
    _BitScanReverse64(&high, n);
    return 63 - static_cast<int>(high);

#elif defined(__GNUC__)

    return __builtin_clzll(static_cast<unsigned long long>(n));

#else

    int z = 0;
    while ((n & 0x8000000000000000) == 0)
    {
        z++;
        n <<= 1;
    }
    return z;

#endif
}

static void GenerateHexDigits(double v, int precision, bool normalize, bool upper, Vector buffer, int* num_digits, int* binary_exponent)
{
    char const* const xdigits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    IEEEDouble const d{v};

    assert(!d.IsSpecial()); // NaN or infinity?
    assert(d.Abs() >= 0);

    if (d.IsZero())
    {
        buffer[(*num_digits)++] = '0';
        *binary_exponent = 0;
        return;
    }

    uint64_t exp = d.Exponent();
    uint64_t sig = d.Significand();

    int e = static_cast<int>(exp) - IEEEDouble::kExponentBias;
    if (normalize)
    {
        if (exp == 0)
        {
            assert(sig != 0);

            e++; // denormal exponent = 1 - bias

            // Shift the highest bit into the hidden-bit position and adjust the
            // exponent.
            int s = CountLeadingZeros64(sig);
            sig <<= s - 12 + 1;
            e    -= s - 12 + 1;

            // Clear out the hidden-bit.
            // This is not used any more and must be cleared to detect overflow
            // when rounding below.
            sig &= IEEEDouble::kSignificandMask;
        }
    }
    else
    {
        if (exp == 0)
            e++; // denormal exponent = 1 - bias
        else
            sig |= IEEEDouble::kHiddenBit;
    }

    // Round?
    if (precision >= 0 && precision < 52/4)
    {
        uint64_t const digit = sig         >> (52 - 4 * precision - 4);
        uint64_t const r     = uint64_t{1} << (52 - 4 * precision    );

        assert(!normalize || (sig & IEEEDouble::kHiddenBit) == 0);

        if (digit & 0x8) // Digit >= 8
        {
            sig += r; // Round...
            if (normalize)
            {
                assert((sig >> 52) <= 1);
                if (sig & IEEEDouble::kHiddenBit) // 0ff... was rounded to 100...
                    e++;
            }
        }

        // Zero out unused bits so that the loop below does not produce more
        // digits than neccessary.
        sig &= r | (0 - r);
    }

    *binary_exponent = e;

    buffer[(*num_digits)++] = xdigits[normalize ? 1 : (sig >> 52)];

    // Ignore everything but the significand.
    // Shift everything to the left; makes the loop below slightly simpler.
    sig <<= 64 - 52;

    while (sig != 0)
    {
        buffer[(*num_digits)++] = xdigits[sig >> (64 - 4)];
        sig <<= 4;
    }
}

Result dtoa::ToHex(char* first, char* last, double d, int precision, Options const& options)
{
    assert(static_cast<size_t>(last - first) >= 52/4 + 1);

    int num_digits = 0;
    int binary_exponent = 0;

    GenerateHexDigits(d, precision, options.normalize, options.use_upper_case_digits, Vector(first, 52/4 + 1), &num_digits, &binary_exponent);

    assert(num_digits > 0);

    int const output_len = ComputeExponentialRepresentationLength(num_digits, binary_exponent, precision, options);
    if (last - first >= output_len)
    {
        CreateExponentialRepresentation(Vector(first, output_len), num_digits, binary_exponent, precision, options);
        return {first + output_len, 0};
    }

    return {last, -1};
}

static int ComputePrecisionForShortFixedRepresentation(int num_digits, int decpt)
{
    if (num_digits <= decpt)
        return 0;

    if (decpt > 0)
        return num_digits - decpt;

    return -decpt + num_digits;
}

static void GenerateShortestDigits(double v, Vector vec, int* num_digits, int* decpt)
{
    using namespace double_conversion;

    assert(vec.length() >= 17 + 1 /*null*/);

    IEEEDouble const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    bool const fast_worked = FastDtoa(v, FAST_DTOA_SHORTEST, 0, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, BIGNUM_DTOA_SHORTEST, -1, vec, num_digits, decpt);
    }
}

Result dtoa::ToShortest(char* first, char* last, double d, Style style, Options const& options)
{
    if (style == Style::hex)
        return ToHex(first, last, d, /*precision*/ -1, options);

    int num_digits = 0;
    int decpt = 0;

    GenerateShortestDigits(d, Vector(first, 17 + 1), &num_digits, &decpt);

    assert(num_digits > 0);

    int const fixed_precision       = ComputePrecisionForShortFixedRepresentation(num_digits, decpt);
    int const fixed_len             = ComputeFixedRepresentationLength(num_digits, decpt, fixed_precision, options);
    int const exponent              = decpt - 1;
    int const exponential_precision = num_digits - 1;
    int const exponential_len       = ComputeExponentialRepresentationLength(num_digits, exponent, exponential_precision, options);

    bool const use_fixed =
        (style == Style::fixed) ||
        (style == Style::general && fixed_len <= exponential_len);

    if (use_fixed)
    {
        if (last - first >= fixed_len)
        {
            CreateFixedRepresentation(Vector(first, fixed_len), num_digits, decpt, fixed_precision, options);
            return {first + fixed_len, 0};
        }
    }
    else
    {
        if (last - first >= exponential_len)
        {
            CreateExponentialRepresentation(Vector(first, exponential_len), num_digits, exponent, exponential_precision, options);
            return {first + exponential_len, 0};
        }
    }

    return {last, -1};
}

Result dtoa::ToECMAScript(char* first, char* last, double d)
{
    Vector buf(first, static_cast<int>(last - first));
    assert(buf.length() >= 24);

    int num_digits = 0;
    int decpt = 0;

    GenerateShortestDigits(d, buf, &num_digits, &decpt);

    assert(num_digits > 0);

    int const k = num_digits;
    int const n = decpt;

    // Use a decimal notation if -6 < n <= 21.

    if (k <= n && n <= 21)
    {
        // digits[000]
        std::fill_n(buf.start() + k, n - k, '0');
        return {first + n, 0};
    }

    if (0 < n && n <= 21)
    {
        // dig.its
        std::copy_backward(buf.start() + n, buf.start() + k, buf.start() + (k + 1));
        buf[n] = '.';
        return {first + (k + 1), 0};
    }

    if (-6 < n && n <= 0)
    {
        // 0.[000]digits
        std::copy_backward(buf.start(), buf.start() + k, buf.start() + (2 + -n + k));
        buf[0] = '0';
        buf[1] = '.';
        std::fill_n(buf.start() + 2, -n, '0');
        return {first + (2 + -n + k), 0};
    }

    // Otherwise use an exponential notation.

    Options options;

    options.min_exponent_digits = 1;
    options.exponent_char = 'e';
    options.emit_positive_exponent_sign = true;

    if (k == 1)
    {
        // dE+123
        int const endpos = AppendExponent(buf, /*pos*/ 1, n - 1, options);
        return {first + endpos, 0};
    }
    else
    {
        // d.igitsE+123
        std::copy_backward(buf.start() + 1, buf.start() + k, buf.start() + (k + 1));
        buf[1] = '.';
        int const endpos = AppendExponent(buf, /*pos*/ k + 1, n - 1, options);
        return {first + endpos, 0};
    }
}

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

//------------------------------------------------------------------------------
// Parts of this software are based on code found at
// https://github.com/google/double-conversion
//
// Original license follows:
//
// Copyright 2006-2011, the V8 project authors. All rights reserved.
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
