// Distributed under the MIT license. See the end of the file for details.

#pragma once

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <cstdio>
#include <iosfwd>
#include <string>
#if _MSC_VER
#  include <string_view>
#else
#  if __has_include(<string_view>)
#    include <string_view>
#  else
#    include <experimental/string_view>
     namespace std { using std::experimental::string_view; }
#  endif
#endif

#ifdef __GNUC__
#  define FMTXX_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#else
#  define FMTXX_VISIBILITY_DEFAULT
#endif

#ifdef FMTXX_SHARED
#  ifdef _MSC_VER
#    ifdef FMTXX_EXPORT
#      define FMTXX_API __declspec(dllexport)
#    else
#      define FMTXX_API __declspec(dllimport)
#    endif
#  else
#    ifdef FMTXX_EXPORT
#      define FMTXX_API FMTXX_VISIBILITY_DEFAULT
#    else
#      define FMTXX_API
#    endif
#  endif
#else
#  define FMTXX_API
#endif

namespace fmtxx {

enum struct errc {
    success                 =  0,
    invalid_format_string   = -1,
    invalid_argument        = -2,
    io_error                = -3,
    index_out_of_range      = -4,
};

struct FMTXX_VISIBILITY_DEFAULT FormatSpec
{
    std::string_view style;
    int  width = 0;
    int  prec  = -1;
    char fill  = ' ';
    char align = '>';
    char sign  = '-';
    char zero  = '\0';
    char hash  = '\0';
    char conv  = '\0';
};

//
// Specialize this if you want your data-type to be treated as a string.
//
// T must have member functions data() and size() and their return values must
// be convertible to char const* and size_t resp.
//

template <typename T>
struct IsString {
    static constexpr bool value = false;
};

//
// Formatting function for user-defined types.
// Implement this in the data-type's namespace!
//

template <typename OS, typename T>
errc fmtxx__FormatValue(OS& os, FormatSpec const& spec, T const& value)
    = delete;

//
// Appends the formatted arguments to the given output stream.
//

//template <typename OS, typename ...Args>
//errc FormatToStream(OS&& os, std::string_view format, Args const&... args);

//
// Appends the formatted arguments to the given string.
//

template <typename ...Args>
errc Format(std::string& os, std::string_view format, Args const&... args);

//
// Returns a std::string containing the formatted arguments.
//

template <typename ...Args>
std::string StringFormat(std::string_view format, Args const&... args);

//
// Appends the formatted arguments to the given stream.
//

template <typename ...Args>
errc Format(std::FILE* os, std::string_view format, Args const&... args);

//
// Appends the formatted arguments to the given stream.
//

template <typename ...Args>
errc Format(std::ostream& os, std::string_view format, Args const&... args);

//
// Appends the formatted arguments to the given array.
//

// Wraps the input range for the Format() function below.
// If a custom formatting function writes into the buffer, it needs to update the next pointer.
//
// assert: next <= last
struct FMTXX_VISIBILITY_DEFAULT CharArray {
    char* next = nullptr;
    char* last = nullptr;
};

template <typename ...Args>
errc Format(CharArray& os, std::string_view format, Args const&... args);

//
// Appends the formatted arguments to the given array.
//  (API as std::to_chars)
//

struct FMTXX_VISIBILITY_DEFAULT FormatToCharArrayResult {
    char* next = nullptr;
    errc  ec   = errc::success;
};

template <typename ...Args>
FormatToCharArrayResult Format(char* first, char* last, std::string_view format, Args const&... args);

} // namespace fmtxx

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>

namespace fmtxx {
namespace impl {

template <typename T>
struct ADLEnabled // re-associate T to namespace fmtxx::impl
{
    T& value;
    explicit ADLEnabled(T& v) : value(v) {}
};

template <typename T>
ADLEnabled<T> EnableADL(T& v) { return ADLEnabled<T>(v); }

class Types
{
public:
    using value_type = uint64_t;

