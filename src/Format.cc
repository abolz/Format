// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"

#include "double-conversion/bignum-dtoa.h"
#include "double-conversion/fast-dtoa.h"
#include "double-conversion/fixed-dtoa.h"

#include <algorithm>
#include <ostream>
#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace fmtxx;
using namespace fmtxx::impl;

using Vector = double_conversion::Vector<char>;

template <typename T> static constexpr T Min(T x, T y) { return y < x ? y : x; }
template <typename T> static constexpr T Max(T x, T y) { return y < x ? x : y; }

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

fmtxx::FormatBuffer::~FormatBuffer()
{
}

bool fmtxx::StringBuffer::Put(char c)
{
    os.push_back(c);
    return true;
}

bool fmtxx::StringBuffer::Write(char const* str, size_t len)
{
    os.append(str, len);
    return true;
}

bool fmtxx::StringBuffer::Pad(char c, size_t count)
{
    os.append(count, c);
    return true;
}

bool fmtxx::FILEBuffer::Put(char c)
{
    return EOF != std::fputc(c, os);
}

bool fmtxx::FILEBuffer::Write(char const* str, size_t len)
{
    return len == std::fwrite(str, 1, len, os);
}

bool fmtxx::FILEBuffer::Pad(char c, size_t count)
{
    const size_t kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        const auto n = Min(count, kBlockSize);

        if (n != std::fwrite(block, 1, n, os))
            return false;

        count -= n;
    }

    return true;
}

bool fmtxx::StreamBuffer::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof())) {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

bool fmtxx::StreamBuffer::Write(char const* str, size_t len)
{
    const auto kMaxLen = static_cast<size_t>( std::numeric_limits<std::streamsize>::max() );

    while (len > 0)
    {
        const auto n = Min(len, kMaxLen);
        const auto k = static_cast<std::streamsize>(n);

        if (k != os.rdbuf()->sputn(str, k)) {
            os.setstate(std::ios_base::badbit);
            return false;
        }

        str += n;
        len -= n;
    }

    return true;
}

bool fmtxx::StreamBuffer::Pad(char c, size_t count)
{
    const size_t kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        const auto n = Min(count, kBlockSize);
        const auto k = static_cast<std::streamsize>(n);

        if (k != os.rdbuf()->sputn(block, k)) {
            os.setstate(std::ios_base::badbit);
            return false;
        }

        count -= n;
    }

    return true;
}

bool fmtxx::CharArrayBuffer::Put(char c)
{
    if (next >= last)
        return false;

    *next++ = c;
    return true;
}

bool fmtxx::CharArrayBuffer::Write(char const* str, size_t len)
{
    if (static_cast<size_t>(last - next) < len)
        return false;

    std::memcpy(next, str, len);
    next += len;
    return true;
}

