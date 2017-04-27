// Distributed under the MIT license. See the end of the file for details.

#pragma once

// 0: assert-no-check   (unsafe; invalid format strings -> UB)
// 1: assert-check      (safe)
// 2: throw             (safe)
#define FMTXX_FORMAT_STRING_CHECK_POLICY 1

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <cstdint>
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
// API
//

enum struct errc {
    success,
    io_error,
    invalid_format_string,
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

struct FMTXX_VISIBILITY_DEFAULT Writer
{
    FMTXX_API virtual ~Writer();

    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
};

struct FMTXX_VISIBILITY_DEFAULT StringWriter : public Writer
{
    std::string& os;

    explicit StringWriter(std::string& v) : os(v) {}

    FMTXX_API bool Put(char c) override;
    FMTXX_API bool Write(char const* str, size_t len) override;
    FMTXX_API bool Pad(char c, size_t count) override;
};

struct FMTXX_VISIBILITY_DEFAULT FILEWriter : public Writer
{
    std::FILE* os;

    explicit FILEWriter(std::FILE* v) : os(v) {}

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;
};

struct FMTXX_VISIBILITY_DEFAULT StreamWriter : public Writer
{
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

struct FMTXX_VISIBILITY_DEFAULT CharArrayWriter : public Writer
{
    CharArray& os;

    explicit CharArrayWriter(CharArray& v) : os(v) {}

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;
};

struct Util
{
    static FMTXX_API bool WriteSignedInt  (Writer& w, int64_t val);
    static FMTXX_API bool WriteUnsignedInt(Writer& w, uint64_t val);
    static FMTXX_API bool WriteHexInt     (Writer& w, uint64_t val); // includes a "0x" prefix
    static FMTXX_API bool WriteDouble     (Writer& w, double val); // guaranteed to round-trip

    // Note:
    // The string must not be null. This function prints len characters, including '\0's.
    static FMTXX_API errc FormatString (Writer& w, FormatSpec const& spec, char const* str, size_t len);
    // Note:
    // This is different from just calling FormatString(str, strlen(str)):
    // This function handles nullptr's and if a precision is specified uses strnlen instead of strlen.
    static FMTXX_API errc FormatString (Writer& w, FormatSpec const& spec, char const* str);
    static FMTXX_API errc FormatInt    (Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext);
    static FMTXX_API errc FormatBool   (Writer& w, FormatSpec const& spec, bool val);
    static FMTXX_API errc FormatChar   (Writer& w, FormatSpec const& spec, char ch);
    static FMTXX_API errc FormatPointer(Writer& w, FormatSpec const& spec, void const* pointer);
    static FMTXX_API errc FormatDouble (Writer& w, FormatSpec const& spec, double x);

    template <typename T>
    static inline errc FormatInt(Writer& w, FormatSpec const& spec, T value) {
        return FormatInt(w, spec, value, std::is_signed<T>{});
    }

private:
    template <typename T>
    static inline errc FormatInt(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::true_type) {
        return FormatInt(w, spec, value, static_cast<std::make_unsigned_t<T>>(value));
    }