    enum EType : unsigned {
        T_NONE,       //  0
        T_OTHER,      //  1
        T_BOOL,       //  2
        T_STRING,     //  3
        T_PVOID,      //  4
        T_PCHAR,      //  5
        T_CHAR,       //  6
        T_SCHAR,      //  7 XXX: promote to int?
        T_SSHORT,     //  8 XXX: promote to int?
        T_SINT,       //  9
                      // 10
        T_SLONGLONG,  // 11
                      // 12
        T_ULONGLONG,  // 13
        T_DOUBLE,     // 14 XXX: float is promoted to double. long double ???
        T_FORMATSPEC, // 15
    };

    const value_type types;

    template <typename ...Args>
    Types(Args const&... args) : types(Make(args...))
    {
    }

    EType operator [](int index) const
    {
        assert(index >= 0);
        if (index >= 16)
            return T_NONE;
        return static_cast<EType>((types >> (4 * index)) & 0xF);
    }

private:
    // XXX:
    // Keep in sync with Arg::Arg() below!!!
    template <typename T>
    static unsigned GetId(T                  const&) { return IsString<T>::value ? T_STRING : T_OTHER; }
    static unsigned GetId(bool               const&) { return T_BOOL; }
    static unsigned GetId(std::string_view   const&) { return T_STRING; }
    static unsigned GetId(std::string        const&) { return T_STRING; }
    static unsigned GetId(std::nullptr_t     const&) { return T_PVOID; }
    static unsigned GetId(void const*        const&) { return T_PVOID; }
    static unsigned GetId(void*              const&) { return T_PVOID; }
    static unsigned GetId(char const*        const&) { return T_PCHAR; }
    static unsigned GetId(char*              const&) { return T_PCHAR; }
    static unsigned GetId(char               const&) { return T_CHAR; }
    static unsigned GetId(signed char        const&) { return T_SCHAR; }
    static unsigned GetId(signed short       const&) { return T_SSHORT; }
    static unsigned GetId(signed int         const&) { return T_SINT; }
#if LONG_MAX == INT_MAX
    static unsigned GetId(signed long        const&) { return T_SINT; }
#else
    static unsigned GetId(signed long        const&) { return T_SLONGLONG; }
#endif
    static unsigned GetId(signed long long   const&) { return T_SLONGLONG; }
    static unsigned GetId(unsigned char      const&) { return T_ULONGLONG; }
    static unsigned GetId(unsigned short     const&) { return T_ULONGLONG; }
    static unsigned GetId(unsigned int       const&) { return T_ULONGLONG; }
    static unsigned GetId(unsigned long      const&) { return T_ULONGLONG; }
    static unsigned GetId(unsigned long long const&) { return T_ULONGLONG; }
    static unsigned GetId(double             const&) { return T_DOUBLE; }
    static unsigned GetId(float              const&) { return T_DOUBLE; }
    static unsigned GetId(FormatSpec         const&) { return T_FORMATSPEC; }

    static value_type Make() { return 0; }

    template <typename A1, typename ...An>
    static value_type Make(A1 const& a1, An const&... an)
    {
        static_assert(1 + sizeof...(An) <= 16, "too many arguments");
        return (Make(an...) << 4) | GetId(a1);
    }
};

class Arg
{
public:
    template <bool>
    struct BoolConst {};

    using Func = errc (*)(void* os, FormatSpec const& spec, void const* value);

    template <typename OS, typename T>
    static errc FormatValue_fn(void* os, FormatSpec const& spec, void const* value)
    {
        return fmtxx__FormatValue(*static_cast<OS*>(os), spec, *static_cast<T const*>(value));
    }

    struct Other { void const* value; Func func; };

    union {
        Other other;
        std::string_view string;
        void const* pvoid;
        char const* pchar;
        char char_;
        bool bool_;
        signed char schar;
        signed short sshort;
        signed int sint;
        signed long long slonglong;
        unsigned long long ulonglong;
        double double_;
    };

    template <typename OS, typename T>
    Arg(OS&, T const& v, BoolConst<true>) : string{ v.data(), v.size() }
    {
    }

    template <typename OS, typename T>
    Arg(OS&, T const& v, BoolConst<false>) : other{ &v, &FormatValue_fn<OS, T> }
    {
    }

