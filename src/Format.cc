// Copyright (c) 2017 Alexander Bolz
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

#include "Format.h"

#ifndef FMTXX_DOUBLE_CONVERSION_EXTERNAL
#include "__double_conversion.h"
#else
#include <double-conversion/bignum-dtoa.h>
#include <double-conversion/fast-dtoa.h>
#include <double-conversion/fixed-dtoa.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <iterator> // stdext::checked_array_iterator
#include <limits>

using namespace fmtxx;
using namespace fmtxx::impl;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static_assert(
    std::numeric_limits<double>::is_iec559
        && std::numeric_limits<double>::digits == 53,
    "IEEE-754 double-precision implementation required for formatting floating-point numbers");

// Maximum supported integer precision (= minimum number of digits).
static constexpr int kMaxIntPrec = 300;
// Maximum supported floating point precision.
static constexpr int kMaxFloatPrec = 1074;

static_assert(kMaxIntPrec >= 64,
    "A minimum precision of 64 is required to print UINT64_MAX in base 2");
static_assert(kMaxFloatPrec >= 1074,
    "A minimum precision of 1074 is required to print denorm_min (= [751 digits] 10^-323) when using %f");

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

static constexpr char const* kUpperDigits = "0123456789ABCDEF";
static constexpr char const* kLowerDigits = "0123456789abcdef";

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

// EXPECT and EXPECT_NOT must evaluate their arguments X exactly once!
#if 0
static /*[[noreturn]]*/ void AssertionFailed(char const* file, unsigned line, char const* what)
{
    std::fprintf(stderr, "%s(%d) : Assertion failed: %s\n", file, line, what);
    std::abort();
}

#define EXPECT(X)     ((X) ? (true) : (AssertionFailed(__FILE__, __LINE__, "Expected: '" #X "'"), false))
#define EXPECT_NOT(X) ((X) ? (AssertionFailed(__FILE__, __LINE__, "Not expected: '" #X "'"), true) : (false))
#else
#define EXPECT(X)     (X)
#define EXPECT_NOT(X) (X)
#endif

template <typename T>
static void MaybeUnused(T&&)
{
}

#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL > 0 && _SECURE_SCL_DEPRECATE)
template <typename RanIt>
static stdext::checked_array_iterator<RanIt> MakeArrayIterator(RanIt buffer, intptr_t buffer_size, intptr_t position = 0)
{
    return stdext::make_checked_array_iterator(buffer, buffer_size, position);
}
#else
template <typename RanIt>
static RanIt MakeArrayIterator(RanIt buffer, intptr_t /*buffer_size*/, intptr_t position = 0)
{
    return buffer + position;
}
#endif

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

fmtxx::Writer::~Writer() noexcept
{
}

ErrorCode fmtxx::FILEWriter::Put(char c)
{
    if (EOF == std::fputc(c, file_))
        return ErrorCode::io_error;

    size_ += 1;
    return {};
}

ErrorCode fmtxx::FILEWriter::Write(char const* ptr, size_t len)
{
    size_t n = std::fwrite(ptr, 1, len, file_);

    // Count the number of characters successfully transmitted.
    // This is unlike ArrayWriter, which counts characters that would have been written on success.
    // (FILEWriter and ArrayWriter are for compatibility with fprintf and snprintf, resp.)
    size_ += n;
    return n == len ? ErrorCode{} : ErrorCode::io_error;
}

ErrorCode fmtxx::FILEWriter::Pad(char c, size_t count)
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::fill_n(block, kBlockSize, c);

    while (count > 0)
    {
        auto const n = std::min(count, kBlockSize);
        if (Failed ec = FILEWriter::Write(block, n))
            return ec;
        count -= n;
    }

    return {};
}

size_t fmtxx::ArrayWriter::finish() noexcept
{
    if (size_ < bufsize_)
        buf_[size_] = '\0';
    else if (bufsize_ > 0)
        buf_[bufsize_ - 1] = '\0';

    return size_;
}

ErrorCode fmtxx::ArrayWriter::Put(char c)
{
    if (size_ < bufsize_)
        buf_[size_] = c;

    size_ += 1;
    return {};
}

