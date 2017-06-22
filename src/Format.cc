// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"

#include "double-conversion/bignum-dtoa.h"
#include "double-conversion/fast-dtoa.h"
#include "double-conversion/fixed-dtoa.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <ostream>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(X) 0
#endif

#if _MSC_VER || __cplusplus >= 201703 || __has_cpp_attribute(fallthrough)
#define FALLTHROUGH [[fallthrough]]
#else
#define FALLTHROUGH
#endif

// 0: assert-no-check   (unsafe; invalid format strings -> UB)
// 1: assert-check      (safe)
// 2: throw             (safe)
#define FORMAT_STRING_CHECK_POLICY 1

using namespace fmtxx;
using namespace fmtxx::impl;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

// Maximum supported minimum field width.
// This is not an implementation limit, but its just a sane upper bound.
enum { kMaxFieldWidth = 1024 * 8 };

// Maximum supported integer precision (= minimum number of digits).
enum { kMaxIntPrec = 256 };

// Maximum supported floating point precision.
// Precision required for denorm_min (= [751 digits] 10^-323) when using %f
enum { kMaxFloatPrec = 751 + 323 };

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename T> inline constexpr T Min(T x, T y) { return y < x ? y : x; }
template <typename T> inline constexpr T Max(T x, T y) { return y < x ? x : y; }

template <typename T>
inline constexpr T Clip(T x, T lower, T upper) { return Min(Max(lower, x), upper); }

template <typename T>
inline void UnusedParameter(T&&) {}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

fmtxx::Writer::~Writer()
{
}

bool fmtxx::StringWriter::Put(char c)
{
    os.push_back(c);
    return true;
}

bool fmtxx::StringWriter::Write(char const* str, size_t len)
{
    os.append(str, len);
    return true;
}

bool fmtxx::StringWriter::Pad(char c, size_t count)
{
    os.append(count, c);
    return true;
}

bool fmtxx::FILEWriter::Put(char c) noexcept
{
    return EOF != std::fputc(c, os);
}

bool fmtxx::FILEWriter::Write(char const* str, size_t len) noexcept
{
    return len == std::fwrite(str, 1, len, os);
}

bool fmtxx::FILEWriter::Pad(char c, size_t count) noexcept
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        auto const n = Min(count, kBlockSize);

        if (n != std::fwrite(block, 1, n, os))
            return false;

        count -= n;
    }

    return true;
}

bool fmtxx::StreamWriter::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof()))
    {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

bool fmtxx::StreamWriter::Write(char const* str, size_t len)
{
    auto const kMaxLen = static_cast<size_t>( std::numeric_limits<std::streamsize>::max() );

    while (len > 0)
    {
        auto const n = Min(len, kMaxLen);

        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(str, k))
        {
            os.setstate(std::ios_base::badbit);
            return false;
        }

        str += n;
        len -= n;
    }

    return true;
}

bool fmtxx::StreamWriter::Pad(char c, size_t count)
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        auto const n = Min(count, kBlockSize);

        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(block, k))
        {
            os.setstate(std::ios_base::badbit);
            return false;
        }

        count -= n;
    }

    return true;
}

bool fmtxx::CharArrayWriter::Put(char c) noexcept
{
    if (os.next >= os.last)
        return false;

    *os.next++ = c;
    return true;
}

bool fmtxx::CharArrayWriter::Write(char const* str, size_t len) noexcept
{
    size_t const n = Min(len, static_cast<size_t>(os.last - os.next));

    std::memcpy(os.next, str, n);
    os.next += n;
    return n == len;
}

bool fmtxx::CharArrayWriter::Pad(char c, size_t count) noexcept
{
    size_t const n = Min(count, static_cast<size_t>(os.last - os.next));

    std::memset(os.next, static_cast<unsigned char>(c), n);
    os.next += n;
    return n == count;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static char ComputeSignChar(bool neg, Sign sign, char fill)
{
    if (neg)
        return '-';
    if (sign == Sign::Plus)
        return '+';
    if (sign == Sign::Space)
        return fill;

    return '\0';
}

struct Padding {
    size_t left       = 0;
    size_t after_sign = 0;
    size_t right      = 0;
};

static Padding ComputePadding(size_t len, Align align, int width)
{
    assert(width >= 0); // internal error

    if (width > kMaxFieldWidth)
        width = kMaxFieldWidth;

    Padding pad;

    size_t const w = static_cast<size_t>(width);
    if (w > len)
    {
        size_t const d = w - len;
        switch (align)
        {
        case Align::Default:
        case Align::Right:
            pad.left = d;
            break;
        case Align::Left:
            pad.right = d;
            break;
        case Align::Center:
            pad.left = d/2;
            pad.right = d - d/2;
            break;
        case Align::PadAfterSign:
            pad.after_sign = d;
            break;
        }
    }

    return pad;
}

// Prints out exactly LEN characters (including '\0's) starting at STR,
// possibly padding on the left and/or right.
static errc PrintAndPadString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    auto const pad = ComputePadding(len, spec.align, spec.width);

    if (pad.left > 0  && !w.Pad(spec.fill, pad.left))
        return errc::io_error;
    if (len > 0       && !w.Write(str, len))
        return errc::io_error;
    if (pad.right > 0 && !w.Pad(spec.fill, pad.right))
        return errc::io_error;

    return errc::success;
}

