// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"

#include "Dtoa.h"

#include <cstring>
#include <algorithm>
#include <ostream>

using namespace fmtxx;
using namespace fmtxx::impl;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

// Maximum supported minimum field width.
// This is not an implementation limit, but its just a sane upper bound.
enum { kMaxFieldWidth = 1024 * 4 };

// Maximum supported integer precision (= minimum number of digits).
enum { kMaxIntPrec = 256 };

// Maximum supported floating point precision.
// Precision required for denorm_min (= [751 digits] 10^-323) when using %f
enum { kMaxFloatPrec = 751 + 323 };

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename T> static constexpr T Min(T x, T y) { return y < x ? y : x; }
template <typename T> static constexpr T Max(T x, T y) { return y < x ? x : y; }

template <typename T>
static inline void UnusedParameter(T&&) {}

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

bool fmtxx::FILEBuffer::Put(char c) noexcept
{
    return EOF != std::fputc(c, os);
}

bool fmtxx::FILEBuffer::Write(char const* str, size_t len) noexcept
{
    return len == std::fwrite(str, 1, len, os);
}

bool fmtxx::FILEBuffer::Pad(char c, size_t count) noexcept
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

bool fmtxx::CharArrayBuffer::Put(char c) noexcept
{
    if (os.next >= os.last)
        return false;

    *os.next++ = c;
    return true;
}

bool fmtxx::CharArrayBuffer::Write(char const* str, size_t len) noexcept
{
    size_t const n = Min(len, static_cast<size_t>(os.last - os.next));

    std::memcpy(os.next, str, n);
    os.next += n;
    return n == len;
}

bool fmtxx::CharArrayBuffer::Pad(char c, size_t count) noexcept
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