bool fmtxx::CharArrayBuffer::Pad(char c, size_t count)
{
    if (static_cast<size_t>(last - next) < count)
        return false;

    std::memset(next, static_cast<unsigned char>(c), count);
    next += count;
    return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static bool IsDigit(char ch) { return '0' <= ch && ch <= '9'; }
static bool IsAlign(char ch) { return ch == '<' || ch == '>' || ch == '^' || ch == '='; }
static bool IsSign (char ch) { return ch == ' ' || ch == '-' || ch == '+'; }
static bool IsPrint(char ch) { return 0x20 <= ch && ch <= 0x7E; }

static char ComputeSignChar(bool neg, char sign, char fill)
{
    if (neg)
        return '-';
    if (sign == '+')
        return '+';
    if (sign == ' ')
        return fill;

    return '\0';
}

static void ComputePadding(size_t len, char align, int width, size_t& lpad, size_t& spad, size_t& rpad)
{
    assert(width >= 0 && "internal error");

    const size_t w = static_cast<size_t>(width);
    if (w <= len)
        return;

    const size_t d = w - len;
    switch (align)
    {
    case '>':
        lpad = d;
        break;
    case '<':
        rpad = d;
        break;
    case '^':
        lpad = d/2;
        rpad = d - d/2;
        break;
    case '=':
        spad = d;
        break;
    }
}

static errc WriteRawString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
{
    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(len, spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0 && !fb.Pad(spec.fill, lpad))
        return errc::io_error;
    if (len > 0  && !fb.Write(str, len))
        return errc::io_error;
    if (rpad > 0 && !fb.Pad(spec.fill, rpad))
        return errc::io_error;

    return errc::success;
}

static errc WriteRawString(FormatBuffer& fb, FormatSpec const& spec, std::string_view str)
{
    return WriteRawString(fb, spec, str.data(), str.size());
}

static size_t ComputeEscapedStringLength(char const* first, char const* last)
{
    size_t n = 0;
    for (; first != last; ++first)
    {
        if (IsPrint(*first))
            n += 1;
        else
            n += 4; // "\xFF" or "\377"
    }
    return n;
}

static bool PutEscapedString(FormatBuffer& fb, char const* str, size_t len, bool upper)
{
    const char* const xdigits = upper
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    char const* f = str;
    char const* l = str + len;
    char const* s = f;
    for (;;)
    {
        while (f != l && IsPrint(*f))
            ++f;

        if (f != s && !fb.Write(s, static_cast<size_t>(f - s)))
            return false;

        if (f == l) // done.
            break;

        if (!fb.Put('\\'))
            return false;
        if (!fb.Put('x'))
            return false;
        if (!fb.Put( xdigits[static_cast<unsigned char>(*f) >> 4       ])) return false;
        if (!fb.Put( xdigits[static_cast<unsigned char>(*f)      & 0xF ])) return false;

        s = ++f;
    }

    return true;
}

#if 0
static bool PutEscapedStringOctal(FormatBuffer& fb, char const* str, size_t len)
{
    char const* f = str;
    char const* l = str + len;
    char const* s = f;
    for (;;)
    {
        while (f != l && IsPrint(*f))
            ++f;

        if (f != s && !fb.Write(s, static_cast<size_t>(f - s)))
            return false;

        if (f == l) // done.
            break;

        if (!fb.Put('\\'))
            return false;
        if (!fb.Put( "01234567"[static_cast<unsigned char>(*f) >> 6      ] )) return false;
        if (!fb.Put( "01234567"[static_cast<unsigned char>(*f) >> 3 & 0x7] )) return false;
        if (!fb.Put( "01234567"[static_cast<unsigned char>(*f)      & 0x7] )) return false;

        s = ++f;
    }

    return true;
}
#endif

static errc WriteEscapedString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len, bool upper)
{
    const size_t ascii_len = ComputeEscapedStringLength(str, str + len);

    if (ascii_len == len)
    {
        // The string only contains printable characters.
        return WriteRawString(fb, spec, str, len);
    }

    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(ascii_len, spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0 && !fb.Pad(spec.fill, lpad))
        return errc::io_error;
    if (len > 0  && !PutEscapedString(fb, str, len, upper))
        return errc::io_error;
    if (rpad > 0 && !fb.Pad(spec.fill, rpad))
        return errc::io_error;

    return errc::success;
}

static errc WriteString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
{
    size_t n = len;
    if (spec.prec >= 0)
    {
        if (n > static_cast<size_t>(spec.prec))
            n = static_cast<size_t>(spec.prec);
    }

    switch (spec.conv) {
    default:
        return WriteRawString(fb, spec, str, n);
    case 'x':
        return WriteEscapedString(fb, spec, str, n, /*upper*/ false);
    case 'X':
        return WriteEscapedString(fb, spec, str, n, /*upper*/ true);
    }
}

static errc WriteString(FormatBuffer& fb, FormatSpec const& spec, char const* str)
{
    if (str == nullptr)
        return WriteRawString(fb, spec, "(null)");

    // Use strnlen if a precision was specified.
    // The string may not be null-terminated!
    size_t len;
    if (spec.prec >= 0)
        len = ::strnlen(str, static_cast<size_t>(spec.prec));
    else
        len = ::strlen(str);

    switch (spec.conv) {
    default:
        return WriteRawString(fb, spec, str, len);
    case 'x':
        return WriteEscapedString(fb, spec, str, len, /*upper*/ false);
    case 'X':
        return WriteEscapedString(fb, spec, str, len, /*upper*/ true);
    }
}

static errc WriteNumber(FormatBuffer& fb, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    const size_t len = (sign ? 1u : 0u) + nprefix + ndigits;

    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(len, spec.zero ? '=' : spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0     && !fb.Pad(spec.fill, lpad))
        return errc::io_error;
    if (sign != '\0' && !fb.Put(sign))
        return errc::io_error;
    if (nprefix > 0  && !fb.Write(prefix, nprefix))
        return errc::io_error;
    if (spad > 0     && !fb.Pad(spec.zero ? '0' : spec.fill, spad))
        return errc::io_error;
    if (ndigits > 0  && !fb.Write(digits, ndigits))
        return errc::io_error;
    if (rpad > 0     && !fb.Pad(spec.fill, rpad))
        return errc::io_error;

    return errc::success;
}

