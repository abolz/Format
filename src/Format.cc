// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"

#include "Dtoa.h"

#include <algorithm>
#include <ostream>

using namespace fmtxx;
using namespace fmtxx::impl;

// Maximum supported floating point precision.
// Precision required for denorm_min (= [751 digits] 10^-323) when using %f
enum { kMaxFloatPrec = 751 + 323 };

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

bool fmtxx::StreamBuffer::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof()))
    {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

bool fmtxx::StreamBuffer::Write(char const* str, size_t len)
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

bool fmtxx::StreamBuffer::Pad(char c, size_t count)
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
    assert(IsAlign(align) && "internal error");
    assert(width >= 0 && "internal error");

    size_t const w = static_cast<size_t>(width);
    if (w <= len)
        return;

    size_t const d = w - len;
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

// Prints out exactly LEN characters (including '\0's) starting at STR,
// possibly padding on the left and/or right.
static errc PrintAndPadString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
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

static errc PrintAndPadString(FormatBuffer& fb, FormatSpec const& spec, std::string_view str)
{
    return PrintAndPadString(fb, spec, str.data(), str.size());
}

errc Util::FormatString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
{
    size_t const n = (spec.prec >= 0)
        ? Min(len, static_cast<size_t>(spec.prec))
        : len;

    return PrintAndPadString(fb, spec, str, n);
}

errc Util::FormatString(FormatBuffer& fb, FormatSpec const& spec, char const* str)
{
    if (str == nullptr)
        return PrintAndPadString(fb, spec, "(null)");

    // Use strnlen if a precision was specified.
    // The string may not be null-terminated!
    size_t const len = (spec.prec >= 0)
        ? ::strnlen(str, static_cast<size_t>(spec.prec))
        : ::strlen(str);

    return PrintAndPadString(fb, spec, str, len);
}

static errc PrintAndPadNumber(FormatBuffer& fb, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    size_t const len = (sign ? 1u : 0u) + nprefix + ndigits;

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
    static char const* const kDecDigits100/*[100*2 + 1]*/ =
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
        *--last = kDecDigits100[2*n + 1];
    }

    return last;
}

static char* IntToAsciiBackwards(char* last/*[-64]*/, uint64_t n, int base, bool capitals)
{
    char const* const xdigits = capitals
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
        for (int i = 0; i < group_len; ++i)
            *--dst = *--src;
        *--dst = sep;
    }

    return nsep;
}

errc Util::FormatInt(FormatBuffer& fb, FormatSpec const& spec, int64_t sext, uint64_t zext)
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
        //[[fallthrough]];
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
        if (spec.hash)
            nprefix = 2;
        break;
    case 'b':
    case 'B':
        base = 2;
        if (spec.hash)
            nprefix = 2;
        break;
    case 'o':
        base = 8;
        if (spec.hash && number != 0)
            nprefix = 1;
        break;
    }

    bool const upper = ('A' <= conv && conv <= 'Z');

    // 64: Max. length of integer in base 2.
    // 15: Max. number of grouping chars.
    char buf[64 + 15];
    char*       l = buf + 64;
    char* const f = IntToAsciiBackwards(l, number, base, upper);

    if (spec.tsep)
    {
        int const group_len = (base == 10) ? 3 : 4;
        int const pos       = static_cast<int>(l - f);

        l += InsertThousandsSep(f, pos, spec.tsep, group_len);
    }

    char const prefix[] = {'0', conv};
    return PrintAndPadNumber(fb, spec, sign, prefix, nprefix, f, static_cast<size_t>(l - f));
}

errc Util::FormatBool(FormatBuffer& fb, FormatSpec const& spec, bool val)
{
    return PrintAndPadString(fb, spec, val ? "true" : "false");
}

errc Util::FormatChar(FormatBuffer& fb, FormatSpec const& spec, char ch)
{
    return PrintAndPadString(fb, spec, &ch, 1u);
}