    // XXX:
    // Keep in sync with Types::GetId() above!!!
    template <typename OS, typename T>
    Arg(OS& os, T const& v) : Arg(os, v, BoolConst<IsString<T>::value>{}) {}

    template <typename OS> Arg(OS&, bool               const& v) : bool_(v) {}
    template <typename OS> Arg(OS&, std::string_view   const& v) : string(v) {}
    template <typename OS> Arg(OS&, std::string        const& v) : string(v) {}
    template <typename OS> Arg(OS&, std::nullptr_t     const& v) : pvoid(v) {}
    template <typename OS> Arg(OS&, void const*        const& v) : pvoid(v) {}
    template <typename OS> Arg(OS&, void*              const& v) : pvoid(v) {}
    template <typename OS> Arg(OS&, char const*        const& v) : pchar(v) {}
    template <typename OS> Arg(OS&, char*              const& v) : pchar(v) {}
    template <typename OS> Arg(OS&, char               const& v) : char_(v) {}
    template <typename OS> Arg(OS&, signed char        const& v) : schar(v) {}
    template <typename OS> Arg(OS&, signed short       const& v) : sshort(v) {}
    template <typename OS> Arg(OS&, signed int         const& v) : sint(v) {}
#if LONG_MAX == INT_MAX
    template <typename OS> Arg(OS&, signed long        const& v) : sint(v) {}
#else
    template <typename OS> Arg(OS&, signed long        const& v) : slonglong(v) {}
#endif
    template <typename OS> Arg(OS&, signed long long   const& v) : slonglong(v) {}
    template <typename OS> Arg(OS&, unsigned char      const& v) : ulonglong(v) {}
    template <typename OS> Arg(OS&, unsigned short     const& v) : ulonglong(v) {}
    template <typename OS> Arg(OS&, unsigned int       const& v) : ulonglong(v) {}
    template <typename OS> Arg(OS&, unsigned long      const& v) : ulonglong(v) {}
    template <typename OS> Arg(OS&, unsigned long long const& v) : ulonglong(v) {}
    template <typename OS> Arg(OS&, double             const& v) : double_(v) {}
    template <typename OS> Arg(OS&, float              const& v) : double_(static_cast<double>(v)) {}
    template <typename OS> Arg(OS&, FormatSpec         const& v) : pvoid(&v) {}
};

// XXX: fmtxx__Put
// XXX: fmtxx__Write
// XXX: fmtxx__Pad
FMTXX_API bool Put   (std::string&  os, char c);
FMTXX_API bool Write (std::string&  os, char const* str, size_t len);
FMTXX_API bool Pad   (std::string&  os, char c, size_t count);
FMTXX_API bool Put   (std::FILE&    os, char c);
FMTXX_API bool Write (std::FILE&    os, char const* str, size_t len);
FMTXX_API bool Pad   (std::FILE&    os, char c, size_t count);
FMTXX_API bool Put   (std::ostream& os, char c);
FMTXX_API bool Write (std::ostream& os, char const* str, size_t len);
FMTXX_API bool Pad   (std::ostream& os, char c, size_t count);
FMTXX_API bool Put   (CharArray&    os, char c);
FMTXX_API bool Write (CharArray&    os, char const* str, size_t len);
FMTXX_API bool Pad   (CharArray&    os, char c, size_t count);

inline bool IsDigit(char ch) { return '0' <= ch && ch <= '9'; }
inline bool IsAlign(char ch) { return ch == '<' || ch == '>' || ch == '^' || ch == '='; }
inline bool IsSign (char ch) { return ch == ' ' || ch == '-' || ch == '+'; }

inline void ComputePadding(size_t min_len, char align, int width, size_t& lpad, size_t& spad, size_t& rpad)
{
    assert(width >= 0 && "internal error");

    const size_t w = static_cast<size_t>(width);
    if (w <= min_len)
        return;

    const size_t d = w - min_len;
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

// XXX: public...
template <typename OS>
inline errc WriteRawString(OS& os, FormatSpec const& spec, char const* str, size_t len)
{
    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(len, spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0 && !Pad(os, spec.fill, lpad))
        return errc::io_error;
    if (len > 0  && !Write(os, str, len))
        return errc::io_error;
    if (rpad > 0 && !Pad(os, spec.fill, rpad))
        return errc::io_error;

    return errc::success;
}

// XXX: public...
template <typename OS>
inline errc WriteRawString(OS& os, FormatSpec const& spec, std::string_view str)
{
    return WriteRawString(os, spec, str.data(), str.size());
}

// XXX: public...
template <typename OS>
inline errc WriteString(OS& os, FormatSpec const& spec, char const* str, size_t len)
{
    size_t n = len;
    if (spec.prec >= 0)
    {
        if (n > static_cast<size_t>(spec.prec))
            n = static_cast<size_t>(spec.prec);
    }

    return WriteRawString(os, spec, str, n);
}

// XXX: public...
template <typename OS>
inline errc WriteString(OS& os, FormatSpec const& spec, char const* str)
{
    if (str == nullptr)
        return WriteRawString(os, spec, "(null)");

    // Use strnlen if a precision was specified.
    // The string may not be null-terminated!
    size_t len;
    if (spec.prec >= 0)
        len = ::strnlen(str, static_cast<size_t>(spec.prec));
    else
        len = ::strlen(str);

    return WriteRawString(os, spec, str, len);
}

// XXX: public...
template <typename OS>
inline errc WriteNumber(OS& os, FormatSpec const& spec, char sign, char const* prefix, size_t nprefix, char const* digits, size_t ndigits)
{
    const size_t min_len = (sign ? 1u : 0u) + nprefix + ndigits;

    size_t lpad = 0;
    size_t spad = 0;
    size_t rpad = 0;

    ComputePadding(min_len, spec.zero ? '=' : spec.align, spec.width, lpad, spad, rpad);

    if (lpad > 0     && !Pad(os, spec.fill, lpad))
        return errc::io_error;
    if (sign != '\0' && !Put(os, sign))
        return errc::io_error;
    if (nprefix > 0  && !Write(os, prefix, nprefix))
        return errc::io_error;
    if (spad > 0     && !Pad(os, spec.zero ? '0' : spec.fill, spad))
        return errc::io_error;
    if (ndigits > 0  && !Write(os, digits, ndigits))
        return errc::io_error;
    if (rpad > 0     && !Pad(os, spec.fill, rpad))
        return errc::io_error;

    return errc::success;
}

struct IntToAsciiResult {
    char*  first;
    char*  last;
    size_t nprefix;
    int    base;
    char   conv;
    char   sign;
};

// XXX: public...
FMTXX_API IntToAsciiResult IntToAscii(char* first, char* last, FormatSpec const& spec, int64_t sext, uint64_t zext);

// XXX: public...
template <typename OS>
inline errc WriteInt(OS& os, FormatSpec const& spec, int64_t sext, uint64_t zext)
{
    char buf[64];

    const auto res = IntToAscii(buf, buf + 64, spec, sext, zext);
    const auto len = static_cast<size_t>(res.last - res.first);

    const char prefix[] = { '0', res.conv };

    return WriteNumber(os, spec, res.sign, prefix, res.nprefix, res.first, len);
}

// XXX: public...
template <typename OS>
inline errc WriteBool(OS& os, FormatSpec const& spec, bool val)
{
    return WriteRawString(os, spec, val ? "true" : "false");
}

// XXX: public...
template <typename OS>
inline errc WriteChar(OS& os, FormatSpec const& spec, char ch)
{
    return WriteString(os, spec, &ch, 1u);
}

// XXX: public...
template <typename OS>
inline errc WritePointer(OS& os, FormatSpec const& spec, void const* pointer)
{
    if (pointer == nullptr)
        return WriteRawString(os, spec, "(nil)");

    FormatSpec f = spec;
    f.hash = '#';
    f.conv = 'x';

    return WriteInt(os, f, 0, reinterpret_cast<uintptr_t>(pointer));
}

struct DoubleToAsciiResult {
    char* first;
    char* last;
    char  conv;
    char  sign;
    bool  isnum;
    bool  hexprefix;
};

// XXX: public...
FMTXX_API DoubleToAsciiResult DoubleToAscii(char* first, char* last, FormatSpec const& spec, double x);

// XXX: public...
template <typename OS>
inline errc WriteDouble(OS& os, FormatSpec const& spec, double x)
{
    static const size_t kBufSize = 1000; // >= 32
    char buf[kBufSize];

    const auto res = DoubleToAscii(buf, buf + kBufSize, spec, x);
    const auto len = static_cast<size_t>(res.last - res.first);

    if (res.isnum)
        return WriteNumber(os, spec, res.sign, "0x", res.hexprefix ? 2u : 0u, res.first, len);

    return WriteRawString(os, spec, res.first, len);
}

//
// Parse a non-negative integer in the range [0, INT_MAX].
// Returns -1 on overflow.
//
// PRE: IsDigit(*s) == true.
//
inline int ParseInt(const char*& s, const char* end)
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

FMTXX_API errc ParseFormatSpec(FormatSpec& spec, const char*& f, const char* end, int& nextarg, Types types, Arg const* args);

template <typename OS>
inline errc CallFormatFunc(OS& os, FormatSpec const& spec, int index, Types types, Arg const* args)
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
        return arg.other.func(static_cast<void*>(&os), spec, arg.other.value);
    case Types::T_STRING:
        return WriteString(os, spec, arg.string.data(), arg.string.size());
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
        return WriteRawString(os, spec, "[[error]]");
    }

