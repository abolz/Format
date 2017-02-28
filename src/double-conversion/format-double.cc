#include "format-double.h"

#include "bignum-dtoa.h"
#include "fast-dtoa.h"
#include "fixed-dtoa.h"

#include <algorithm>
#include <cstring>
#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace fmtxx;

template <typename T> static constexpr T Min(T const& x, T const& y) { return y < x ? y : x; }
template <typename T> static constexpr T Max(T const& x, T const& y) { return y < x ? x : y; }

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

enum struct DtoaMode {
    // Produce the shortest correct representation.
    // For example the output of 0.299999999999999988897 is (the less accurate but correct) 0.3.
    SHORTEST,
    // Produce a fixed number of digits after the decimal point.
    // For instance fixed(0.1, 4) becomes 0.1000
    // If the input number is big, the output will be big.
    FIXED,
    // Fixed number of digits (independent of the decimal point).
    PRECISION,
};

static bool DoubleToAsciiFixed(
    double                          v,
    int                             requested_digits,
    double_conversion::Vector<char> vec,
    int*                            num_digits,
    int*                            decpt)
{
    using namespace double_conversion;

    const Double d { v };
    const int e = d.UnbiasedExponent();

    const double log2_of_10 = 3.32192809488736235;
    const int min_buffer_length
        = requested_digits
            + (e <= 0 ? 0
                      : static_cast<int>((e + 1) / log2_of_10))
            + 1 // null
            ;

    if (vec.length() < min_buffer_length)
        return false;

    if (!FastFixedDtoa(v, requested_digits, vec, num_digits, decpt))
        BignumDtoa(v, BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decpt);

    assert(*num_digits <= min_buffer_length);

    return true;
}