ErrorCode fmtxx::ArrayWriter::Write(char const* ptr, size_t len)
{
    if (size_ < bufsize_)
    {
        auto I = MakeArrayIterator(buf_, static_cast<intptr_t>(bufsize_), static_cast<intptr_t>(size_));
        std::copy_n(ptr, std::min(len, bufsize_ - size_), I);
    }

    size_ += len;
    return {};
}

ErrorCode fmtxx::ArrayWriter::Pad(char c, size_t count)
{
    if (size_ < bufsize_)
    {
        auto I = MakeArrayIterator(buf_, static_cast<intptr_t>(bufsize_), static_cast<intptr_t>(size_));
        std::fill_n(I, std::min(count, bufsize_ - size_), c);
    }

    size_ += count;
    return {};
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static char ComputeSignChar(bool neg, Sign sign, char fill)
{
    if (neg)
        return '-';
    if (sign == Sign::plus)
        return '+';
    if (sign == Sign::space)
        return fill;

    return '\0';
}

namespace {

struct Padding
{
    size_t left       = 0;
    size_t after_sign = 0;
    size_t right      = 0;
};

} // namespace

static Padding ComputePadding(size_t len, Align align, int width)
{
    assert(width >= 0); // internal error

    Padding pad;

    size_t const w = static_cast<size_t>(width);
    if (w > len)
    {
        size_t const d = w - len;
        switch (align)
        {
        case Align::use_default:
        case Align::right:
            pad.left = d;
            break;
        case Align::left:
            pad.right = d;
            break;
        case Align::center:
            pad.left = d/2;
            pad.right = d - d/2;
            break;
        case Align::pad_after_sign:
            pad.after_sign = d;
            break;
        }
    }

    return pad;
}

// Prints out exactly LEN characters (including '\0's) starting at STR,
// possibly padding on the left and/or right.
static ErrorCode PrintAndPadString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    auto const pad = ComputePadding(len, spec.align, spec.width);

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = w.write(str, len))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return {};
}

static ErrorCode PrintAndPadString(Writer& w, FormatSpec const& spec, string_view str)
{
    return PrintAndPadString(w, spec, str.data(), str.size());
}

template <typename F>
static ErrorCode ForEachQuoted(char const* str, size_t len, F func)
{
    for (size_t i = 0; i < len; ++i)
    {
        char const ch = str[i];
        switch (ch)
        {
        case '"':
        case '\\':
            if (Failed ec = func('\\'))
                return ec;
            if (Failed ec = func(ch))
                return ec;
            break;
        default:
            if (Failed ec = func(ch))
                return ec;
            break;
        }
    }

    return {};
}

static ErrorCode WriteQuoted(Writer& w, char const* str, size_t len, size_t quoted_len)
{
    if (len == 0)
        return {};
    if (len == quoted_len)
        return w.write(str, len);

    return ForEachQuoted(str, len, [&](char c) { return w.put(c); });
}

static ErrorCode PrintAndPadQuotedString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t quoted_len = 0;

    ForEachQuoted(str, len, [&](char) -> ErrorCode {
        ++quoted_len;
        return {};
    });

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

    return {};
}

static bool IsASCIIPrintable(char ch)
{
    return 0x20 <= ch && ch <= 0x7E;
}

template <typename F>
static ErrorCode ForEachEscaped(char const* str, size_t len, F func)
{
    for (size_t i = 0; i < len; ++i)
    {
        char const ch = str[i];
        if (IsASCIIPrintable(ch))
        {
            if (Failed ec = func(ch))
                return ec;
        }
        else
        {
            unsigned char uch = static_cast<unsigned char>(ch);

            if (Failed ec = func('\\'))
                return ec;
            if (Failed ec = func(kUpperDigits[(uch >> 6)      ]))
                return ec;
            if (Failed ec = func(kUpperDigits[(uch >> 3) & 0x7]))
                return ec;
            if (Failed ec = func(kUpperDigits[(uch >> 0) & 0x7]))
                return ec;
        }
    }

    return {};
}

