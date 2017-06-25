// Distributed under the MIT license. See the end of the file for details.

#pragma once
#define FMTXX_FORMAT_H 1

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iosfwd>
#include <string>
#if _MSC_VER || __cplusplus >= 201703
#  include <string_view>
#else
#  include <experimental/string_view>
   namespace std { using std::experimental::string_view; }
#endif

#ifdef _MSC_VER
#  define FMTXX_VISIBILITY_DEFAULT
#else
#  define FMTXX_VISIBILITY_DEFAULT __attribute__((visibility("default")))
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

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

enum struct errc {
    success,
    io_error,
};

enum struct Align : unsigned char {
    Default,
    Left,
    Right,
    Center,
    PadAfterSign,
};

enum struct Sign : unsigned char {
    Default, // = Minus
    Minus,   // => '-' if negative, nothing otherwise
    Plus,    // => '-' if negative, '+' otherwise
    Space,   // => '-' if negative, fill-char otherwise
};

struct FMTXX_VISIBILITY_DEFAULT FormatSpec
{
    std::string_view style;
    int   width = 0;
    int   prec  = -1;
    char  fill  = ' ';
    Align align = Align::Default;
    Sign  sign  = Sign::Default;
    bool  hash  = false;
    bool  zero  = false;
    char  tsep  = '\0';
    char  conv  = '\0';
};

class FMTXX_VISIBILITY_DEFAULT Writer
{
public:
    FMTXX_API virtual ~Writer();

    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
};

class FMTXX_VISIBILITY_DEFAULT StringWriter : public Writer
{
public:
    std::string& os;

    explicit StringWriter(std::string& v) : os(v) {}

    FMTXX_API bool Put(char c) override;
    FMTXX_API bool Write(char const* str, size_t len) override;
    FMTXX_API bool Pad(char c, size_t count) override;
};

class FMTXX_VISIBILITY_DEFAULT FILEWriter : public Writer
{
public:
    std::FILE* os;

    explicit FILEWriter(std::FILE* v) : os(v) {}

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;
};

class FMTXX_VISIBILITY_DEFAULT StreamWriter : public Writer
{
public:
    std::ostream& os;

    explicit StreamWriter(std::ostream& v) : os(v) {}

    FMTXX_API bool Put(char c) override;
    FMTXX_API bool Write(char const* str, size_t len) override;
    FMTXX_API bool Pad(char c, size_t count) override;
};

struct FMTXX_VISIBILITY_DEFAULT CharArray
{
    char*       next;
    char* const last;

    explicit CharArray(char* f, char* l) : next(f), last(l) {}

    template <size_t N>
    explicit CharArray(char (&buf)[N]) : next(buf), last(buf + N) {}
};

class FMTXX_VISIBILITY_DEFAULT CharArrayWriter : public Writer
{
public:
    CharArray& os;

    explicit CharArrayWriter(CharArray& v) : os(v) {}

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;
};

struct Util
{
    // Note:
    // The string must not be null. This function prints len characters, including '\0's.
    static FMTXX_API errc format_string (Writer& w, FormatSpec const& spec, char const* str, size_t len);
    // Note:
    // This is different from just calling format_string(str, strlen(str)):
    // This function handles nullptr's and if a precision is specified uses strnlen instead of strlen.
    static FMTXX_API errc format_string (Writer& w, FormatSpec const& spec, char const* str);
    static FMTXX_API errc format_int    (Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext);
    static FMTXX_API errc format_bool   (Writer& w, FormatSpec const& spec, bool val);
    static FMTXX_API errc format_char   (Writer& w, FormatSpec const& spec, char ch);
    static FMTXX_API errc format_pointer(Writer& w, FormatSpec const& spec, void const* pointer);
    static FMTXX_API errc format_double (Writer& w, FormatSpec const& spec, double x);

    template <typename T>
    static inline errc format_int(Writer& w, FormatSpec const& spec, T value) {
        return format_int(w, spec, value, std::is_signed<T>{});
    }

private:
    template <typename T>
    static inline errc format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::true_type) {
        return format_int(w, spec, value, static_cast<std::make_unsigned_t<T>>(value));
    }

    template <typename T>
    static inline errc format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::false_type) {
        return format_int(w, spec, 0, value);
    }
};

