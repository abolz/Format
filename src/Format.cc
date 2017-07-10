#include "Format.h"

#ifndef FMTXX_DOUBLE_CONVERSION_EXTERNAL
#include "double_conversion_inline.h"
#else
#include <double-conversion/bignum-dtoa.h>
#include <double-conversion/fast-dtoa.h>
#include <double-conversion/fixed-dtoa.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator> // stdext::checked_array_iterator
#include <limits>

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(X) 0
#endif

#if __has_cpp_attribute(clang::fallthrough)
#  define FALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(fallthrough) || __cplusplus >= 201703 || (_MSC_VER >= 1910 && _HAS_CXX17)
#  define FALLTHROUGH [[fallthrough]]
#else
#  define FALLTHROUGH
#endif

static_assert(std::numeric_limits<double>::is_iec559,
    "IEEE-754 implementation required for formatting floating-point numbers");

using namespace fmtxx;
using namespace fmtxx::impl;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

//#define FAIL(MSG) (0)
#define FAIL(MSG) (assert(!(MSG)))
//#define FAIL(MSG) (throw std::runtime_error(MSG))

#define EXPECTED(EXPR)     ((EXPR) ? true : (FAIL(#EXPR), false))
#define NOT_EXPECTED(EXPR) ((EXPR) ? (FAIL(#EXPR), true) : false)

// Maximum supported minimum field width.
// This is not an implementation limit, but just an upper bound to prevent
// allocating huge amounts of memory...
static constexpr int kMaxFieldWidth = 10000;

// Maximum supported integer precision (= minimum number of digits).
static constexpr int kMaxIntPrec = 256;

// Maximum supported floating point precision.
// Precision required for denorm_min (= [751 digits] 10^-323) when using %f
static constexpr int kMaxFloatPrec = 751 + 323;

static constexpr char const* kUpperDigits = "0123456789ABCDEF";
static constexpr char const* kLowerDigits = "0123456789abcdef";

static constexpr char const* kDecDigits100 =
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

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

// std::clamp...
template <typename T>
static constexpr T Clip(T x, T lower, T upper) { return std::min(std::max(lower, x), upper); }

template <typename T>
static void UnusedParameter(T&&) {}

#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL > 0 && _SECURE_SCL_DEPRECATE)
template <typename RanIt>
static stdext::checked_array_iterator<RanIt> MakeArrayIterator(RanIt first, intptr_t n)
{
    return stdext::make_checked_array_iterator(first, n);
}
#else
template <typename RanIt>
static RanIt MakeArrayIterator(RanIt first, intptr_t /*n*/)
{
    return first;
}
#endif

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

fmtxx::Writer::~Writer() noexcept
{
}

errc fmtxx::FILEWriter::Put(char c) noexcept
{
    if (EOF == std::fputc(c, file_))
        return errc::io_error;

    size_ += 1;
    return errc::success;
}

errc fmtxx::FILEWriter::Write(char const* str, size_t len) noexcept
{
    size_t n = std::fwrite(str, 1, len, file_);

    // Count the number of characters successfully transmitted.
    // This is unlike ArrayWriter, which counts characters that would have been written on success.
    // (FILEWriter and ArrayWriter are for compatibility with fprintf and snprintf, resp.)
    size_ += n;
    return n == len ? errc::success : errc::io_error;
}

errc fmtxx::FILEWriter::Pad(char c, size_t count) noexcept
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        auto const n = std::min(count, kBlockSize);
        if (Failed ec = FILEWriter::Write(block, n))
            return ec;
        count -= n;
    }

    return errc::success;
}

size_t fmtxx::ArrayWriter::finish() noexcept
{
    if (size_ < bufsize_)
        buf_[size_] = '\0';
    else if (bufsize_ > 0)
        buf_[bufsize_ - 1] = '\0';

    return size_;
}

errc fmtxx::ArrayWriter::Put(char c) noexcept
{
    if (size_ < bufsize_)
        buf_[size_] = c;

    size_ += 1;
    return errc::success;
}

