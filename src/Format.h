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

#ifndef FMTXX_FORMAT_H
#define FMTXX_FORMAT_H 1

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class string_view // A minimal std::string_view replacement
{
public:
    using value_type      = char;
    using pointer         = char const*;
    using const_pointer   = char const*;
    using reference       = char const&;
    using const_reference = char const&;
    using iterator        = char const*;
    using const_iterator  = char const*;
    using size_type       = size_t;

private:
    const_pointer data_ = nullptr;
    size_t size_ = 0;

private:
    static size_t Min(size_t x, size_t y) { return y < x ? y : x; }
    static size_t Max(size_t x, size_t y) { return y < x ? x : y; }

    static int Compare(char const* s1, char const* s2, size_t n) noexcept {
        return n == 0 ? 0 : ::memcmp(s1, s2, n);
    }

    static char const* Find(char const* s, size_t n, char ch) noexcept {
        assert(n != 0);
        return static_cast<char const*>(::memchr(s, static_cast<unsigned char>(ch), n));
    }

public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    constexpr string_view() noexcept = default;
    constexpr string_view(string_view const&) noexcept = default;

    string_view(const_pointer ptr, size_t len) noexcept
        : data_(ptr)
        , size_(len)
    {
        assert(size_ == 0 || data_ != nullptr);
    }

    string_view(const_pointer c_str) noexcept
        : data_(c_str)
        , size_(c_str ? ::strlen(c_str) : 0u)
    {
    }

    template <
        typename String,
        typename DataT = decltype(std::declval<String const&>().data()),
        typename SizeT = decltype(std::declval<String const&>().size()),
        typename = typename std::enable_if<
            std::is_convertible<DataT, const_pointer>::value && std::is_convertible<SizeT, size_t>::value
        >::type
    >
    string_view(String const& str) noexcept
        : data_(str.data())
        , size_(str.size())
    {
        assert(size_ == 0 || data_ != nullptr);
    }

    template <
        typename T,
        typename = typename std::enable_if<
            std::is_constructible<T, const_iterator, const_iterator>::value
        >::type
    >
    explicit operator T() const
    {
        return T(begin(), end());
    }

    // Returns a pointer to the start of the string.
    constexpr const_pointer data() const noexcept { return data_; }

    // Returns the length of the string.
    constexpr size_t size() const noexcept { return size_; }

    // Returns whether the string is empty.
    constexpr bool empty() const noexcept { return size_ == 0; }

    // Returns an iterator pointing to the start of the string.
    constexpr const_iterator begin() const noexcept { return data_; }

    // Returns an iterator pointing past the end of the string.
    constexpr const_iterator end() const noexcept { return data_ + size_; }

    // Returns a reference to the N-th character of the string.
    const_reference operator[](size_t n) const noexcept {
        assert(n < size_);
        return data_[n];
    }
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

enum struct ErrorCode {
    success                 = 0,
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

struct FormatSpec
{
    string_view style; // XXX: Points into the format string. Only valid while formatting arguments...
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
class Writer
{
public:
    virtual ~Writer() noexcept;

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

// Write to std::FILE's, keeping track of the number of characters (successfully) transmitted.
class FILEWriter : public Writer
{
    std::FILE* const file_;
    size_t           size_ = 0;

public:
    explicit FILEWriter(std::FILE* v) : file_(v) {
        assert(file_ != nullptr);
    }

    // Returns the FILE stream.
    std::FILE* file() const { return file_; }

    // Returns the number of bytes successfully transmitted (since construction).
    size_t size() const { return size_; }

private:
    ErrorCode Put(char c) override;
    ErrorCode Write(char const* ptr, size_t len) override;
    ErrorCode Pad(char c, size_t count) override;
};

// Write to a user allocated buffer.
// If the buffer overflows, keep track of the number of characters that would
// have been written if the buffer were large enough. This is for compatibility
// with snprintf.
class ArrayWriter : public Writer
{
    char*  const buf_     = nullptr;
    size_t const bufsize_ = 0;
    size_t       size_    = 0;

public:
    ArrayWriter(char* buffer, size_t buffer_size) : buf_(buffer), bufsize_(buffer_size) {
        assert(bufsize_ == 0 || buf_ != nullptr);
    }

    template <size_t N>
    explicit ArrayWriter(char (&buf)[N]) : ArrayWriter(buf, N) {}

    ~ArrayWriter() { // null-terminate on destruction
        finish();
    }

    // Returns a pointer to the string.
    // The string is null-terminated if finish() has been called.
    char* data() const { return buf_; }

    // Returns the buffer capacity.
    size_t capacity() const { return bufsize_; }

    // Returns the length of the string.
    size_t size() const { return size_; }

    // Returns true if the buffer was too small.
    bool overflow() const { return size_ >= bufsize_; }

    // Returns the string.
    string_view view() const { return string_view(data(), size()); }

    // Null-terminate the buffer.
    // Returns the length of the string not including the null-character.
    size_t finish() noexcept;

private:
    ErrorCode Put(char c) override;
    ErrorCode Write(char const* ptr, size_t len) override;
    ErrorCode Pad(char c, size_t count) override;
};

// Returned by the format_to_chars/printf_to_chars function (below).
// Like std::to_chars.
struct ToCharsResult
{
    char*     next = nullptr;
    ErrorCode ec   = ErrorCode{};

    ToCharsResult() = default;
    ToCharsResult(char* next_, ErrorCode ec_) : next(next_), ec(ec_) {}

    // Test for successful conversions
    explicit operator bool() const { return ec == ErrorCode{}; }
};

// Returned by the string_format/string_printf functions (below).
struct StringFormatResult
{
    std::string str;
    ErrorCode ec = ErrorCode{};

    StringFormatResult() = default;
    StringFormatResult(std::string str_, ErrorCode ec_) : str(std::move(str_)), ec(ec_) {}

    // Test for successful conversion
    explicit operator bool() const { return ec == ErrorCode{}; }
};

struct Util
{
    // Note:
    // This function prints len characters, including '\0's.
    static ErrorCode format_string      (Writer& w, FormatSpec const& spec, char const* str, size_t len);
    // Note:
    // This is different from just calling format_string(str, strlen(str)):
    // This function handles nullptr's and if a precision is specified uses strnlen instead of strlen.
    static ErrorCode format_char_pointer(Writer& w, FormatSpec const& spec, char const* str);
    static ErrorCode format_int         (Writer& w, FormatSpec const& spec, int64_t sext, uint64_t zext);
    static ErrorCode format_bool        (Writer& w, FormatSpec const& spec, bool val);
    static ErrorCode format_char        (Writer& w, FormatSpec const& spec, char ch);
    static ErrorCode format_pointer     (Writer& w, FormatSpec const& spec, void const* pointer);
    static ErrorCode format_double      (Writer& w, FormatSpec const& spec, double x);

    template <typename T>
    static ErrorCode format_string(Writer& w, FormatSpec const& spec, T const& value)
    {
        return format_string(w, spec, value.data(), value.size());
    }

    template <typename T>
    static ErrorCode format_int(Writer& w, FormatSpec const& spec, T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        return format_int(w, spec, value, std::is_signed<T>{});
    }

private:
    template <typename T>
    static ErrorCode format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::true_type)
    {
        return format_int(w, spec, value, static_cast<typename std::make_unsigned<T>::type>(value));
    }

    template <typename T>
    static ErrorCode format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::false_type)
    {
        return format_int(w, spec, 0, value);
    }
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

template <typename T>
struct DefaultTreatAsString
    : std::false_type
{
};

template <>
struct DefaultTreatAsString<string_view>
    : std::true_type
{
};

template <typename Alloc>
struct DefaultTreatAsString<std::basic_string<char, std::char_traits<char>, Alloc>>
    : std::true_type
{
};

} // namespace fmtxx::impl

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

// Provides the member constant value equal to true if objects of type T should
// be treated as strings by the Format library.
// Objects of type T must have member functions data() and size() and their
// return values must be convertible to 'char const*' and 'size_t' resp.
template <typename T, typename /*Enable*/ = void>
struct TreatAsString
    : impl::DefaultTreatAsString<T>
{
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

template <typename>
struct AlwaysFalse
    : std::false_type
{
};

template <typename ...>
struct AlwaysVoid
{
    using type = void;
};

template <typename ...Ts>
using Void_t = typename AlwaysVoid<Ts...>::type;

enum struct Type : int {
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

template <Type Val>
using Type_t = std::integral_constant<Type, Val>;

template <typename T> struct SelectType                     : Type_t<TreatAsString<T>::value ? Type::string : Type::other> {};
template <>           struct SelectType<FormatSpec        > : Type_t<Type::formatspec> {};
template <>           struct SelectType<char const*       > : Type_t<Type::pchar> {};
template <>           struct SelectType<char*             > : Type_t<Type::pchar> {};
template <>           struct SelectType<std::nullptr_t    > : Type_t<Type::pvoid> {};
template <>           struct SelectType<void const*       > : Type_t<Type::pvoid> {};
template <>           struct SelectType<void*             > : Type_t<Type::pvoid> {};
template <>           struct SelectType<bool              > : Type_t<Type::bool_> {};
template <>           struct SelectType<char              > : Type_t<Type::char_> {};
template <>           struct SelectType<signed char       > : Type_t<Type::schar> {};
template <>           struct SelectType<signed short      > : Type_t<Type::sshort> {};
template <>           struct SelectType<signed int        > : Type_t<Type::sint> {};
#if LONG_MAX == INT_MAX
template <>           struct SelectType<signed long       > : Type_t<Type::sint> {};
#else
template <>           struct SelectType<signed long       > : Type_t<Type::slonglong> {};
#endif
template <>           struct SelectType<signed long long  > : Type_t<Type::slonglong> {};
template <>           struct SelectType<unsigned char     > : Type_t<Type::ulonglong> {};
template <>           struct SelectType<unsigned short    > : Type_t<Type::ulonglong> {};
template <>           struct SelectType<unsigned int      > : Type_t<Type::ulonglong> {};
template <>           struct SelectType<unsigned long     > : Type_t<Type::ulonglong> {};
template <>           struct SelectType<unsigned long long> : Type_t<Type::ulonglong> {};
template <>           struct SelectType<double            > : Type_t<Type::double_> {};
template <>           struct SelectType<float             > : Type_t<Type::double_> {};

template <typename T, Type Val = SelectType<T>::value>
struct SelectType_checked : Type_t<Val>
{
};

template <typename T>
struct SelectType_checked<T, Type::other> : Type_t<Type::other>
{
    // FIXME?!?!
    //
    // The next three assertions are not really required. These types could be
    // supported (and indeed are supported by std::ostream - more or less) by
    // providing a specicialization of FormatValue<T>.
    //
    // But this does not work in the current implementation: At the where
    // FormatValue<T> is instantiated, these conditions have already been
    // checked.

    static_assert(
        !std::is_function<T>::value,
        "Formatting function types is not supported.");         // ostream: as void* (or stream manipulator)
    static_assert(
        !std::is_pointer<T>::value,
        "Formatting non-void pointer types is not supported."); // ostream: as void*
    static_assert(
        !std::is_member_pointer<T>::value,
        "Formatting member pointer types is not supported.");   // ostream: as bool...
};

// Test if a specialization TreatAsString<T> is valid.
template <typename T, typename = void>
struct MayTreatAsString
    : std::false_type
{
};

template <typename T>
struct MayTreatAsString<T, Void_t< decltype(std::declval<T const&>().data(), std::declval<T const&>().size()) >>
    : std::integral_constant<
        bool,
        std::is_convertible< decltype(std::declval<T const&>().data()), char const* >::value &&
        std::is_convertible< decltype(std::declval<T const&>().size()), size_t >::value
      >
{
};

template <typename T>
struct SelectType_checked<T, Type::string> : Type_t<Type::string>
{
    static_assert(
        MayTreatAsString<T>::value,
        "TreatAsString is specialized for T, but the specialization is invalid. "
        "Note: T must provide member functions data() and size() and their return types must be "
        "convertible to 'char const*' and 'size_t' resp. Implement FormatValue<T> instead.");
};

template <typename T>
using TypeFor = typename SelectType_checked<typename std::decay<T>::type>::type;

template <typename T, Type Val = TypeFor<T>::value>
struct DefaultFormatValue
{
    static_assert(
        AlwaysFalse<T>::value,
        "Formatting objects of type T is not supported. "
        "Specialize FormatValue<T> or TreatAsString<T>, or implement operator<<(std::ostream&, T const&) "
        "and include Format_ostream.h.");
};

template <typename T>
struct DefaultFormatValue<T, Type::string>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return Util::format_string(w, spec, val.data(), val.size());
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::pchar>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, char const* val) const {
        return Util::format_char_pointer(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::pvoid>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, void const* val) const {
        return Util::format_pointer(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::bool_>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, bool val) const {
        return Util::format_bool(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::char_>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, char val) const {
        return Util::format_char(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::schar>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed char val) const {
        return Util::format_int(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::sshort>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed short val) const {
        return Util::format_int(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::sint>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed int val) const {
        return Util::format_int(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::slonglong>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, signed long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::ulonglong>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, unsigned long long val) const {
        return Util::format_int(w, spec, val);
    }
};

template <typename T>
struct DefaultFormatValue<T, Type::double_>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, double val) const {
        return Util::format_double(w, spec, val);
    }
};

} // namespace fmtxx::impl

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename T = void, typename /*Enable*/ = void>
struct FormatValue
    : impl::DefaultFormatValue<T>
{
};

template <>
struct FormatValue<void>
{
    template <typename T>
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return FormatValue<typename std::decay<T>::type>{}(w, spec, val);
    }
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

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

    template <typename T> Arg(T const& v, Type_t<Type::formatspec>) : pvoid(&v) {}
    template <typename T> Arg(T const& v, Type_t<Type::string>    ) : string{v.data(), v.size()}  {}
    template <typename T> Arg(T const& v, Type_t<Type::other>     ) : other{&v, &FormatValue_fn<T>} {}
    template <typename T> Arg(T const& v, Type_t<Type::pchar>     ) : pchar(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::pvoid>     ) : pvoid(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::bool_>     ) : bool_(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::char_>     ) : char_(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::schar>     ) : schar(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::sshort>    ) : sshort(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::sint>      ) : sint(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::slonglong> ) : slonglong(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::ulonglong> ) : ulonglong(v) {}
    template <typename T> Arg(T const& v, Type_t<Type::double_>   ) : double_(v) {}

    Arg() {}//= default;

    template <typename T>
    /*implicit*/ Arg(T const& v) : Arg(v, TypeFor<T>{})
    {
    }
};

template <typename ...Args>
using ArgArray = typename std::conditional< sizeof...(Args) == 0, impl::Arg const*, impl::Arg const [sizeof...(Args)] >::type;

struct Types
{
    using value_type = uint64_t;

    static constexpr int kBitsPerArg = 4; // Should be computed from Type::last...
    static constexpr int kMaxArgs    = CHAR_BIT * sizeof(value_type) / kBitsPerArg;
    static constexpr int kMaxTypes   = 1 << kBitsPerArg;
    static constexpr int kTypeMask   = kMaxTypes - 1;

    static_assert(static_cast<int>(Type::none) == 0,
        "Internal error: Type::none must be 0");
    static_assert(static_cast<int>(Type::last) <= kMaxTypes,
        "Internal error: Value of kBitsPerArg too small");
    static_assert(static_cast<int>(Type::last) > (1 << (kBitsPerArg - 1)),
        "Internal error: Value of kBitsPerArg too large");

    value_type const types = 0;

    constexpr Types() = default;
    constexpr Types(value_type t) : types(t) {}

    Type operator[](int index) const
    {
        if (index < 0 || index >= kMaxArgs)
            return Type::none;

        return static_cast<Type>((types >> (kBitsPerArg * index)) & kTypeMask);
    }
};

template <typename ...Ts>
struct MakeTypes;

template <>
struct MakeTypes<>
{
    static constexpr const Types::value_type value = 0;
};

template <typename T, typename ...Ts>
struct MakeTypes<T, Ts...>
{
    static_assert(1 + sizeof...(Ts) <= Types::kMaxArgs, "Too many arguments");

    static constexpr const Types::value_type value
        = static_cast<Types::value_type>(TypeFor<T>::value) | (MakeTypes<Ts...>::value << Types::kBitsPerArg);
};

ErrorCode DoFormat(Writer&      w,    string_view format, Arg const* args, Types types);
ErrorCode DoPrintf(Writer&      w,    string_view format, Arg const* args, Types types);
ErrorCode DoFormat(std::FILE*   file, string_view format, Arg const* args, Types types);
ErrorCode DoPrintf(std::FILE*   file, string_view format, Arg const* args, Types types);
ErrorCode DoFormat(std::string& str,  string_view format, Arg const* args, Types types);
ErrorCode DoPrintf(std::string& str,  string_view format, Arg const* args, Types types);

ToCharsResult DoFormatToChars(char* first, char* last, string_view format, Arg const* args, Types types);
ToCharsResult DoPrintfToChars(char* first, char* last, string_view format, Arg const* args, Types types);

// fprintf compatible formatting functions.
int DoFileFormat(std::FILE* file, string_view format, Arg const* args, Types types);
int DoFilePrintf(std::FILE* file, string_view format, Arg const* args, Types types);

// snprintf compatible formatting functions.
int DoArrayFormat(char* buf, size_t bufsize, string_view format, Arg const* args, Types types);
int DoArrayPrintf(char* buf, size_t bufsize, string_view format, Arg const* args, Types types);

} // namespace fmtxx::impl

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename ...Args>
ErrorCode format(Writer& w, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormat(w, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ErrorCode printf(Writer& w, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintf(w, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ErrorCode format(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormat(file, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ErrorCode printf(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintf(file, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ErrorCode format(std::string& str, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormat(str, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ErrorCode printf(std::string& str, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintf(str, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
StringFormatResult string_format(string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = fmtxx::format(r.str, format, args...);
    return r;
}

template <typename ...Args>
StringFormatResult string_printf(string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = fmtxx::printf(r.str, format, args...);
    return r;
}

template <typename ...Args>
ToCharsResult format_to_chars(char* first, char* last, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormatToChars(first, last, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
ToCharsResult printf_to_chars(char* first, char* last, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintfToChars(first, last, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
int fformat(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFileFormat(file, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
int fprintf(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFilePrintf(file, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
int snformat(char* buf, size_t bufsize, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoArrayFormat(buf, bufsize, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <typename ...Args>
int snprintf(char* buf, size_t bufsize, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoArrayPrintf(buf, bufsize, format, arr, fmtxx::impl::MakeTypes<Args...>::value);
}

template <size_t N, typename ...Args>
int snformat(char (&buf)[N], string_view format, Args const&... args)
{
    return fmtxx::snformat(&buf[0], N, format, args...);
}

template <size_t N, typename ...Args>
int snprintf(char (&buf)[N], string_view format, Args const&... args)
{
    return fmtxx::snprintf(&buf[0], N, format, args...);
}

} // namespace fmtxx

#endif // FMTXX_FORMAT_H