    return errc::success; // unreachable -- fix warning
}

template <typename OS>
errc DoFormatImpl(OS& os, std::string_view format, Types types, Arg const* args)
{
    //const int num_args = types.value == 0 ? 0 : (16 - (CountLeadingZeros64(types.value) / 4));

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

        if (f != s && !Write(os, s, static_cast<size_t>(f - s)))
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

        const errc ec = CallFormatFunc(os, spec, index, types, args);
        if (ec != errc::success)
            return ec;

        if (f == end) // done.
            break;

        s = ++f; // skip '}'
    }

    return errc::success;
}

template <typename OS>
errc DoFormat(OS& os, std::string_view format, Types types, Arg const* args)
{
    return DoFormatImpl(os, format, types, args);
}

FMTXX_API errc DoFormat(std::string&  os, std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(std::FILE&    os, std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(CharArray&    os, std::string_view format, Types types, Arg const* args);

template <typename OS, typename ...Args>
errc Format(OS&& os, std::string_view format, Args const&... args)
{
    Arg arr[] = { Arg(os, args)... };

    // NOTE:
    // No std::forward here! DoFormat always takes the output stream by &
    return DoFormat(os, format, Types(args...), arr);
}

template <typename OS>
errc Format(OS&& os, std::string_view format)
{
    return DoFormat(os, format, 0, nullptr);
}

} // namespace impl
} // namespace fmtxx

//template <typename OS, typename ...Args>
//inline fmtxx::errc fmtxx::FormatToStream(OS&& os, std::string_view format, Args const&... args)
//{
//    return fmtxx::impl::Format(std::forward<OS>(os), format, args...);
//}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(std::string& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
inline std::string fmtxx::StringFormat(std::string_view format, Args const&... args)
{
    std::string os;
    fmtxx::Format(os, format, args...);
    return os;
}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(std::FILE* os, std::string_view format, Args const&... args)
{
    assert(os != nullptr);
    return fmtxx::impl::Format(*os, format, args...);
}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(std::ostream& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(CharArray& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
inline fmtxx::FormatToCharArrayResult fmtxx::Format(char* first, char* last, std::string_view format, Args const&... args)
{
    CharArray os { first, last };

    FormatToCharArrayResult res;

    res.ec = fmtxx::Format(os, format, args...);
    res.next = os.next;

    return res;
}

//------------------------------------------------------------------------------
// Copyright 2016 A. Bolz
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