static char* DecIntToAsciiBackwards(char* last/*[-20]*/, uint64_t n)
{
    static const char* const kDecDigits100/*[100*2 + 1]*/ =
        "00010203040506070809"
        "10111213141516171819"
        "20212223242526272829"
        "30313233343536373839"
        "40414243444546474849"
        "50515253545556575859"
        "60616263646566676869"
        "70717273747576777879"
        "80818283848586878889"
        "90919293949596979899";

    while (n >= 100)
    {
        uint64_t r = n % 100;
        n /= 100;
        *--last = kDecDigits100[2*r + 1];
        *--last = kDecDigits100[2*r + 0];
    }

    if (n >= 10)
    {
        *--last = kDecDigits100[2*n + 1];
        *--last = kDecDigits100[2*n + 0];
    }
    else
    {
        *--last = static_cast<char>('0' + n);
    }

    return last;
}

static char* IntToAsciiBackwards(char* last/*[-64]*/, uint64_t n, int base, bool capitals)
{
    const char* const xdigits = capitals
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    assert(base >= 2);
    assert(base <= 16);

    switch (base)
    {
    case 10:
        return DecIntToAsciiBackwards(last, n);
    case 16:
        do *--last = xdigits[n & 15]; while (n >>= 4);
        return last;
    case 8:
        do *--last = xdigits[n & 7]; while (n >>= 3);
        return last;
    case 2:
        do *--last = xdigits[n & 1]; while (n >>= 1);
        return last;
    }

    assert(!"not implemented");
    return last;
}

// Inserts thousands separators into [buf, +pt).
// Returns the number of separators inserted.
//
// Example:
//  "12345.6789" ---> "12'345.6789"
//   ^    ^    ^
//   0    pt   last
//  Returns 1.
//
static int InsertThousandsSep(Vector buf, int pt, int last, char sep, int group_len)
{
    assert(pt >= 0);
    assert(pt <= last);
    assert(sep != '\0');
    assert(group_len > 0);

    const int nsep = (pt - 1) / group_len;

    if (nsep <= 0)
        return 0;

    int shift = nsep;

    for (int i = last - 1; i >= pt; --i)
        buf[i + shift] = buf[i];

    for (int i = pt - 1; shift > 0; --shift, i -= group_len)
    {
        for (int j = 0; j < group_len; ++j)
            buf[i - j + shift] = buf[i - j];

        buf[i - group_len + shift] = sep;
    }

    return nsep;
}

static errc WriteInt(FormatBuffer& fb, FormatSpec const& spec, int64_t sext, uint64_t zext)
{
    uint64_t number = zext;
    char     conv = spec.conv;
    char     sign = '\0';
    int      base = 10;
    size_t   nprefix = 0;

    switch (conv)
    {
    default:
        // I'm sorry Dave, I'm afraid I can't do that.
    case '\0':
    case 'd':
    case 'i':
        sign = ComputeSignChar(sext < 0, spec.sign, spec.fill);
        if (sext < 0)
            number = 0 - static_cast<uint64_t>(sext);
        //[[fallthrough]];
    case 'u':
        base = 10;
        break;
    case 'x':
    case 'X':
        base = 16;
        if (spec.hash != '\0')
            nprefix = 2;
        break;
    case 'b':
    case 'B':
        base = 2;
        if (spec.hash != '\0')
            nprefix = 2;
        break;
    case 'o':
        base = 8;
        if (spec.hash != '\0' && number != 0)
            nprefix = 1;
        break;
    }

    const char prefix[] = {'0', conv};

    const bool upper = ('A' <= conv && conv <= 'Z');

    // Generate digits backwards at buf+64. (64 is the number of digits of UINT64_MAX in base 2)
    // Then insert thousands-separators - if any. (15 = (64 - 1) / 3)
    char buf[64 + 15];
    char*       l = buf + 64;
    char* const f = IntToAsciiBackwards(l, number, base, upper);

    if (spec.tsep)
    {
        const int group_len = (base == 10) ? 3 : 4;
        const int pos       = static_cast<int>(l - f);
        const int last      = pos;

        Vector vec(f, pos + 15);
        l += InsertThousandsSep(vec, pos, last, spec.tsep, group_len);
    }

    return WriteNumber(fb, spec, sign, prefix, nprefix, f, static_cast<size_t>(l - f));
}

static errc WriteBool(FormatBuffer& fb, FormatSpec const& spec, bool val)
{
    return WriteRawString(fb, spec, val ? "true" : "false");
}

static errc WriteChar(FormatBuffer& fb, FormatSpec const& spec, char ch)
{
    return WriteString(fb, spec, &ch, 1u);
}

