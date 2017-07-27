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

#ifndef FMTXX_FORMAT_CORE_H
#define FMTXX_FORMAT_CORE_H 1

#include "cxx_string_view.h"

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

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

enum struct ErrorCode {
    conversion_error        = 1, // Value could not be converted to string (E.g. trying to format a non-existant date.)
    index_out_of_range      = 2, // Argument index out of range
    invalid_argument        = 3,
    invalid_format_string   = 4,
    io_error                = 5, // Writer failed
    not_supported           = 6, // Conversion not supported
    value_out_of_range      = 7, // Value of integer argument out of range [INT_MIN, INT_MAX]
};

// Wraps an error code, may be checked for failure.
// Replaces ErrorCode::operator bool() in most cases (and is more explicit).
struct Failed
{
    ErrorCode const ec = ErrorCode{};

    Failed() = default;
    Failed(ErrorCode ec_) : ec(ec_) {}

    // Test for failure.
    explicit operator bool() const { return ec != ErrorCode{}; }

    operator ErrorCode() const { return ec; }
};

enum struct Align : unsigned char {
    use_default,
    left,
    right,
    center,
    pad_after_sign,
};

enum struct Sign : unsigned char {
    use_default, // => minus
    minus,       // => '-' if negative, nothing otherwise
    plus,        // => '-' if negative, '+' otherwise
    space,       // => '-' if negative, fill-char otherwise
};

struct FMTXX_VISIBILITY_DEFAULT FormatSpec
{
    cxx::string_view style; // XXX: Points into the format string. Only valid while formatting arguments...
    int   width = 0;
    int   prec  = -1;
    char  fill  = ' ';
    Align align = Align::use_default;
    Sign  sign  = Sign::use_default;
    bool  hash  = false;
    bool  zero  = false;
    char  tsep  = '\0';
    char  conv  = '\0';
};

// The base class for output streams.
class FMTXX_VISIBILITY_DEFAULT Writer
{
public:
    FMTXX_API virtual ~Writer() noexcept;

    // Write a character to the output stream.
    ErrorCode put(char c) { return Put(c); }

    // Write a character to the output stream iff it is not the null-character.
    ErrorCode put_nonnull(char c) { return c == '\0' ? ErrorCode{} : Put(c); }

    // Insert a range of characters into the output stream.
    ErrorCode write(char const* str, size_t len) { return len == 0 ? ErrorCode{} : Write(str, len); }

    // Insert a character multiple times into the output stream.
    ErrorCode pad(char c, size_t count) { return count == 0 ? ErrorCode{} : Pad(c, count); }

private:
    virtual ErrorCode Put(char c) = 0;
    virtual ErrorCode Write(char const* str, size_t len) = 0;
    virtual ErrorCode Pad(char c, size_t count) = 0;
};

// Returned by the format_to_chars/printf_to_chars function (below).
// Like std::to_chars.
struct ToCharsResult
{
    char* next = nullptr;
    ErrorCode ec = ErrorCode{};

    ToCharsResult() = default;
    ToCharsResult(char* next_, ErrorCode ec_) : next(next_), ec(ec_) {}

    // Test for successful conversions
    explicit operator bool() const { return ec == ErrorCode{}; }
};

struct Util
{
    // Note:
    // The string must not be null. This function prints len characters, including '\0's.
    static FMTXX_API ErrorCode format_string (Writer& w, FormatSpec const& spec, char const* str, size_t len);
    // Note:
    // This is different from just calling format_string(str, strlen(str)):
    // This function handles nullptr's and if a precision is specified uses strnlen instead of strlen.
    static FMTXX_API ErrorCode format_string (Writer& w, FormatSpec const& spec, char const* str);
    static FMTXX_API ErrorCode format_int    (Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext);
    static FMTXX_API ErrorCode format_bool   (Writer& w, FormatSpec const& spec, bool val);
    static FMTXX_API ErrorCode format_char   (Writer& w, FormatSpec const& spec, char ch);
    static FMTXX_API ErrorCode format_pointer(Writer& w, FormatSpec const& spec, void const* pointer);
    static FMTXX_API ErrorCode format_double (Writer& w, FormatSpec const& spec, double x);