// v = buf * 10^(decpt - buffer_length)
static bool DoubleToAscii(
    double   v,
    DtoaMode mode,
    int      requested_digits,
    char*    buffer,
    int      buffer_length,
    int*     num_digits,
    int*     decpt)
{
    using namespace double_conversion;

    if (buffer_length < 1)
        return false;

    const Double d { v };

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);
    assert(mode == DtoaMode::SHORTEST || requested_digits >= 0);

    if (mode == DtoaMode::PRECISION && requested_digits == 0) {
        *num_digits = 0;
        *decpt = 0;
        return true;
    }

    if (d.IsZero()) {
        *buffer = '0';
        *num_digits = 1;
        *decpt = 1;
        return true;
    }

    Vector<char> vec(buffer, buffer_length);

    switch (mode) {
    case DtoaMode::SHORTEST:
        assert(buffer_length >= 17 + 1/*null*/);
        if (!FastDtoa(v, FAST_DTOA_SHORTEST, 0, vec, num_digits, decpt))
            BignumDtoa(v, BIGNUM_DTOA_SHORTEST, -1, vec, num_digits, decpt);
        break;
    case DtoaMode::PRECISION:
        if (buffer_length < requested_digits + 1/*null*/)
            return false;
        if (!FastDtoa(v, FAST_DTOA_PRECISION, requested_digits, vec, num_digits, decpt))
            BignumDtoa(v, BIGNUM_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
        break;
    case DtoaMode::FIXED:
        return DoubleToAsciiFixed(v, requested_digits, vec, num_digits, decpt);
    }

    return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputeFixedRepresentationLength(
    int                  num_digits,
    int                  decpt,
    int                  precision,
    const FormatOptions& options)
{
    if (decpt <= 0)
        return 1 + (precision > 0 ? 1 + precision : 0);

    const int extra = options.grouping_char != '\0' ? (decpt - 1) / 3 : 0;

    if (decpt >= num_digits)
        return extra + decpt + (precision > 0 ? 1 + precision : 0);

    return extra + decpt + 1 + precision;
}

static void InsertGroupingChars(char* buf, int buflen, int decpt, const FormatOptions& options)
{
    assert(options.grouping_char != '\0');
    assert(decpt > 0);

    int shift = (decpt - 1) / 3;

    if (shift <= 0)
        return;

    for (int i = buflen - 1; i >= decpt; --i)
        buf[i + shift] = buf[i];

    for (int i = decpt - 1; shift > 0; shift--, i -= 3)
    {
        assert(i - 2 >= 0);
        buf[i - 0 + shift    ] = buf[i - 0];
        buf[i - 1 + shift    ] = buf[i - 1];
        buf[i - 2 + shift    ] = buf[i - 2];
        buf[i - 2 + shift - 1] = options.grouping_char;
    }
}

static void CreateFixedRepresentation(
    char*                buf,
    int                  num_digits,
    int                  decpt,
    int                  precision,
    const FormatOptions& options)
{
    // Create a representation that is padded with zeros if needed.

    if (decpt <= 0)
    {
        // 0.[000]digits[000]
        assert(precision == 0 || precision >= -decpt + num_digits);

        if (precision > 0)
        {
            std::fill_n(buf + num_digits, 2 + (precision - num_digits), '0');
            buf[num_digits + 1] = options.decimal_point_char;
            std::rotate(buf, buf + num_digits, buf + (num_digits + 2 + -decpt));
        }
        else
        {
            buf[0] = '0';
        }

        return;
    }

    int buflen = 0;
    if (decpt >= num_digits)
    {
        // digits[000].0

        if (precision > 0)
        {
            std::fill_n(buf + num_digits, decpt - num_digits + 1 + precision, '0');
            buf[decpt] = options.decimal_point_char;
            buflen = decpt + 1 + precision;
        }
        else
        {
            std::fill_n(buf + num_digits, decpt - num_digits, '0');
            buflen = decpt;
        }
    }
    else
    {
        // 0 < decpt < num_digits
        // dig.its
        assert(precision >= num_digits - decpt);

        std::copy_backward(buf + decpt, buf + num_digits, buf + (num_digits + 1));
        buf[decpt] = options.decimal_point_char;
        std::fill_n(buf + (num_digits + 1), precision, '0');

        buflen = decpt + 1 + precision;
    }

    if (options.grouping_char != '\0')
        InsertGroupingChars(buf, buflen, decpt, options);
}

FormatResult fmtxx::Format_f_non_negative(
    char*                first,
    char*                last,
    const double         d,
    const int            precision,
    const FormatOptions& options)
{
    int num_digits = 0;
    int decpt = 0;

    if (!DoubleToAscii(d,
                       DtoaMode::FIXED,
                       precision,
                       first,
                       static_cast<int>(last - first),
                       &num_digits,
                       &decpt))
    {
        return { last, -1 };
    }

    assert(num_digits >= 0);

    const int fixed_len = ComputeFixedRepresentationLength(num_digits, decpt, precision, options);

    if (last - first < fixed_len)
        return { last, -1 };

    CreateFixedRepresentation(first, num_digits, decpt, precision, options);
    return { first + fixed_len, 0 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputeExponentLength(int exponent, const FormatOptions& options)
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

    if (exponent >= 1000 || options.min_exponent_length >= 4) return len + 4;
    if (exponent >=  100 || options.min_exponent_length >= 3) return len + 3;
    if (exponent >=   10 || options.min_exponent_length >= 2) return len + 2;
    return len + 1;
}

static void AppendExponent(char*& p, int exponent, const FormatOptions& options)
{
    assert(-10000 < exponent && exponent < 10000);
    assert(1 <= options.min_exponent_length && options.min_exponent_length <= 4);

    *p++ = options.exponent_char;

    if (exponent < 0)
    {
        *p++ = '-';
        exponent = -exponent;
    }
    else if (options.emit_positive_exponent_sign)
    {
        *p++ = '+';
    }

    const int k = exponent;

    if (k >= 1000 || options.min_exponent_length >= 4) { *p++ = static_cast<char>('0' + exponent / 1000); exponent %= 1000; }
    if (k >=  100 || options.min_exponent_length >= 3) { *p++ = static_cast<char>('0' + exponent /  100); exponent %=  100; }
    if (k >=   10 || options.min_exponent_length >= 2) { *p++ = static_cast<char>('0' + exponent /   10); exponent %=   10; }
    *p++ = static_cast<char>('0' + exponent % 10);
}

static int ComputeExponentialRepresentationLength(
    int                  num_digits,
    int                  exponent,
    int                  num_digits_after_point,
    const FormatOptions& options)
{
    int len = 0;

    len += num_digits;
    if (num_digits > 1)
    {
        len += 1; // decimal point
        const int nz = num_digits_after_point - (num_digits - 1);
        if (nz > 0)
            len += nz;
    }
    else if (num_digits_after_point > 0)
    {
        len += 1 + num_digits_after_point;
    }

    return len + ComputeExponentLength(exponent, options);
}

static void CreateExponentialRepresentation(char* buf, int num_digits, int exponent, int num_digits_after_point, const FormatOptions& options)
{
    assert(num_digits > 0);

    buf += 1;
    if (num_digits > 1)
    {
        std::copy_backward(buf, buf + (num_digits - 1), buf + num_digits);
        buf[0] = options.decimal_point_char;
        buf += (num_digits - 1) + 1;

        const int nz = num_digits_after_point - (num_digits - 1);
        if (nz > 0)
        {
            std::fill_n(buf, nz, '0');
            buf += nz;
        }
    }
    else if (num_digits_after_point > 0)
    {
        std::fill_n(buf, num_digits_after_point + 1, '0');
        buf[0] = options.decimal_point_char;
        buf += num_digits_after_point + 1;
    }

    AppendExponent(buf, exponent, options);
}

FormatResult fmtxx::Format_e_non_negative(
    char*                first,
    char*                last,
    const double         d,
    const int            precision,
    const FormatOptions& options)
{
    int num_digits = 0;
    int decpt = 0;

    if (!DoubleToAscii(d,
                       DtoaMode::PRECISION,
                       precision + 1,
                       first,
                       static_cast<int>(last - first),
                       &num_digits,
                       &decpt))
    {
        return { last, -1 };
    }

    assert(num_digits > 0);

    const int exponent = decpt - 1;
    const int exponential_len
        = ComputeExponentialRepresentationLength(num_digits, exponent, precision, options);

    if (last - first < exponential_len)
        return { last, -1 };

    CreateExponentialRepresentation(first, num_digits, exponent, precision, options);
    return { first + exponential_len, 0 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

FormatResult fmtxx::Format_g_non_negative(
    char*                first,
    char*                last,
    const double         d,
    const int            precision,
    const FormatOptions& options)
{
    int num_digits = 0;
    int decpt = 0;

    const int P = precision == 0 ? 1 : precision;

    if (!DoubleToAscii(d,
                       DtoaMode::PRECISION,
                       P,
                       first,
                       static_cast<int>(last - first),
                       &num_digits,
                       &decpt))
    {
        return { last, -1 };
    }

    assert(num_digits > 0);
    assert(num_digits == P);

    const int X = decpt - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && first[num_digits - 1] == '0')
        --num_digits;

    if (-4 <= X && X < P)
    {
        int prec = P - (X + 1);

        // Adjust precision not to print trailing zeros.
        if (prec > num_digits - decpt)
            prec = num_digits - decpt;

        const int output_len = ComputeFixedRepresentationLength(num_digits, decpt, prec, options);
        if (last - first >= output_len)
        {
            CreateFixedRepresentation(first, num_digits, decpt, prec, options);
            return { first + output_len, 0 };
        }
    }
    else
    {
        const int prec = P - 1;

        const int output_len = ComputeExponentialRepresentationLength(num_digits, X, prec, options);
        if (last - first >= output_len)
        {
            CreateExponentialRepresentation(first, num_digits, X, prec, options);
            return { first + output_len, 0 };
        }
    }

    return { last, -1 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int CountLeadingZeros64(uint64_t n)
{
    assert(n != 0);

#if defined(_MSC_VER) && (defined(_M_X64) /* || defined(_M_ARM64) */)

    unsigned long high = 0; // index of most significant 1-bit
    if (0 == _BitScanReverse64(&high, n))
    {
    }
    return 63 - static_cast<int>(high);

#elif defined(__GNUC__)

    return __builtin_clzll(static_cast<unsigned long long>(n));

#else

    int z = 0;
    while ((n & 0x8000000000000000) == 0) {
        z++;
        n <<= 1;
    }
    return z;

#endif
}

static void HexDoubleToAscii(
    double v,
    int    precision,
    bool   normalize,
    bool   upper,
    char*  buffer,
    int*   num_digits,
    int*   binary_exponent)
{
    const char* const kHexDigits = upper
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    const Double d { v };

    if (d.IsZero()) {
        buffer[(*num_digits)++] = '0';
        return;
    }

    uint64_t exp = d.Exponent();
    uint64_t sig = d.Significand();

    int e = static_cast<int>(exp) - Double::kExponentBias;
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
            sig &= Double::kSignificandMask;
        }
    }
    else
    {
        if (exp == 0)
            e++; // denormal exponent = 1 - bias
        else
            sig |= Double::kHiddenBit;
    }

    // Round?
    if (precision >= 0 && precision < 52/4)
    {
        const uint64_t digit = sig         >> (52 - 4*precision - 4);
        const uint64_t r     = uint64_t{1} << (52 - 4*precision    );

        assert(!normalize || (sig & Double::kHiddenBit) == 0);

        if (digit & 0x8)    // Digit >= 8
        {
            sig += r;       // Round...
            if (normalize)
            {
                assert((sig >> 52) <= 1);
                if (sig & Double::kHiddenBit) // 0ff... was rounded to 100...
                    e++;
            }
        }

        // Zero out unused bits so that the loop below does not produce more
        // digits than neccessary.
        sig &= r | (0 - r);
    }

    *binary_exponent = e;

    buffer[(*num_digits)++] = kHexDigits[normalize ? 1 : (sig >> 52)];

    // Ignore everything but the significand.
    // Shift everything to the left; makes the loop below slightly simpler.
    sig <<= 64 - 52;

    while (sig != 0)
    {
        buffer[(*num_digits)++] = kHexDigits[sig >> (64 - 4)];
        sig <<= 4;
    }
}

FormatResult fmtxx::Format_a_non_negative(
    char*                first,
    char*                last,
    const double         d,
    const int            precision,
    const FormatOptions& options)
{
    char buf[32];

    int num_digits = 0;
    int binary_exponent = 0;

    const bool use_buf = (last - first < 52/4);
    HexDoubleToAscii(d,
                     precision,
                     options.normalize,
                     options.use_upper_case_digits,
                     use_buf ? buf : first,
                     &num_digits,
                     &binary_exponent);

    const int output_len
        = ComputeExponentialRepresentationLength(num_digits, binary_exponent, precision, options);

    if (last - first > output_len)
    {
        if (use_buf)
            std::memcpy(first, buf, static_cast<size_t>(num_digits));

        CreateExponentialRepresentation(first, num_digits, binary_exponent, precision, options);
        return { first + output_len, 0 };
    }

    return { last, -1 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputePrecisionForShortFixedRepresentation(int num_digits, int decpt)
{
    if (num_digits <= decpt)
        return 0;

    if (0 < decpt)
        return num_digits - decpt;

    return -decpt + num_digits;
}

FormatResult fmtxx::Format_s_non_negative(
    char*                first,
    char*                last,
    const double         d,
    const FormatStyle    style,
    const FormatOptions& options)
{
    char buf[17 + 1];

    int num_digits = 0;
    int decpt = 0;

    const bool use_buf = (last - first < 17 + 1);
    DoubleToAscii(d,
                  DtoaMode::SHORTEST,
                  -1, // precision -- ignored.
                  use_buf ? buf : first,
                  17 + 1,
                  &num_digits,
                  &decpt);

    assert(num_digits > 0);

    const int fixed_precision = ComputePrecisionForShortFixedRepresentation(num_digits, decpt);
    const int fixed_len       = ComputeFixedRepresentationLength(num_digits, decpt, fixed_precision, options);
    const int exponent        = decpt - 1;
    const int exponential_len = ComputeExponentialRepresentationLength(num_digits, exponent, num_digits - 1, options);

    const bool use_fixed
        = (style == FormatStyle::fixed)
            || (style == FormatStyle::general && fixed_len <= exponential_len);

    if (use_fixed)
    {
        if (last - first >= fixed_len)
        {
            if (use_buf)
                std::memcpy(first, buf, static_cast<size_t>(num_digits));

            CreateFixedRepresentation(first, num_digits, decpt, fixed_precision, options);
            return { first + fixed_len, 0 };
        }
    }
    else
    {
        if (last - first >= exponential_len)
        {
            if (use_buf)
                std::memcpy(first, buf, static_cast<size_t>(num_digits));

            CreateExponentialRepresentation(first, num_digits, exponent, /*num_digits_after_point*/ num_digits - 1, options);
            return { first + exponential_len, 0 };
        }
    }

    return { last, -1 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

FormatResult fmtxx::Printf_non_negative(
    char*        first,
    char*        last,
    const double d,
    const int    precision,
    const char   grouping_char,
    const bool   /*alt*/,
    const char   conversion_specifier)
{
    FormatOptions options;

    options.use_upper_case_digits       = true;          //       A
    options.normalize                   = true;          //       A
    options.grouping_char               = grouping_char; // F   G   S
    options.decimal_point_char          = '.';           // F E G A S
    options.emit_trailing_dot           = false;         // F E G A S
    options.emit_trailing_zero          = false;         // F E G A S
    options.min_exponent_length         = 2;             //   E G A S
    options.exponent_char               = 'e';           //   E G A S
    options.emit_positive_exponent_sign = true;          //   E G A S

    int prec = precision;
    switch (conversion_specifier)
    {
    case 'f':
    case 'F':
        if (prec < 0)
            prec = 6;
        return Format_f_non_negative(first, last, d, prec, options);
    case 'e':
    case 'E':
        if (prec < 0)
            prec = 6;
        options.exponent_char = conversion_specifier;
        return Format_e_non_negative(first, last, d, prec, options);
    case 'g':
        if (prec < 0)
            prec = 6;
        options.exponent_char = 'e';
        return Format_g_non_negative(first, last, d, prec, options);
    case 'G':
        if (prec < 0)
            prec = 6;
        options.exponent_char = 'E';
        return Format_g_non_negative(first, last, d, prec, options);
    case 'a':
        if (last - first < 2)
            return { last, -1 };
        *first++ = '0';
        *first++ = 'x';
        options.use_upper_case_digits = false;
        options.min_exponent_length   = 1;
        options.exponent_char         = 'p';
        return Format_a_non_negative(first, last, d, prec, options);
    case 'A':
        if (last - first < 2)
            return { last, -1 };
        *first++ = '0';
        *first++ = 'X';
        options.use_upper_case_digits = true;
        options.min_exponent_length   = 1;
        options.exponent_char         = 'P';
        return Format_a_non_negative(first, last, d, prec, options);
    default:
        assert(!"invalid conversion specifier");
        return { last, -1 };
    }
}