errc fmtxx::ArrayWriter::Write(char const* str, size_t len) noexcept
{
    if (size_ < bufsize_)
        std::memcpy(buf_ + size_, str, std::min(len, bufsize_ - size_));

    size_ += len;
    return errc::success;
}

errc fmtxx::ArrayWriter::Pad(char c, size_t count) noexcept
{
    if (size_ < bufsize_)
        std::memset(buf_ + size_, static_cast<unsigned char>(c), std::min(count, bufsize_ - size_));

    size_ += count;
    return errc::success;
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

    if NOT_EXPECTED(width > kMaxFieldWidth)
        width = kMaxFieldWidth; // [recover]

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

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = w.write(str, len))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return errc::success;
}

static errc PrintAndPadString(Writer& w, FormatSpec const& spec, StringView str)
{
    return PrintAndPadString(w, spec, str.data(), str.size());
}

template <typename F>
static errc ForEachEscaped(char const* str, size_t len, F func)
{
    for (size_t i = 0; i < len; ++i)
    {
        char const ch = str[i];
        switch (ch)
        {
        case '"':
        case '\\':
        //case '\'':
        //case '?':
            if (Failed ec = func('\\')) return ec;
            if (Failed ec = func(ch)  ) return ec;
            break;
        //case '\a':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('a') ) return ec;
        //    break;
        //case '\b':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('b') ) return ec;
        //    break;
        //case '\f':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('f') ) return ec;
        //    break;
        //case '\n':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('n') ) return ec;
        //    break;
        //case '\r':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('r') ) return ec;
        //    break;
        //case '\t':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('t') ) return ec;
        //    break;
        //case '\v':
        //    if (Failed ec = func('\\')) return ec;
        //    if (Failed ec = func('v') ) return ec;
        //    break;
        default:
            if (Failed ec = func(ch)) return ec;
            break;
        }
    }

    return errc::success;
}

static errc WriteQuoted(Writer& w, char const* str, size_t len, size_t quoted_len)
{
    if (len == 0)
        return errc::success;
    if (len == quoted_len)
        return w.write(str, len);

    return ForEachEscaped(str, len, [&](char c) { return w.put(c); });
}

static errc PrintAndPadQuotedString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t quoted_len = 0;
    ForEachEscaped(str, len, [&](char) { ++quoted_len; return errc::success; });

    auto const pad = ComputePadding(2 + quoted_len, spec.align, spec.width);

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = w.put('"'))
        return ec;
    if (Failed ec = WriteQuoted(w, str, len, quoted_len))
        return ec;
    if (Failed ec = w.put('"'))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return errc::success;
}

errc fmtxx::Util::format_string(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t const n = (spec.prec >= 0)
        ? std::min(len, static_cast<size_t>(spec.prec))
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

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = (sign == '\0') ? errc::success : w.put(sign))
        return ec;
    if (Failed ec = w.write(prefix, nprefix))
        return ec;
    if (Failed ec = w.pad(spec.zero ? '0' : spec.fill, pad.after_sign))
        return ec;
    if (Failed ec = w.write(digits, ndigits))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return errc::success;
}

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

static char* IntToAsciiBackwards(char* last/*[-64]*/, uint64_t n, int base, bool capitals)
{
    char const* const xdigits = capitals ? kUpperDigits : kLowerDigits;

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
        do *--last = kUpperDigits[n & 7]; while (n >>= 3);
        return last;
    case 2:
        do *--last = kUpperDigits[n & 1]; while (n >>= 1);
        return last;
    }

    assert(!"not implemented"); // internal error
    return last;
}