errc Util::FormatPointer(FormatBuffer& fb, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return PrintAndPadString(fb, spec, "(nil)");

    FormatSpec f = spec;
    f.hash = true;
    f.conv = 'x';

    return FormatInt(fb, f, 0, reinterpret_cast<uintptr_t>(pointer));
}

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

static errc HandleSpecialFloat(Double const d, FormatBuffer& fb, FormatSpec const& spec, char sign, bool upper)
{
    assert(d.IsSpecial());

    if (d.IsNaN())
        return PrintAndPadString(fb, spec, upper ? "NAN" : "nan");

    char  inf_lower[] = " inf";
    char  inf_upper[] = " INF";
    char* str = upper ? inf_upper : inf_lower;

    if (sign != '\0')
        *str = sign;
    else
        ++str; // skip leading space

    return PrintAndPadString(fb, spec, str);
}

errc Util::FormatDouble(FormatBuffer& fb, FormatSpec const& spec, double x)
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

    char        conv = spec.conv;
    int         prec = spec.prec;
    char const* prefix = nullptr;

    switch (conv)
    {
    default:
        conv = 's';
        //[[fallthrough]];
    case 'S':
    case 's':
        options.use_alternative_form = false;
        options.exponent_char = (conv == 's') ? 'e' : 'E';
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
        prefix = "0X"; // Always add a prefix. Like printf.
        break;
    case 'a':
        options.exponent_char = 'p';
        options.min_exponent_digits = 1;
        prefix = "0x"; // Always add a prefix. Like printf.
        break;
    case 'X':
        options.use_upper_case_digits = true;
        options.normalize = true;
        options.use_alternative_form = false;
        options.exponent_char = 'P';
        options.min_exponent_digits = 1;
        if (spec.hash) // Add a prefix only if '#' was specified. As with integers.
            prefix = "0X";
        break;
    case 'x':
        options.normalize = true;
        options.use_alternative_form = false;
        options.exponent_char = 'p';
        options.min_exponent_digits = 1;
        if (spec.hash) // Add a prefix only if '#' was specified. As with integers.
            prefix = "0x";
        break;
    }

    if (prec > kMaxFloatPrec)
        prec = kMaxFloatPrec;

    Double const d { x };

    bool   const neg = (d.Sign() != 0);
    double const abs_x = d.Abs();
    char   const sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
    {
        bool const upper = ('A' <= conv && conv <= 'Z');
        return HandleSpecialFloat(d, fb, spec, sign, upper);
    }

    // Allow printing *ALL* double-precision floating-point values with prec <= kMaxFloatPrec
    enum { kBufSize = 309 + 1/*.*/ + kMaxFloatPrec };
    char buf[kBufSize];

    dtoa::Result res;
    switch (conv)
    {
    case 's':
    case 'S':
        res = dtoa::ToECMAScript(buf, buf + kBufSize, abs_x, options.decimal_point, options.exponent_char);
        break;
    case 'f':
    case 'F':
        res = dtoa::ToFixed(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'e':
    case 'E':
        res = dtoa::ToExponential(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'g':
    case 'G':
        res = dtoa::ToGeneral(buf, buf + kBufSize, abs_x, prec, options);
        break;
    case 'a':
    case 'A':
    case 'x':
    case 'X':
        res = dtoa::ToHex(buf, buf + kBufSize, abs_x, prec, options);
        break;
    default:
        res = { false, 0 };
        assert(!"internal error");
        break;
    }

    if (!res.success)
        return PrintAndPadString(fb, spec, "[[internal buffer too small]]");

    size_t const buflen = static_cast<size_t>(res.size);
    return PrintAndPadNumber(fb, spec, sign, prefix, prefix != nullptr ? 2u : 0u, buf, buflen);
}

//
// Parse a non-negative integer in the range [0, INT_MAX].
// Returns -1 on overflow.
//
// PRE: IsDigit(*s) == true.
//
template <typename It>
static int ParseInt(It& s, It const end)
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
        spec.width = -spec.width;
        spec.align = '<';
    }
}