static errc PrintAndPadString(Writer& w, FormatSpec const& spec, std::string_view str)
{
    return PrintAndPadString(w, spec, str.data(), str.size());
}

template <typename F>
static bool ForEachEscaped(char const* str, size_t len, F func)
{
    for (size_t i = 0; i < len; ++i)
    {
        char const ch = str[i];

        bool res = true;
        switch (ch)
        {
        case '"':
        case '\\':
//      case '\'':
//      case '?':
            res = func('\\') && func(ch);
            break;
//      case '\a':
//          res = func('\\') && func('a');
//          break;
//      case '\b':
//          res = func('\\') && func('b');
//          break;
//      case '\f':
//          res = func('\\') && func('f');
//          break;
//      case '\n':
//          res = func('\\') && func('n');
//          break;
//      case '\r':
//          res = func('\\') && func('r');
//          break;
//      case '\t':
//          res = func('\\') && func('t');
//          break;
//      case '\v':
//          res = func('\\') && func('v');
//          break;
        default:
            res = func(ch);
            break;
        }

        if (!res)
            return false;
    }

    return true;
}

static errc PrintAndPadQuotedString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t quoted_len = 0;
    ForEachEscaped(str, len, [&](char) { ++quoted_len; return true; });

    auto const pad = ComputePadding(2 + quoted_len, spec.align, spec.width);

    if (pad.left > 0 && !w.Pad(spec.fill, pad.left))
        return errc::io_error;
    if (!w.Put('"'))
        return errc::io_error;
    if (len > 0)
    {
        if (len < quoted_len)
        {
            if (!ForEachEscaped(str, len, [&](char ch) { return w.Put(ch); }))
                return errc::io_error;
        }
        else
        {
            if (!w.Write(str, len))
                return errc::io_error;
        }
    }
    if (!w.Put('"'))
        return errc::io_error;
    if (pad.right > 0 && !w.Pad(spec.fill, pad.right))
        return errc::io_error;

    return errc::success;
}

errc fmtxx::Util::format_string(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t const n = (spec.prec >= 0)
        ? Min(len, static_cast<size_t>(spec.prec))
        : len;

    switch (spec.conv) {
    default:
        return PrintAndPadString(w, spec, str, n);
    case 'q':
        return PrintAndPadQuotedString(w, spec, str, n);
    }
}

errc fmtxx::Util::format_string(Writer& w, FormatSpec const& spec, char const* str)
{
    if (str == nullptr)
        return PrintAndPadString(w, spec, "(null)");

    // Use strnlen if a precision was specified.
    // The string may not be null-terminated!
    size_t const len = (spec.prec >= 0)
        ? ::strnlen(str, static_cast<size_t>(spec.prec))
        : ::strlen(str);

    switch (spec.conv) {
    default:
        return PrintAndPadString(w, spec, str, len);
    case 'q':
        return PrintAndPadQuotedString(w, spec, str, len);
    }
}

static errc PrintAndPadNumber(Writer& w, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    size_t const len = (sign ? 1u : 0u) + nprefix + ndigits;

    auto const pad = ComputePadding(len, spec.zero ? Align::PadAfterSign : spec.align, spec.width);

    if (pad.left > 0       && !w.Pad(spec.fill, pad.left))
        return errc::io_error;
    if (sign != '\0'       && !w.Put(sign))
        return errc::io_error;
    if (nprefix > 0        && !w.Write(prefix, nprefix))
        return errc::io_error;
    if (pad.after_sign > 0 && !w.Pad(spec.zero ? '0' : spec.fill, pad.after_sign))
        return errc::io_error;
    if (ndigits > 0        && !w.Write(digits, ndigits))
        return errc::io_error;
    if (pad.right > 0      && !w.Pad(spec.fill, pad.right))
        return errc::io_error;

    return errc::success;
}

static constexpr char const* kDecDigits100/*[100*2 + 1]*/ =
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

static char* DecIntToAsciiBackwards(char* last/*[-20]*/, uint64_t n)
{
    while (n >= 100)
    {
        auto const q = n / 100;
        auto const r = n % 100;
        *--last = kDecDigits100[2*r + 1];
        *--last = kDecDigits100[2*r + 0];
        n = q;
    }

    if (n >= 10)
    {
        *--last = kDecDigits100[2*n + 1];
        *--last = kDecDigits100[2*n + 0];
    }
    else
    {
        *--last = kDecDigits100[2*n + 1];
    }

    return last;
}

static constexpr char const* kUpperHexDigits = "0123456789ABCDEF";
static constexpr char const* kLowerHexDigits = "0123456789abcdef";

static char* IntToAsciiBackwards(char* last/*[-64]*/, uint64_t n, int base, bool capitals)
{
    char const* const xdigits = capitals ? kUpperHexDigits : kLowerHexDigits;

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
        do *--last = kUpperHexDigits[n & 7]; while (n >>= 3);
        return last;
    case 2:
        do *--last = kUpperHexDigits[n & 1]; while (n >>= 1);
        return last;
    }

    assert(!"not implemented"); // internal error
    return last;
}

// Inserts thousands separators into [buf, +pos).
// Returns the number of separators inserted.
static int InsertThousandsSep(char* buf, int pos, char sep, int group_len)
{
    assert(pos >= 0);
    assert(sep != '\0');
    assert(group_len > 0);

    int const nsep = (pos - 1) / group_len;

    if (nsep <= 0)
        return 0;

    char* src = buf + pos;
    char* dst = buf + (pos + nsep);

    for (int i = 0; i < nsep; ++i)
    {
        for (int j = 0; j < group_len; ++j)
            *--dst = *--src;
        *--dst = sep;
    }

    return nsep;
}