// Inserts thousands separators into [first, +off1).
// Returns the number of separators inserted.
static int InsertThousandsSep(char* first, char* last, int off1, int off2, char sep, int group_len)
{
    assert(off1 >= 0);
    assert(off2 >= off1);
    assert(sep != '\0');
    assert(group_len > 0);

    int const nsep = (off1 - 1) / group_len;

    if (nsep <= 0)
        return 0;

    if (off1 != off2)
    {
        auto I = MakeArrayIterator(first, last - first);
        std::copy_backward(I + off1, I + off2, I + (off2 + nsep));
    }

    char* src = first + off1;
    char* dst = first + (off1 + nsep);

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

    constexpr int kMaxSeps = (kMaxIntPrec - 1) / 3;
    constexpr int kBufSize = kMaxIntPrec + kMaxSeps;

    char buf[kBufSize];

    char* l = buf + kMaxIntPrec;
    char* f = IntToAsciiBackwards(l, number, base, upper);

    if (spec.prec >= 0)
    {
        int const prec = std::min(spec.prec, kMaxIntPrec);
        while (l - f < prec)
        {
            *--f = '0';
        }
    }

    if (spec.tsep != '\0')
    {
        int const pos = static_cast<int>(l - f);
        l += InsertThousandsSep(f, buf + kBufSize, pos, pos, spec.tsep, base == 10 ? 3 : 4);
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
        return Util::format_int(w, spec, static_cast<unsigned char>(ch));
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
    static constexpr uint64_t kSignMask        = 0x8000000000000000;
    static constexpr uint64_t kExponentMask    = 0x7FF0000000000000;
    static constexpr uint64_t kSignificandMask = 0x000FFFFFFFFFFFFF;
    static constexpr uint64_t kHiddenBit       = 0x0010000000000000;

    static constexpr int kExponentBias = 0x3FF;

    union {
        double const d;
        uint64_t const bits;
    };

    explicit Double(double d) : d(d) {}
    explicit Double(uint64_t bits) : bits(bits) {}

    uint64_t Sign()        const { return (bits & kSignMask       ) >> 63; }
    uint64_t Exponent()    const { return (bits & kExponentMask   ) >> 52; }
    uint64_t Significand() const { return (bits & kSignificandMask);       }

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

static int CreateFixedRepresentation(char* buf, int bufsize, int num_digits, int decpt, int precision, Options const& options)
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
            std::fill_n(buf + num_digits, nextra, '0');
            buf[num_digits + 1] = options.decimal_point;

            // digits0.[000][000] --> 0.[000]digits[000]
            std::rotate(buf, buf + num_digits, buf + (num_digits + 2 + -decpt));

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

        std::fill_n(buf + num_digits, nzeros + nextra, '0');
        if (nextra > 0)
        {
            buf[decpt] = options.decimal_point;
        }

        last = decpt + nextra;
    }
    else
    {
        // dig.its[000]

        auto I = MakeArrayIterator(buf, bufsize);

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
        last += InsertThousandsSep(buf, buf + bufsize, decpt, last, options.thousands_sep, 3);
    }

    return last;
}

static void GenerateFixedDigits(double v, int requested_digits, char* buf, int bufsize, int* num_digits, int* decpt)
{
    Double const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);
    assert(requested_digits >= 0);

    if (d.IsZero())
    {
        buf[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    double_conversion::Vector<char> vec(buf, bufsize);

    bool const fast_worked = FastFixedDtoa(v, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_FIXED, requested_digits, vec, num_digits, decpt);
    }
}

static int ToFixed(char* buf, int bufsize, double d, int precision, Options const& options)
{
    int num_digits = 0;
    int decpt = 0;

    GenerateFixedDigits(d, precision, buf, bufsize, &num_digits, &decpt);

    assert(num_digits >= 0);

    return CreateFixedRepresentation(buf, bufsize, num_digits, decpt, precision, options);
}

// Append a decimal representation of EXPONENT to BUF.
// Returns pos + number of characters written.
static int AppendExponent(char* buf, int /*bufsize*/, int pos, int exponent, Options const& options)
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

    if (k >= 1000 || options.min_exponent_digits >= 4) { buf[pos++] = kUpperDigits[exponent / 1000]; exponent %= 1000; }
    if (k >=  100 || options.min_exponent_digits >= 3) { buf[pos++] = kUpperDigits[exponent /  100]; exponent %=  100; }
    if (k >=   10 || options.min_exponent_digits >= 2) { buf[pos++] = kUpperDigits[exponent /   10]; exponent %=   10; }
    buf[pos++] = static_cast<char>('0' + exponent % 10);

    return pos;
}

