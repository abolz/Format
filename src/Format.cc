// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"
#include "Format-double.h"

#include <ostream>

using namespace fmtxx;
using namespace fmtxx::impl;

template <typename T> static constexpr T Min(T x, T y) { return y < x ? y : x; }
template <typename T> static constexpr T Max(T x, T y) { return y < x ? x : y; }

// API
fmtxx::FormatBuffer::~FormatBuffer()
{
}

// API
bool fmtxx::StringBuffer::Put(char c)
{
    os.push_back(c);
    return true;
}

// API
bool fmtxx::StringBuffer::Write(char const* str, size_t len)
{
    os.append(str, len);
    return true;
}

// API
bool fmtxx::StringBuffer::Pad(char c, size_t count)
{
    os.append(count, c);
    return true;
}

// API
bool fmtxx::FILEBuffer::Put(char c)
{
    return EOF != std::fputc(c, os);
}

// API
bool fmtxx::FILEBuffer::Write(char const* str, size_t len)
{
    return len == std::fwrite(str, 1, len, os);
}

// API
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

// API
bool fmtxx::StreamBuffer::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof())) {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

// API
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

// API
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

// API
bool fmtxx::CharArrayBuffer::Put(char c)
{
    if (next >= last)
        return false;

    *next++ = c;
    return true;
}

// API
bool fmtxx::CharArrayBuffer::Write(char const* str, size_t len)
{
    if (static_cast<size_t>(last - next) < len)
        return false;

    std::memcpy(next, str, len);
    next += len;
    return true;
}

// API
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

static errc WriteString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
{
    size_t n = len;
    if (spec.prec >= 0)
    {
        if (n > static_cast<size_t>(spec.prec))
            n = static_cast<size_t>(spec.prec);
    }

    return WriteRawString(fb, spec, str, n);
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

    return WriteRawString(fb, spec, str, len);
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

static int InsertThousandsSep(char* buf, int pos, char sep, int group_len)
{
    const int nsep = (pos - 1) / group_len;

    if (nsep <= 0)
        return 0;

    int shift = nsep;
    for (int i = pos - 1; shift > 0; --shift, i -= group_len)
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
        // [[fall_through]]
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

    char buf[64 + 32];
    char*       l = buf + 64;
    char* const f = IntToAsciiBackwards(l, number, base, upper);

    if (spec.tsep)
    {
        const int group_len = (base == 10) ? 3 : 4;
        l += InsertThousandsSep(f, static_cast<int>(l - f), spec.tsep, group_len);
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

static errc WriteDouble(FormatBuffer& fb, FormatSpec const& spec, double x)
{
    char conv = spec.conv;
    bool upper = false;
    bool tostr = false;
    bool tohex = false;
    switch (conv)
    {
    default:
        // I'm sorry Dave, I'm afraid I can't do that.
    case '\0':
        conv = 's';
        tostr = true;
        break;
    case 'S':
        upper = true;
        tostr = true;
        break;
    case 'f':
    case 'e':
    case 'g':
    case 'a':
        break;
    case 'F':
    case 'E':
    case 'G':
    case 'A':
        upper = true;
        break;
    case 'X':
        upper = true;
    case 'x':
        tohex = true;
        break;
    }

    const dtoa::Double d { x };

    const bool neg = (d.Sign() != 0);
    const double abs_x = d.Abs();

    const char sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
    {
        if (d.IsNaN())
            return WriteRawString(fb, spec, upper ? "NAN" : "nan");

        const char inf[] = { sign, upper ? 'I' : 'i', upper ? 'N' : 'n', upper ? 'F' : 'f', '\0' };
        return WriteRawString(fb, spec, inf + (sign == '\0' ? 1 : 0));
    }
    else if (tostr)
    {
        dtoa::FormatOptions options;

        options.use_upper_case_digits       = true;
        options.thousands_sep               = spec.tsep;
        options.use_alternative_form        = false;
        options.min_exponent_digits         = 1;
        options.exponent_char               = 'e';
        options.emit_positive_exponent_sign = true;

        char buf[32];
        const auto res = dtoa::Format_short(buf, buf + 32, abs_x, dtoa::FormatStyle::general, options);
        assert(!res.ec);

        return WriteNumber(fb, spec, sign, nullptr, 0, buf, static_cast<size_t>(res.next - buf));
    }
    else if (tohex)
    {
        dtoa::FormatOptions options;

        options.use_upper_case_digits       = upper;
        options.thousands_sep               = spec.tsep;
        options.use_alternative_form        = false;
        options.min_exponent_digits         = 1;
        options.exponent_char               = 'p';
        options.emit_positive_exponent_sign = true;

        const bool alt = (spec.hash != '\0');

        char buf[32];
        const auto res = dtoa::Format_hex(buf, buf + 32, abs_x, spec.prec, options);
        assert(!res.ec);

        const size_t nprefix = alt ? 2u : 0u;

        return WriteNumber(fb, spec, sign, "0x", nprefix, buf, static_cast<size_t>(res.next - buf));
    }
    else
    {
        const int kBufSize = 1500;
        char buf[kBufSize];

        const bool alt = (spec.hash != '\0');

        const auto res = dtoa::Printf(buf, buf + kBufSize, abs_x, spec.prec, spec.tsep, alt, conv);
        if (res.ec)
            return WriteRawString(fb, spec, "[[internal buffer too small]]");

        return WriteNumber(fb, spec, sign, nullptr, 0, buf, static_cast<size_t>(res.next - buf));
    }
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