template <typename It>
static errc ParseFormatSpec(FormatSpec& spec, It& f, It const end, int& nextarg, Types types, Arg const* args)
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
                spec.hash = true;
            else if (*f == '0')
                spec.zero = true;
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
            int const i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing '}'
            spec.width = i;
        }

        if (*f == '.')
        {
            ++f;
            if (f == end)
                return errc::invalid_format_string; // missing '}'

            if (IsDigit(*f))
            {
                int const i = ParseInt(f, end);
                if (i < 0)
                    return errc::invalid_format_string; // overflow
                if (f == end)
                    return errc::invalid_format_string; // missing '}'
                spec.prec = i;
            }
            else
            {
                spec.prec = 0;
            }
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
        auto const f0 = ++f;

        while (f != end && *f != '}')
            ++f;

        spec.style = { &*f0, static_cast<size_t>(f - f0) };
    }

    if (f == end || *f != '}')
        return errc::invalid_format_string; // missing '}'

    return errc::success;
}

static errc CallFormatFunc(FormatBuffer& fb, FormatSpec const& spec, int index, Types types, Arg const* args)
{
    auto const type = types[index];

    if (type == Types::T_NONE)
        return errc::index_out_of_range;

    auto const& arg = args[index];

    switch (type)
    {
    case Types::T_NONE:
        break; // unreachable -- fix warning
    case Types::T_OTHER:
        return arg.other.func(fb, spec, arg.other.value);
    case Types::T_STRING:
        return Util::FormatString(fb, spec, arg.string.data(), arg.string.size());
    case Types::T_PVOID:
        return Util::FormatPointer(fb, spec, arg.pvoid);
    case Types::T_PCHAR:
        return Util::FormatString(fb, spec, arg.pchar);
    case Types::T_CHAR:
        return Util::FormatChar(fb, spec, arg.char_);
    case Types::T_BOOL:
        return Util::FormatBool(fb, spec, arg.bool_);
    case Types::T_SCHAR:
        return Util::FormatInt(fb, spec, arg.schar, static_cast<unsigned char>(arg.schar));
    case Types::T_SSHORT:
        return Util::FormatInt(fb, spec, arg.sshort, static_cast<unsigned short>(arg.sshort));
    case Types::T_SINT:
        return Util::FormatInt(fb, spec, arg.sint, static_cast<unsigned int>(arg.sint));
    case Types::T_SLONGLONG:
        return Util::FormatInt(fb, spec, arg.slonglong, static_cast<unsigned long long>(arg.slonglong));
    case Types::T_ULONGLONG:
        return Util::FormatInt(fb, spec, 0, arg.ulonglong);
    case Types::T_DOUBLE:
        return Util::FormatDouble(fb, spec, arg.double_);
    case Types::T_FORMATSPEC:
        return PrintAndPadString(fb, spec, "[[error]]");
    }

    return errc::success; // unreachable -- fix warning
}

errc fmtxx::impl::DoFormat(FormatBuffer& fb, std::string_view format, Types types, Arg const* args)
{
    if (format.empty())
        return errc::success;

    int nextarg = 0;

    auto       f   = format.begin();
    auto const end = format.end();
    auto       s   = f;
    for (;;)
    {
        while (f != end && *f != '{' && *f != '}')
            ++f;

        if (f != s && !fb.Write(&*s, static_cast<size_t>(f - s)))
            return errc::io_error;

        if (f == end) // done.
            break;

        char const c = *f;

        ++f; // skip '{' or '}'
        if (f == end)
            return errc::invalid_format_string; // missing '}' or stray '}'

        if (*f == c) // '{{' or '}}'
        {
            s = f++;
            continue;
        }

        if (c == '}')
            return errc::invalid_format_string; // stray '}'

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
            auto const ec = ParseFormatSpec(spec, f, end, nextarg, types, args);
            if (ec != errc::success)
                return ec;
        }

        if (index < 0)
            index = nextarg++;

        auto const ec = CallFormatFunc(fb, spec, index, types, args);
        if (ec != errc::success)
            return ec;

        if (f == end) // done.
            break;

        s = ++f; // skip '}'
    }

    return errc::success;
}