errc fmtxx::Util::format_int(Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext)
{
    uint64_t number = zext;
    char     conv = spec.conv;
    char     sign = '\0';
    int      base = 10;
    size_t   nprefix = 0;

    switch (conv)
    {
    default:
        conv = 'd';
        FALLTHROUGH;
    case 'd':
    case 'i':
        sign = ComputeSignChar(sext < 0, spec.sign, spec.fill);
        if (sext < 0)
            number = 0 - static_cast<uint64_t>(sext);
        FALLTHROUGH;
    case 'u':
        base = 10;
        break;
    case 'x':
    case 'X':
        base = 16;
        nprefix = spec.hash ? 2 : 0;
        break;
    case 'b':
    case 'B':
        base = 2;
        nprefix = spec.hash ? 2 : 0;
        break;
    case 'o':
        base = 8;
        nprefix = (spec.hash && number != 0) ? 1 : 0;
        break;
    }

    bool const upper = ('A' <= conv && conv <= 'Z');

    static_assert(kMaxIntPrec >= 64, "at least 64-characters are required for UINT64_MAX in base 2");

    enum { kMaxSeps = (kMaxIntPrec - 1) / 3 };
    enum { kBufSize = kMaxIntPrec + kMaxSeps };

    char buf[kBufSize];

    char* l = buf + kMaxIntPrec;
    char* f = IntToAsciiBackwards(l, number, base, upper);

    if (spec.prec >= 0)
    {
        int const prec = Min(spec.prec, static_cast<int>(kMaxIntPrec));
        while (l - f < prec) {
            *--f = '0';
        }
    }

    if (spec.tsep != '\0') {
        l += InsertThousandsSep(f, static_cast<int>(l - f), spec.tsep, base == 10 ? 3 : 4);
    }

    char const prefix[] = {'0', conv};
    return PrintAndPadNumber(w, spec, sign, prefix, nprefix, f, static_cast<size_t>(l - f));
}

errc fmtxx::Util::format_bool(Writer& w, FormatSpec const& spec, bool val)
{
    switch (spec.conv)
    {
    default:
        return PrintAndPadString(w, spec, val ? "true" : "false");
    case 'y':
        return PrintAndPadString(w, spec, val ? "yes" : "no");
    case 'o':
        return PrintAndPadString(w, spec, val ? "on" : "off");
    }
}

errc fmtxx::Util::format_char(Writer& w, FormatSpec const& spec, char ch)
{
    switch (spec.conv)
    {
    default:
        return PrintAndPadString(w, spec, &ch, 1u);
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
    case 'o':
        return Util::format_int(w, spec, ch);
    }
}

errc fmtxx::Util::format_pointer(Writer& w, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return PrintAndPadString(w, spec, "(nil)");

    FormatSpec fs = spec;
    switch (fs.conv)
    {
    default:
        if (fs.prec < 0)
            fs.prec = 2 * sizeof(uintptr_t);
        fs.hash = true;
        fs.conv = 'x';
        break;
    case 'S':
    case 'P':
        if (fs.prec < 0)
            fs.prec = 2 * sizeof(uintptr_t);
        fs.hash = true;
        fs.conv = 'X';
        break;
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
    case 'o':
        break;
    }

    return Util::format_int(w, fs, reinterpret_cast<uintptr_t>(pointer));
}

namespace {

struct Double
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

} // namespace

namespace dtoa {

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

using Vector = double_conversion::Vector<char>;

template <typename RanIt>
static auto MakeArrayIterator(RanIt first, intptr_t n)
{
#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL > 0 && _SECURE_SCL_DEPRECATE)
    return stdext::make_checked_array_iterator(first, n);
#else
    UnusedParameter(n);
    return first;
#endif
}

static int InsertThousandsSep(Vector buf, int pt, int last, char sep)
{
    assert(pt >= 0);
    assert(pt <= last);
    assert(sep != '\0');

    int const nsep = (pt - 1) / 3;

    if (nsep <= 0)
        return 0;

    auto I = MakeArrayIterator(buf.start(), buf.length());
    std::copy_backward(I + pt, I + last, I + (last + nsep));

    for (int l = pt - 1, shift = nsep; shift > 0; --shift, l -= 3)
    {
        buf[l - 0 + shift] = buf[l - 0];
        buf[l - 1 + shift] = buf[l - 1];
        buf[l - 2 + shift] = buf[l - 2];
        buf[l - 3 + shift] = sep;
    }

    return nsep;
}

static int CreateFixedRepresentation(Vector buf, int num_digits, int decpt, int precision, Options const& options)
{
    assert(options.decimal_point != '\0');

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
            buf[num_digits + 1] = options.decimal_point;

            // digits0.[000][000] --> 0.[000]digits[000]
            std::rotate(buf.start(), buf.start() + num_digits, buf.start() + (num_digits + 2 + -decpt));

            return 2 + precision;
        }
        else
        {
            buf[0] = '0';
            if (options.use_alternative_form)
                buf[1] = options.decimal_point;

            return 1 + (options.use_alternative_form ? 1 : 0);
        }
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
            buf[decpt] = options.decimal_point;
        }

        last = decpt + nextra;
    }
    else
    {
        // dig.its[000]

        auto I = MakeArrayIterator(buf.start(), buf.length());

        assert(precision >= num_digits - decpt); // >= 1

        // digits --> dig.its
        std::copy_backward(I + decpt, I + num_digits, I + (num_digits + 1));
        I[decpt] = options.decimal_point;
        // dig.its --> dig.its[000]
        std::fill_n(I + (num_digits + 1), precision - (num_digits - decpt), '0');

        last = decpt + 1 + precision;
    }

    if (options.thousands_sep != '\0')
    {
        last += InsertThousandsSep(buf, decpt, last, options.thousands_sep);
    }

    return last;
}