static errc WritePointer(FormatBuffer& fb, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return WriteRawString(fb, spec, "(nil)");

    FormatSpec f = spec;
    f.hash = '#';
    f.conv = 'x';

    return WriteInt(fb, f, 0, reinterpret_cast<uintptr_t>(pointer));
}

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

struct DtoaResult {
    char* next;
    int ec;
};

struct DtoaOptions {
    bool use_upper_case_digits       = true;  //       A
    bool normalize                   = true;  //       A
    char thousands_sep               = '\0';  // F   G
    char decimal_point_char          = '.';   // F E G A
    bool use_alternative_form        = false; // F E G A
    int  min_exponent_digits         = 2;     //   E G A
    char exponent_char               = 'e';   //   E G A
    bool emit_positive_exponent_sign = true;  //   E G A
};

static void CreateFixedRepresentation(Vector buf, int num_digits, int decpt, int precision, DtoaOptions const& options)
{
    assert(options.decimal_point_char != '\0');

    if (decpt <= 0)
    {
        // 0.[000]digits[000]

        assert(precision == 0 || precision >= -decpt + num_digits);

        if (precision > 0)
        {
            std::fill_n(buf.begin() + num_digits, 2 + (precision - num_digits), '0');
            buf[num_digits + 1] = options.decimal_point_char;
            // digits0.[000][000] ---> 0.[000]digits[000]
            std::rotate(buf.begin(), buf.begin() + num_digits, buf.begin() + (num_digits + 2 + -decpt));
        }
        else
        {
            buf[0] = '0';
            if (options.use_alternative_form)
                buf[1] = options.decimal_point_char;
        }

        return;
    }

    int buflen = 0;
    if (decpt >= num_digits)
    {
        // digits[000].0

        const int nz = decpt - num_digits;
        const int nextra = precision > 0 ? 1 + precision
                                         : (options.use_alternative_form ? 1 : 0);
        // (nextra includes the decimal point)

        std::fill_n(buf.begin() + num_digits, nz + nextra, '0');
        if (nextra > 0)
            buf[decpt] = options.decimal_point_char;
        buflen = decpt + nextra;
    }
    else
    {
        // 0 < decpt < num_digits
        // dig.its
        assert(precision >= num_digits - decpt); // >= 1

        // digits ---> dig_its
        std::copy_backward(buf.begin() + decpt, buf.begin() + num_digits, buf.begin() + (num_digits + 1));
        // dig.its
        buf[decpt] = options.decimal_point_char;
        // dig.its[000]
        std::fill_n(buf.begin() + (num_digits + 1), precision - (num_digits - decpt), '0');

        buflen = decpt + 1 + precision;
    }

    if (options.thousands_sep != '\0')
        InsertThousandsSep(buf, decpt, buflen, options.thousands_sep, 3);
}

static int ComputeFixedRepresentationLength(int num_digits, int decpt, int precision, DtoaOptions const& options)
{
    assert(num_digits >= 0);

    if (decpt <= 0)
    {
        if (precision > 0)
            return 2 + precision;
        else
            return 1 + (options.use_alternative_form ? 1 : 0);
    }

    const int tseps = options.thousands_sep != '\0' ? (decpt - 1) / 3 : 0;

    if (decpt >= num_digits)
    {
        if (precision > 0)
            return tseps + decpt + 1 + precision;
        else
            return tseps + decpt + (options.use_alternative_form ? 1 : 0);
    }

    assert(precision >= num_digits - decpt);
    return tseps + decpt + 1 + precision;
}

// v = buf * 10^(decpt - num_digits)
//
// Produce a fixed number of digits after the decimal point.
// For instance fixed(0.1, 4) becomes 0.1000
// If the input number is big, the output will be big.
static bool GenerateFixedDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 1);

    using namespace double_conversion;

    const Double d { v };

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

    const bool fast_worked = FastFixedDtoa(v, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
        BignumDtoa(v, BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decpt);

    assert(*num_digits <= min_buffer_length);

    return true;
}

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
//
static DtoaResult DtoaFixed(char* first, char* last, double d, int precision, DtoaOptions const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    if (!GenerateFixedDigits(d, precision, buf, &num_digits, &decpt))
        return { last, -1 };

    assert(num_digits >= 0);

    const int fixed_len = ComputeFixedRepresentationLength(num_digits, decpt, precision, options);

    if (last - first < fixed_len)
        return { last, -1 };

    CreateFixedRepresentation(Vector(first, fixed_len), num_digits, decpt, precision, options);
    return { first + fixed_len, 0 };
}

