#include "format-double.h"

#include "bignum-dtoa.h"
#include "fast-dtoa.h"
#include "fixed-dtoa.h"

#include <cstring>
#ifdef _MSC_VER
#include <intrin.h>
#endif

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

static bool DoubleToAsciiFixed(double v, int requested_digits, char* buffer, int buffer_length, int* num_digits, int* decimal_point)
{
    using namespace double_conversion;

    const Double d { v };
    const int e = d.UnbiasedExponent();

    const double kLog2_10 = 3.321928094887362347870319429489390175864831393024580612054;
    const int min_buffer_length
        = requested_digits + (e <= 0 ? 0 : static_cast<int>(e / kLog2_10)) + 1/*null*/;

    if (buffer_length < min_buffer_length)
        return false;

    Vector<char> vec(buffer, buffer_length);

    if (!FastFixedDtoa(v, requested_digits, vec, num_digits, decimal_point)) {
        BignumDtoa(v, BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decimal_point);
    }

    assert(*num_digits <= min_buffer_length);

    return true;
}

// v = buf * 10^(decimal_point - buffer_length)
static bool DoubleToAscii(double v, DtoaMode mode, int requested_digits, char* buffer, int buffer_length, int* num_digits, int* decimal_point)
{
    using namespace double_conversion;

    if (buffer_length < 1)
        return false;

    assert(!Double(v).IsSpecial());
    assert(Double(v).Abs() >= 0);
    assert(mode == DtoaMode::SHORTEST || requested_digits >= 0);

    if (mode == DtoaMode::PRECISION && requested_digits == 0)
    {
        *num_digits = 0;
        *decimal_point = 0;
        return true;
    }

    if (v == 0)
    {
        *buffer = '0';
        *num_digits = 1;
        *decimal_point = 1;
        return true;
    }

    switch (mode)
    {
    case DtoaMode::SHORTEST:
        assert(buffer_length >= 17 + 1/*null*/);
        if (!FastDtoa(v, FAST_DTOA_SHORTEST, 0, Vector<char>(buffer, buffer_length), num_digits, decimal_point)) {
            BignumDtoa(v, BIGNUM_DTOA_SHORTEST, -1, Vector<char>(buffer, buffer_length), num_digits, decimal_point);
        }
        break;
    case DtoaMode::PRECISION:
        if (buffer_length < requested_digits + 1/*null*/)
            return false;
        if (!FastDtoa(v, FAST_DTOA_PRECISION, requested_digits, Vector<char>(buffer, buffer_length), num_digits, decimal_point)) {
            BignumDtoa(v, BIGNUM_DTOA_PRECISION, requested_digits, Vector<char>(buffer, buffer_length), num_digits, decimal_point);
        }
        break;
    case DtoaMode::FIXED:
        return DoubleToAsciiFixed(v, requested_digits, buffer, buffer_length, num_digits, decimal_point);
    }

    return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputeFixedRepresentationLength(int num_digits, int decimal_point, int precision)
{
    int len = 0;

    if (decimal_point <= 0)
    {
        len += 1
            + (precision <= 0
                ? 0
                : (1 + (-decimal_point) + num_digits + (precision - (-decimal_point) - num_digits)));
    }
    else if (decimal_point >= num_digits)
    {
        len += num_digits + (decimal_point - num_digits)
            + (precision <= 0
                ? 0
                : 1 + precision);
    }
    else
    {
        len += decimal_point + 1 + (num_digits - decimal_point) + (precision - (num_digits - decimal_point));
    }

    return len;
}

static void CreateFixedRepresentation(char* buf, int num_digits, int decimal_point, int precision)
{
    // Create a representation that is padded with zeros if needed.

    if (decimal_point <= 0)
    {
        // 0.[000]digits[000]

        const int shift = 1 + (precision > 0 ? (1 + (-decimal_point)) : 0);

        std::memmove(buf + shift, buf, static_cast<size_t>(num_digits));
        buf[0] = '0';
        if (precision > 0)
        {
            buf[1] = '.';
            std::memset(buf + 2, '0', static_cast<size_t>(-decimal_point));
            std::memset(buf + (2 + (-decimal_point) + num_digits), '0', static_cast<size_t>(precision - (-decimal_point) - num_digits));
        }
    }
    else if (decimal_point >= num_digits)
    {
        // digits[000].0

        std::memset(buf + num_digits, '0', static_cast<size_t>(decimal_point - num_digits));
        if (precision > 0)
        {
            buf[decimal_point] = '.';
            std::memset(buf + (decimal_point + 1), '0', static_cast<size_t>(precision));
        }
    }
    else
    {
        // dig.its

        std::memmove(buf + (decimal_point + 1), buf + decimal_point, static_cast<size_t>(num_digits - decimal_point));
        buf[decimal_point] = '.';
        std::memset(buf + (num_digits + 1), '0', static_cast<size_t>(precision - (num_digits - decimal_point)));
    }
}

FormatResult Format_f_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision)
{
    int num_digits = 0;
    int decimal_point = 0;

    if (!DoubleToAscii(d, DtoaMode::FIXED, precision, first, static_cast<int>(last - first), &num_digits, &decimal_point))
        return { last, -1 };

    assert(num_digits >= 0);

    const int output_len = ComputeFixedRepresentationLength(num_digits, decimal_point, precision);

    if (last - first < output_len)
        return { last, -1 };

    CreateFixedRepresentation(first, num_digits, decimal_point, precision);
    return { first + output_len, 0 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputeExponentLength(int exponent, int min_exponent_length, bool emit_positive_exponent_sign)
{
    int len = 1; // exponent_char

    if (exponent < 0) {
        len += 1;
        exponent = -exponent;
    }
    else if (emit_positive_exponent_sign) {
        len += 1;
    }

    if (exponent >= 1000 || min_exponent_length >= 4) return len + 4;
    if (exponent >=  100 || min_exponent_length >= 3) return len + 3;
    if (exponent >=   10 || min_exponent_length >= 2) return len + 2;
    return len + 1;
}

static void AppendExponent(char*& p, int exponent, int min_exponent_length, char exponent_char, bool emit_positive_exponent_sign)
{
    assert(-10000 < exponent && exponent < 10000);
    assert(1 <= min_exponent_length && min_exponent_length <= 4);

    *p++ = exponent_char;

    if (exponent < 0)
    {
        *p++ = '-';
        exponent = -exponent;
    }
    else if (emit_positive_exponent_sign)
    {
        *p++ = '+';
    }

    const int k = exponent;
    if (k >= 1000 || min_exponent_length >= 4) { *p++ = static_cast<char>('0' + exponent / 1000); exponent %= 1000; };
    if (k >=  100 || min_exponent_length >= 3) { *p++ = static_cast<char>('0' + exponent /  100); exponent %=  100; };
    if (k >=   10 || min_exponent_length >= 2) { *p++ = static_cast<char>('0' + exponent /   10); exponent %=   10; };
    *p++ = static_cast<char>('0' + exponent % 10);
}

static int ComputeExponentialRepresentationLength(int num_digits, int exponent, int num_digits_after_point, int min_exponent_length, bool emit_positive_exponent_sign)
{
    int len = 0;

    len += num_digits;
    if (num_digits > 1)
    {
        len += 1; // decimal point
        if (num_digits_after_point > num_digits - 1)
            len += num_digits_after_point - (num_digits - 1);
    }
    else if (num_digits_after_point > 0)
    {
        len += 1 + num_digits_after_point;
    }

    return len + ComputeExponentLength(exponent, min_exponent_length, emit_positive_exponent_sign);
}

static void CreateExponentialRepresentation(char* buf, int num_digits, int exponent, int num_digits_after_point, int min_exponent_length, char exponent_char, bool emit_positive_exponent_sign)
{
    assert(num_digits > 0);

    buf += 1;
    if (num_digits > 1)
    {
        std::memmove(buf + 1, buf, static_cast<size_t>(num_digits - 1));
        buf[0] = '.';
        buf += num_digits;
        if (num_digits_after_point > num_digits - 1)
        {
            const int z = num_digits_after_point - (num_digits - 1);
            std::memset(buf, '0', static_cast<size_t>(z));
            buf += z;
        }
    }
    else if (num_digits_after_point > 0)
    {
        buf[0] = '.';
        std::memset(buf + 1, '0', static_cast<size_t>(num_digits_after_point));

        buf += 1 + num_digits_after_point;
    }

    AppendExponent(buf, exponent, min_exponent_length, exponent_char, emit_positive_exponent_sign);
}

FormatResult Format_e_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const int min_exponent_length,
    const char exponent_char,
    const bool emit_positive_exponent_sign)
{
    int num_digits = 0;
    int decimal_point = 0;

    if (!DoubleToAscii(d, DtoaMode::PRECISION, precision + 1, first, static_cast<int>(last - first), &num_digits, &decimal_point))
        return { last, -1 };

    assert(num_digits > 0);

    const int exponent = decimal_point - 1;
    const int output_len = ComputeExponentialRepresentationLength(num_digits, exponent, precision, min_exponent_length, emit_positive_exponent_sign);

    if (last - first < output_len)
        return { last, -1 };

    CreateExponentialRepresentation(first, num_digits, exponent, precision, min_exponent_length, exponent_char, emit_positive_exponent_sign);
    return { first + output_len, 0 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

FormatResult Format_g_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const int min_exponent_length,
    const char exponent_char,
    const bool emit_positive_exponent_sign)
{
    int num_digits = 0;
    int decimal_point = 0;

    const int P = precision == 0 ? 1 : precision;

    if (!DoubleToAscii(d, DtoaMode::PRECISION, P, first, static_cast<int>(last - first), &num_digits, &decimal_point))
        return { last, -1 };

    assert(num_digits > 0);
    assert(num_digits == P);

    const int X = decimal_point - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && first[num_digits - 1] == '0')
    {
        --num_digits;
    }

    if (-4 <= X && X < P)
    {
        int prec = P - (X + 1);

        // Adjust precision not to print trailing zeros.
        if (prec > num_digits - decimal_point)
            prec = num_digits - decimal_point;

        const int output_len = ComputeFixedRepresentationLength(num_digits, decimal_point, prec);
        if (last - first >= output_len)
        {
            CreateFixedRepresentation(first, num_digits, decimal_point, prec);
            return { first + output_len, 0 };
        }
    }
    else
    {
        const int prec = P - 1;

        const int output_len = ComputeExponentialRepresentationLength(num_digits, X, prec, min_exponent_length, emit_positive_exponent_sign);
        if (last - first >= output_len)
        {
            CreateExponentialRepresentation(first, num_digits, X, prec, min_exponent_length, exponent_char, emit_positive_exponent_sign);
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

static void HexDoubleToAscii(double v, int precision, bool normalize, bool upper, char* buffer, int* num_digits, int* binary_exponent)
{
    const char* const kHexDigits = upper
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    const Double d { v };

    if (d.IsZero())
    {
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

FormatResult Format_a_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const bool upper_case_digits,
    const int min_exponent_length,
    const char exponent_char,
    const bool emit_positive_exponent_sign)
{
    int num_digits = 0;
    int binary_exponent = 0;
    char buf[32];

    HexDoubleToAscii(d, precision, /*normalize*/ true, upper_case_digits, buf, &num_digits, &binary_exponent);

    const int output_len = ComputeExponentialRepresentationLength(num_digits, binary_exponent, precision, min_exponent_length, emit_positive_exponent_sign);

    if (last - first > output_len)
    {
        std::memcpy(first, buf, static_cast<size_t>(num_digits));

        CreateExponentialRepresentation(first, num_digits, binary_exponent, precision, min_exponent_length, exponent_char, emit_positive_exponent_sign);
        return { first + output_len, 0 };
    }

    return { last, -1 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int ComputePrecisionForShortFixedRepresentation(int num_digits, int decimal_point)
{
    if (num_digits <= decimal_point)
        return 0;

    if (0 < decimal_point)
        return num_digits - decimal_point;

    return -decimal_point + num_digits;
}

FormatResult Format_s_non_negative(
    char* first,
    char* last,
    const double d,
    const FormatStyle style,
    const int min_exponent_length,
    const char exponent_char,
    const bool emit_positive_exponent_sign)
{
    const int kBufSize = 32;
    char buf[kBufSize];

    int num_digits = 0;
    int decimal_point = 0;

    if (!DoubleToAscii(d, DtoaMode::SHORTEST, /*precision*/ -1, buf, kBufSize, &num_digits, &decimal_point))
        return { last, -1 };

    assert(num_digits > 0);

    const int fixed_precision        = ComputePrecisionForShortFixedRepresentation(num_digits, decimal_point);
    const int fixed_output_len       = ComputeFixedRepresentationLength(num_digits, decimal_point, fixed_precision);
    const int exponent               = decimal_point - 1;
    const int exponential_output_len = ComputeExponentialRepresentationLength(num_digits, exponent, /*num_digits_after_point*/ num_digits - 1, min_exponent_length, emit_positive_exponent_sign);

    const bool use_fixed =
        (style == FormatStyle::fixed) ||
        (style == FormatStyle::general && fixed_output_len <= exponential_output_len);

    if (use_fixed)
    {
        if (last - first >= fixed_output_len)
        {
            std::memcpy(first, buf, static_cast<size_t>(num_digits));

            CreateFixedRepresentation(first, num_digits, decimal_point, fixed_precision);
            return { first + fixed_output_len, 0 };
        }
    }
    else
    {
        if (last - first >= exponential_output_len)
        {
            std::memcpy(first, buf, static_cast<size_t>(num_digits));

            CreateExponentialRepresentation(first, num_digits, exponent, /*num_digits_after_point*/ num_digits - 1, min_exponent_length, exponent_char, emit_positive_exponent_sign);
            return { first + exponential_output_len, 0 };
        }
    }

    return { last, -1 };
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

FormatResult Printf_non_negative(
    char* first,
    char* last,
    const double d,
    const int precision,
    const char conversion_specifier)
{
    int prec = precision;
    switch (conversion_specifier)
    {
    case 'f':
    case 'F':
        if (prec < 0)
            prec = 6;
        return Format_f_non_negative(first, last, d, prec);
    case 'e':
    case 'E':
        if (prec < 0)
            prec = 6;
        return Format_e_non_negative(first, last, d, prec, 2, conversion_specifier, true);
    case 'g':
        if (prec < 0)
            prec = 6;
        return Format_g_non_negative(first, last, d, prec, 2, 'e', true);
    case 'G':
        if (prec < 0)
            prec = 6;
        return Format_g_non_negative(first, last, d, prec, 2, 'E', true);
    case 'a':
        if (last - first < 2)
            return { last, -1 };
        *first++ = '0';
        *first++ = 'x';
        return Format_a_non_negative(first, last, d, prec, false, 1, 'p', true);
    case 'A':
        if (last - first < 2)
            return { last, -1 };
        *first++ = '0';
        *first++ = 'X';
        return Format_a_non_negative(first, last, d, prec, true, 1, 'P', true);
    default:
        return { last, -1 };
    }
}