static void GenerateFixedDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    Double const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);
    assert(requested_digits >= 0);

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    bool const fast_worked = FastFixedDtoa(v, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decpt);
    }
}

static int ToFixed(char* first, char* last, double d, int precision, Options const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    GenerateFixedDigits(d, precision, buf, &num_digits, &decpt);

    assert(num_digits >= 0);

    return CreateFixedRepresentation(buf, num_digits, decpt, precision, options);
}

// Append a decimal representation of EXPONENT to BUF.
// Returns the number of characters written.
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
    assert(options.decimal_point != '\0');

    int pos = 0;

    pos += 1; // leading digit
    if (num_digits > 1)
    {
        // d.igits[000]e+123

        auto I = MakeArrayIterator(buf.start(), buf.length());

        std::copy_backward(I + pos, I + (pos + num_digits - 1), I + (pos + num_digits));
        I[pos] = options.decimal_point;
        pos += 1 + (num_digits - 1);

        if (precision > num_digits - 1)
        {
            int const nzeros = precision - (num_digits - 1);
            std::fill_n(I + pos, nzeros, '0');
            pos += nzeros;
        }
    }
    else if (precision > 0)
    {
        // d.0[000]e+123

        std::fill_n(buf.start() + pos, 1 + precision, '0');
        buf[pos] = options.decimal_point;
        pos += 1 + precision;
    }
    else
    {
        // d[.]e+123

        if (options.use_alternative_form)
            buf[pos++] = options.decimal_point;
    }

    return AppendExponent(buf, pos, exponent, options);
}

static void GeneratePrecisionDigits(double v, int requested_digits, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 1);
    assert(requested_digits >= 0);

    Double const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (requested_digits == 0)
    {
        *num_digits = 0;
        *decpt = 0;
        return;
    }

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    bool const fast_worked = FastDtoa(v, double_conversion::FAST_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    }
}

static int ToExponential(char* first, char* last, double d, int precision, Options const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    GeneratePrecisionDigits(d, precision + 1, buf, &num_digits, &decpt);

    assert(num_digits > 0);

    int const exponent = decpt - 1;
    return CreateExponentialRepresentation(buf, num_digits, exponent, precision, options);
}

static int ToGeneral(char* first, char* last, double d, int precision, Options const& options)
{
    assert(precision >= 0);

    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int decpt = 0;

    int const P = precision == 0 ? 1 : precision;

    GeneratePrecisionDigits(d, P, buf, &num_digits, &decpt);

    assert(num_digits > 0);
    assert(num_digits <= P); // GeneratePrecisionDigits is allowed to return fewer digits if they are all 0's.

    int const X = decpt - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && first[num_digits - 1] == '0')
    {
        --num_digits;
    }

    if (-4 <= X && X < P)
    {
        int prec = P - decpt;
        if (!options.use_alternative_form)
        {
            prec = Min(prec, num_digits - decpt);
        }

        return CreateFixedRepresentation(buf, num_digits, decpt, prec, options);
    }
    else
    {
        int prec = P - 1;
        if (!options.use_alternative_form)
        {
            prec = Min(prec, num_digits - 1);
        }

        return CreateExponentialRepresentation(buf, num_digits, X, prec, options);
    }
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
    assert(buffer.length() >= 52/4 + 1);

    char const* const xdigits = upper ? kUpperHexDigits : kLowerHexDigits;

    Double const d{v};

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
        uint64_t const digit = sig         >> (52 - 4 * precision - 4);
        uint64_t const r     = uint64_t{1} << (52 - 4 * precision    );

        assert(!normalize || (sig & Double::kHiddenBit) == 0);

        if (digit & 0x8) // Digit >= 8
        {
            sig += r; // Round...
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

static int ToHex(char* first, char* last, double d, int precision, Options const& options)
{
    Vector buf(first, static_cast<int>(last - first));

    int num_digits = 0;
    int binary_exponent = 0;

    GenerateHexDigits(d, precision, options.normalize, options.use_upper_case_digits, buf, &num_digits, &binary_exponent);

    assert(num_digits > 0);

    return CreateExponentialRepresentation(buf, num_digits, binary_exponent, precision, options);
}

static void GenerateShortestDigits(double v, Vector vec, int* num_digits, int* decpt)
{
    assert(vec.length() >= 17 + 1 /*null*/);

    Double const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (d.IsZero())
    {
        vec[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    bool const fast_worked = FastDtoa(v, double_conversion::FAST_DTOA_SHORTEST, -1, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_SHORTEST, -1, vec, num_digits, decpt);
    }
}

static int ToECMAScript(char* first, char* last, double d, char decimal_point, char exponent_char)
{
    Vector buf(first, static_cast<int>(last - first));
    assert(buf.length() >= 24);

    auto I = MakeArrayIterator(buf.start(), buf.length());

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

        std::fill_n(I + k, n - k, '0');
        return n;
    }

    if (0 < n && n <= 21)
    {
        // dig.its

        std::copy_backward(I + n, I + k, I + (k + 1));
        I[n] = decimal_point;
        return k + 1;
    }

    if (-6 < n && n <= 0)
    {
        // 0.[000]digits

        std::copy_backward(I, I + k, I + (2 + -n + k));
        I[0] = '0';
        I[1] = decimal_point;
        std::fill_n(I + 2, -n, '0');
        return 2 + (-n) + k;
    }

    // Otherwise use an exponential notation.

    Options options;

    options.min_exponent_digits = 1;
    options.exponent_char = exponent_char;
    options.emit_positive_exponent_sign = true;

    if (k == 1)
    {
        // dE+123

        int const endpos = AppendExponent(buf, /*pos*/ 1, n - 1, options);
        return endpos;
    }
    else
    {
        // d.igitsE+123

        std::copy_backward(I + 1, I + k, I + (k + 1));
        I[1] = decimal_point;
        int const endpos = AppendExponent(buf, /*pos*/ k + 1, n - 1, options);
        return endpos;
    }
}

} // namespace dtoa