static ErrorCode WriteEscaped(Writer& w, char const* str, size_t len, size_t escaped_len)
{
    if (len == 0)
        return {};
    if (len == escaped_len)
        return w.write(str, len);

    return ForEachEscaped(str, len, [&](char c) { return w.put(c); });
}

static ErrorCode PrintAndPadEscapedString(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t escaped_len = 0;

    ForEachEscaped(str, len, [&](char) -> ErrorCode {
        ++escaped_len;
        return {};
    });

    auto const pad = ComputePadding(escaped_len, spec.align, spec.width);

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = WriteEscaped(w, str, len, escaped_len))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return {};
}

ErrorCode fmtxx::Util::format_string(Writer& w, FormatSpec const& spec, char const* str, size_t len)
{
    size_t const n = (spec.prec >= 0)
        ? std::min(len, static_cast<size_t>(spec.prec))
        : len;

    switch (spec.conv) {
    default:
        return PrintAndPadString(w, spec, str, n);
    case 'q':
        return PrintAndPadQuotedString(w, spec, str, n);
    case 'x':
        return PrintAndPadEscapedString(w, spec, str, n);
    }
}

ErrorCode fmtxx::Util::format_char_pointer(Writer& w, FormatSpec const& spec, char const* str)
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
    case 'x':
        return PrintAndPadEscapedString(w, spec, str, len);
    }
}