static void ComputePadding(size_t len, Align align, int width, size_t& lpad, size_t& spad, size_t& rpad)
{
    assert(width >= 0); // internal error

    if (width > kMaxFieldWidth)
        width = kMaxFieldWidth;

    size_t const w = static_cast<size_t>(width);
    if (w <= len)
        return;

    size_t const d = w - len;
    switch (align)
    {
    case Align::Default:
    case Align::Right:
        lpad = d;
        break;
    case Align::Left:
        rpad = d;
        break;
    case Align::Center:
        lpad = d/2;
        rpad = d - d/2;
        break;
    case Align::PadAfterSign:
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

errc fmtxx::Util::FormatString(FormatBuffer& fb, FormatSpec const& spec, char const* str, size_t len)
{
    size_t const n = (spec.prec >= 0)
        ? Min(len, static_cast<size_t>(spec.prec))
        : len;

    return PrintAndPadString(fb, spec, str, n);
}

errc fmtxx::Util::FormatString(FormatBuffer& fb, FormatSpec const& spec, char const* str)
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

    ComputePadding(len, spec.zero ? Align::PadAfterSign : spec.align, spec.width, lpad, spad, rpad);

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

bool fmtxx::Util::WriteSignedInt(FormatBuffer& fb, int64_t val)
{
    uint64_t n = (val >= 0) ? static_cast<uint64_t>(val) : 0 - static_cast<uint64_t>(val);

    char buf[64];
    char* const l = buf + 64;
    char*       f = DecIntToAsciiBackwards(l, n);

    if (val < 0)
        *--f = '-';

    return fb.Write(f, static_cast<size_t>(l - f));
}

bool fmtxx::Util::WriteUnsignedInt(FormatBuffer& fb, uint64_t val)
{
    char buf[64];
    char* const l = buf + 64;
    char* const f = DecIntToAsciiBackwards(l, val);

    return fb.Write(f, static_cast<size_t>(l - f));
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
        do *--last = xdigits[n & 7]; while (n >>= 3);
        return last;
    case 2:
        do *--last = xdigits[n & 1]; while (n >>= 1);
        return last;
    }

    assert(!"not implemented"); // internal error
    return last;
}

bool fmtxx::Util::WriteHexInt(FormatBuffer& fb, uint64_t val)
{
    char buf[64];
    char* const l = buf + 64;
    char*       f = IntToAsciiBackwards(l, val, /*base*/ 16, /*capitals*/ false);

    *--f = 'x';
    *--f = '0';

    return fb.Write(f, static_cast<size_t>(l - f));
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

errc fmtxx::Util::FormatInt(FormatBuffer& fb, FormatSpec const& spec, int64_t sext, uint64_t zext)
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

    static constexpr int kMaxSeps = (kMaxIntPrec - 1) / 3;
    static constexpr int kBufSize = kMaxIntPrec + kMaxSeps;

    // Need at least 64-characters for UINT64_MAX in base 2.
    static_assert(kMaxIntPrec >= 64, "invalid parameter");

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
    return PrintAndPadNumber(fb, spec, sign, prefix, nprefix, f, static_cast<size_t>(l - f));
}

errc fmtxx::Util::FormatBool(FormatBuffer& fb, FormatSpec const& spec, bool val)
{
    switch (spec.conv)
    {
    default:
    case 's':
    case 't':
        return PrintAndPadString(fb, spec, val ? "true" : "false");
    case 'y':
        return PrintAndPadString(fb, spec, val ? "yes" : "no");
    case 'o':
        return PrintAndPadString(fb, spec, val ? "on" : "off");
    }
}

errc fmtxx::Util::FormatChar(FormatBuffer& fb, FormatSpec const& spec, char ch)
{
    switch (spec.conv)
    {
    default:
    case 's':
    case 'c':
        return PrintAndPadString(fb, spec, &ch, 1u);
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
    case 'o':
        return FormatInt(fb, spec, ch);
    }
}

errc fmtxx::Util::FormatPointer(FormatBuffer& fb, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return PrintAndPadString(fb, spec, "(nil)");

    FormatSpec fs = spec;
    switch (fs.conv)
    {
    default:
    case 's':
    case 'p':
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

    return FormatInt(fb, fs, reinterpret_cast<uintptr_t>(pointer));
}

namespace {

struct Double
{
    static uint64_t const kSignMask        = 0x8000000000000000;
    static uint64_t const kExponentMask    = 0x7FF0000000000000;
    static uint64_t const kSignificandMask = 0x000FFFFFFFFFFFFF;

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

bool fmtxx::Util::WriteDouble(FormatBuffer& fb, double x)
{
    Double const d { x };

    bool   const neg = (d.Sign() != 0);
    double const abs_x = d.Abs();

    if (d.IsNaN())
        return fb.Write("nan", 3);
    if (d.IsInf())
        return fb.Write(neg ? "-inf" : "inf", neg ? 4u : 3u);

    char buf[64];
    buf[0] = '-';

    auto const res = dtoa::ToECMAScript(buf + 1 /*sign*/, buf + 64, abs_x);
    assert(res.success); // cannot fail with a large enough buffer

    return fb.Write(neg ? buf : buf + 1, static_cast<size_t>(res.size) + (neg ? 1u : 0u));
}

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

errc fmtxx::Util::FormatDouble(FormatBuffer& fb, FormatSpec const& spec, double x)
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
        //[[fallthrough]];
    case 's':
    case 'S':
        options.exponent_char = (conv == 's') ? 'e' : 'E';
        options.min_exponent_digits = 1;
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
        conv = 'x';
        options.min_exponent_digits = 1;
        options.exponent_char = 'p';
        nprefix = 2;
        break;
    case 'A':
        conv = 'X';
        options.use_upper_case_digits = true;
        options.min_exponent_digits = 1;
        options.exponent_char = 'P';
        nprefix = 2;
        break;
    case 'x':
        options.normalize = true;
        options.use_alternative_form = false;
        options.min_exponent_digits = 1;
        options.exponent_char = 'p';
        nprefix = spec.hash ? 2 : 0;
        break;
    case 'X':
        options.use_upper_case_digits = true;
        options.normalize = true;
        options.use_alternative_form = false;
        options.min_exponent_digits = 1;
        options.exponent_char = 'P';
        nprefix = spec.hash ? 2 : 0;
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
    case 'x':
    case 'X':
        res = dtoa::ToHex(buf, buf + kBufSize, abs_x, prec, options);
        break;
    default:
        res = { false, 0 };
        assert(!"internal error");
        break;
    }

    assert(res.success); // ouch :-(

    size_t const buflen = static_cast<size_t>(res.size);

    char const prefix[] = {'0', conv};
    return PrintAndPadNumber(fb, spec, sign, prefix, nprefix, buf, buflen);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#if FMTXX_FORMAT_STRING_CHECK_POLICY == 0
#define EXPECT(EXPR, MSG) (assert(EXPR), true)
#elif FMTXX_FORMAT_STRING_CHECK_POLICY == 1
#define EXPECT(EXPR, MSG) (assert(EXPR), (EXPR))
#elif FMTXX_FORMAT_STRING_CHECK_POLICY == 2
#define EXPECT(EXPR, MSG) ((EXPR) ? true : throw std::runtime_error(MSG))
#endif

static errc CallFormatFunc(FormatBuffer& fb, FormatSpec const& spec, Types::value_type type, Arg const& arg)
{
    switch (type)
    {
    case Types::T_NONE:
        assert(!"internal error");
        return errc::success;
    case Types::T_OTHER:
        return arg.other.func(fb, spec, arg.other.value);
    case Types::T_STRING:
        return fmtxx::Util::FormatString(fb, spec, arg.string.data(), arg.string.size());
    case Types::T_PVOID:
        return fmtxx::Util::FormatPointer(fb, spec, arg.pvoid);
    case Types::T_PCHAR:
        return fmtxx::Util::FormatString(fb, spec, arg.pchar);
    case Types::T_CHAR:
        return fmtxx::Util::FormatChar(fb, spec, arg.char_);
    case Types::T_BOOL:
        return fmtxx::Util::FormatBool(fb, spec, arg.bool_);
    case Types::T_SCHAR:
        return fmtxx::Util::FormatInt(fb, spec, arg.schar);
    case Types::T_SSHORT:
        return fmtxx::Util::FormatInt(fb, spec, arg.sshort);
    case Types::T_SINT:
        return fmtxx::Util::FormatInt(fb, spec, arg.sint);
    case Types::T_SLONGLONG:
        return fmtxx::Util::FormatInt(fb, spec, arg.slonglong);
    case Types::T_ULONGLONG:
        return fmtxx::Util::FormatInt(fb, spec, arg.ulonglong);
    case Types::T_DOUBLE:
        return fmtxx::Util::FormatDouble(fb, spec, arg.double_);
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

    int x = *f - '0';

    while (++f != end && IsDigit(*f))
    {
        if (!EXPECT( x <= INT_MAX / 10 && *f - '0' <= INT_MAX - 10 * x, "integer overflow" ))
        {
            while (++f != end && IsDigit(*f)) {}
            return INT_MAX;
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
        if (args[index].slonglong < INT_MIN)
            value = INT_MIN;
        else if (args[index].slonglong > INT_MAX)
            value = INT_MAX;
        else
            value = static_cast<int>(args[index].slonglong);
        break;
    case Types::T_ULONGLONG:
        if (args[index].ulonglong > INT_MAX)
            value = INT_MAX;
        else
            value = static_cast<int>(args[index].ulonglong);
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

    int const index = IsDigit(*f)
        ? ParseInt(f, end)
        : nextarg++;

    if (!EXPECT(types[index] == Types::T_FORMATSPEC, "invalid argument: FormatSpec expected"))
        return;

    spec = *static_cast<FormatSpec const*>(args[index].pvoid);

    if (spec.width < 0) {
        spec.width = (spec.width == INT_MIN) ? INT_MAX : -spec.width;
        spec.align = Align::Left;
    }
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
            if (spec.width < 0) {
                spec.width = (spec.width == INT_MIN) ? INT_MAX : -spec.width;
                spec.align = Align::Left;
            }
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
        case ',':
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
    assert(f != end && *f == ',');

    auto const f0 = ++f;

    if (!EXPECT(f0 != end, "unexpected end of format-string"))
        return;

    while (*f != '}' && ++f != end)
        ;

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

    if (*f == ',')
    {
        ParseStyle(spec, f, end);
        if (!EXPECT(f != end, "unexpected end of format-string"))
            return;
    }

    if (!EXPECT(*f == '}', "unexpected characters in format-spec"))
    {
#if 0
        for (;;) {
            if (++f == end)
                return;
            if (*f == '}')
                break;
        }
#else
        return;
#endif
    }

    ++f;
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
            return errc::io_error; // io-error

        if (f == end) // done.
            break;

        auto const prev = f;
        ++f; // skip '{' or '}'

        if (!EXPECT(f != end, "missing '}' or stray '}'"))
            return fb.Put(*prev) ? errc::success : errc::io_error;

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

        auto const err = CallFormatFunc(fb, spec, arg_type, args[arg_index]);
        if (err != errc::success)
            return err;
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

errc fmtxx::impl::DoFormat(CharArray& os, std::string_view format, Types types, Arg const* args)
{
    CharArrayBuffer fb { os };
    return DoFormat(fb, format, types, args);
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
            if (!EXPECT(f != end, "unexpected end of format-string"))
                return;
            // If this number ends with a '$' its actually a positional argument
            // index and not the field width.
            if (*f == '$') {
                ++f;
                arg_index = spec.width - 1;
            }
            break;
        case '*':
            ParseAsterisk(spec.width, f, end, nextarg, types, args);
            if (spec.width < 0) {
                spec.width = (spec.width == INT_MIN) ? INT_MAX : -spec.width;
                spec.align = Align::Left;
            }
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
        case 'I':
            // I32 and I64 are Microsoft extensions.
            if (end - f >= 3 && ((f[1] == '3' && f[2] == '2') || (f[1] == '6' && f[2] == '4'))) {
                f += 3;
            }
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
        case 'p':
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

        auto const prev = f;
        ++f; // skip '%'

        if (!EXPECT(f != end, "unexpected end of format-string"))
            return fb.Put(*prev) ? errc::success : errc::io_error;

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

        auto const err = CallFormatFunc(fb, spec, arg_type, args[arg_index]);
        if (err != errc::success)
            return err;
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

errc fmtxx::impl::DoPrintf(CharArray& os, std::string_view format, Types types, Arg const* args)
{
    CharArrayBuffer fb { os };
    return DoPrintf(fb, format, types, args);
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