static int ComputeExponentLength(int exponent, DtoaOptions const& options)
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

static void AppendExponent(Vector buf, int pos, int exponent, DtoaOptions const& options)
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

    const int k = exponent;

    if (k >= 1000 || options.min_exponent_digits >= 4) { buf[pos++] = static_cast<char>('0' + exponent / 1000); exponent %= 1000; }
    if (k >=  100 || options.min_exponent_digits >= 3) { buf[pos++] = static_cast<char>('0' + exponent /  100); exponent %=  100; }
    if (k >=   10 || options.min_exponent_digits >= 2) { buf[pos++] = static_cast<char>('0' + exponent /   10); exponent %=   10; }
    buf[pos++] = static_cast<char>('0' + exponent % 10);
}

static void CreateExponentialRepresentation(Vector buf, int num_digits, int exponent, int precision, DtoaOptions const& options)
{
    assert(options.decimal_point_char != '\0');

    int pos = 0;

    pos += 1; // leading digit
    if (num_digits > 1)
    {
        std::copy_backward(buf.begin() + pos, buf.begin() + (pos + num_digits - 1), buf.begin() + (pos + num_digits));
        buf[pos] = options.decimal_point_char;
        pos += 1 + (num_digits - 1);

        if (precision > num_digits - 1)
        {
            const int nz = precision - (num_digits - 1);
            std::fill_n(buf.begin() + pos, nz, '0');
            pos += nz;
        }
    }
    else if (precision > 0)
    {
        std::fill_n(buf.begin() + pos, 1 + precision, '0');
        buf[pos] = options.decimal_point_char;
        pos += 1 + precision;
    }
    else // precision <= 0
    {
        if (options.use_alternative_form)
            buf[pos++] = options.decimal_point_char;
    }

    AppendExponent(buf, pos, exponent, options);
}

static int ComputeExponentialRepresentationLength(int num_digits, int exponent, int precision, DtoaOptions const& options)
{
    assert(num_digits > 0);
    assert(exponent > -10000);
    assert(exponent <  10000);
    assert(precision < 0 || precision >= num_digits - 1);

    int len = 0;

    len += num_digits;
    if (num_digits > 1)
    {
        len += 1; // decimal point
        if (precision > num_digits - 1)
            len += precision - (num_digits - 1);
    }
    else if (precision > 0)
    {
        len += 1 + precision;
    }
    else // precision <= 0
    {
        if (options.use_alternative_form)
            len += 1; // decimal point
    }

    return len + ComputeExponentLength(exponent, options);
}

// v = buf * 10^(decpt - num_digits)
//
// Fixed number of digits (independent of the decimal point).
static bool GeneratePrecisionDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 1);

    using namespace double_conversion;

    const Double d { v };

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);
    assert(requested_digits >= 0);

    if (requested_digits == 0)
    {
        *num_digits = 0;
        *decpt = 0;
        return true;
    }

    if (vec.length() < requested_digits + 1/*null*/)
        return false;

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return true;
    }

    const bool fast_worked = FastDtoa(v, FAST_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
        BignumDtoa(v, BIGNUM_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);

    return true;
}

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
//
static DtoaResult DtoaExponential(char* first, char* last, double d, int precision, DtoaOptions const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    if (!GeneratePrecisionDigits(d, precision + 1, buf, &num_digits, &decpt))
        return { last, -1 };

    assert(num_digits > 0);

    const int exponent = decpt - 1;
    const int exponential_len = ComputeExponentialRepresentationLength(num_digits, exponent, precision, options);

    if (last - first < exponential_len)
        return { last, -1 };

    CreateExponentialRepresentation(Vector(first, exponential_len), num_digits, exponent, precision, options);
    return { first + exponential_len, 0 };
}

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
//
static DtoaResult DtoaGeneral(char* first, char* last, double d, int precision, DtoaOptions const& options)
{
    assert(precision >= 0);

    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    const int P = precision == 0 ? 1 : precision;

    if (!GeneratePrecisionDigits(d, P, buf, &num_digits, &decpt))
        return { last, -1 };

    assert(num_digits > 0);
    assert(num_digits == P);

    const int X = decpt - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && first[num_digits - 1] == '0') {
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

        const int output_len = ComputeFixedRepresentationLength(num_digits, decpt, prec, options);
        if (last - first >= output_len)
        {
            CreateFixedRepresentation(Vector(first, output_len), num_digits, decpt, prec, options);
            return { first + output_len, 0 };
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

        const int output_len = ComputeExponentialRepresentationLength(num_digits, X, prec, options);
        if (last - first >= output_len)
        {
            CreateExponentialRepresentation(Vector(first, output_len), num_digits, X, prec, options);
            return { first + output_len, 0 };
        }
    }

    return { last, -1 };
}

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