static ErrorCode PrintAndPadNumber(Writer& w, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    size_t const len = (sign ? 1u : 0u) + nprefix + ndigits;

    auto const pad = ComputePadding(len, spec.zero ? Align::pad_after_sign : spec.align, spec.width);

    if (Failed ec = w.pad(spec.fill, pad.left))
        return ec;
    if (Failed ec = w.put_nonnull(sign))
        return ec;
    if (Failed ec = w.write(prefix, nprefix))
        return ec;
    if (Failed ec = w.pad(spec.zero ? '0' : spec.fill, pad.after_sign))
        return ec;
    if (Failed ec = w.write(digits, ndigits))
        return ec;
    if (Failed ec = w.pad(spec.fill, pad.right))
        return ec;

    return {};
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

    assert(false && "not implemented"); // internal error
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

ErrorCode fmtxx::Util::format_int(Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext)
{
    // N1570, p. 310
    //
    // '+' flag:
    //      The result of a signed conversion always begins with a plus or minus sign. (It
    //      begins with a sign only when a negative value is converted if this flag is not
    //      specified.)
    //
    // 'space' flag:
    //      If the first character of a signed conversion is not a sign, or if a signed conversion
    //      results in no characters, a space is prefixed to the result

    uint64_t number = zext;
    char     conv = spec.conv;
    char     sign = '\0';
    int      base = 10;
    size_t   nprefix = 0;
    bool     upper = false;

    switch (conv)
    {
    default:
    case 'd':
    case 'i':
        base = 10;
        sign = ComputeSignChar(sext < 0, spec.sign, spec.fill);
        if (sext < 0)
            number = 0 - static_cast<uint64_t>(sext);
        break;
    case 'u':
        base = 10;
        break;
    case 'x':
    case 'X':
        upper = (conv == 'X');
        base = 16;
        nprefix = spec.hash ? 2 : 0;
        break;
    case 'b':
    case 'B':
        upper = (conv == 'B');
        base = 2;
        nprefix = spec.hash ? 2 : 0;
        break;
    case 'o':
        base = 8;
        nprefix = (spec.hash && number != 0) ? 1 : 0;
        break;
    }

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

ErrorCode fmtxx::Util::format_bool(Writer& w, FormatSpec const& spec, bool val)
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

ErrorCode fmtxx::Util::format_char(Writer& w, FormatSpec const& spec, char ch)
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

ErrorCode fmtxx::Util::format_pointer(Writer& w, FormatSpec const& spec, void const* pointer)
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

namespace dtoa {

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

    explicit Double(double d_) : d(d_) {}
    explicit Double(uint64_t bits_) : bits(bits_) {}

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

} // namespace

static int CreateFixedRepresentation(char* buf, int bufsize, int num_digits, int decpt, int precision, Options const& options)
{
    assert(options.decimal_point != '\0');

    if (decpt <= 0)
    {
        // 0.[000]digits[000]

        assert(precision == 0 || precision >= -decpt + num_digits);

        if (precision > 0)
        {
            int const len       = 2 + precision;
            int const pad_front = 2 + -decpt;
            int const pad_back  = len - num_digits - pad_front;

            auto I = MakeArrayIterator(buf, bufsize);

            // digits --> 0.[000]digits
            std::copy_backward(I, I + num_digits, I + (num_digits + pad_front));
            std::fill_n(I, pad_front, '0');
            I[1] = options.decimal_point;

            // 0.[000]digits --> 0.[000]digits[000]
            std::fill_n(I + (num_digits + pad_front), pad_back, '0');

            return len;
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

        assert(precision >= num_digits - decpt); // >= 1

        auto I = MakeArrayIterator(buf, bufsize);

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
    assert(!d.IsNegative());
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
    assert(!d.IsNegative());

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
    MaybeUnused(buffer_size);

    char const* const xdigits = upper ? kUpperDigits : kLowerDigits;

    Double const d{v};

    assert(!d.IsSpecial()); // NaN or infinity?
    assert(!d.IsNegative());

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

        //
        // FIXME:
        // Use round-to-nearest-even!
        //

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
    assert(!d.IsNegative());

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

static ErrorCode HandleSpecialFloat(Writer& w, FormatSpec const& spec, char sign, bool upper, bool is_nan)
{
    if (is_nan)
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

ErrorCode fmtxx::Util::format_double(Writer& w, FormatSpec const& spec, double x)
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
    bool   upper = false;

    switch (conv)
    {
    default:
        conv = 's';
        options.exponent_char = 'e';
        break;
    case 's':
    case 'S':
        upper = (conv == 'S');
        options.exponent_char = upper ? 'E' : 'e';
        break;
    case 'e':
    case 'E':
        upper = (conv == 'E');
        options.exponent_char = conv;
        if (prec < 0)
            prec = 6;
        break;
    case 'f':
    case 'F':
        upper = (conv == 'F');
        if (prec < 0)
            prec = 6;
        break;
    case 'g':
    case 'G':
        upper = (conv == 'G');
        options.exponent_char = upper ? 'E' : 'e';
        if (prec < 0)
            prec = 6;
        break;
    case 'a':
    case 'A':
        upper = (conv == 'A');
        conv = upper ? 'X' : 'x';
        options.use_upper_case_digits = upper;
        options.min_exponent_digits   = 1;
        options.exponent_char         = upper ? 'P' : 'p';
        nprefix = 2;
        break;
    case 'x':
    case 'X':
        upper = (conv == 'X');
        options.use_upper_case_digits = upper;
        options.normalize             = true;
        options.use_alternative_form  = false;
        options.min_exponent_digits   = 1;
        options.exponent_char         = upper ? 'P' : 'p';
        nprefix = spec.hash ? 2 : 0;
        break;
    }

    dtoa::Double const d { x };

    bool   const neg = (d.Sign() != 0);
    double const abs_x = d.Abs();
    char   const sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
        return HandleSpecialFloat(w, spec, sign, upper, d.IsNaN());

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
        assert(false && "internal error");
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
        spec.width = EXPECT_NOT(spec.width == INT_MIN) ? INT_MAX /*[recover]*/ : -spec.width;
        spec.align = Align::left;
    }
}

static ErrorCode CallFormatFunc(Writer& w, FormatSpec const& spec, Arg const& arg, Type type)
{
    switch (type)
    {
    case Type::none:
    case Type::formatspec:
        assert(false && "internal error");
        break;
    case Type::other:
        return arg.other.func(w, spec, arg.other.value);
    case Type::string:
        return Util::format_string(w, spec, arg.string.data, arg.string.size);
    case Type::pvoid:
        return Util::format_pointer(w, spec, arg.pvoid);
    case Type::pchar:
        return Util::format_char_pointer(w, spec, arg.pchar);
    case Type::char_:
        return Util::format_char(w, spec, arg.char_);
    case Type::bool_:
        return Util::format_bool(w, spec, arg.bool_);
    case Type::schar:
        return Util::format_int(w, spec, arg.schar);
    case Type::sshort:
        return Util::format_int(w, spec, arg.sshort);
    case Type::sint:
        return Util::format_int(w, spec, arg.sint);
    case Type::slonglong:
        return Util::format_int(w, spec, arg.slonglong);
    case Type::ulonglong:
        return Util::format_int(w, spec, arg.ulonglong);
    case Type::double_:
        return Util::format_double(w, spec, arg.double_);
    case Type::last:
        assert(false && "internal error");
        break;
    }

    return {}; // unreachable
}

static bool IsDigit(char ch) { return '0' <= ch && ch <= '9'; }

static bool ParseInt(int& value, string_view::const_iterator& f, string_view::const_iterator end)
{
    assert(f != end && IsDigit(*f)); // internal error
    auto const f0 = f;

    int x = *f - '0';

    while (++f != end && IsDigit(*f))
    {
        if ((f - f0) + 1 > std::numeric_limits<int>::digits10)
        {
            if EXPECT_NOT(x > INT_MAX / 10 || (*f - '0') > INT_MAX - 10 * x)
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

static ErrorCode GetIntArg(int& value, int index, Arg const* args, Types types)
{
    switch (types[index])
    {
    case Type::none:
        static_cast<void>(EXPECT_NOT("index out of range"));
        return ErrorCode::index_out_of_range;

    case Type::schar:
        value = args[index].schar;
        return {};

    case Type::sshort:
        value = args[index].sshort;
        return {};

    case Type::sint:
        value = args[index].sint;
        return {};

    case Type::slonglong:
        if EXPECT_NOT(args[index].slonglong > INT_MAX)
            return ErrorCode::value_out_of_range;
        if EXPECT_NOT(args[index].slonglong < INT_MIN)
            return ErrorCode::value_out_of_range;
        value = static_cast<int>(args[index].slonglong);
        return {};

    case Type::ulonglong:
        if EXPECT_NOT(args[index].ulonglong > INT_MAX)
            return ErrorCode::value_out_of_range;
        value = static_cast<int>(args[index].ulonglong);
        return {};

    default:
        static_cast<void>(EXPECT_NOT("invalid integer argument"));
        return ErrorCode::invalid_argument;
    }
}

static ErrorCode ParseLBrace(int& value, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '{'); // internal error

    ++f; // skip '{'

    if EXPECT_NOT(f == end)
        return ErrorCode::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        bool ok = ParseInt(index, f, end);
        if EXPECT_NOT(!ok)
            return ErrorCode::invalid_format_string;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }
    else
    {
        index = nextarg++;
    }

    if EXPECT_NOT(*f != '}')
        return ErrorCode::invalid_format_string;

    ++f; // skip '}'

    return GetIntArg(value, index, args, types);
}

static ErrorCode ParseFormatSpecArg(FormatSpec& spec, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '*');

    ++f;
    if EXPECT_NOT(f == end)
        return ErrorCode::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        bool ok = ParseInt(index, f, end);
        if EXPECT_NOT(!ok)
            return ErrorCode::invalid_format_string;
    }
    else
    {
        index = nextarg++;
    }

    if EXPECT_NOT(types[index] == Type::none)
        return ErrorCode::index_out_of_range;
    if EXPECT_NOT(types[index] != Type::formatspec)
        return ErrorCode::invalid_argument;

    spec = *static_cast<FormatSpec const*>(args[index].pvoid);
    FixNegativeFieldWidth(spec);

    return {};
}

static bool ParseAlign(FormatSpec& spec, char c)
{
    switch (c) {
    case '<':
        spec.align = Align::left;
        return true;
    case '>':
        spec.align = Align::right;
        return true;
    case '^':
        spec.align = Align::center;
        return true;
    case '=':
        spec.align = Align::pad_after_sign;
        return true;
    }

    return false;
}

static ErrorCode ParseFormatSpec(FormatSpec& spec, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == ':');

    ++f;
    if EXPECT_NOT(f == end)
        return ErrorCode::invalid_format_string;

    if (f + 1 != end && ParseAlign(spec, *(f + 1)))
    {
        spec.fill = *f;
        f += 2;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }
    else if (ParseAlign(spec, *f))
    {
        ++f;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }

    for (;;)
    {
        switch (*f)
        {
// Flags
        case '-':
            spec.sign = Sign::minus;
            ++f;
            break;
        case '+':
            spec.sign = Sign::plus;
            ++f;
            break;
        case ' ':
            spec.sign = Sign::space;
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
                bool ok = ParseInt(spec.width, f, end);
                if EXPECT_NOT(!ok)
                    return ErrorCode::invalid_format_string;
            }
            break;
        case '{':
            {
                Failed ec = ParseLBrace(spec.width, f, end, nextarg, args, types);
                if EXPECT_NOT(ec)
                    return ec;
                FixNegativeFieldWidth(spec);
            }
            break;
// Precision
        case '.':
            ++f;
            if EXPECT_NOT(f == end)
                return ErrorCode::invalid_format_string;
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
                {
                    bool ok = ParseInt(spec.prec, f, end);
                    if EXPECT_NOT(!ok)
                        return ErrorCode::invalid_format_string;
                }
                break;
            case '{':
                {
                    Failed ec = ParseLBrace(spec.prec, f, end, nextarg, args, types);
                    if EXPECT_NOT(ec)
                        return ec;
                }
                break;
            default:
                spec.prec = 0;
                break;
            }
            break;
// Conversion
        case '!':
        case '}':
            return {};
        default:
            spec.conv = *f;
            ++f;
            return {};
        }

        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }
}