static int CreateExponentialRepresentation(char* buf, int bufsize, int num_digits, int exponent, int precision, Options const& options)
{
    assert(options.decimal_point != '\0');

    int pos = 0;

    pos += 1; // leading digit
    if (num_digits > 1)
    {
        // d.igits[000]e+123

        auto I = MakeArrayIterator(buf, bufsize);

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

        std::fill_n(buf + pos, 1 + precision, '0');
        buf[pos] = options.decimal_point;
        pos += 1 + precision;
    }
    else
    {
        // d[.]e+123

        if (options.use_alternative_form)
            buf[pos++] = options.decimal_point;
    }

    return AppendExponent(buf, bufsize, pos, exponent, options);
}

static void GeneratePrecisionDigits(double v, int requested_digits, char* buf, int bufsize, int* num_digits, int* decpt)
{
    assert(bufsize >= 1);
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
        buf[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    double_conversion::Vector<char> vec(buf, bufsize);

    bool const fast_worked = FastDtoa(v, double_conversion::FAST_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_PRECISION, requested_digits, vec, num_digits, decpt);
    }
}

static int ToExponential(char* buf, int bufsize, double d, int precision, Options const& options)
{
    int num_digits = 0;
    int decpt = 0;

    GeneratePrecisionDigits(d, precision + 1, buf, bufsize, &num_digits, &decpt);

    assert(num_digits > 0);

    int const exponent = decpt - 1;
    return CreateExponentialRepresentation(buf, bufsize, num_digits, exponent, precision, options);
}

static int ToGeneral(char* buf, int bufsize, double d, int precision, Options const& options)
{
    assert(precision >= 0);

    int num_digits = 0;
    int decpt = 0;

    int const P = precision == 0 ? 1 : precision;

    GeneratePrecisionDigits(d, P, buf, bufsize, &num_digits, &decpt);

    assert(num_digits > 0);
    assert(num_digits <= P); // GeneratePrecisionDigits is allowed to return fewer digits if they are all 0's.

    int const X = decpt - 1;

    // Trim trailing zeros.
    while (num_digits > 0 && buf[num_digits - 1] == '0')
    {
        --num_digits;
    }

    if (-4 <= X && X < P)
    {
        int prec = P - decpt;
        if (!options.use_alternative_form)
        {
            prec = std::min(prec, num_digits - decpt);
        }

        return CreateFixedRepresentation(buf, bufsize, num_digits, decpt, prec, options);
    }
    else
    {
        int prec = P - 1;
        if (!options.use_alternative_form)
        {
            prec = std::min(prec, num_digits - 1);
        }

        return CreateExponentialRepresentation(buf, bufsize, num_digits, X, prec, options);
    }
}

static int CountLeadingZeros64(uint64_t n)
{
    assert(n != 0);

    int z = 0;
    while ((n & (uint64_t{1} << 63)) == 0)
    {
        ++z;
        n <<= 1;
    }

    return z;
}

static void GenerateHexDigits(double v, int precision, bool normalize, bool upper, char* buffer, int buffer_size, int* num_digits, int* binary_exponent)
{
    assert(buffer_size >= 52/4 + 1);
    UnusedParameter(buffer_size);

    char const* const xdigits = upper ? kUpperDigits : kLowerDigits;

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

static int ToHex(char* buf, int bufsize, double d, int precision, Options const& options)
{
    int num_digits = 0;
    int binary_exponent = 0;

    GenerateHexDigits(d, precision, options.normalize, options.use_upper_case_digits, buf, bufsize, &num_digits, &binary_exponent);

    assert(num_digits > 0);

    return CreateExponentialRepresentation(buf, bufsize, num_digits, binary_exponent, precision, options);
}

static void GenerateShortestDigits(double v, char* buf, int bufsize, int* num_digits, int* decpt)
{
    assert(bufsize >= 17 + 1 /*null*/);

    Double const d{v};

    assert(!d.IsSpecial());
    assert(d.Abs() >= 0);

    if (d.IsZero())
    {
        buf[0] = '0';
        *num_digits = 1;
        *decpt = 1;
        return;
    }

    double_conversion::Vector<char> vec(buf, bufsize);

    bool const fast_worked = FastDtoa(v, double_conversion::FAST_DTOA_SHORTEST, -1, vec, num_digits, decpt);
    if (!fast_worked)
    {
        BignumDtoa(v, double_conversion::BIGNUM_DTOA_SHORTEST, -1, vec, num_digits, decpt);
    }
}

static int ToECMAScript(char* buf, int bufsize, double d, char decimal_point, char exponent_char)
{
    assert(bufsize >= 24);

    auto I = MakeArrayIterator(buf, bufsize);

    int num_digits = 0;
    int decpt = 0;

    GenerateShortestDigits(d, buf, bufsize, &num_digits, &decpt);

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

        int const endpos = AppendExponent(buf, bufsize, /*pos*/ 1, n - 1, options);
        return endpos;
    }
    else
    {
        // d.igitsE+123

        std::copy_backward(I + 1, I + k, I + (k + 1));
        I[1] = decimal_point;
        int const endpos = AppendExponent(buf, bufsize, /*pos*/ k + 1, n - 1, options);
        return endpos;
    }
}

} // namespace dtoa

