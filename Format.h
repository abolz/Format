// Distributed under the MIT license. See the end of the file for deteils.

#pragma once

#if _MSC_VER
#include <string_view>
using std__string_view = std::string_view;
#else
#include <experimental/string_view>
using std__string_view = std::experimental::string_view;
#endif
#include <iosfwd>

#ifdef FMTXX_SHARED
#  ifdef _MSC_VER
#    ifdef FMTXX_EXPORT
#      define FMTXX_API __declspec(dllexport)
#    else
#      define FMTXX_API __declspec(dllimport)
#    endif
#  else
#    ifdef FMTXX_EXPORT
#      define FMTXX_API __attribute__((visibility("default")))
#    else
#      define FMTXX_API
#    endif
#  endif
#else
#  define FMTXX_API
#endif

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace fmtxx {

struct FormatSpec
{
    std__string_view style;
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
// Implement this to printf your own data-type.
//  (NOTE: In the data-type's namespace!)
//
template <typename T>
int fmtxx__FormatValue(std::ostream& out, FormatSpec const& spec, T const& value)
#if 1
    = delete;
#else
{
    //
    // XXX:
    //
    // Set the stream flags depending on the format first... and reset after
    // streaming the value...
    //
    out << value;
    return out.good() ? 0 : -1;
}
#endif

//
// The formatting function which does all the work
//
template <typename ...Args>
int Format(std::ostream& out, std__string_view format, Args const&... args);

} // namespace fmtxx

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <cassert>
#include <climits>
#include <cstddef>

namespace fmtxx {
namespace impl {

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

    unsigned operator [](int index) const
    {
        assert(index >= 0);
        if (index >= 16)
            return 0;
        return static_cast<unsigned>((types >> (4 * index)) & 0xF);
    }

private:
    // XXX:
    // Keep in sync with Arg::Arg() below!!!
    template <typename T>
    unsigned GetId(T                  const&) { return IsString<T>::value ? T_STRING : T_OTHER; }
    unsigned GetId(bool               const&) { return T_BOOL; }
    unsigned GetId(std__string_view   const&) { return T_STRING; }
    unsigned GetId(std::string        const&) { return T_STRING; }
    unsigned GetId(std::nullptr_t     const&) { return T_PVOID; }
    unsigned GetId(void const*        const&) { return T_PVOID; }
    unsigned GetId(void*              const&) { return T_PVOID; }
    unsigned GetId(char const*        const&) { return T_PCHAR; }
    unsigned GetId(char*              const&) { return T_PCHAR; }
    unsigned GetId(char               const&) { return T_CHAR; }
    unsigned GetId(signed char        const&) { return T_SCHAR; }
    unsigned GetId(signed short       const&) { return T_SSHORT; }
    unsigned GetId(signed int         const&) { return T_SINT; }
#if LONG_MAX == INT_MAX
    unsigned GetId(signed long        const&) { return T_SINT; }
#else
    unsigned GetId(signed long        const&) { return T_SLONGLONG; }
#endif
    unsigned GetId(signed long long   const&) { return T_SLONGLONG; }
    unsigned GetId(unsigned char      const&) { return T_ULONGLONG; }
    unsigned GetId(unsigned short     const&) { return T_ULONGLONG; }
    unsigned GetId(unsigned int       const&) { return T_ULONGLONG; }
    unsigned GetId(unsigned long      const&) { return T_ULONGLONG; }
    unsigned GetId(unsigned long long const&) { return T_ULONGLONG; }
    unsigned GetId(double             const&) { return T_DOUBLE; }
    unsigned GetId(float              const&) { return T_DOUBLE; }
    unsigned GetId(FormatSpec         const&) { return T_FORMATSPEC; }

    value_type Make() { return 0; }

    template <typename A1, typename ...An>
    value_type Make(A1 const& a1, An const&... an)
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

    using Func = int (*)(std::ostream& buf, FormatSpec const& spec, void const* value);

    template <typename T>
    static int FormatValue_template(std::ostream& buf, FormatSpec const& spec, void const* value)
    {
        return fmtxx__FormatValue(buf, spec, *static_cast<T const*>(value));
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
    Arg(std__string_view   const& v) : string { v.data(), v.size() } {}
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

FMTXX_API int DoFormat(std::ostream& buf, std__string_view format, Types types, Arg const* args);

} // namespace impl
} // namespace fmtxx

template <typename ...Args>
int fmtxx::Format(std::ostream& buf, std__string_view format, Args const&... args)
{
    constexpr size_t N = sizeof...(Args);
    const impl::Arg arr[N ? N : 1] = { args... };

    return impl::DoFormat(buf, format, impl::Types(args...), arr);
}

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