static ErrorCode ParseStyle(FormatSpec& spec, string_view::const_iterator& f, string_view::const_iterator end)
{
    assert(f != end && *f == '!');

    ++f;
    if EXPECT_NOT(f == end)
        return ErrorCode::invalid_format_string;

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

    if EXPECT_NOT(f0 == end)
        return ErrorCode::invalid_format_string;

    f = std::find(f, end, delim == '\0' ? '}' : delim);

    spec.style = {&*f0, static_cast<size_t>(f - f0)};

    if (delim != '\0')
    {
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
        ++f; // skip delim
    }

    return {};
}

static ErrorCode ParseReplacementField(FormatSpec& spec, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end);

    if (*f == '*')
    {
        Failed ec = ParseFormatSpecArg(spec, f, end, nextarg, args, types);
        if EXPECT_NOT(ec)
            return ec;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }

    if (*f == ':')
    {
        Failed ec = ParseFormatSpec(spec, f, end, nextarg, args, types);
        if EXPECT_NOT(ec)
            return ec;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }

    if (*f == '!')
    {
        Failed ec = ParseStyle(spec, f, end);
        if EXPECT_NOT(ec)
            return ec;
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }

    if EXPECT_NOT(*f != '}')
        return ErrorCode::invalid_format_string;

    ++f;

    return {};
}