static errc HandleSpecialFloat(Double d, Writer& w, FormatSpec const& spec, char sign, bool upper)
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
    constexpr int kMaxDigitsBeforePoint = 309;
    constexpr int kMaxSeps = (kMaxDigitsBeforePoint - 1) / 3;
    constexpr int kBufSize = kMaxDigitsBeforePoint + kMaxSeps + 1 + kMaxFloatPrec + 1/*null*/;

    char buf[kBufSize];

    int buflen;
    switch (conv)
    {
    case 's':
    case 'S':
        buflen = dtoa::ToECMAScript(buf, kBufSize, abs_x, options.decimal_point, options.exponent_char);
        break;
    case 'f':
    case 'F':
        buflen = dtoa::ToFixed(buf, kBufSize, abs_x, prec, options);
        break;
    case 'e':
    case 'E':
        buflen = dtoa::ToExponential(buf, kBufSize, abs_x, prec, options);
        break;
    case 'g':
    case 'G':
        buflen = dtoa::ToGeneral(buf, kBufSize, abs_x, prec, options);
        break;
    case 'x':
    case 'X':
        buflen = dtoa::ToHex(buf, kBufSize, abs_x, prec, options);
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

static void FixNegativeFieldWidth(FormatSpec& spec)
{
    if (spec.width < 0)
    {
        spec.width = NOT_EXPECTED(spec.width == INT_MIN) ? INT_MAX /*[recover]*/ : -spec.width;
        spec.align = Align::Left;
    }
}

static errc CallFormatFunc(Writer& w, FormatSpec const& spec, Arg const& arg, Arg::Type type)
{
    switch (type)
    {
    case Arg::T_NONE:
    case Arg::T_FORMATSPEC:
        assert(!"internal error");
        break;
    case Arg::T_OTHER:
        return arg.other.func(w, spec, arg.other.value);
    case Arg::T_STRING:
        return Util::format_string(w, spec, arg.string.data, arg.string.size);
    case Arg::T_PVOID:
        return Util::format_pointer(w, spec, arg.pvoid);
    case Arg::T_PCHAR:
        return Util::format_string(w, spec, arg.pchar);
    case Arg::T_CHAR:
        return Util::format_char(w, spec, arg.char_);
    case Arg::T_BOOL:
        return Util::format_bool(w, spec, arg.bool_);
    case Arg::T_SCHAR:
        return Util::format_int(w, spec, arg.schar);
    case Arg::T_SSHORT:
        return Util::format_int(w, spec, arg.sshort);
    case Arg::T_SINT:
        return Util::format_int(w, spec, arg.sint);
    case Arg::T_SLONGLONG:
        return Util::format_int(w, spec, arg.slonglong);
    case Arg::T_ULONGLONG:
        return Util::format_int(w, spec, arg.ulonglong);
    case Arg::T_DOUBLE:
        return Util::format_double(w, spec, arg.double_);
    case Arg::T_LAST:
        assert(!"internal error");
        break;
    }

    return errc::success; // unreachable
}

static bool IsDigit(char ch) { return '0' <= ch && ch <= '9'; }

static bool ParseInt(int& value, StringView::iterator& f, StringView::iterator end)
{
    assert(f != end && IsDigit(*f)); // internal error
    auto const f0 = f;

    int x = *f - '0';

    while (++f != end && IsDigit(*f))
    {
        if ((f - f0) + 1 > std::numeric_limits<int>::digits10)
        {
            if (x > INT_MAX / 10 || (*f - '0') > INT_MAX - 10 * x)
            {
                while (++f != end && IsDigit(*f)) {}
                return false;
            }
        }

        x = 10 * x + (*f - '0');
    }

    value = x;
    return true;
}

static errc GetIntArg(int& value, int index, Arg const* args, Types types)
{
    switch (types[index])
    {
    case Arg::T_NONE:
        return errc::index_out_of_range;

    case Arg::T_SCHAR:
        value = args[index].schar;
        return errc::success;

    case Arg::T_SSHORT:
        value = args[index].sshort;
        return errc::success;

    case Arg::T_SINT:
        value = args[index].sint;
        return errc::success;

    case Arg::T_SLONGLONG:
        if NOT_EXPECTED(args[index].slonglong > INT_MAX)
            return errc::value_out_of_range;
        if NOT_EXPECTED(args[index].slonglong < INT_MIN)
            return errc::value_out_of_range;
        value = static_cast<int>(args[index].slonglong);
        return errc::success;

    case Arg::T_ULONGLONG:
        if NOT_EXPECTED(args[index].ulonglong > INT_MAX)
            return errc::value_out_of_range;
        value = static_cast<int>(args[index].ulonglong);
        return errc::success;

    default:
        return errc::invalid_argument;
    }
}

static errc ParseLBrace(int& value, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '{'); // internal error

    ++f;
    if NOT_EXPECTED(f == end)
        return errc::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        if NOT_EXPECTED(!ParseInt(index, f, end))
            return errc::invalid_format_string;
        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
        if NOT_EXPECTED(*f != '}')
            return errc::invalid_format_string; // [recover?!?!]
        ++f;
    }
    else
    {
        index = nextarg++;
    }

    return GetIntArg(value, index, args, types);
}