    template <typename T>
    static inline ErrorCode format_int(Writer& w, FormatSpec const& spec, T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        return format_int(w, spec, value, std::is_signed<T>{});
    }

private:
    template <typename T>
    static inline ErrorCode format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::true_type)
    {
        return format_int(w, spec, value, static_cast<typename std::make_unsigned<T>::type>(value));
    }

    template <typename T>
    static inline ErrorCode format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::false_type)
    {
        return format_int(w, spec, 0, value);
    }
};

//
// Provides the member constant value equal to true if objects of type T should
// be treated as strings by the Format library.
// Objects of type T must have member functions data() and size() and their
// return values must be convertible to 'char const*' and 'size_t' resp.
//
template <typename T, typename /*Enable*/ = void>
struct TreatAsString : std::false_type {};

template <>
struct TreatAsString< cxx::string_view >
    : std::true_type
{
};

// Dynamically created argument list.
class FormatArgs;

namespace impl
{
    template <typename> struct AlwaysFalse : std::false_type {};

    template <typename T, typename = void>
    struct StreamValue
    {
        static_assert(AlwaysFalse<T>::value,
            "Formatting objects of type T is not supported. "
            "Specialize 'FormatValue' or 'TreatAsString', or implement 'operator<<(std::ostream&, T const&)' "
            "and include 'Format_ostream.h'.");
    };
}

//
// Specialize this to format user-defined types.
//
template <typename T = void, typename /*Enable*/ = void>
struct FormatValue : impl::StreamValue<T>
{
};

template <typename T>
struct FormatValue<T, typename std::enable_if< TreatAsString<T>::value >::type>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return Util::format_string(w, spec, val.data(), val.size());
    }
};

template <>
struct FormatValue<char const*> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, char const* val) const {
        return Util::format_string(w, spec, val);
    }
};

template <>
struct FormatValue<char*> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, char* val) const {
        return Util::format_string(w, spec, val);
    }
};

template <>
struct FormatValue<void const*> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, void const* val) const {
        return Util::format_pointer(w, spec, val);
    }
};

template <>
struct FormatValue<void*> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, void* val) const {
        return Util::format_pointer(w, spec, val);
    }
};

template <>
struct FormatValue<std::nullptr_t> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, std::nullptr_t) const {
        return Util::format_pointer(w, spec, nullptr);
    }
};

template <>
struct FormatValue<bool> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, bool val) const {
        return Util::format_bool(w, spec, val);
    }
};

template <>
struct FormatValue<char> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, char val) const {
        return Util::format_char(w, spec, val);
    }
};

template <>
struct FormatValue<signed char> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed char val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed short> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed short val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed int> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed int val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed long> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<signed long long> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned char> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned char val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned short> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned short val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned int> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned int val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned long> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<unsigned long long> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <>
struct FormatValue<double> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, double val) const {
        return Util::format_double(w, spec, val);
    }
};

template <>
struct FormatValue<float> {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, float val) const {
        return Util::format_double(w, spec, static_cast<double>(val));
    }
};

template <>
struct FormatValue<void>
{
    template <typename T>
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return FormatValue<typename std::decay<T>::type>{}(w, spec, val);
    }
};

namespace impl {

#if 0
// C++14 or later

template <typename ...Ts>
using Void_t = void;

#else

template <typename ...>
struct AlwaysVoid { using type = void; };

template <typename ...Ts>
using Void_t = typename AlwaysVoid<Ts...>::type;

#endif

enum struct Type {
    none,
    formatspec,
    string,
    other,
    pchar,
    pvoid,
    bool_,
    char_,
    schar,      // XXX: promote to int?
    sshort,     // XXX: promote to int?
    sint,
    slonglong,
    ulonglong,
    double_,    // Includes 'float'
    last,       // Unused -- must be last.
};

struct Arg
{
    using Func = ErrorCode (*)(Writer& w, FormatSpec const& spec, void const* value);

    template <typename T>
    static ErrorCode FormatValue_fn(Writer& w, FormatSpec const& spec, void const* value)
    {
        return FormatValue<>{}(w, spec, *static_cast<T const*>(value));
    }

    struct String { char const* data; size_t size; };
    struct Other { void const* value; Func func; };

    union {
        String             string;
        Other              other;
        void const*        pvoid;
        char const*        pchar;
        char               char_;
        bool               bool_;
        signed char        schar;
        signed short       sshort;
        signed int         sint;
        signed long long   slonglong;
        unsigned long long ulonglong;
        double             double_;
    };

    Arg() = default;