errc fmtxx::impl::DoFormat(std::string& os, std::string_view format, Types types, Arg const* args)
{
    StringBuffer fb { os };
    return DoFormat(fb, format, types, args);
}

errc fmtxx::impl::DoFormat(std::FILE* os, std::string_view format, Types types, Arg const* args)
{
    FILEBuffer fb { os };
    return DoFormat(fb, format, types, args);
}

errc fmtxx::impl::DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    std::ostream::sentry const se(os);
    if (se)
    {
        StreamBuffer fb { os };
        return DoFormat(fb, format, types, args);
    }

    return errc::io_error;
}

static errc GetIntArg(int& value, int index, Types types, Arg const* args)
{
    switch (types[index])
    {
    case Types::T_SCHAR:
        value = args[index].schar;
        return errc::success;

    case Types::T_SSHORT:
        value = args[index].sshort;
        return errc::success;

    case Types::T_SINT:
        value = args[index].sint;
        return errc::success;

    case Types::T_SLONGLONG:
        if (args[index].slonglong < INT_MIN || args[index].slonglong > INT_MAX)
            return errc::invalid_argument;
        value = static_cast<int>(args[index].slonglong);
        return errc::success;

    case Types::T_ULONGLONG:
        if (args[index].ulonglong > static_cast<unsigned long long>(INT_MAX))
            return errc::invalid_argument;
        value = static_cast<int>(args[index].ulonglong);
        return errc::success;

    default:
        return errc::invalid_argument; // not an integer type
    }
}

template <typename It>
static errc ParseStar(int& value, It& f, It const end, int& nextarg, Types types, Arg const* args)
{
    assert(*f == '*');

    ++f;
    if (f == end)
        return errc::invalid_format_string; // missing conversion

    int index;
    if (IsDigit(*f))
    {
        int const i = ParseInt(f, end);
        if (i < 0)
            return errc::invalid_format_string; // overflow
        if (f == end)
            return errc::invalid_format_string; // missing conversion

        if (*f != '$')
            return errc::invalid_format_string; // missing '$'

        ++f;
        if (f == end)
            return errc::invalid_format_string; // missing conversion

        // Positional arguments are 1-based.
        if (i < 1)
            return errc::index_out_of_range;

        index = i - 1;
    }
    else
    {
        index = nextarg++;
    }

    return GetIntArg(value, index, types, args);
}