static void GenerateHexDigits(double v, int precision, bool normalize, bool upper, Vector buffer, int* num_digits, int* binary_exponent)
{
    const char* const xdigits = upper
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    const Double d { v };

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
//
static DtoaResult DtoaHex(char* first, char* last, double d, int precision, DtoaOptions const& options)
{
    assert(static_cast<size_t>(last - first) >= 52/4 + 1);

    int num_digits = 0;
    int binary_exponent = 0;

    GenerateHexDigits(
            d,
            precision,
            options.normalize,
            options.use_upper_case_digits,
            Vector(first, 52/4 + 1),
            &num_digits,
            &binary_exponent);

    assert(num_digits > 0);

    const int output_len = ComputeExponentialRepresentationLength(num_digits, binary_exponent, precision, options);
    if (last - first >= output_len)
    {
        CreateExponentialRepresentation(Vector(first, output_len), num_digits, binary_exponent, precision, options);
        return { first + output_len, 0 };
    }

    return { last, -1 };
}

enum struct DtoaShortStyle {
    fixed,
    exponential,
    general,
};

static int ComputePrecisionForShortFixedRepresentation(int num_digits, int decpt)
{
    if (num_digits <= decpt)
        return 0;

    if (0 < decpt)
        return num_digits - decpt;

    return -decpt + num_digits;
}

// v = buf * 10^(decpt - num_digits)
//
// Produce the shortest correct representation.
// For example the output of 0.299999999999999988897 is (the less accurate but correct) 0.3.
static void GenerateShortestDigits(double v, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 17 + 1/*null*/);

    using namespace double_conversion;

    const Double d { v };

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    const bool fast_worked = FastDtoa(v, FAST_DTOA_SHORTEST, 0, vec, num_digits, decpt);
    if (!fast_worked)
        BignumDtoa(v, BIGNUM_DTOA_SHORTEST, -1, vec, num_digits, decpt);
}

static DtoaResult DtoaShortest(char* first, char* last, double d, DtoaShortStyle style, DtoaOptions const& options)
{
    assert(static_cast<size_t>(last - first) >= 17 + 1);

    int num_digits = 0;
    int decpt = 0;

    GenerateShortestDigits(d, Vector(first, 17 + 1), &num_digits, &decpt);

    assert(num_digits > 0);

    const int fixed_precision = ComputePrecisionForShortFixedRepresentation(num_digits, decpt);
    const int fixed_len       = ComputeFixedRepresentationLength(num_digits, decpt, fixed_precision, options);
    const int exponent        = decpt - 1;
    const int exponential_len = ComputeExponentialRepresentationLength(num_digits, exponent, num_digits - 1, options);

    const bool use_fixed
        = (style == DtoaShortStyle::fixed) ||
          (style == DtoaShortStyle::general && fixed_len <= exponential_len);

    if (use_fixed)
    {
        if (last - first >= fixed_len)
        {
            CreateFixedRepresentation(Vector(first, fixed_len), num_digits, decpt, fixed_precision, options);
            return { first + fixed_len, 0 };
        }
    }
    else
    {
        if (last - first >= exponential_len)
        {
            CreateExponentialRepresentation(Vector(first, exponential_len), num_digits, exponent, num_digits - 1, options);
            return { first + exponential_len, 0 };
        }
    }

    return { last, -1 };
}