//
// Specialize this to format user-defined types.
//
// The default implementation uses std::ostringstream to convert the value into
// a string and then writes the string to the output buffer.
//
// I.e., the default implementation requires:
//      #include <sstream>
// !!!
//
template <typename T = void>
struct FormatValue
{
    template <typename Stream = std::ostringstream>
    errc operator()(Writer& w, FormatSpec const& spec, T const& val) const
    {
        Stream stream;
        stream << val;
        auto const& str = stream.str();
        return Util::format_string(w, spec, str.data(), str.size());
    }
};

template <>
struct FormatValue<bool> {
    errc operator()(Writer& w, FormatSpec const& spec, bool val) const {
        return Util::format_bool(w, spec, val);
    }
};

template <>
struct FormatValue<std::string_view> {
    errc operator()(Writer& w, FormatSpec const& spec, std::string_view val) const {
        return Util::format_string(w, spec, val.data(), val.size());
    }
};

template <>
struct FormatValue<std::string> {
    errc operator()(Writer& w, FormatSpec const& spec, std::string const& val) const {
        return Util::format_string(w, spec, val.data(), val.size());
    }
};

template <>
struct FormatValue<char const*> {
    errc operator()(Writer& w, FormatSpec const& spec, char const* val) const {
        return Util::format_string(w, spec, val);
    }
};

template <>
struct FormatValue<char*> {
    errc operator()(Writer& w, FormatSpec const& spec, char* val) const {
        return Util::format_string(w, spec, val);
    }
};

template <>
struct FormatValue<char> {
    errc operator()(Writer& w, FormatSpec const& spec, char val) const {
        return Util::format_char(w, spec, val);
    }
};

template <>
struct FormatValue<void const*> {
    errc operator()(Writer& w, FormatSpec const& spec, void const* val) const {
        return Util::format_pointer(w, spec, val);
    }
};

template <>
struct FormatValue<void*> {
    errc operator()(Writer& w, FormatSpec const& spec, void* val) const {
        return Util::format_pointer(w, spec, val);
    }
};

template <>
struct FormatValue<std::nullptr_t> {
    errc operator()(Writer& w, FormatSpec const& spec, std::nullptr_t) const {
        return Util::format_pointer(w, spec, nullptr);
    }
};