ErrorCode fmtxx::impl::DoFormat(Writer& w, string_view format, Arg const* args, Types types)
{
    if (format.empty())
        return {};

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

        if EXPECT_NOT(f == end) // "missing '}' or stray '}'"
            return ErrorCode::invalid_format_string;

        if (*prev == *f) // '{{' or '}}'
        {
            s = f;
            ++f;
            continue;
        }

        if EXPECT_NOT(*prev == '}')
            return ErrorCode::invalid_format_string;

        int arg_index = -1;
        if (IsDigit(*f))
        {
            bool ok = ParseInt(arg_index, f, end);
            if EXPECT_NOT(!ok)
                return ErrorCode::invalid_format_string;
            if EXPECT_NOT(f == end)
                return ErrorCode::invalid_format_string;
        }

        FormatSpec spec;
        if (*f != '}')
        {
            Failed ec = ParseReplacementField(spec, f, end, nextarg, args, types);
            if EXPECT_NOT(ec)
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

        if EXPECT_NOT(arg_type == Type::none)
            return ErrorCode::index_out_of_range;
        if EXPECT_NOT(arg_type == Type::formatspec)
            return ErrorCode::invalid_argument;

        if (Failed ec = CallFormatFunc(w, spec, args[arg_index], arg_type))
            return ec;
    }

    return {};
}

