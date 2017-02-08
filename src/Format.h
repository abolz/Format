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
#if 1
    = delete;
#else
{
    os << value;
    return os.good() ? errc::success : errc::io_error;
}
#endif

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
struct EnableADL // re-associate T to namespace fmtxx::impl
{
    T& value;
    explicit EnableADL(T& v) : value(v) {}
};

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

// It's a class template because the output-stream needs to be known here...
// Is there a way to avoid this??
template <typename OS>
class Arg
{
public:
    template <bool>
    struct BoolConst {};

    using Func = errc (*)(OS& os, FormatSpec const& spec, void const* value);

    template <typename T>
    static errc FormatValue_template(OS& os, FormatSpec const& spec, void const* value)
    {
        return fmtxx__FormatValue(os, spec, *static_cast<T const*>(value));
    }

    struct Other { void const* value; Func func; };
    struct String { char const* str; size_t len; };

    union {
        Other other;
        String string;
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

    Arg() = default;

    template <typename T>
    Arg(T const& v, BoolConst<true>) : string{ v.data(), v.size() }
    {
    }

    template <typename T>
    Arg(T const& v, BoolConst<false>) : other{ &v, &FormatValue_template<T> }
    {
    }

    // XXX:
    // Keep in sync with Types::GetId() above!!!
    template <typename T>
    Arg(T                  const& v) : Arg(v, BoolConst<IsString<T>::value>{}) {}
    Arg(bool               const& v) : bool_(v) {}
    Arg(std::string_view   const& v) : string { v.data(), v.size() } {}
    Arg(std::string        const& v) : string { v.data(), v.size() } {}
    Arg(std::nullptr_t     const& v) : pvoid(v) {}
    Arg(void const*        const& v) : pvoid(v) {}
    Arg(void*              const& v) : pvoid(v) {}
    Arg(char const*        const& v) : pchar(v) {}
    Arg(char*              const& v) : pchar(v) {}
    Arg(char               const& v) : char_(v) {}
    Arg(signed char        const& v) : schar(v) {}
    Arg(signed short       const& v) : sshort(v) {}
    Arg(signed int         const& v) : sint(v) {}
#if LONG_MAX == INT_MAX
    Arg(signed long        const& v) : sint(v) {}
#else
    Arg(signed long        const& v) : slonglong(v) {}
#endif
    Arg(signed long long   const& v) : slonglong(v) {}
    Arg(unsigned char      const& v) : ulonglong(v) {}
    Arg(unsigned short     const& v) : ulonglong(v) {}
    Arg(unsigned int       const& v) : ulonglong(v) {}
    Arg(unsigned long      const& v) : ulonglong(v) {}
    Arg(unsigned long long const& v) : ulonglong(v) {}
    Arg(double             const& v) : double_(v) {}
    Arg(float              const& v) : double_(static_cast<double>(v)) {}
    Arg(FormatSpec         const& v) : pvoid(&v) {}
};

template <typename T> inline constexpr T Min(T x, T y) { return y < x ? y : x; }
template <typename T> inline constexpr T Max(T x, T y) { return y < x ? x : y; }

// XXX: fmtxx__Put
// XXX: fmtxx__Write
// XXX: fmtxx__Pad
FMTXX_API bool Put   (std::string  & os, char c);
FMTXX_API bool Write (std::string  & os, char const* str, size_t len);
FMTXX_API bool Pad   (std::string  & os, char c, size_t count);
FMTXX_API bool Put   (std::FILE*   & os, char c);
FMTXX_API bool Write (std::FILE*   & os, char const* str, size_t len);
FMTXX_API bool Pad   (std::FILE*   & os, char c, size_t count);
FMTXX_API bool Put   (std::ostream & os, char c);
FMTXX_API bool Write (std::ostream & os, char const* str, size_t len);
FMTXX_API bool Pad   (std::ostream & os, char c, size_t count);
FMTXX_API bool Put   (CharArray    & os, char c);
FMTXX_API bool Write (CharArray    & os, char const* str, size_t len);
FMTXX_API bool Pad   (CharArray    & os, char c, size_t count);

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
    return WriteRawString(os, spec, val ? "true" : "false", val ? 4u : 5u);
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
        return WriteRawString(os, spec, "(nil)", 5);

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

inline void FixFormatSpec(FormatSpec& spec)
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

template <typename OS>
inline errc ParseFormatSpec_part1(FormatSpec& spec, const char*& f, const char* end, int& nextarg, Types types, Arg<OS> const* args)
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

    return errc::success;
}

FMTXX_API errc ParseFormatSpec_part2(FormatSpec& spec, const char*& f, const char* end);

template <typename OS>
inline errc CallFormatFunc(OS& os, FormatSpec const& spec, int index, Types types, Arg<OS> const* args)
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
        return WriteRawString(os, spec, "[[error]]", 9);
    }

    return errc::success; // unreachable -- fix warning
}

template <typename OS>
errc DoFormatImpl(OS& os, std::string_view format, Types types, Arg<OS> const* args)
{
    assert(args != nullptr);

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
            errc ec = ParseFormatSpec_part1(spec, f, end, nextarg, types, args);
            if (ec != errc::success)
                return ec;
            ec = ParseFormatSpec_part2(spec, f, end);
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

#if 0
template <typename OS>
inline errc DoFormat(OS& os, std::string_view format, Types types, Arg<OS> const* args)
{
    return DoFormatImpl(os, format, types, args);
}
#endif

FMTXX_API errc DoFormat(std::string  & os, std::string_view format, Types types, Arg<std::string > const* args);
FMTXX_API errc DoFormat(std::FILE*   & os, std::string_view format, Types types, Arg<std::FILE*  > const* args);
FMTXX_API errc DoFormat(std::ostream & os, std::string_view format, Types types, Arg<std::ostream> const* args);
FMTXX_API errc DoFormat(CharArray    & os, std::string_view format, Types types, Arg<CharArray   > const* args);

} // namespace impl
} // namespace fmtxx

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(std::string& os, std::string_view format, Args const&... args)
{
    const size_t N = sizeof...(Args);
    const fmtxx::impl::Arg<std::string> arr[N ? N : 1] = { args... };

    return fmtxx::impl::DoFormat(os, format, fmtxx::impl::Types(args...), arr);
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
    const size_t N = sizeof...(Args);
    const fmtxx::impl::Arg<std::FILE*> arr[N ? N : 1] = { args... };

    return fmtxx::impl::DoFormat(os, format, fmtxx::impl::Types(args...), arr);
}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(std::ostream& os, std::string_view format, Args const&... args)
{
    const size_t N = sizeof...(Args);
    const fmtxx::impl::Arg<std::ostream> arr[N ? N : 1] = { args... };

    return fmtxx::impl::DoFormat(os, format, fmtxx::impl::Types(args...), arr);
}

template <typename ...Args>
inline fmtxx::errc fmtxx::Format(CharArray& os, std::string_view format, Args const&... args)
{
    const size_t N = sizeof...(Args);
    const fmtxx::impl::Arg<CharArray> arr[N ? N : 1] = { args... };

    return fmtxx::impl::DoFormat(os, format, fmtxx::impl::Types(args...), arr);
}

template <typename ...Args>
inline fmtxx::FormatToCharArrayResult fmtxx::Format(char* first, char* last, std::string_view format, Args const&... args)
{
    CharArray os { first, last };

    FormatToCharArrayResult res;

    res.ec = Format(os, format, args...);
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