    template <typename T> Arg(T const& v, /*IsString*/ std::true_type) : string{v.data(), v.size()} {}
    template <typename T> Arg(T const& v, /*IsString*/ std::false_type) : other{ &v, &FormatValue_fn<T> }
    {
        static_assert(
            !std::is_function<T>::value,
            "Formatting function types is not supported");
        static_assert(
            !std::is_pointer<T>::value && !std::is_member_pointer<T>::value,
            "Formatting non-void pointer types is not allowed. "
            "A cast to void* or intptr_t is required.");
        static_assert(
            !std::is_same<FormatArgs, T>::value,
            "Formatting variadic FormatArgs in combination with other arguments "
            "is (currently) not supported. The only valid syntax for FormatArgs is "
            "as a single argument to the formatting functions.");
    }

    // XXX:
    // Keep in sync with SelectType<> below!!!
    template <typename T>
    Arg(T                  const& v) : Arg(v, TreatAsString<typename std::decay<T>::type>{}) {}
    Arg(FormatSpec         const& v) : pvoid(&v) {}
    Arg(char const*        const& v) : pchar(v) {}
    Arg(char*              const& v) : pchar(v) {}
    Arg(std::nullptr_t     const& v) : pvoid(v) {}
    Arg(void const*        const& v) : pvoid(v) {}
    Arg(void*              const& v) : pvoid(v) {}
    Arg(bool               const& v) : bool_(v) {}
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
};

template <size_t N>
using ArgArray = typename std::conditional< N != 0, Arg[], Arg* >::type;

// XXX:
// Keep in sync with Arg::Arg() above!!!
template <typename T> struct SelectType                     { static const Type value = TreatAsString<T>::value ? Type::string : Type::other; };
template <>           struct SelectType<FormatSpec        > { static const Type value = Type::formatspec; };
template <>           struct SelectType<char const*       > { static const Type value = Type::pchar; };
template <>           struct SelectType<char*             > { static const Type value = Type::pchar; };
template <>           struct SelectType<std::nullptr_t    > { static const Type value = Type::pvoid; };
template <>           struct SelectType<void const*       > { static const Type value = Type::pvoid; };
template <>           struct SelectType<void*             > { static const Type value = Type::pvoid; };
template <>           struct SelectType<bool              > { static const Type value = Type::bool_; };
template <>           struct SelectType<char              > { static const Type value = Type::char_; };
template <>           struct SelectType<signed char       > { static const Type value = Type::schar; };
template <>           struct SelectType<signed short      > { static const Type value = Type::sshort; };
template <>           struct SelectType<signed int        > { static const Type value = Type::sint; };
#if LONG_MAX == INT_MAX
template <>           struct SelectType<signed long       > { static const Type value = Type::sint; };
#else
template <>           struct SelectType<signed long       > { static const Type value = Type::slonglong; };
#endif
template <>           struct SelectType<signed long long  > { static const Type value = Type::slonglong; };
template <>           struct SelectType<unsigned char     > { static const Type value = Type::ulonglong; };
template <>           struct SelectType<unsigned short    > { static const Type value = Type::ulonglong; };
template <>           struct SelectType<unsigned int      > { static const Type value = Type::ulonglong; };
template <>           struct SelectType<unsigned long     > { static const Type value = Type::ulonglong; };
template <>           struct SelectType<unsigned long long> { static const Type value = Type::ulonglong; };
template <>           struct SelectType<double            > { static const Type value = Type::double_; };
template <>           struct SelectType<float             > { static const Type value = Type::double_; };

template <typename T>
struct TypeFor : SelectType<typename std::decay<T>::type>
{
};

template <typename T>
struct IsSafeRValueType : std::integral_constant<
    bool,
    //
    // Do not allow to push rvalue references of these types into a FormatArg list.
    // The Arg class stores pointers to these arguments.
    //
    TypeFor<T>::value != Type::formatspec &&
    TypeFor<T>::value != Type::string &&
    TypeFor<T>::value != Type::other
    >
{
};

template <>
struct IsSafeRValueType<cxx::string_view> : std::true_type {};

struct Types
{
    using value_type = uint64_t;

    static constexpr int kBitsPerArg = 4;
    static constexpr int kMaxArgs    = CHAR_BIT * sizeof(value_type) / kBitsPerArg;
    static constexpr int kMaxTypes   = 1 << kBitsPerArg;
    static constexpr int kTypeMask   = kMaxTypes - 1;