static errc WriteDouble(FormatBuffer& fb, FormatSpec const& spec, double x)
{
    DtoaOptions options;

    options.use_upper_case_digits       = false;
    options.normalize                   = false;
    options.thousands_sep               = spec.tsep;
    options.decimal_point_char          = '.';
    options.use_alternative_form        = spec.hash != '\0';
    options.min_exponent_digits         = 2;
    options.exponent_char               = '\0';
    options.emit_positive_exponent_sign = true;

    char        conv = spec.conv;
    int         prec = spec.prec;
    char const* prefix = nullptr;
    size_t      nprefix = 0;

    switch (conv)
    {
    default:
        // I'm sorry Dave, I'm afraid I can't do that.
    case '\0':
        conv = 's';
    case 'S':
    case 's':
        options.use_alternative_form = false;
        options.exponent_char = 'e';
        options.min_exponent_digits = 1;
        break;
    case 'E':
    case 'e':
        options.exponent_char = conv;
        if (prec < 0)
            prec = 6;
        break;
    case 'F':
    case 'f':
        if (prec < 0)
            prec = 6;
        break;
    case 'G':
    case 'g':
        options.exponent_char = (conv == 'g') ? 'e' : 'E';
        if (prec < 0)
            prec = 6;
        break;
    case 'A':
        options.use_upper_case_digits = true;
        options.exponent_char = 'P';
        options.min_exponent_digits = 1;
        prefix = "0X";
        nprefix = 2; // Always add a prefix. Like printf.
        break;
    case 'a':
        options.exponent_char = 'p';
        options.min_exponent_digits = 1;
        prefix = "0x";
        nprefix = 2; // Always add a prefix. Like printf.
        break;
    case 'X':
        options.use_upper_case_digits = true;
    case 'x':
        options.normalize = true;
        options.use_alternative_form = false;
        options.exponent_char = 'p';
        options.min_exponent_digits = 1;
        prefix = "0x";
        nprefix = (spec.hash != '\0') ? 2u : 0u; // Add a prefix only if '#' was specified. As with integers.
        break;
    }

    const Double d { x };

    const bool   neg = (d.Sign() != 0);
    const double abs_x = d.Abs();
    const char   sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
    {
        const bool upper = ('A' <= conv && conv <= 'Z');

        if (d.IsNaN())
            return WriteRawString(fb, spec, upper ? "NAN" : "nan");

        const char inf[] = { sign, upper ? 'I' : 'i', upper ? 'N' : 'n', upper ? 'F' : 'f', '\0' };
        return WriteRawString(fb, spec, inf + (sign == '\0' ? 1 : 0));
    }

    const int kBufSize = 1500;
    char buf[kBufSize];

    DtoaResult res;
    switch (conv)
    {
    case 's':
    case 'S':
        res = DtoaShortest(buf, buf + kBufSize, abs_x, DtoaShortStyle::general, options);
        break;
    case 'f':
    case 'F':
        res = DtoaFixed(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'e':
    case 'E':
        res = DtoaExponential(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'g':
    case 'G':
        res = DtoaGeneral(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'a':
    case 'A':
    case 'x':
    case 'X':
        res = DtoaHex(buf, buf + kBufSize, abs_x, prec, options);
        break;
    }

    if (res.ec)
        return WriteRawString(fb, spec, "[[internal buffer too small]]");

    const size_t ndigits = static_cast<size_t>(res.next - buf);
    return WriteNumber(fb, spec, sign, prefix, nprefix, buf, ndigits);
}

//
// Parse a non-negative integer in the range [0, INT_MAX].
// Returns -1 on overflow.
//
// PRE: IsDigit(*s) == true.
//
static int ParseInt(const char*& s, const char* end)
{
    int x = *s - '0';

    while (++s != end && IsDigit(*s))
    {
        if (x > INT_MAX / 10 || *s - '0' > INT_MAX - 10*x)
            return -1;
        x = 10*x + (*s - '0');
    }

    return x;
}

static void FixFormatSpec(FormatSpec& spec)
{
    if (spec.align != '\0' && !IsAlign(spec.align))
        spec.align = '>';

    if (spec.sign != '\0' && !IsSign(spec.sign))
        spec.sign = '-';

    if (spec.width < 0)
    {
        if (spec.width < -INT_MAX)
            spec.width = -INT_MAX;
        spec.align = '<';
        spec.width = -spec.width;
    }
}

static errc ParseFormatSpec(FormatSpec& spec, const char*& f, const char* end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end);

    if (*f == '*')
    {
        ++f;
        if (f == end)
            return errc::invalid_format_string; // missing '}'

        int spec_index = -1;
        if (IsDigit(*f))
        {
            spec_index = ParseInt(f, end);
            if (spec_index < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }
        else
        {
            spec_index = nextarg++;
        }

        if (types[spec_index] != Types::T_FORMATSPEC)
            return errc::invalid_argument;

        spec = *static_cast<FormatSpec const*>(args[spec_index].pvoid);
        FixFormatSpec(spec);
    }

    if (*f == ':')
    {
        ++f;
        if (f == end)
            return errc::invalid_format_string; // missing '}'

        if (f + 1 != end && IsAlign(*(f + 1)))
        {
            spec.fill = *f++;
            spec.align = *f++;
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }
        else if (IsAlign(*f))
        {
            spec.align = *f++;
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }

        for (;;) // Parse flags
        {
            if (IsSign(*f))
                spec.sign = *f;
            else if (*f == '#')
                spec.hash = *f;
            else if (*f == '0')
                spec.zero = *f;
            else if (*f == '\'' || *f == '_')
                spec.tsep = *f;
            else
                break;

            ++f;
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }

        if (IsDigit(*f))
        {
            const int i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing '}'
            spec.width = i;
        }

        if (*f == '.')
        {
            ++f;
            if (f == end || !IsDigit(*f))
                return errc::invalid_format_string; // missing '}' or digit expected
            const int i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing '}'
            spec.prec = i;
        }

        if (*f != ',' && *f != '}')
        {
            spec.conv = *f++;
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }
    }

    if (*f == ',')
    {
        const auto f0 = ++f;

        while (f != end && *f != '}')
            ++f;

        spec.style = { f0, static_cast<size_t>(f - f0) };
    }

    if (f == end || *f != '}')
        return errc::invalid_format_string; // missing '}'

    return errc::success;
}

static errc CallFormatFunc(FormatBuffer& fb, FormatSpec const& spec, int index, Types types, Arg const* args)
{
    const auto type = types[index];

    if (type == Types::T_NONE)
        return errc::index_out_of_range;

    const auto& arg = args[index];

    switch (type)
    {
    case Types::T_NONE:
        break; // unreachable -- fix warning
    case Types::T_OTHER:
        return arg.other.func(fb, spec, arg.other.value);
    case Types::T_STRING:
        return WriteString(fb, spec, arg.string.data(), arg.string.size());
    case Types::T_PVOID:
        return WritePointer(fb, spec, arg.pvoid);
    case Types::T_PCHAR:
        return WriteString(fb, spec, arg.pchar);
    case Types::T_CHAR:
        return WriteChar(fb, spec, arg.char_);
    case Types::T_BOOL:
        return WriteBool(fb, spec, arg.bool_);
    case Types::T_SCHAR:
        return WriteInt(fb, spec, arg.schar, static_cast<unsigned char>(arg.schar));
    case Types::T_SSHORT:
        return WriteInt(fb, spec, arg.sshort, static_cast<unsigned short>(arg.sshort));
    case Types::T_SINT:
        return WriteInt(fb, spec, arg.sint, static_cast<unsigned int>(arg.sint));
    case Types::T_SLONGLONG:
        return WriteInt(fb, spec, arg.slonglong, static_cast<unsigned long long>(arg.slonglong));
    case Types::T_ULONGLONG:
        return WriteInt(fb, spec, 0, arg.ulonglong);
    case Types::T_DOUBLE:
        return WriteDouble(fb, spec, arg.double_);
    case Types::T_FORMATSPEC:
        return WriteRawString(fb, spec, "[[error]]");
    }

    return errc::success; // unreachable -- fix warning
}

// Internal API
errc fmtxx::impl::DoFormat(FormatBuffer& fb, std::string_view format, Types types, Arg const* args)
{
    if (format.empty())
        return errc::success;

    int nextarg = 0;

    const char*       f   = format.data();
    const char* const end = f + format.size();
    const char*       s   = f;
    for (;;)
    {
        while (f != end && *f != '{' && *f != '}')
            ++f;

        if (f != s && !fb.Write(s, static_cast<size_t>(f - s)))
            return errc::io_error;

        if (f == end) // done.
            break;

        const char c = *f++; // skip '{' or '}'

        if (*f == c) // '{{' or '}}'
        {
            s = f++;
            continue;
        }

        if (c == '}')
            return errc::invalid_format_string; // stray '}'
        if (f == end)
            return errc::invalid_format_string; // missing '}'

        int index = -1;
        if (IsDigit(*f))
        {
            index = ParseInt(f, end);
            if (index < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing '}'
        }

        FormatSpec spec;
        if (*f != '}')
        {
            const errc ec = ParseFormatSpec(spec, f, end, nextarg, types, args);
            if (ec != errc::success)
                return ec;
        }

        if (index < 0)
            index = nextarg++;

        const errc ec = CallFormatFunc(fb, spec, index, types, args);
        if (ec != errc::success)
            return ec;

        if (f == end) // done.
            break;

        s = ++f; // skip '}'
    }

    return errc::success;
}

// Internal API
errc fmtxx::impl::DoFormat(std::string& os, std::string_view format, Types types, Arg const* args)
{
    StringBuffer fb { os };
    return DoFormat(fb, format, types, args);
}

// Internal API
errc fmtxx::impl::DoFormat(std::FILE* os, std::string_view format, Types types, Arg const* args)
{
    FILEBuffer fb { os };
    return DoFormat(fb, format, types, args);
}

// Internal API
errc fmtxx::impl::DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    const std::ostream::sentry se(os);
    if (se)
    {
        StreamBuffer fb { os };
        return DoFormat(fb, format, types, args);
    }

    return errc::io_error;
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