static ErrorCode ParseAsterisk(int& value, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *f == '*'); // internal error

    ++f;
    if EXPECT_NOT(f == end)
        return ErrorCode::invalid_format_string;

    int index;
    if (IsDigit(*f))
    {
        bool ok = ParseInt(index, f, end);
        if EXPECT_NOT(!ok)
            return ErrorCode::invalid_format_string;

        index -= 1; // Positional arguments are 1-based.

        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
        if EXPECT_NOT(index < 0)
            return ErrorCode::invalid_format_string;
        if EXPECT_NOT(*f != '$')
            return ErrorCode::invalid_format_string;
        ++f;
    }
    else
    {
        index = nextarg++;
    }

    return GetIntArg(value, index, args, types);
}

static ErrorCode ParsePrintfSpec(int& arg_index, FormatSpec& spec, string_view::const_iterator& f, string_view::const_iterator end, int& nextarg, Arg const* args, Types types)
{
    assert(f != end && *(f - 1) == '%');

    bool has_precision = false;

    for (;;)
    {
        switch (*f)
        {
// Flags
        case '-':
            // N1570, p. 310:
            // If the 0 and - flags both appear, the 0 flag is ignored
            spec.zero = false;
            spec.align = Align::left;
            ++f;
            break;
        case '+':
            spec.sign = Sign::plus;
            ++f;
            break;
        case ' ':
            // N1570, p. 310:
            // If the space and + flags both appear, the space flag is ignored
            if (spec.sign != Sign::plus)
                spec.sign = Sign::space;
            ++f;
            break;
        case '#':
            spec.hash = true;
            ++f;
            break;
        case '0':
            // N1570, p. 310:
            // If the 0 and - flags both appear, the 0 flag is ignored
            if (spec.align != Align::left)
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

                bool ok = ParseInt(n, f, end);
                if EXPECT_NOT(!ok)
                    return ErrorCode::invalid_format_string;

                if EXPECT_NOT(f == end)
                    return ErrorCode::invalid_format_string;

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
            {
                Failed ec = ParseAsterisk(spec.width, f, end, nextarg, args, types);
                if EXPECT_NOT(ec)
                    return ec;
                FixNegativeFieldWidth(spec);
            }
            break;
// Precision
        case '.':
            has_precision = true;
            ++f;
            if EXPECT_NOT(f == end)
                return ErrorCode::invalid_format_string;
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
                {
                    bool ok = ParseInt(spec.prec, f, end);
                    if EXPECT_NOT(!ok)
                        return ErrorCode::invalid_format_string;
                }
                break;
            case '*':
                {
                    Failed ec = ParseAsterisk(spec.prec, f, end, nextarg, args, types);
                    if EXPECT_NOT(ec)
                        return ec;
                }
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
            // N1570, pp. 310:
            // For d, i, o, u, x, and X conversions, if a precision is specified, the 0 flag is ignored
            if (has_precision)
                spec.zero = false;
            spec.conv = *f;
            ++f;
            return {};
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
            return {};
        case 'n':
            // The number of characters written so far is stored into the integer
            // indicated by the int * (or variant) pointer argument.
            // No argument is converted.
            static_cast<void>(EXPECT_NOT("'n' conversion not supported"));
            return ErrorCode::not_supported;
        case 'm':
            // (Glibc extension.) Print output of strerror(errno).
            // No argument is required.
            static_cast<void>(EXPECT_NOT("'m' conversion not supported"));
            return ErrorCode::not_supported;
        default:
            static_cast<void>(EXPECT_NOT("unknown conversion"));
            return ErrorCode::invalid_format_string;
        }

        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;
    }
}

ErrorCode fmtxx::impl::DoPrintf(Writer& w, string_view format, Arg const* args, Types types)
{
    if (format.empty())
        return {};

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
        if EXPECT_NOT(f == end)
            return ErrorCode::invalid_format_string;

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
            Failed ec = ParsePrintfSpec(arg_index, spec, f, end, nextarg, args, types);
            if EXPECT_NOT(ec)
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

        if EXPECT_NOT(arg_type == Type::none)
            return ErrorCode::index_out_of_range;
        if EXPECT_NOT(arg_type == Type::formatspec)
            return ErrorCode::invalid_argument;

        if (Failed ec = CallFormatFunc(w, spec, args[arg_index], arg_type))
            return ec;
    }

    return {};
}