    static_assert(static_cast<int>(Type::last) <= kMaxTypes, "Invalid value for kBitsPerArg");

    value_type /*const*/ types = 0;

    Types() = default;
    Types(Types const&) = default;

    template <typename Arg1, typename ...Args>
    explicit Types(Arg1 const& arg1, Args const&... args) : types(make_types(arg1, args...))
    {
    }

    Type operator[](int index) const {
        return (index >= 0 && index < kMaxArgs) ? get_type(index) : Type::none;
    }

    Type get_type(int index) const
    {
        assert(index >= 0);
        assert(index < kMaxArgs);
        auto const t = (types >> (kBitsPerArg * index)) & kTypeMask;
        assert(t < kMaxTypes);
        return static_cast<Type>(t);
    }

    void set_type(int index, Type type)
    {
        assert(index >= 0);
        assert(index < kMaxArgs);
        types |= static_cast<value_type>(type) << (kBitsPerArg * index);
    }

    static value_type make_types() { return 0; }

    template <typename A1, typename ...An>
    static value_type make_types(A1 const& /*a1*/, An const&... an)
    {
        static_assert(1 + sizeof...(An) <= kMaxArgs, "Too many arguments");
        return (make_types(an...) << kBitsPerArg) | static_cast<value_type>(TypeFor<A1>::value);
    }
};

FMTXX_API ErrorCode DoFormat(Writer& w, cxx::string_view format, Arg const* args, Types types);
FMTXX_API ErrorCode DoPrintf(Writer& w, cxx::string_view format, Arg const* args, Types types);

FMTXX_API ToCharsResult DoFormatToChars(char* first, char* last, cxx::string_view format, Arg const* args, Types types);
FMTXX_API ToCharsResult DoPrintfToChars(char* first, char* last, cxx::string_view format, Arg const* args, Types types);

} // namespace fmtxx::impl

class FormatArgs
{
public:
    static constexpr int kMaxArgs = impl::Types::kMaxArgs;

//private:
    impl::Arg   args_[kMaxArgs];
    impl::Types types_ = {};
    int         size_ = 0;

public:
    int size() const { return size_; }
    int max_size() const { return kMaxArgs; }

    // Add arguments to this list.
    // PRE: max_size() - size() >= sizeof...(Ts)
    template <typename ...Ts>
    void push_back(Ts&&... vals)
    {
        static_assert(sizeof...(Ts) >= 1, "Too few arguments");
        static_assert(sizeof...(Ts) <= kMaxArgs, "Too many arguments");

        assert(size_ + sizeof...(Ts) <= kMaxArgs);

        int const unused[] = { (push_back_(static_cast<Ts&&>(vals)), 0)... };
        static_cast<void>(unused);
    }

private:
    template <typename T>
    void push_back_(T&& val)
    {
        static_assert(
            std::is_lvalue_reference<T>::value || impl::IsSafeRValueType<typename std::decay<T>::type>::value,
            "Adding temporaries of non-built-in types to FormatArgs is not allowed. ");

        args_[size_] = static_cast<T&&>(val);
        types_.set_type(size_, impl::TypeFor<T>::value);
        ++size_;
    }
};

template <typename ...Args>
inline ErrorCode format(Writer& w, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFormat(w, format, arr, impl::Types{args...});
}

template <typename ...Args>
inline ErrorCode printf(Writer& w, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoPrintf(w, format, arr, impl::Types{args...});
}

inline ErrorCode format(Writer& w, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFormat(w, format, args.args_, args.types_);
}

inline ErrorCode printf(Writer& w, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoPrintf(w, format, args.args_, args.types_);
}

template <typename ...Args>
inline ToCharsResult format_to_chars(char* first, char* last, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFormatToChars(first, last, format, arr, impl::Types{args...});
}

template <typename ...Args>
inline ToCharsResult printf_to_chars(char* first, char* last, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoPrintfToChars(first, last, format, arr, impl::Types{args...});
}

inline ToCharsResult format_to_chars(char* first, char* last, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFormatToChars(first, last, format, args.args_, args.types_);
}

inline ToCharsResult printf_to_chars(char* first, char* last, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoPrintfToChars(first, last, format, args.args_, args.types_);
}

} // namespace fmtxx

#endif // FMTXX_FORMAT_CORE_H