static errc ParseFormatSpecArg(FormatSpec& spec, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '*');

    ++f;
    if NOT_EXPECTED(f == end)
        return errc::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        if NOT_EXPECTED(!ParseInt(index, f, end))
            return errc::invalid_format_string;
    }
    else
    {
        index = nextarg++;
    }

    if NOT_EXPECTED(types[index] == Arg::T_NONE)
        return errc::index_out_of_range;
    if NOT_EXPECTED(types[index] != Arg::T_FORMATSPEC)
        return errc::invalid_argument;

    spec = *static_cast<FormatSpec const*>(args[index].pvoid);
    FixNegativeFieldWidth(spec);

    return errc::success;
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

static errc ParseFormatSpec(FormatSpec& spec, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == ':');

    ++f;
    if NOT_EXPECTED(f == end)
        return errc::invalid_format_string;

    if (f + 1 != end && ParseAlign(spec, *(f + 1)))
    {
        spec.fill = *f;
        f += 2;
        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
    }
    else if (ParseAlign(spec, *f))
    {
        ++f;
        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
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
            if NOT_EXPECTED(!ParseInt(spec.width, f, end))
                spec.width = 0; // [recover]
            break;
        case '{':
            if (Failed(ParseLBrace(spec.width, f, end, nextarg, args, types)))
                spec.width = 0; // [recover]
            else
                FixNegativeFieldWidth(spec);
            break;
// Precision
        case '.':
            ++f;
            if NOT_EXPECTED(f == end)
                return errc::invalid_format_string;
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
                if NOT_EXPECTED(!ParseInt(spec.prec, f, end))
                    spec.prec = -1; // [recover]
                break;
            case '{':
                if (Failed(ParseLBrace(spec.prec, f, end, nextarg, args, types)))
                    spec.prec = -1; // [recover]
                break;
            default:
                spec.prec = 0;
                break;
            }
            break;