template <>
struct FormatValue<signed char> {
    errc operator()(Writer& w, FormatSpec const& spec, signed char val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed short> {
    errc operator()(Writer& w, FormatSpec const& spec, signed short val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed int> {
    errc operator()(Writer& w, FormatSpec const& spec, signed int val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed long> {
    errc operator()(Writer& w, FormatSpec const& spec, signed long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed long long> {
    errc operator()(Writer& w, FormatSpec const& spec, signed long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned char> {
    errc operator()(Writer& w, FormatSpec const& spec, unsigned char val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned short> {
    errc operator()(Writer& w, FormatSpec const& spec, unsigned short val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned int> {
    errc operator()(Writer& w, FormatSpec const& spec, unsigned int val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned long> {
    errc operator()(Writer& w, FormatSpec const& spec, unsigned long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned long long> {
    errc operator()(Writer& w, FormatSpec const& spec, unsigned long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<double> {
    errc operator()(Writer& w, FormatSpec const& spec, double val) const {
        return Util::format_double(w, spec, val);
    }
};

template <>
struct FormatValue<float> {
    errc operator()(Writer& w, FormatSpec const& spec, float val) const {
        return Util::format_double(w, spec, static_cast<double>(val));
    }
};

template <>
struct FormatValue<long double> {
    errc operator()(Writer& w, FormatSpec const& spec, long double val) const {
        return Util::format_double(w, spec, static_cast<double>(val));
    }
};

template <>
struct FormatValue<void>
{
    template <typename T>
    errc operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return FormatValue<T>{}(w, spec, val);
    }
};

template <
    typename WriterT,
    typename T,
    typename = std::enable_if_t< std::is_base_of<Writer, std::remove_reference_t<WriterT>>::value >
>
errc format_value(WriterT&& w, FormatSpec const& spec, T const& value) {
    return FormatValue<T>{}(w, spec, value);
}

// Appends the formatted arguments to the given output stream.
template <typename ...Args>
errc format(Writer& w, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given string.
template <typename ...Args>
errc format(std::string& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc format(std::FILE* os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc format(std::ostream& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc format(CharArray& os, std::string_view format, Args const&... args);

// Returns a std::string containing the formatted arguments.
template <typename ...Args>
std::string sformat(std::string_view format, Args const&... args);

// Appends the formatted arguments to the given output stream.
template <typename ...Args>
errc printf(Writer& w, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given string.
template <typename ...Args>
errc printf(std::string& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc printf(std::FILE* os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc printf(std::ostream& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc printf(CharArray& os, std::string_view format, Args const&... args);

// Returns a std::string containing the formatted arguments.
template <typename ...Args>
std::string sprintf(std::string_view format, Args const&... args);

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

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
        T_SLONGLONG,  // 10
        T_ULONGLONG,  // 11
                      // 12
                      // 13
        T_DOUBLE,     // 14 XXX: float is promoted to double. long double ???
        T_FORMATSPEC, // 15
    };

    value_type const types = 0;

    Types() = default;

    template <typename ...Args>
    explicit Types(Args const&... args) : types(Make(args...))
    {
    }

    EType operator[](int index) const
    {
        if (index < 0 || index >= 16)
            return T_NONE;
        return static_cast<EType>((types >> (4 * index)) & 0xF);
    }

private:
    // XXX:
    // Keep in sync with Arg::Arg() below!!!
    template <typename T>
    static unsigned GetId(T                  const&) { return T_OTHER; }
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
    static unsigned GetId(long double        const&) { return T_DOUBLE; }
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
    using Func = errc (*)(Writer& w, FormatSpec const& spec, void const* value);

    template <typename T>
    static errc FormatValue_fn(Writer& w, FormatSpec const& spec, void const* value)
    {
        return FormatValue<T>{}(w, spec, *static_cast<T const*>(value));
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

    // XXX:
    // Keep in sync with Types::GetId() above!!!
    template <typename T>
    Arg(T                  const& v) : other{ &v, &FormatValue_fn<T> } {}
    Arg(bool               const& v) : bool_(v) {}
    Arg(std::string_view   const& v) : string(v) {}
    Arg(std::string        const& v) : string(v) {}
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
    Arg(long double        const& v) : double_(static_cast<double>(v)) {}
    Arg(FormatSpec         const& v) : pvoid(&v) {}
};

FMTXX_API errc DoFormat(Writer& w,        std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(std::string& os,  std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(std::FILE* os,    std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(std::ostream& os, std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat(CharArray& os,    std::string_view format, Types types, Arg const* args);

template <typename WriterT, typename ...Args>
inline errc Format(WriterT& w, std::string_view format, Args const&... args)
{
    Arg arr[] = { args... };
    return fmtxx::impl::DoFormat(w, format, Types(args...), arr);
}

template <typename WriterT>
inline errc Format(WriterT& w, std::string_view format)
{
    return fmtxx::impl::DoFormat(w, format, Types(), nullptr);
}

FMTXX_API errc DoPrintf(Writer& w,        std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf(std::string& os,  std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf(std::FILE* os,    std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf(std::ostream& os, std::string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf(CharArray& os,    std::string_view format, Types types, Arg const* args);

template <typename WriterT, typename ...Args>
inline errc Printf(WriterT& w, std::string_view format, Args const&... args)
{
    Arg arr[] = { args... };
    return fmtxx::impl::DoPrintf(w, format, Types(args...), arr);
}

template <typename WriterT>
inline errc Printf(WriterT& w, std::string_view format)
{
    return fmtxx::impl::DoPrintf(w, format, Types(), nullptr);
}

} // namespace impl

template <typename ...Args>
errc format(Writer& w, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(w, format, args...);
}

template <typename ...Args>
errc format(std::string& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
errc format(std::FILE* os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
errc format(std::ostream& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
errc format(CharArray& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
std::string sformat(std::string_view format, Args const&... args)
{
    std::string os;
    fmtxx::format(os, format, args...); // Returns true or throws (OOM)
    return os;
}

template <typename ...Args>
errc printf(Writer& w, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(w, format, args...);
}

template <typename ...Args>
errc printf(std::string& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
errc printf(std::FILE* os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
errc printf(std::ostream& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
errc printf(CharArray& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
std::string sprintf(std::string_view format, Args const&... args)
{
    std::string os;
    fmtxx::printf(os, format, args...); // Returns true or throws (OOM)
    return os;
}

} // namespace fmtxx

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