template <typename It>
static errc ParsePrintfSpec(FormatSpec& spec, It& f, It const end, int& nextarg, Types types, Arg const* args, bool skip_flags_and_width)
{
    assert(f != end);

    if (!skip_flags_and_width)
    {
        for (;;)
        {
            if (*f == '-')
                spec.align = '<';
            else if (*f == '+')
                spec.sign = '+';
            else if (*f == ' ')
            {
                // N1570 (p. 310):
                // If the space and + flags both appear, the space flag is ignored.
                if (spec.sign != '+')
                    spec.sign = ' ';
            }
            else if (*f == '#')
                spec.hash = true;
            else if (*f == '0')
                spec.zero = true;
            else if (*f == '\'' || *f == '_')
                spec.tsep = *f;
            else
                break;

            ++f;
            if (f == end)
                return errc::invalid_format_string; // missing conversion
        }

        // Field width
        if (IsDigit(*f))
        {
            int const i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing conversion

            spec.width = i;
        }
        else if (*f == '*')
        {
            errc const err = ParseStar(spec.width, f, end, nextarg, types, args);
            if (err != errc::success)
                return err;

            if (spec.width < 0)
            {
                if (spec.width < -INT_MAX)
                    spec.width = -INT_MAX;
                spec.width = -spec.width;
                spec.align = '<';
            }
        }
    }

    // Precision
    if (*f == '.')
    {
        ++f;
        if (f == end)
            return errc::invalid_format_string; // missing conversion

        if (IsDigit(*f))
        {
            int const i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing conversion

            spec.prec = i;
        }
        else if (*f == '*')
        {
            errc const err = ParseStar(spec.prec, f, end, nextarg, types, args);
            if (err != errc::success)
                return err;

            if (spec.prec < 0)
                spec.prec = -1;
        }
        else
        {
            spec.prec = 0;
        }
    }

    // Length modifiers. Ignored.
    switch (*f)
    {
    case 'h':
    case 'l':
        ++f;
        if (f != end && *f == *std::prev(f))
            ++f;
        if (f == end)
            return errc::invalid_format_string; // missing conversion
        break;
    case 'j':
    case 'z':
    case 't':
    case 'L':
        ++f;
        if (f == end)
            return errc::invalid_format_string; // missing conversion
        break;
    }

    // Conversion
    switch (*f)
    {
    case 's':
    case 'S':
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
    case 'o':
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
    case 'p':
    case 'c':
        spec.conv = *f++;
        break;
    default:
#if 0
        spec.conv = 's';
        break;
#else
        return errc::invalid_format_string; // invalid conversion
#endif
    }

    return errc::success;
}

errc fmtxx::impl::DoPrintf(FormatBuffer& fb, std::string_view format, Types types, Arg const* args)
{
    if (format.empty())
        return errc::success;

    int nextarg = 0;

    auto       f   = format.begin();
    auto const end = format.end();
    auto       s   = f;
    for (;;)
    {
        while (f != end && *f != '%')
            ++f;

        if (f != s && !fb.Write(&*s, static_cast<size_t>(f - s)))
            return errc::io_error;

        if (f == end) // done.
            break;

        ++f; // skip '%'
        if (f == end)
            return errc::invalid_format_string; // missing conversion

        if (*f == '%') // '%%'
        {
            s = f++;
            continue;
        }

        FormatSpec spec;
        bool skip_flags_and_width = false;

        int index = -1;
        if (IsDigit(*f) && *f != '0')
        {
            int const i = ParseInt(f, end);
            if (i < 0)
                return errc::invalid_format_string; // overflow
            if (f == end)
                return errc::invalid_format_string; // missing conversion

            if (*f != '$')
            {
                // This is actually NOT an argument index. It is the field width.
                spec.width = i;
                skip_flags_and_width = true;
            }
            else
            {
                ++f;
                if (f == end)
                    return errc::invalid_format_string; // missing conversion

                // Positional arguments are 1-based.
                assert(i >= 1);
                index = i - 1;
            }
        }

        // Parse (the rest of) the format specification.
        {
            auto const ec = ParsePrintfSpec(spec, f, end, nextarg, types, args, skip_flags_and_width);
            if (ec != errc::success)
                return ec;
        }

        if (index < 0)
            index = nextarg++;

        auto const ec = CallFormatFunc(fb, spec, index, types, args);
        if (ec != errc::success)
            return ec;

        if (f == end) // done.
            break;

        s = f;
    }

    return errc::success;
}

errc fmtxx::impl::DoPrintf(std::string& os, std::string_view format, Types types, Arg const* args)
{
    StringBuffer fb { os };
    return DoPrintf(fb, format, types, args);
}

errc fmtxx::impl::DoPrintf(std::FILE* os, std::string_view format, Types types, Arg const* args)
{
    FILEBuffer fb { os };
    return DoPrintf(fb, format, types, args);
}

errc fmtxx::impl::DoPrintf(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    std::ostream::sentry const se(os);
    if (se)
    {
        StreamBuffer fb { os };
        return DoPrintf(fb, format, types, args);
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