// Conversion
        case '!':
        case '}':
            return errc::success;
        default:
            spec.conv = *f;
            ++f;
            return errc::success;
        }

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string; // [recover?!?!]
    }
}

static errc ParseStyle(FormatSpec& spec, StringView::iterator& f, StringView::iterator end)
{
    assert(f != end && *f == '!');

    ++f;
    if NOT_EXPECTED(f == end)
        return errc::invalid_format_string;

    char delim;
    switch (*f)
    {
    case '\'':
        ++f;
        delim = '\'';
        break;
    case '"':
        ++f;
        delim = '"';
        break;
    case '{':
        ++f;
        delim = '}';
        break;
    case '(':
        ++f;
        delim = ')';
        break;
    case '[':
        ++f;
        delim = ']';
        break;
    default:
        delim = '\0';
        break;
    }

    auto const f0 = f;

    if NOT_EXPECTED(f0 == end)
        return errc::invalid_format_string;

    f = std::find(f, end, delim == '\0' ? '}' : delim);

    spec.style = {&*f0, static_cast<size_t>(f - f0)};

    if (delim != '\0')
    {
        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
        ++f; // skip delim
    }

    return errc::success;
}

static errc ParseReplacementField(FormatSpec& spec, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end);

    if (*f == '*')
    {
        if (Failed(ParseFormatSpecArg(spec, f, end, nextarg, args, types)))
            spec = {}; // [recover]

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
    }

    if (*f == ':')
    {
        if (Failed(ParseFormatSpec(spec, f, end, nextarg, args, types)))
            spec = {}; // [recover]

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
    }

    if (*f == '!')
    {
        if (Failed(ParseStyle(spec, f, end)))
            spec.style = {}; // [recover]

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
    }

    if NOT_EXPECTED(*f != '}')
        return errc::invalid_format_string; // [XXX: recover?!?!]

    ++f;

    return errc::success;
}

errc fmtxx::impl::Format(Writer& w, StringView format, Arg const* args, Types types)
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
        if (f != s)
        {
            if (Failed ec = w.write(&*s, static_cast<size_t>(f - s)))
                return ec;
        }

        if (f == end) // done.
            break;

        auto const prev = f;
        ++f; // skip '{' or '}'

        if NOT_EXPECTED(f == end) // "missing '}' or stray '}'"
            return errc::invalid_format_string;

        if (*prev == *f) // '{{' or '}}'
        {
            s = f;
            ++f;
            continue;
        }

        if NOT_EXPECTED(*prev == '}')
        {
            s = prev; // [recover]
            continue;
        }

        int arg_index = -1;
        if (IsDigit(*f))
        {
            if NOT_EXPECTED(!ParseInt(arg_index, f, end))
                arg_index = INT_MAX; // [recover: 'index out of range' (below)]

            if NOT_EXPECTED(f == end)
                return errc::invalid_format_string;
        }

        FormatSpec spec;
        if (*f != '}')
        {
            if (Failed ec = ParseReplacementField(spec, f, end, nextarg, args, types))
                return ec;
        }
        else
        {
            ++f; // skip '}'
        }

        if (arg_index < 0)
            arg_index = nextarg++;

        s = f;

        auto const arg_type = types[arg_index];

        if NOT_EXPECTED(arg_type == Arg::T_NONE)
            return errc::index_out_of_range;
        if NOT_EXPECTED(arg_type == Arg::T_FORMATSPEC)
            return errc::invalid_argument;

        if (Failed ec = CallFormatFunc(w, spec, args[arg_index], arg_type))
            return ec;
    }

    return errc::success;
}

static errc ParseAsterisk(int& value, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '*'); // internal error

    ++f;
    if NOT_EXPECTED(f == end)
        return errc::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        if NOT_EXPECTED(!ParseInt(index, f, end))
            return errc::invalid_format_string;

        index -= 1; // Positional arguments are 1-based.

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;
        if NOT_EXPECTED(index < 0)
            return errc::invalid_format_string;
        if NOT_EXPECTED(*f != '$')
            return errc::invalid_format_string; // [recover?!?!]
        ++f;
    }
    else
    {
        index = nextarg++;
    }

    return GetIntArg(value, index, args, types);
}