static errc HandleSpecialFloat(Double const d, Writer& w, FormatSpec const& spec, char sign, bool upper)
{
    assert(d.IsSpecial());

    if (d.IsNaN())
        return PrintAndPadString(w, spec, upper ? "NAN" : "nan");

    char  inf_lower[] = " inf";
    char  inf_upper[] = " INF";
    char* str = upper ? inf_upper : inf_lower;

    if (sign != '\0')
        *str = sign;
    else
        ++str; // skip leading space

    return PrintAndPadString(w, spec, str);
}

errc fmtxx::Util::format_double(Writer& w, FormatSpec const& spec, double x)
{
    dtoa::Options options;

    options.use_upper_case_digits       = false;
    options.normalize                   = false;
    options.thousands_sep               = spec.tsep;
    options.decimal_point               = '.';
    options.use_alternative_form        = spec.hash;
    options.min_exponent_digits         = 2;
    options.exponent_char               = '\0';
    options.emit_positive_exponent_sign = true;

    char   conv = spec.conv;
    int    prec = spec.prec;
    size_t nprefix = 0;

    switch (conv)
    {
    default:
        conv = 's';
        FALLTHROUGH;
    case 's':
    case 'S':
        options.exponent_char = (conv == 's') ? 'e' : 'E';
        break;
    case 'e':
    case 'E':
        options.exponent_char = conv;
        if (prec < 0)
            prec = 6;
        break;
    case 'f':
    case 'F':
        if (prec < 0)
            prec = 6;
        break;
    case 'g':
    case 'G':
        options.exponent_char = (conv == 'g') ? 'e' : 'E';
        if (prec < 0)
            prec = 6;
        break;
    case 'a':
    case 'A':
        conv = (conv == 'a') ? 'x' : 'X';
        options.use_upper_case_digits = (conv == 'X');
        options.min_exponent_digits   = 1;
        options.exponent_char         = (conv == 'x') ? 'p' : 'P';
        nprefix = 2;
        break;
    case 'x':
    case 'X':
        options.use_upper_case_digits = (conv == 'X');
        options.normalize             = true;
        options.use_alternative_form  = false;
        options.min_exponent_digits   = 1;
        options.exponent_char         = (conv == 'x') ? 'p' : 'P';
        nprefix = spec.hash ? 2 : 0;
        break;
    }

    Double const d { x };

    bool   const neg = (d.Sign() != 0);
    double const abs_x = d.Abs();
    char   const sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
    {
        bool const upper = ('A' <= conv && conv <= 'Z');
        return HandleSpecialFloat(d, w, spec, sign, upper);
    }

    if (prec > kMaxFloatPrec)
        prec = kMaxFloatPrec;

    // Allow printing *ALL* double-precision floating-point values with prec <= kMaxFloatPrec
    // and thousands separators.
    // 
    // Mode         Max length
    // ---------------------------------------------
    // ECMAScript   27
    // Fixed        [309 digits + separators].[prec digits]
    // Scientific   D.[prec digits]E+123
    // Hex          0xH.[prec digits]P+1234
    //
    enum { kMaxDigitsBeforePoint = 309 };
    enum { kMaxSeps = (kMaxDigitsBeforePoint - 1) / 3 };
    enum { kBufSize = kMaxDigitsBeforePoint + kMaxSeps + 1 + kMaxFloatPrec + 1/*null*/ };

    char buf[kBufSize];

    int buflen;
    switch (conv)
    {
    case 's':
    case 'S':
        buflen = dtoa::ToECMAScript(buf, buf + kBufSize, abs_x, options.decimal_point, options.exponent_char);
        break;
    case 'f':
    case 'F':
        buflen = dtoa::ToFixed(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'e':
    case 'E':
        buflen = dtoa::ToExponential(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'g':
    case 'G':
        buflen = dtoa::ToGeneral(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'x':
    case 'X':
        buflen = dtoa::ToHex(buf, buf + kBufSize, abs_x, prec, options);
        break;
    default:
        buflen = 0;
        assert(!"internal error");
        break;
    }

    assert(buflen >= 0);

    char const prefix[] = {'0', conv};
    return PrintAndPadNumber(w, spec, sign, prefix, nprefix, buf, static_cast<size_t>(buflen));
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#if FORMAT_STRING_CHECK_POLICY == 0
#  define EXPECT(EXPR, MSG) (assert(EXPR), true)
#elif FORMAT_STRING_CHECK_POLICY == 1
#  define EXPECT(EXPR, MSG) (assert(EXPR), (EXPR))
#elif FORMAT_STRING_CHECK_POLICY == 2
#  define EXPECT(EXPR, MSG) ((EXPR) ? true : throw std::runtime_error(MSG))
#endif

static void FixNegativeFieldWidth(FormatSpec& spec)
{
    if (spec.width >= 0)
        return;

    spec.width = (spec.width == INT_MIN) ? INT_MAX : -spec.width;
    spec.align = Align::Left;
}

static errc CallFormatFunc(Writer& w, FormatSpec const& spec, Types::value_type type, Arg const& arg)
{
    switch (type)
    {
    case Types::T_NONE:
        assert(!"internal error");
        return errc::success;
    case Types::T_OTHER:
        return arg.other.func(w, spec, arg.other.value);
    case Types::T_STRING:
        return Util::format_string(w, spec, arg.string.data(), arg.string.size());
    case Types::T_PVOID:
        return Util::format_pointer(w, spec, arg.pvoid);
    case Types::T_PCHAR:
        return Util::format_string(w, spec, arg.pchar);
    case Types::T_CHAR:
        return Util::format_char(w, spec, arg.char_);
    case Types::T_BOOL:
        return Util::format_bool(w, spec, arg.bool_);
    case Types::T_SCHAR:
        return Util::format_int(w, spec, arg.schar);
    case Types::T_SSHORT:
        return Util::format_int(w, spec, arg.sshort);
    case Types::T_SINT:
        return Util::format_int(w, spec, arg.sint);
    case Types::T_SLONGLONG:
        return Util::format_int(w, spec, arg.slonglong);
    case Types::T_ULONGLONG:
        return Util::format_int(w, spec, arg.ulonglong);
    case Types::T_DOUBLE:
        return Util::format_double(w, spec, arg.double_);
    case Types::T_FORMATSPEC:
        assert(!"internal error");
        return errc::success;
    }

    return errc::success; // unreachable -- fix warning
}

static bool IsDigit(char ch) { return '0' <= ch && ch <= '9'; }

static int ParseInt(std::string_view::iterator& f, std::string_view::iterator const end)
{
    assert(f != end && IsDigit(*f)); // internal error
    auto const f0 = f;

    int x = *f - '0';

    while (++f != end && IsDigit(*f))
    {
        if ((f - f0) + 1 > std::numeric_limits<int>::digits10)
        {
            if (!EXPECT( x <= INT_MAX / 10 && *f - '0' <= INT_MAX - 10 * x, "integer overflow" ))
            {
                while (++f != end && IsDigit(*f)) {}
                return INT_MAX;
            }
        }

        x = 10 * x + (*f - '0');
    }

    return x;
}

static void GetIntArg(int& value, int index, Types types, Arg const* args)
{
    switch (types[index])
    {
    case Types::T_NONE:
        static_cast<void>(EXPECT(false, "argument index out of range"));
        break;
    case Types::T_SCHAR:
        value = args[index].schar;
        break;
    case Types::T_SSHORT:
        value = args[index].sshort;
        break;
    case Types::T_SINT:
        value = args[index].sint;
        break;
    case Types::T_SLONGLONG:
        value = static_cast<int>( Clip<long long>(args[index].slonglong, INT_MIN, INT_MAX) );
        break;
    case Types::T_ULONGLONG:
        value = static_cast<int>( Min(args[index].ulonglong, static_cast<unsigned long long>(INT_MAX)) );
        break;
    default:
        static_cast<void>(EXPECT(false, "argument is not an integer type"));
        break;
    }
}

static void ParseLBrace(int& value, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end && *f == '{'); // internal error

    ++f;
    if (!EXPECT(f != end, "unexpected end of format-string"))
        return;

    int index;
    if (IsDigit(*f))
    {
        index = ParseInt(f, end);

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
        if (!EXPECT(*f == '}', "unexpected character in dynamic int argument"))
            return;
        ++f;
    }
    else
    {
        index = nextarg++;
    }

    GetIntArg(value, index, types, args);
}

static void ParseFormatSpecArg(FormatSpec& spec, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end && *f == '*');
    UnusedParameter(types);

    ++f;
    if (!EXPECT(f != end, "unexpected end of format-string"))
        return;

    int const index = IsDigit(*f) ? ParseInt(f, end) : nextarg++;

    if (!EXPECT(types[index] == Types::T_FORMATSPEC, "invalid argument: FormatSpec expected"))
        return;

    spec = *static_cast<FormatSpec const*>(args[index].pvoid);
    FixNegativeFieldWidth(spec);
}

static bool ParseAlign(FormatSpec& spec, char c)
{
    switch (c) {
    case '<':
        spec.align = Align::Left;
        return true;
    case '>':
        spec.align = Align::Right;
        return true;
    case '^':
        spec.align = Align::Center;
        return true;
    case '=':
        spec.align = Align::PadAfterSign;
        return true;
    }

    return false;
}

static void ParseFormatSpec(FormatSpec& spec, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end && *f == ':');

    ++f;
    if (!EXPECT(f != end, "unexpected end of format-string"))
        return;

    if (f + 1 != end && ParseAlign(spec, *(f + 1)))
    {
        spec.fill = *f;
        f += 2;
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }
    else if (ParseAlign(spec, *f))
    {
        ++f;
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }

    for (;;)
    {
        switch (*f)
        {
// Flags
        case '-':
            spec.sign = Sign::Minus;
            ++f;
            break;
        case '+':
            spec.sign = Sign::Plus;
            ++f;
            break;
        case ' ':
            spec.sign = Sign::Space;
            ++f;
            break;
        case '#':
            spec.hash = true;
            ++f;
            break;
        case '0':
            spec.zero = true;
            ++f;
            break;
        case '\'':
        case '_':
        case ',':
            spec.tsep = *f;
            ++f;
            break;
// Width
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            spec.width = ParseInt(f, end);
            break;
        case '{':
            ParseLBrace(spec.width, f, end, nextarg, types, args);
            FixNegativeFieldWidth(spec);
            break;
// Precision
        case '.':
            ++f;
            if (!EXPECT(f != end, "unexpected end of format-string"))
                return;
            switch (*f)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                spec.prec = ParseInt(f, end);
                break;
            case '{':
                ParseLBrace(spec.prec, f, end, nextarg, types, args);
                break;
            default:
                spec.prec = 0;
                break;
            }
            break;
// Conversion
        case '!':
        case '}':
            return;
        default:
            spec.conv = *f;
            ++f;
            return;
        }

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }
}

