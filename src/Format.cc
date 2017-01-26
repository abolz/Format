// Distributed under the MIT license. See the end of the file for details.

#include "Format.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <ostream>
#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace fmtxx;
using namespace fmtxx::impl;

static bool Put(std::ostream& os, char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof())) {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

static bool Write(std::ostream& os, char const* str, size_t len)
{
    if (len > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
        return false;

    const auto n = static_cast<std::streamsize>(len);

    if (n == 1)
        return Put(os, str[0]);
    if (n == 2)
        return Put(os, str[0]) && Put(os, str[1]);
    if (n == 3)
        return Put(os, str[0]) && Put(os, str[1]) && Put(os, str[2]);
    if (n == 4)
        return Put(os, str[0]) && Put(os, str[1]) && Put(os, str[2]) && Put(os, str[3]);

    if (n != os.rdbuf()->sputn(str, n)) {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

static bool Pad(std::ostream& os, char c, size_t count)
{
    if (count > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
        return false;

    auto n = static_cast<std::streamsize>(count);

    if (n == 1)
        return Put(os, c);
    if (n == 2)
        return Put(os, c) && Put(os, c);
    if (n == 3)
        return Put(os, c) && Put(os, c) && Put(os, c);
    if (n == 4)
        return Put(os, c) && Put(os, c) && Put(os, c) && Put(os, c);

    const std::streamsize kBlockSize = 16;
    const char block[] = {c, c, c, c, c, c, c, c, c, c, c, c, c, c, c, c};

    for ( ; n >= kBlockSize; n -= kBlockSize)
    {
        if (kBlockSize != os.rdbuf()->sputn(block, kBlockSize)) {
            os.setstate(std::ios_base::badbit);
            return false;
        }
    }

    return n > 0 ? Write(os, block, static_cast<size_t>(n)) : true;
}

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

static int WriteRawString(std::ostream& os, FormatSpec const& spec, char const* str, size_t len)
{
    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(len, spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0 && !Pad(os, spec.fill, lpad))
        return -1;
    if (len > 0  && !Write(os, str, len))
        return -1;
    if (rpad > 0 && !Pad(os, spec.fill, rpad))
        return -1;

    return 0;
}

static int WriteString(std::ostream& os, FormatSpec const& spec, char const* str, size_t len)
{
    size_t n = len;
    if (spec.prec >= 0)
    {
        if (n > static_cast<size_t>(spec.prec))
            n = static_cast<size_t>(spec.prec);
    }

    return WriteRawString(os, spec, str, n);
}

static int WriteString(std::ostream& os, FormatSpec const& spec, char const* str)
{
    if (str == nullptr)
        return WriteRawString(os, spec, "(null)", 6);

    // Use strnlen if a precision was specified.
    // The string may not be null-terminated!
    size_t len;
    if (spec.prec >= 0)
        len = ::strnlen(str, static_cast<size_t>(spec.prec));
    else
        len = ::strlen(str);

    return WriteRawString(os, spec, str, len);
}

static char* IntToDecAsciiBackwards(char* last/*[-20]*/, uint64_t n)
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
        return IntToDecAsciiBackwards(last, n);
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

static int WriteNumber(std::ostream& os, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    const size_t len = (sign ? 1u : 0u) + nprefix + ndigits;

    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(len, spec.zero ? '=' : spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0     && !Pad(os, spec.fill, lpad))
        return -1;
    if (sign != '\0' && !Put(os, sign))
        return -1;
    if (nprefix > 0  && !Write(os, prefix, nprefix))
        return -1;
    if (spad > 0     && !Pad(os, spec.zero ? '0' : spec.fill, spad))
        return -1;
    if (ndigits > 0  && !Write(os, digits, ndigits))
        return -1;
    if (rpad > 0     && !Pad(os, spec.fill, rpad))
        return -1;

    return 0;
}

static int WriteInt(std::ostream& os, FormatSpec const& spec, int64_t sext, uint64_t zext)
{
    uint64_t number  = zext;
    int      base    = 10;
    char     conv    = spec.conv;
    char     sign    = '\0';
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

    const bool upper = ('A' <= conv && conv <= 'Z');

    char buf[64];
    const auto l = buf + 64;
    const auto f = IntToAsciiBackwards(l, number, base, upper);

    const char prefix[] = { '0', conv };

    return WriteNumber(os, spec, sign, prefix, nprefix, f, static_cast<size_t>(l - f));
}

static int WriteBool(std::ostream& os, FormatSpec const& spec, bool val)
{
    return WriteRawString(os, spec, val ? "true" : "false", val ? 4u : 5u);
}

static int WriteChar(std::ostream& os, FormatSpec const& spec, char ch)
{
    return WriteString(os, spec, &ch, 1u);
}

static int WritePointer(std::ostream& os, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return WriteRawString(os, spec, "(nil)", 5);

    FormatSpec f = spec;
    f.hash = '#';
    f.conv = 'x';

    return WriteInt(os, f, 0, reinterpret_cast<uintptr_t>(pointer));
}

static int CountDecimalDigits32(uint32_t x)
{
    if (x < 10) return 1;
    if (x < 100) return 2;
    if (x < 1000) return 3;
    if (x < 10000) return 4;
    if (x < 100000) return 5;
    if (x < 1000000) return 6;
    if (x < 10000000) return 7;
    if (x < 100000000) return 8;
    if (x < 1000000000) return 9;
    return 10;
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

namespace {

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

    uint64_t Sign()        const { return (bits & kSignMask      ) >> 63; }
    uint64_t Exponent()    const { return (bits & kExponentMask  ) >> 52; }
    uint64_t Significand() const { return (bits & kSignificandMask)      ; }

    int UnbiasedExponent() const {
        return static_cast<int>(Exponent()) - kExponentBias;
    }

    bool IsZero() const {
        return (bits & ~kSignMask) == 0;
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

#if 0
    bool IsIntegral() const
    {
        const int e = UnbiasedExponent();
        return e >= 52 || (e >= 0 && (Significand() & (kSignificandMask >> e)) == 0);
    }

    //bool IsSafeInt() const
    //{
    //    const int e = UnbiasedExponent();
    //    return (e >= 0 && e <= 52 && (Significand() & (kSignificandMask >> e)) == 0)
    //        || (e >= 52 && e <= 63 - 1/*hidden bit*/);
    //}
#endif
};

struct Fp // f * 2^e
{
    // The integer significand
    uint64_t f;
    // The exponent in base 2
    int e;

    Fp() : f(0), e(0) {}
    Fp(uint64_t f, int e) : f(f), e(e) {}

    // Construct a Fp from a IEEE-754 double precision value.
    // Result is NOT normalized!
    static Fp FromDouble(double d);

    // Returns X - Y.
    static Fp Sub(Fp x, Fp y);

    // Returns X * Y. The result is rounded.
    static Fp Mul(Fp x, Fp y);

    // Normalize X such that the significand is at least 2^63.
    static Fp Normalize(Fp x);

    // Normalize X such that the result has the exponent E.
    static Fp NormalizeTo(Fp x, int e);
};

inline Fp Fp::FromDouble(double d)
{
    const Double u { d };

    uint64_t e = u.Exponent();
    uint64_t f = u.Significand();
    if (e == 0) // denormal?
        e++;
    else
        f |= Double::kHiddenBit;

    return { f, static_cast<int>(e) - Double::kExponentBias - 52 };
}

inline Fp Fp::Sub(Fp x, Fp y)
{
    assert(x.e == y.e);
    assert(x.f >= y.f);

    return Fp { x.f - y.f,  x.e};
}

inline Fp Fp::Mul(Fp x, Fp y)
{
#if defined(_MSC_VER) && (defined(_M_X64) /* || defined(_M_ARM) || defined(_M_ARM64) */)

    uint64_t h = 0;
    uint64_t l = _umul128(x.f, y.f, &h);
    h += l >> 63;

    return Fp { h, x.e + y.e + 64 };

#elif defined(__GNUC__) && defined(__SIZEOF_INT128__)

    __extension__ using Uint128 = unsigned __int128;

    const Uint128 p = Uint128{x.f} * Uint128{y.f};

    uint64_t h = static_cast<uint64_t>(p >> 64);
    uint64_t l = static_cast<uint64_t>(p);
    h += l >> 63;

    return Fp { h, x.e + y.e + 64 };

#else

    const uint64_t M32 = 0xFFFFFFFF;

    const uint64_t xh = x.f >> 32;
    const uint64_t xl = x.f & M32;
    const uint64_t yh = y.f >> 32;
    const uint64_t yl = y.f & M32;

    const uint64_t xhyh = xh * yh;
    const uint64_t xhyl = xh * yl;
    const uint64_t xlyh = xl * yh;
    const uint64_t xlyl = xl * yl;

    uint64_t temp = (xlyl >> 32) + (xhyl & M32) + (xlyh & M32);

    // round:
    temp += uint64_t{1} << 31;

    const uint64_t f = xhyh + (xhyl >> 32) + (xlyh >> 32) + (temp >> 32);

    return Fp { f, x.e + y.e + 64 };

#endif
}

inline Fp Fp::Normalize(Fp x)
{
    assert(x.f != 0);

    const int leading_zeros = CountLeadingZeros64(x.f);
    return Fp { x.f << leading_zeros, x.e - leading_zeros };
}

inline Fp Fp::NormalizeTo(Fp x, int e)
{
    const int delta = x.e - e;

    assert(delta >= 0);
    assert(((x.f << delta) >> delta) == x.f);

    return Fp { x.f << delta, e };
}

struct CachedPower {
    const uint64_t f;
    const int16_t e;
};

enum { kAlpha                   =   -60 };
enum { kGamma                   =   -32 };
enum { kCachedPowersSize        =    87 };
enum { kCachedPowersStep        =     8 };
enum { kCachedPowersFirstExp    = -1220 };
enum { kCachedPowersLastExp     =  1066 };
enum { kCachedPowersFirstDecExp =  -348 };

static const CachedPower kCachedPowers[kCachedPowersSize] = {
    { 0xFA8FD5A0081C0288, -1220 }, // -348
    { 0xBAAEE17FA23EBF76, -1193 }, // -340
    { 0x8B16FB203055AC76, -1166 }, // -332
    { 0xCF42894A5DCE35EA, -1140 }, // -324
    { 0x9A6BB0AA55653B2D, -1113 }, // -316
    { 0xE61ACF033D1A45DF, -1087 }, // -308
    { 0xAB70FE17C79AC6CA, -1060 }, // -300
    { 0xFF77B1FCBEBCDC4F, -1034 }, // -292
    { 0xBE5691EF416BD60C, -1007 }, // -284
    { 0x8DD01FAD907FFC3C,  -980 }, // -276
    { 0xD3515C2831559A83,  -954 }, // -268
    { 0x9D71AC8FADA6C9B5,  -927 }, // -260
    { 0xEA9C227723EE8BCB,  -901 }, // -252
    { 0xAECC49914078536D,  -874 }, // -244
    { 0x823C12795DB6CE57,  -847 }, // -236
    { 0xC21094364DFB5637,  -821 }, // -228
    { 0x9096EA6F3848984F,  -794 }, // -220
    { 0xD77485CB25823AC7,  -768 }, // -212
    { 0xA086CFCD97BF97F4,  -741 }, // -204
    { 0xEF340A98172AACE5,  -715 }, // -196
    { 0xB23867FB2A35B28E,  -688 }, // -188
    { 0x84C8D4DFD2C63F3B,  -661 }, // -180
    { 0xC5DD44271AD3CDBA,  -635 }, // -172
    { 0x936B9FCEBB25C996,  -608 }, // -164
    { 0xDBAC6C247D62A584,  -582 }, // -156
    { 0xA3AB66580D5FDAF6,  -555 }, // -148
    { 0xF3E2F893DEC3F126,  -529 }, // -140
    { 0xB5B5ADA8AAFF80B8,  -502 }, // -132
    { 0x87625F056C7C4A8B,  -475 }, // -124
    { 0xC9BCFF6034C13053,  -449 }, // -116
    { 0x964E858C91BA2655,  -422 }, // -108
    { 0xDFF9772470297EBD,  -396 }, // -100
    { 0xA6DFBD9FB8E5B88F,  -369 }, //  -92
    { 0xF8A95FCF88747D94,  -343 }, //  -84
    { 0xB94470938FA89BCF,  -316 }, //  -76
    { 0x8A08F0F8BF0F156B,  -289 }, //  -68
    { 0xCDB02555653131B6,  -263 }, //  -60
    { 0x993FE2C6D07B7FAC,  -236 }, //  -52
    { 0xE45C10C42A2B3B06,  -210 }, //  -44
    { 0xAA242499697392D3,  -183 }, //  -36
    { 0xFD87B5F28300CA0E,  -157 }, //  -28
    { 0xBCE5086492111AEB,  -130 }, //  -20
    { 0x8CBCCC096F5088CC,  -103 }, //  -12
    { 0xD1B71758E219652C,   -77 }, //   -4
    { 0x9C40000000000000,   -50 }, //    4
    { 0xE8D4A51000000000,   -24 }, //   12
    { 0xAD78EBC5AC620000,     3 }, //   20
    { 0x813F3978F8940984,    30 }, //   28
    { 0xC097CE7BC90715B3,    56 }, //   36
    { 0x8F7E32CE7BEA5C70,    83 }, //   44
    { 0xD5D238A4ABE98068,   109 }, //   52
    { 0x9F4F2726179A2245,   136 }, //   60
    { 0xED63A231D4C4FB27,   162 }, //   68
    { 0xB0DE65388CC8ADA8,   189 }, //   76
    { 0x83C7088E1AAB65DB,   216 }, //   84
    { 0xC45D1DF942711D9A,   242 }, //   92
    { 0x924D692CA61BE758,   269 }, //  100
    { 0xDA01EE641A708DEA,   295 }, //  108
    { 0xA26DA3999AEF774A,   322 }, //  116
    { 0xF209787BB47D6B85,   348 }, //  124
    { 0xB454E4A179DD1877,   375 }, //  132
    { 0x865B86925B9BC5C2,   402 }, //  140
    { 0xC83553C5C8965D3D,   428 }, //  148
    { 0x952AB45CFA97A0B3,   455 }, //  156
    { 0xDE469FBD99A05FE3,   481 }, //  164
    { 0xA59BC234DB398C25,   508 }, //  172
    { 0xF6C69A72A3989F5C,   534 }, //  180
    { 0xB7DCBF5354E9BECE,   561 }, //  188
    { 0x88FCF317F22241E2,   588 }, //  196
    { 0xCC20CE9BD35C78A5,   614 }, //  204
    { 0x98165AF37B2153DF,   641 }, //  212
    { 0xE2A0B5DC971F303A,   667 }, //  220
    { 0xA8D9D1535CE3B396,   694 }, //  228
    { 0xFB9B7CD9A4A7443C,   720 }, //  236
    { 0xBB764C4CA7A44410,   747 }, //  244
    { 0x8BAB8EEFB6409C1A,   774 }, //  252
    { 0xD01FEF10A657842C,   800 }, //  260
    { 0x9B10A4E5E9913129,   827 }, //  268
    { 0xE7109BFBA19C0C9D,   853 }, //  276
    { 0xAC2820D9623BF429,   880 }, //  284
    { 0x80444B5E7AA7CF85,   907 }, //  292
    { 0xBF21E44003ACDD2D,   933 }, //  300
    { 0x8E679C2F5E44FF8F,   960 }, //  308
    { 0xD433179D9C8CB841,   986 }, //  316
    { 0x9E19DB92B4E31BA9,  1013 }, //  324
    { 0xEB96BF6EBADF77D9,  1039 }, //  332
    { 0xAF87023B9BF0EE6B,  1066 }, //  340
};

} // namespace

static Fp GetCachedPower(int e_min, int e_max, int& k)
{
    const int p = kCachedPowersSize - 1;
    const int q = kCachedPowersLastExp - kCachedPowersFirstExp;

    const int idx = (e_max - kCachedPowersFirstExp) * p / q;
    assert(idx >= 0);
    assert(idx < kCachedPowersSize);

    const CachedPower cached = kCachedPowers[idx];
    assert(e_min <= cached.e);
    assert(e_max >= cached.e);
    static_cast<void>(e_min); // fix unused warning...

    k = kCachedPowersFirstDecExp + kCachedPowersStep * idx;
    return Fp { cached.f, cached.e };
}

static Fp GetCachedPower(int e, int& k)
{
    const int e_min = kAlpha - e - 64;
    const int e_max = kGamma - e - 64;

    return GetCachedPower(e_min, e_max, k);
}

static void NormalizedBoundaries(double x, Fp& m_minus, Fp& m_plus)
{
    const Fp v = Fp::FromDouble(x);

    const Fp plus = Fp::Normalize(Fp { (v.f << 1) + 1, v.e - 1 });

    const bool closer = (v.f == Double::kHiddenBit); // ie, exp != 0 && sig == 0
    const Fp minus = closer
        ? Fp { (v.f << 2) - 1, v.e - 2 }
        : Fp { (v.f << 1) - 1, v.e - 1 };

    m_minus = Fp::NormalizeTo(minus, plus.e);
    m_plus  = plus;
}

static void Grisu2Round(char* buf, int len, uint64_t dist, uint64_t delta, uint64_t rest, uint64_t ten_kappa)
{
    assert(rest <= delta);

    while (rest < dist
        && delta - rest >= ten_kappa
        && (rest + ten_kappa < dist || dist - rest > rest + ten_kappa - dist))
    {
        buf[len - 1]--;
        rest += ten_kappa;
    }
}

static void Grisu2DigitGen(Fp Wm, Fp W, Fp Wp, char* buf, int& len, int& K)
{
    static const uint32_t kPow10_32[] = {
        1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000,
    };

// init:

    const Fp low  = Fp { Wm.f + 1, Wm.e };
    const Fp high = Fp { Wp.f - 1, Wp.e };

// setup:

    const Fp one = Fp { uint64_t{1} << -W.e, W.e };

    uint64_t delta = Fp::Sub(high, low).f;
    uint64_t dist  = Fp::Sub(high, W  ).f;

    uint32_t p1 = static_cast<uint32_t>(high.f >> -one.e);
    uint64_t p2 = high.f & (one.f - 1);

// integrals:

    int kappa = CountDecimalDigits32(p1);
    while (kappa > 0)
    {
        const uint32_t div = kPow10_32[kappa - 1];

        const uint32_t d = p1 / div;
        p1 %= div;
        buf[len++] = static_cast<char>('0' + d);
        kappa--;

        const uint64_t rest = (uint64_t{p1} << -one.e) + p2;
        if (rest <= delta)
        {
            const uint64_t ten_kappa = uint64_t{div} << -one.e;

            K += kappa;
            return Grisu2Round(buf, len, dist, delta, rest, ten_kappa);
        }
    }

// fractionals:

    uint64_t unit = 1;
    for (;;)
    {
        unit  *= 10; // unit = kPow10_64[-kappa + 1];
        p2    *= 10;
        delta *= 10;

        const uint64_t d = p2 >> -one.e;
        p2 &= one.f - 1;
        buf[len++] = static_cast<char>('0' + d);
        kappa--;

        if (p2 < delta)
        {
            K += kappa;
            return Grisu2Round(buf, len, dist * unit, delta, p2, one.f);
        }
    }
}

// X = BUF * 10^K
static void Grisu2(double x, char* buf, int& len, int& K)
{
    const Fp w = Fp::Normalize(Fp::FromDouble(x));

    Fp wm;
    Fp wp;
    NormalizedBoundaries(x, wm, wp);

    int mk = 0; // -k
    const Fp ten_mk = GetCachedPower(wp.e, mk);

    const Fp W  = Fp::Mul(w,  ten_mk);
    const Fp Wm = Fp::Mul(wm, ten_mk);
    const Fp Wp = Fp::Mul(wp, ten_mk);

    K = -mk;

    Grisu2DigitGen(Wm, W, Wp, buf, len, K);
}

static int WriteExponent(int e, char* buf)
{
    int len = 0;

    if (e < 0)
        buf[len++] = '-', e = -e;
    else
        buf[len++] = '+';

    assert(e < 10000);

    const int k = e;
    if (k >= 1000) { buf[len++] = static_cast<char>('0' + e / 1000); e %= 1000; }
    if (k >=  100) { buf[len++] = static_cast<char>('0' + e /  100); e %=  100; }
    if (k >=   10) { buf[len++] = static_cast<char>('0' + e /   10); e %=   10; }

    buf[len++] = static_cast<char>('0' + e);

    return len;
}

// https://www.ecma-international.org/ecma-262/5.1/#sec-9.8.1
//
// 9.8.1 ToString Applied to the Number Type
//
// The abstract operation ToString converts a Number m to String format as
// follows:
//
//  1. If m is NaN, return the String "NaN".
//  2. If m is +0 or -0, return the String "0".
//  3. If m is less than zero, return the String concatenation of the String "-"
//     and ToString(-m).
//  4. If m is infinity, return the String "Infinity".
//  5. Otherwise, let n, k, and s be integers such that k >= 1,
//     10^(k-1) <= s < 10^k, the Number value for s * 10^(n-k) is m, and k is as
//     small as possible. Note that k is the number of digits in the decimal
//     representation of s, that s is not divisible by 10, and that the least
//     significant digit of s is not necessarily uniquely determined by these
//     criteria.
//  6. If k <= n <= 21, return the String consisting of the k digits of the
//     decimal representation of s (in order, with no leading zeroes), followed
//     by n-k occurrences of the character '0'.
//  7. If 0 < n <= 21, return the String consisting of the most significant n
//     digits of the decimal representation of s, followed by a decimal point
//     '.', followed by the remaining k-n digits of the decimal representation
//     of s.
//  8. If -6 < n <= 0, return the String consisting of the character '0',
//     followed by a decimal point '.', followed by -n occurrences of the
//     character '0', followed by the k digits of the decimal representation of
//     s.
//  9. Otherwise, if k = 1, return the String consisting of the single digit of
//     s, followed by lowercase character 'e', followed by a plus sign '+' or
//     minus sign '-' according to whether n-1 is positive or negative, followed
//     by the decimal representation of the integer abs(n-1) (with no leading
//     zeros).
// 10. Return the String consisting of the most significant digit of the decimal
//     representation of s, followed by a decimal point '.', followed by the
//     remaining k-1 digits of the decimal representation of s, followed by the
//     lowercase character 'e', followed by a plus sign '+' or minus sign '-'
//     according to whether n-1 is positive or negative, followed by the decimal
//     representation of the integer abs(n-1) (with no leading zeros).
//
static char* NumberToString(double x, char* buf, bool trailing_dot_zero = false)
{
    // x = s * 10^(n-k)
    // k is the number of decimal digits in s.
    // n is the position of the decimal point relative to the start of s.

    int n_minus_k = 0;
    int k = 0;
    char s[32];

    if (x > 0)
        Grisu2(x, s, k, n_minus_k);
    else
        s[k++] = '0';

    assert(k > 0);
    const int n = k + n_minus_k;

    if (k <= n && n <= 21)
    {
        std::memcpy(buf, s, static_cast<size_t>(k));
        buf += k;
        if (k != n) {
            std::memset(buf, '0', static_cast<size_t>(n - k));
            buf += n - k;
        }
        if (trailing_dot_zero) {
            *buf++ = '.';
            *buf++ = '0';
        }
    }
    else if (0 < n && n <= 21)
    {
        std::memcpy(buf, s, static_cast<size_t>(n));
        buf[n] = '.';
        std::memcpy(buf + (n + 1), s + n, static_cast<size_t>(k - n));
        buf += k + 1;
    }
    else if (-6 < n && n <= 0)
    {
        *buf++ = '0';
        *buf++ = '.';
        if (n != 0) {
            std::memset(buf, '0', static_cast<size_t>(-n));
            buf += -n;
        }
        std::memcpy(buf , s, static_cast<size_t>(k));
        buf += k;
    }
    else if (k == 1)
    {
        *buf++ = s[0];
        if (trailing_dot_zero) {
#if 0 // hm... with a trailing exponent this already looks like a float...
            *buf++ = '.';
            *buf++ = '0';
#endif
        }
        *buf++ = 'e';
        buf += WriteExponent(n - 1, buf);
    }
    else
    {
        *buf++ = s[0];
        *buf++ = '.';
        std::memcpy(buf, s + 1, static_cast<size_t>(k - 1));
        buf += k - 1;
        *buf++ = 'e';
        buf += WriteExponent(n - 1, buf);
    }

    return buf;
}

static void GenHexDigits(double x, int prec, bool normalize, bool capitals, char* buf, int& len, int& e)
{
    const char* const xdigits = capitals
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    const Double d { x };

    assert(!d.Sign());
    assert(!d.IsZero());
    assert(!d.IsSpecial());

    uint64_t exp = d.Exponent();
    uint64_t sig = d.Significand();

    e = static_cast<int>(exp) - Double::kExponentBias;
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
    if (prec >= 0 && prec < 52/4)
    {
        const uint64_t digit = sig         >> (52 - 4*prec - 4);
        const uint64_t r     = uint64_t{1} << (52 - 4*prec    );

        assert(!normalize || (sig & Double::kHiddenBit) == 0);

//      if ((digit & 0xF) >= 8)
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

    buf[len++] = xdigits[normalize ? 1 : (sig >> 52)];

    // Ignore everything but the significand.
    // Shift everything to the left; makes the loop below slightly simpler.
    sig <<= 64 - 52;

    while (sig != 0)
    {
        buf[len++] = xdigits[sig >> (64 - 4)];
        sig <<= 4;
    }
}

static char* NumberToHexString(double x, char* buf, bool trailing_dot_zero = false)
{
    int  e = 0;
    int  k = 0;
    char s[32];

    if (x > 0)
        GenHexDigits(x, /*prec*/-1, /*normalize*/true, /*capitals*/true, s, k, e);
    else
        s[k++] = '0';

    assert(k > 0);

    *buf++ = s[0];
    if (k > 1)
    {
        *buf++ = '.';
        std::memcpy(buf, s + 1, static_cast<size_t>(k - 1));
        buf += k - 1;
    }
    else
    {
        if (trailing_dot_zero) {
#if 0 // hm... with a trailing exponent this already looks like a float...
            *buf++ = '.';
            *buf++ = '0';
#endif
        }
    }

    *buf++ = 'p';
    buf += WriteExponent(e, buf);

    return buf;
}

static int DoubleToAscii(std::ostream& os, FormatSpec const& spec, char sign, char type, int prec, double abs_x)
{
    static const size_t kBufSize = 1000; // >= 9 !!!
    char buf[kBufSize];

    int n;
    if (prec >= 0)
    {
        const char format[] = {'%', '.', '*', type, '\0'};
        n = std::snprintf(buf, kBufSize, format, prec, abs_x);
    }
    else
    {
        const char format[] = {'%', type, '\0'};
        n = std::snprintf(buf, kBufSize, format, abs_x);
    }
    assert(n >= 0); // invalid format spec

    size_t len = static_cast<size_t>(n);
    if (len >= kBufSize)
    {
        std::memcpy(buf + (kBufSize - 9), "[[NOBUF]]", 9);
        len = kBufSize;
    }

    return WriteNumber(os, spec, sign, nullptr, 0, buf, len);
}

static int WriteDouble(std::ostream& os, FormatSpec const& spec, double x)
{
    char conv = spec.conv;
    bool tostr = false;
    bool tohex = false;

    switch (conv)
    {
    default:
        // I'm sorry Dave, I'm afraid I can't do that.
    case '\0':
        conv = 's';
    case 's':
    case 'S':
        tostr = true;
        break;
    case 'g':
    case 'G':
    case 'e':
    case 'E':
    case 'a':
    case 'A':
        break;
    case 'f':
    case 'F':
        conv = 'f';
        break;
    case 'x':
    case 'X':
        tohex = true;
        break;
    }

    const bool upper = ('A' <= conv && conv <= 'Z');

    const Double d { x };

    const bool neg = (d.Sign() != 0);
    const char sign = ComputeSignChar(neg, spec.sign, spec.fill);

    if (d.IsSpecial())
    {
        const bool inf = (d.Significand() == 0);
        const char* s =
            inf ? (neg ? (upper ? "-INF" : "-inf")
                       : (upper ?  "INF" :  "inf"))
                : (upper ? "NAN" : "nan");

        return WriteRawString(os, spec, s, ::strlen(s));
    }

    const double abs_x = d.Abs();

    if (tostr || tohex)
    {
        const bool trailing_dot_zero = (spec.hash != '\0');

        char repr[32];
        const auto f = repr;
        const auto l = tostr ? NumberToString(abs_x, f, trailing_dot_zero)
                             : NumberToHexString(abs_x, f, trailing_dot_zero);

        const size_t nrepr = static_cast<size_t>(l - f);
        assert(nrepr <= 32);

        return WriteNumber(os, spec, sign, "0x", (tohex && spec.hash) ? 2u : 0u, repr, nrepr);
    }

    return DoubleToAscii(os, spec, sign, conv, spec.prec, abs_x);
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
    if (spec.align && !IsAlign(spec.align))
        spec.align = '>';

    if (spec.sign && !IsSign(spec.sign))
        spec.sign = '-';

    if (spec.width < 0)
    {
        if (spec.width < -INT_MAX)
            spec.width = -INT_MAX;
        spec.align = '<';
        spec.width = -spec.width;
    }
}

static int ParseFormatSpec(FormatSpec& spec, const char*& f, const char* end, int& nextarg, Types types, Arg const* args)
{
    assert(f != end);

    if (*f == '*')
    {
        ++f;
        if (f == end)
            return -1; // missing '}'

        int spec_index = -1;
        if (IsDigit(*f))
        {
            spec_index = ParseInt(f, end);
            if (spec_index < 0)
                return -1; // overflow
            if (f == end)
                return -1; // missing '}'
        }
        else
        {
            spec_index = nextarg++;
        }

        if (types[spec_index] != Types::T_FORMATSPEC)
            return -1; // invalid argument

        spec = *static_cast<FormatSpec const*>(args[spec_index].pvoid);
        FixFormatSpec(spec);
    }

    if (*f == ':')
    {
        ++f;
        if (f == end)
            return -1; // missing '}'

        if (f + 1 != end && IsAlign(*(f + 1)))
        {
            spec.fill = *f++;
            spec.align = *f++;
            if (f == end)
                return -1; // missing '}'
        }
        else if (IsAlign(*f))
        {
            spec.align = *f++;
            if (f == end)
                return -1; // missing '}'
        }

        if (IsSign(*f))
        {
            spec.sign = *f++;
            if (f == end)
                return -1; // missing '}'
        }

        if (*f == '#')
        {
            spec.hash = *f++;
            if (f == end)
                return -1; // missing '}'
        }

        if (*f == '0')
        {
            spec.zero = *f++;
            if (f == end)
                return -1; // missing '}'
        }

        if (IsDigit(*f))
        {
            const int i = ParseInt(f, end);
            if (i < 0)
                return -1; // overflow
            if (f == end)
                return -1; // missing '}'
            spec.width = i;
        }

        if (*f == '.')
        {
            ++f;
            if (f == end || !IsDigit(*f))
                return -1; // missing '}' or digit expected
            const int i = ParseInt(f, end);
            if (i < 0)
                return -1; // overflow
            if (f == end)
                return -1; // missing '}'
            spec.prec = i;
        }

        if (*f != ',' && *f != '}')
        {
            spec.conv = *f++;
            if (f == end)
                return -1; // missing '}'
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
        return -1; // missing '}'

    return 0;
}

static int CallFormatFunc(std::ostream& os, FormatSpec const& spec, int index, Types types, Arg const* args)
{
    const unsigned type = types[index];

    if (type == 0)
        return -1; // index out of range

    const auto& arg = args[index];

    switch (type)
    {
    case Types::T_OTHER:
        return arg.other.func(os, spec, arg.other.value);
    case Types::T_STRING:
        return WriteString(os, spec, arg.string.str, arg.string.len);
    case Types::T_PVOID:
        return WritePointer(os, spec, arg.pvoid);
    case Types::T_PCHAR:
        return WriteString(os, spec, arg.pchar);
    case Types::T_CHAR:
        return WriteChar(os, spec, arg.char_);
    case Types::T_BOOL:
        return WriteBool(os, spec, arg.bool_);
    case Types::T_SCHAR:
        return WriteInt(os, spec, arg.schar, static_cast<unsigned char>(arg.schar));
    case Types::T_SSHORT:
        return WriteInt(os, spec, arg.sshort, static_cast<unsigned short>(arg.sshort));
    case Types::T_SINT:
        return WriteInt(os, spec, arg.sint, static_cast<unsigned int>(arg.sint));
    case Types::T_SLONGLONG:
        return WriteInt(os, spec, arg.slonglong, static_cast<unsigned long long>(arg.slonglong));
    case Types::T_ULONGLONG:
        return WriteInt(os, spec, 0, arg.ulonglong);
    case Types::T_DOUBLE:
        return WriteDouble(os, spec, arg.double_);
    case Types::T_FORMATSPEC:
        return -1;
    }

    assert(!"unreachable");
    return -1;
}

static int DoFormatImpl(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    assert(args != nullptr);

    if (format.empty())
        return 0;

    int nextarg = 0;

    const char*       f   = format.data();
    const char* const end = f + format.size();
    const char*       s   = f;
    for (;;)
    {
        while (f != end && *f != '{' && *f != '}')
            ++f;

        if (f != s && !Write(os, s, static_cast<size_t>(f - s)))
            return -1;

        if (f == end) // done.
            break;

        const char c = *f++; // skip '{' or '}'

        if (*f == c) // '{{' or '}}'
        {
            s = f++;
            continue;
        }

        if (c == '}')
            return -1; // stray '}'
        if (f == end)
            return -1; // missing '}'

        int index = -1;
        if (IsDigit(*f))
        {
            index = ParseInt(f, end);
            if (index < 0)
                return -1; // overflow
            if (f == end)
                return -1; // missing '}'
        }

        FormatSpec spec;
        if (*f != '}')
        {
            const int err = ParseFormatSpec(spec, f, end, nextarg, types, args);
            if (err != 0)
                return err;
        }

        if (index < 0)
            index = nextarg++;

        const int err = CallFormatFunc(os, spec, index, types, args);
        if (err != 0)
            return err;

        if (f == end) // done.
            break;

        s = ++f; // skip '}'
    }

    return 0;
}

int fmtxx::impl::DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args)
{
    const std::ostream::sentry se(os);
    if (se)
        return DoFormatImpl(os, format, types, args);

    return -1;
}

#if 0
#ifdef _MSC_VER // cpplib

#include <fstream>
int fmtxx::impl::DoFormat(std::FILE* buf, std::string_view format, Types types, Arg const* args)
{
    std::ofstream out { buf };
    return DoFormat(out, format, types, args);
}

#else // libstdc++

#include <ext/stdio_filebuf.h>
int fmtxx::impl::DoFormat(std::FILE* buf, std::string_view format, Types types, Arg const* args)
{
    __gnu_cxx::stdio_filebuf<char> fbuf { buf, std::ios::out };

    std::ostream out { &fbuf };
    return DoFormat(out, format, types, args);
}

#endif
#endif

//------------------------------------------------------------------------------
// Copyright 2016 Alexander Bolz
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
// http://florian.loitsch.com/publications/
//
// Original license follows:
//
//   Copyright (c) 2009 Florian Loitsch
//
//   Permission is hereby granted, free of charge, to any person
//   obtaining a copy of this software and associated documentation
//   files (the "Software"), to deal in the Software without
//   restriction, including without limitation the rights to use,
//   copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the
//   Software is furnished to do so, subject to the following
//   conditions:
//
//   The above copyright notice and this permission notice shall be
//   included in all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
//   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
//   OTHER DEALINGS IN THE SOFTWARE.

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