static errc ParsePrintfSpec(int& arg_index, FormatSpec& spec, StringView::iterator& f, StringView::iterator end, int& nextarg, Arg const* args, Types types)
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
                int n;
                if NOT_EXPECTED(!ParseInt(n, f, end))
                    n = 0; // [recover: ==> arg_index = -1, width = 0]

                if NOT_EXPECTED(f == end)
                    return errc::invalid_format_string;

                // If this number ends with a '$' its actually a positional argument
                // index and not the field width.
                if (*f == '$')
                {
                    ++f;
                    arg_index = n - 1; // Positional arguments are 1-based
                }
                else
                {
                    spec.width = n;
                }
            }
            break;
        case '*':
            if (Failed(ParseAsterisk(spec.width, f, end, nextarg, args, types)))
                spec.width = 0; // [recover]
            else
                FixNegativeFieldWidth(spec);
            break;
// Precision
        case '.':
            ++f;
            if NOT_EXPECTED(f == end)
                return errc::invalid_format_string;
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
                if NOT_EXPECTED(!ParseInt(spec.prec, f, end))
                    spec.prec = -1; // [recover]
                break;
            case '*':
                if (Failed(ParseAsterisk(spec.prec, f, end, nextarg, args, types)))
                    spec.prec = -1; // [recover]
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
            return errc::success;
        case 'n':
            // The number of characters written so far is stored into the integer
            // indicated by the int * (or variant) pointer argument.
            // No argument is converted.
            FAIL("'n' conversion not supported");
            return errc::not_supported;
        case 'm':
            // (Glibc extension.) Print output of strerror(errno).
            // No argument is required.
            FAIL("'m' conversion not supported");
            return errc::not_supported;
        default:
            FAIL("unknown conversion");
            spec.conv = 's'; // [recover]
            return errc::success;
        }

        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string; // [recover?!?!]
    }
}

errc fmtxx::impl::Printf(Writer& w, StringView format, Arg const* args, Types types)
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
        if (f != s)
        {
            if (Failed ec = w.write(&*s, static_cast<size_t>(f - s)))
                return ec;
        }

        if (f == end) // done.
            break;

        ++f; // skip '%'
        if NOT_EXPECTED(f == end)
            return errc::invalid_format_string;

        if (*f == '%') // '%%'
        {
            s = f;
            ++f;
            continue;
        }

        int arg_index = -1;

        FormatSpec spec;
        if (*f != 's') // %s is like {}
        {
            if (Failed ec = ParsePrintfSpec(arg_index, spec, f, end, nextarg, args, types))
                return ec;
        }
        else
        {
            ++f; // skip 's'
        }

        // No *f from here on!

        if (arg_index < 0)
            arg_index = nextarg++;

        s = f;

        auto const arg_type = types[arg_index];

        if NOT_EXPECTED(arg_type == Arg::T_NONE)
            return errc::index_out_of_range;
        if NOT_EXPECTED(arg_type == Arg::T_FORMATSPEC)
            return errc::invalid_argument;

        if (Failed ec = CallFormatFunc(w, spec, args[arg_index], arg_type))
            return ec;
    }

    return errc::success;
}

errc fmtxx::impl::Format(std::FILE* file, StringView format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return fmtxx::impl::Format(w, format, args, types);
}

errc fmtxx::impl::Printf(std::FILE* file, StringView format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return fmtxx::impl::Printf(w, format, args, types);
}

int fmtxx::impl::FileFormat(std::FILE* file, StringView format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(fmtxx::impl::Format(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::FilePrintf(std::FILE* file, StringView format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(fmtxx::impl::Printf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::ArrayFormat(char* buf, size_t bufsize, StringView format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(fmtxx::impl::Format(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    w.finish();
    return static_cast<int>(w.size());
}

int fmtxx::impl::ArrayPrintf(char* buf, size_t bufsize, StringView format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(fmtxx::impl::Printf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    w.finish();
    return static_cast<int>(w.size());
}