static void ParseStyle(FormatSpec& spec, std::string_view::iterator& f, std::string_view::iterator const end)
{
    assert(f != end && *f == '!');

    auto const f0 = ++f;

    // The test is required because of the deref below. (string_view is not constructible
    // from string_view::iterator's.)
    if (!EXPECT(f0 != end, "unexpected end of format-string"))
        return;

    f = std::find_if(f, end, [](char ch) { return ch == '}'; });

    spec.style = { &*f0, static_cast<size_t>(f - f0) };
}

static void ParseReplacementField(FormatSpec& spec, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end);

    if (*f == '*')
    {
        ParseFormatSpecArg(spec, f, end, nextarg, types, args);
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }

    if (*f == ':')
    {
        ParseFormatSpec(spec, f, end, nextarg, types, args);
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }

    if (*f == '!')
    {
        ParseStyle(spec, f, end);
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }

    if (!EXPECT(*f == '}', "unexpected characters in format-spec"))
        return;

    ++f;
}

errc fmtxx::impl::DoFormat(Writer& w, std::string_view format, Types types, Arg const* args)
{
    if (format.empty())
        return errc::success;

    int nextarg = 0;

    auto       f   = format.begin();
    auto const end = format.end();
    auto       s   = f;
    for (;;)
    {
        f = std::find_if(f, end, [](char ch) { return ch == '{' || ch == '}'; });

        if (f != s && !w.Write(&*s, static_cast<size_t>(f - s)))
            return errc::io_error; // io-error

        if (f == end) // done.
            break;

        auto const prev = f;
        ++f; // skip '{' or '}'

        if (!EXPECT(f != end, "missing '}' or stray '}'"))
            return w.Put(*prev) ? errc::success : errc::io_error;

        if (*prev == *f) // '{{' or '}}'
        {
            s = f;
            ++f;
            continue;
        }

        if (!EXPECT(*prev != '}', "unescaped '}'"))
        {
            s = prev;
            continue;
        }

        int arg_index = -1;
        if (IsDigit(*f))
        {
            arg_index = ParseInt(f, end);
            if (!EXPECT(f != end, "unexpected end of format-string"))
                return errc::success;
        }

        FormatSpec spec;
        if (*f != '}')
            ParseReplacementField(spec, f, end, nextarg, types, args);
        else
            ++f; // skip '}'

        if (arg_index < 0)
            arg_index = nextarg++;

        s = f;

        auto const arg_type = types[arg_index];

        if (!EXPECT(arg_type != Types::T_NONE, "argument index out of range"))
            continue;
        if (!EXPECT(arg_type != Types::T_FORMATSPEC, "invalid formatting argument (FormatSpec)"))
            continue;

        auto const err = CallFormatFunc(w, spec, arg_type, args[arg_index]);
        if (err != errc::success)
            return err;
    }

    return errc::success;
}