ErrorCode fmtxx::impl::DoFormat(std::FILE* file, string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return ::fmtxx::impl::DoFormat(w, format, args, types);
}

ErrorCode fmtxx::impl::DoPrintf(std::FILE* file, string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return ::fmtxx::impl::DoPrintf(w, format, args, types);
}

namespace {

class StringWriter final : public Writer
{
public:
    std::string& str;

    explicit StringWriter(std::string& s) : str(s) {}

private:
    ErrorCode Put(char c) override;
    ErrorCode Write(char const* ptr, size_t len) override;
    ErrorCode Pad(char c, size_t count) override;
};

inline ErrorCode StringWriter::Put(char c)
{
    str.push_back(c);
    return {};
}

inline ErrorCode StringWriter::Write(char const* ptr, size_t len)
{
    str.append(ptr, len);
    return {};
}

inline ErrorCode StringWriter::Pad(char c, size_t count)
{
    str.append(count, c);
    return {};
}

} // namespace

ErrorCode fmtxx::impl::DoFormat(std::string& str, string_view format, Arg const* args, Types types)
{
    StringWriter w{str};
    return ::fmtxx::impl::DoFormat(w, format, args, types);
}

ErrorCode fmtxx::impl::DoPrintf(std::string& str, string_view format, Arg const* args, Types types)
{
    StringWriter w{str};
    return ::fmtxx::impl::DoPrintf(w, format, args, types);
}

namespace {

class ToCharsWriter final : public Writer
{
public:
    char*       next = nullptr;
    char* const last = nullptr;

    ToCharsWriter() = default;
    ToCharsWriter(char* first_, char* last_) : next(first_), last(last_) {}

private:
    ErrorCode Put(char c) noexcept override;
    ErrorCode Write(char const* str, size_t len) noexcept override;
    ErrorCode Pad(char c, size_t count) noexcept override;
};

inline ErrorCode ToCharsWriter::Put(char c) noexcept
{
    if (last - next < 1)
        return ErrorCode::io_error;

    *next++ = c;
    return {};
}

inline ErrorCode ToCharsWriter::Write(char const* ptr, size_t len) noexcept
{
    //
    // XXX:
    // Write as much as possible?!?!
    //

    if (static_cast<size_t>(last - next) < len)
        return ErrorCode::io_error;

    std::copy_n(ptr, len, MakeArrayIterator(next, last - next));
    next += len;
    return {};
}

inline ErrorCode ToCharsWriter::Pad(char c, size_t count) noexcept
{
    //
    // XXX:
    // Write as much as possible?!?!
    //

    if (static_cast<size_t>(last - next) < count)
        return ErrorCode::io_error;

    std::fill_n(MakeArrayIterator(next, last - next), count, c);
    next += count;
    return {};
}

} // namespace

ToCharsResult fmtxx::impl::DoFormatToChars(char* first, char* last, string_view format, Arg const* args, Types types)
{
    ToCharsWriter w{first, last};

    if (Failed ec = fmtxx::impl::DoFormat(w, format, args, types))
        return ToCharsResult{last, ec};

    return ToCharsResult{w.next, ErrorCode{}};
}

ToCharsResult fmtxx::impl::DoPrintfToChars(char* first, char* last, string_view format, Arg const* args, Types types)
{
    ToCharsWriter w{first, last};

    if (Failed ec = fmtxx::impl::DoPrintf(w, format, args, types))
        return ToCharsResult{last, ec};

    return ToCharsResult{w.next, ErrorCode{}};
}

int fmtxx::impl::DoFileFormat(std::FILE* file, string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(::fmtxx::impl::DoFormat(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::DoFilePrintf(std::FILE* file, string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(::fmtxx::impl::DoPrintf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::DoArrayFormat(char* buf, size_t bufsize, string_view format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(::fmtxx::impl::DoFormat(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::DoArrayPrintf(char* buf, size_t bufsize, string_view format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(::fmtxx::impl::DoPrintf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}