    template <typename T>
    static inline errc FormatInt(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::false_type) {
        return FormatInt(w, spec, 0, value);
    }
};

//
// Specialize this if you want your data-type to be treated as a string.
//
// T must have member functions data() and size() and their return values must
// be convertible to char const* and size_t resp.
//
template <typename T>
struct TreatAsString {
    static constexpr bool value = false;
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
template <typename T>
struct FormatValue {
    errc operator()(Writer& w, FormatSpec const& spec, T const& value) const;
};

template <typename T>
struct WriteValue {
    bool operator()(Writer& w, T const& value) const;
};

// Appends the formatted arguments to the given output stream.
template <typename ...Args>
errc Format(Writer& w, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given string.
template <typename ...Args>
errc Format(std::string& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Format(std::FILE* os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Format(std::ostream& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Format(CharArray& os, std::string_view format, Args const&... args);

//struct ArrayFormatResult {
//    char* next;
//    bool ec;
//};

//template <typename ...Args>
//ArrayFormatResult ArrayFormat(char* next, char* last, std::string_view format, Args const&... args) noexcept;

// Returns a std::string containing the formatted arguments.
template <typename ...Args>
std::string StringFormat(std::string_view format, Args const&... args);

// Appends the formatted arguments to the given output stream.
template <typename ...Args>
errc Printf(Writer& w, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given string.
template <typename ...Args>
errc Printf(std::string& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Printf(std::FILE* os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Printf(std::ostream& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Printf(CharArray& os, std::string_view format, Args const&... args);

//template <typename ...Args>
//ArrayFormatResult ArrayPrintf(char* next, char* last, std::string_view format, Args const&... args) noexcept;

// Returns a std::string containing the formatted arguments.
template <typename ...Args>
std::string StringPrintf(std::string_view format, Args const&... args);

//------------------------------------------------------------------------------
// Stream API

inline Writer& operator<<(Writer& w, char ch) {
    w.Put(ch);
    return w;
}

inline Writer& operator<<(Writer& w, std::string_view str) {
    w.Write(str.data(), str.size());
    return w;
}

inline Writer& operator<<(Writer& w, char const* str) {
    return w << std::string_view(str != nullptr ? str : "(null)");
}

inline Writer& operator<<(Writer& w, char* str) {
    return w << std::string_view(str != nullptr ? str : "(null)");
}

inline Writer& operator<<(Writer& w, bool val) {
    return w << std::string_view(val ? "true" : "false");
}

inline Writer& operator<<(Writer& w, void const* ptr) {
    fmtxx::Util::WriteHexInt(w, reinterpret_cast<uintptr_t>(ptr));
    return w;
}

inline Writer& operator<<(Writer& w, void* ptr) {
    fmtxx::Util::WriteHexInt(w, reinterpret_cast<uintptr_t>(ptr));
    return w;
}

inline Writer& operator<<(Writer& w, signed char val) {
    fmtxx::Util::WriteSignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, signed short val) {
    fmtxx::Util::WriteSignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, signed int val) {
    fmtxx::Util::WriteSignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, signed long val) {
    fmtxx::Util::WriteSignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, signed long long val) {
    fmtxx::Util::WriteSignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, unsigned char val) {
    fmtxx::Util::WriteUnsignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, unsigned short val) {
    fmtxx::Util::WriteUnsignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, unsigned int val) {
    fmtxx::Util::WriteUnsignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, unsigned long val) {
    fmtxx::Util::WriteUnsignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, unsigned long long val) {
    fmtxx::Util::WriteUnsignedInt(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, double val) {
    fmtxx::Util::WriteDouble(w, val);
    return w;
}

inline Writer& operator<<(Writer& w, float val) {
    fmtxx::Util::WriteDouble(w, static_cast<double>(val));
    return w;
}

template <typename T>
inline Writer& operator<<(Writer& w, T const& value) {
    fmtxx::WriteValue<T>{}(w, value);
    return w;
}

template <
    typename WriterT,
    typename T,
    typename = std::enable_if_t< !std::is_lvalue_reference<WriterT>::value && std::is_base_of<Writer, WriterT>::value >
>
inline WriterT&& operator<<(WriterT&& w, T const& val) {
    w << val;
    return std::move(w);
}

} // namespace fmtxx

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <climits>

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
        assert(index >= 0);
        if (index < 0 || index >= 16)
            return T_NONE;
        return static_cast<EType>((types >> (4 * index)) & 0xF);
    }

private:
    // XXX:
    // Keep in sync with Arg::Arg() below!!!
    template <typename T>
    static unsigned GetId(T                  const&) { return TreatAsString<T>::value ? T_STRING : T_OTHER; }
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
    using Func = errc (*)(Writer& buf, FormatSpec const& spec, void const* value);

    template <typename T>
    static errc FormatValue_fn(Writer& buf, FormatSpec const& spec, void const* value)
    {
        return FormatValue<T>{}(buf, spec, *static_cast<T const*>(value));
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

    template <typename T>
    Arg(T const& v, std::true_type) : string{ v.data(), v.size() }
    {
    }

    template <typename T>
    Arg(T const& v, std::false_type) : other{ &v, &FormatValue_fn<T> }
    {
    }

    // XXX:
    // Keep in sync with Types::GetId() above!!!
    template <typename T>
    Arg(T const& v) : Arg(v, std::integral_constant<bool, TreatAsString<T>::value>{}) {}

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

template <typename Stream = std::ostringstream, typename T>
errc StreamValue(Writer& w, FormatSpec const& spec, T const& value)
{
    Stream stream;
    stream << value;
    auto const& str = stream.str(); // Efficient read-only access would be nice...
    return Util::FormatString(w, spec, str.data(), str.size());
}

template <typename Stream = std::ostringstream, typename T>
bool StreamValue(Writer& w, T const& value)
{
    Stream stream;
    stream << value;
    auto const& str = stream.str(); // Efficient read-only access would be nice...
    return w.Write(str.data(), str.size());
}

} // namespace impl
} // namespace fmtxx

template <typename T>
fmtxx::errc fmtxx::FormatValue<T>::operator()(Writer& w, FormatSpec const& spec, T const& value) const
{
    return fmtxx::impl::StreamValue(w, spec, value);
}

template <typename T>
bool fmtxx::WriteValue<T>::operator()(Writer& w, T const& value) const
{
    return fmtxx::impl::StreamValue(w, value);
}

template <typename ...Args>
fmtxx::errc fmtxx::Format(Writer& w, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(w, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Format(std::string& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Format(std::FILE* os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Format(std::ostream& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Format(CharArray& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Format(os, format, args...);
}

template <typename ...Args>
std::string fmtxx::StringFormat(std::string_view format, Args const&... args)
{
    std::string os;
    fmtxx::Format(os, format, args...); // Returns true or throws (OOM)
    return os;
}

template <typename ...Args>
fmtxx::errc fmtxx::Printf(Writer& w, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(w, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Printf(std::string& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Printf(std::FILE* os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Printf(std::ostream& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
fmtxx::errc fmtxx::Printf(CharArray& os, std::string_view format, Args const&... args)
{
    return fmtxx::impl::Printf(os, format, args...);
}

template <typename ...Args>
std::string fmtxx::StringPrintf(std::string_view format, Args const&... args)
{
    std::string os;
    fmtxx::Printf(os, format, args...); // Returns true or throws (OOM)
    return os;
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