errc fmtxx::impl::DoFormat(std::string& os, std::string_view format, Types types, Arg const* args)
{
    StringWriter w { os };
    return DoFormat(w, format, types, args);
}

errc fmtxx::impl::DoFormat(std::FILE* os, std::string_view format, Types types, Arg const* args)
{
    FILEWriter w { os };
    return DoFormat(w, format, types, args);
}

errc fmtxx::impl::DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    std::ostream::sentry const se(os);
    if (se)
    {
        StreamWriter w { os };
        return DoFormat(w, format, types, args);
    }

    return errc::io_error;
}

errc fmtxx::impl::DoFormat(CharArray& os, std::string_view format, Types types, Arg const* args)
{
    CharArrayWriter w { os };
    return DoFormat(w, format, types, args);
}

static void ParseAsterisk(int& value, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end && *f == '*'); // internal error

    ++f;
    if (!EXPECT(f != end, "unexpected end of format-string"))
        return;

    int index;
    if (IsDigit(*f))
    {
        // Positional arguments are 1-based.
        index = ParseInt(f, end) - 1;
        if (!EXPECT(index >= 0, "argument index must be >= 1"))
            return;

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
        if (!EXPECT(*f == '$', "unexpected character in format-string ('$' expected)"))
            return;
        ++f;
    }
    else
    {
        index = nextarg++;
    }

    GetIntArg(value, index, types, args);
}

static void ParsePrintfSpec(int& arg_index, FormatSpec& spec, std::string_view::iterator& f, std::string_view::iterator const end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end && *(f - 1) == '%');

    for (;;)
    {
        switch (*f)
        {
// Flags
        case '-':
            spec.align = Align::Left;
            ++f;
            break;
        case '+':
            spec.sign = Sign::Plus;
            ++f;
            break;
        case ' ':
            spec.sign = Sign::Space;
            ++f;
            break;
        case '#':
            spec.hash = true;
            ++f;
            break;
        case '0':
            spec.zero = true;
            ++f;
            break;
        case '\'':
        case '_':
        case ',':
            spec.tsep = *f;
            ++f;
            break;
// Width
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            {
                int const n = ParseInt(f, end);
                if (!EXPECT(f != end, "unexpected end of format-string"))
                    return;
                // If this number ends with a '$' its actually a positional argument
                // index and not the field width.
                if (*f == '$')
                {
                    ++f;
                    arg_index = n - 1;
                }
                else
                {
                    spec.width = n;
                }
            }
            break;
        case '*':
            ParseAsterisk(spec.width, f, end, nextarg, types, args);
            FixNegativeFieldWidth(spec);
            break;
// Precision
        case '.':
            ++f;
            if (!EXPECT(f != end, "unexpected end of format-string"))
                return;
            switch (*f)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                spec.prec = ParseInt(f, end);
                break;
            case '*':
                ParseAsterisk(spec.prec, f, end, nextarg, types, args);
                break;
            default:
                spec.prec = 0;
                break;
            }
            break;
// Length modifiers
        case 'h':
        case 'l':
        case 'j':
        case 'z':
        case 't':
        case 'L':
            ++f;
            break;
// Conversion
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
        case 'b':
        case 'B':
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
        case 'c':
        case 's':
        case 'S':
        case 'p':
        case 'q': // EXTENSION: quoted strings
        case 'y': // EXTENSION: bool "yes"/"no"
            spec.conv = *f;
            ++f;
            return;
        case 'n':
            // The number of characters written so far is stored into the integer
            // indicated by the int * (or variant) pointer argument.
            // No argument is converted.
            static_cast<void>(EXPECT(false, "sorry, not implemented: 'n' conversion"));
            abort();
            return;
        case 'm':
            // (Glibc extension.) Print output of strerror(errno).
            // No argument is required.
            static_cast<void>(EXPECT(false, "sorry, not implemented: 'm' conversion"));
            abort();
            return;
        default:
            static_cast<void>(EXPECT(false, "unknown conversion"));
            spec.conv = 's';
            return;
        }

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }
}

errc fmtxx::impl::DoPrintf(Writer& w, std::string_view format, Types types, Arg const* args)
{
    if (format.empty())
        return errc::success;

    int nextarg = 0;

    auto       f   = format.begin();
    auto const end = format.end();
    auto       s   = f;
    for (;;)
    {
        f = std::find(f, end, '%');

        if (f != s && !w.Write(&*s, static_cast<size_t>(f - s)))
            return errc::io_error;

        if (f == end) // done.
            break;

        auto const prev = f;
        ++f; // skip '%'

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return w.Put(*prev) ? errc::success : errc::io_error;

        if (*f == '%') // '%%'
        {
            s = f;
            ++f;
            continue;
        }

        int arg_index = -1;

        FormatSpec spec;
        if (*f != 's') // %s is like {}
            ParsePrintfSpec(arg_index, spec, f, end, nextarg, types, args);
        else
            ++f; // skip 's'

        if (arg_index < 0)
            arg_index = nextarg++;

        s = f;

        auto const arg_type = types[arg_index];

        if (!EXPECT(arg_type != Types::T_NONE, "argument index out of range"))
            continue;
        if (!EXPECT(arg_type != Types::T_FORMATSPEC, "invalid formatting argument (FormatSpec)"))
            continue;

        auto const err = CallFormatFunc(w, spec, arg_type, args[arg_index]);
        if (err != errc::success)
            return err;
    }

    return errc::success;
}

errc fmtxx::impl::DoPrintf(std::string& os, std::string_view format, Types types, Arg const* args)
{
    StringWriter w { os };
    return DoPrintf(w, format, types, args);
}

errc fmtxx::impl::DoPrintf(std::FILE* os, std::string_view format, Types types, Arg const* args)
{
    FILEWriter w { os };
    return DoPrintf(w, format, types, args);
}

errc fmtxx::impl::DoPrintf(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    std::ostream::sentry const se(os);
    if (se)
    {
        StreamWriter w { os };
        return DoPrintf(w, format, types, args);
    }

    return errc::io_error;
}

errc fmtxx::impl::DoPrintf(CharArray& os, std::string_view format, Types types, Arg const* args)
{
    CharArrayWriter w { os };
    return DoPrintf(w, format, types, args);
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
