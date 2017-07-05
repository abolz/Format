#pragma once

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

class StringView // A minimal replacement for std::string_view
{
    char const* data_ = nullptr;
    size_t      size_ = 0;

public:
    using pointer  = char const*;
    using iterator = char const*;

public:
    StringView() = default;
    StringView(pointer p, size_t len) : data_(p), size_(len) {}

    // Construct from null-terminated C-string.
    StringView(pointer c_str)
        : data_(c_str)
        , size_(c_str ? ::strlen(c_str) : 0u)
    {
    }

    // Construct from iterator range.
    StringView(iterator first, iterator last)
        : data_(first)
        , size_(static_cast<size_t>(last - first))
    {
    }

    // Construct from contiguous character sequence
    template <
        typename T,
        typename DataT = decltype(std::declval<T const&>().data()),
        typename SizeT = decltype(std::declval<T const&>().size()),
        typename = typename std::enable_if<
            std::is_convertible<DataT, pointer>::value &&
            std::is_convertible<SizeT, size_t>::value
        >::type
    >
    StringView(T const& str)
        : data_(str.data())
        , size_(str.size())
    {
    }

    // Convert to string types.
    template <
        typename T,
        typename = typename std::enable_if<
            std::is_constructible<T, pointer, size_t>::value
        >::type
    >
    explicit operator T() const { return T(data(), size()); }

    // Returns a pointer to the start of the string.
    // NOTE: Not neccessarily null-terminated!
    pointer data() const { return data_; }

    // Returns the length of the string.
    size_t size() const { return size_; }

    // Returns whether the string is empty.
    bool empty() const { return size_ == 0; }

    // Returns an iterator pointing to the start of the string.
    iterator begin() const { return data_; }

    // Returns an iterator pointing past the end of the string.
    iterator end() const { return data_ + size_; }
};

inline bool operator==(StringView lhs, StringView rhs) {
    return lhs.size() == rhs.size() && ::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool operator!=(StringView lhs, StringView rhs) {
    return !(lhs == rhs);
}

inline bool operator<(StringView lhs, StringView rhs) {
    int c = ::strncmp(lhs.data(), rhs.data(), lhs.size() < rhs.size() ? lhs.size() : rhs.size());
    return c < 0 || (c == 0 && lhs.size() < rhs.size());
}

inline bool operator>(StringView lhs, StringView rhs) {
    return rhs < lhs;
}

inline bool operator<=(StringView lhs, StringView rhs) {
    return !(rhs < lhs);
}

inline bool operator>=(StringView lhs, StringView rhs) {
    return !(lhs < rhs);
}

enum struct errc {
    success = 0,
    conversion_error,       // Value could not be converted to string. (E.g.: trying to format a non-existant date.)
    index_out_of_range,     // Argument index out of range
    invalid_argument,
    invalid_format_string,
    io_error,               // Writer failed. (XXX: Writer::Put() etc should probably return an error code?!)
    not_supported,          // Conversion not supported
    value_out_of_range,     // Value of integer argument out of range [INT_MIN, INT_MAX]
};

// Wraps an error code, may be checked for failure.
// Replaces err::operator bool() in most cases (and is more explicit).
struct Failed
{
    const errc ec = errc::success;

    Failed() = default;
    Failed(errc ec) : ec(ec) {}
    operator errc() const { return ec; }
    explicit operator bool() const { return ec != errc::success; }
};

//struct Succeeded
//{
//    const errc ec = errc::success;
//
//    Succeeded() = default;
//    Succeeded(errc ec) : ec(ec) {}
//    operator errc() const { return ec; }
//    explicit operator bool() const { return ec == errc::success; }
//};

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
    StringView style; // XXX: Points into the format string. Only valid while formatting arguments...
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
    FMTXX_API virtual ~Writer() noexcept;

    errc put(char c) {
        return Put(c);
    }

    errc put_nonzero(char c) {
        return c == '\0' ? errc::success : Put(c);
    }

    errc write(char const* str, size_t len) {
        return len == 0 ? errc::success : Write(str, len);
    }

    errc pad(char c, size_t count) {
        return count == 0 ? errc::success : Pad(c, count);
    }

private:
    virtual errc Put(char c) = 0;
    virtual errc Write(char const* str, size_t len) = 0;
    virtual errc Pad(char c, size_t count) = 0;
};

class FMTXX_VISIBILITY_DEFAULT FILEWriter : public Writer
{
    std::FILE* file_;
    size_t     size_ = 0;

public:
    explicit FILEWriter(std::FILE* v) : file_(v)
    {
        assert(file_ != nullptr);
    }

    // Returns the FILE stream.
    std::FILE* file() const { return file_; }

    // Returns the number of bytes successfully transmitted (since construction).
    size_t size() const { return size_; }

private:
    FMTXX_API errc Put(char c) noexcept override;
    FMTXX_API errc Write(char const* str, size_t len) noexcept override;
    FMTXX_API errc Pad(char c, size_t count) noexcept override;
};

class FMTXX_VISIBILITY_DEFAULT ArrayWriter : public Writer
{
    char*  const buf_     = nullptr;
    size_t const bufsize_ = 0;
    size_t       size_    = 0;

public:
    ArrayWriter(char* buffer, size_t buffer_size) : buf_(buffer), bufsize_(buffer_size)
    {
        assert(bufsize_ == 0 || buf_ != nullptr);
    }

    template <size_t N>
    explicit ArrayWriter(char (&buf)[N]) : ArrayWriter(buf, N) {}

    // Returns a pointer to the string.
    // The string is null-terminated if Finish() has been called.
    char* data() const { return buf_; }

    // Returns the buffer capacity.
    size_t capacity() const { return bufsize_; }

    // Returns the length of the string.
    size_t size() const { return size_; }

    // Returns the string.
    StringView view() const { return StringView(data(), size()); }

    // Null-terminate the buffer.
    // Returns the length of the string not including the null-character.
    FMTXX_API size_t Finish() noexcept;

private:
    FMTXX_API errc Put(char c) noexcept override;
    FMTXX_API errc Write(char const* str, size_t len) noexcept override;
    FMTXX_API errc Pad(char c, size_t count) noexcept override;
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
        return format_int(w, spec, value, static_cast<typename std::make_unsigned<T>::type>(value));
    }

    template <typename T>
    static inline errc format_int(Writer& w, FormatSpec const& spec, T value, /*is_signed*/ std::false_type) {
        return format_int(w, spec, 0, value);
    }
};

//
// Provides the member constant value equal to true if objects of type T should
// be treated as strings by the Format library.
// Objects of type T must have member functions data() and size() and their
// return values must be convertible to 'char const*' and 'size_t' resp.
//
template <typename T>
struct TreatAsString : std::false_type {};

template <>
struct TreatAsString< StringView > : std::true_type {};

namespace impl
{
    // The second template parameter is used in Format_ostream.h to "specialize"
    // this class template for all T's.
    template <typename T, typename = void>
    struct StreamValue
    {
        static_assert(sizeof(T) == 0,
            "Formatting objects of type T is not natively supported. "
            "Specialize FormatValue or TreatAsString, or, if objects of type T "
            "should be formatted using operator<<(std::ostream, ...), include "
            "'Format_ostream.h'");
    };
}

//
// Specialize this to format user-defined types.
//
template <typename T = void, typename /*XXX Internal. Do not use. XXX*/ = void>
struct FormatValue : impl::StreamValue<T>
{
};

template <typename T>
struct FormatValue<T, typename std::enable_if< TreatAsString<T>::value >::type>
{
    errc operator()(Writer& w, FormatSpec const& spec, T const& val) const {
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
struct FormatValue<bool> {
    errc operator()(Writer& w, FormatSpec const& spec, bool val) const {
        return Util::format_bool(w, spec, val);
    }
};

template <>
struct FormatValue<char> {
    errc operator()(Writer& w, FormatSpec const& spec, char val) const {
        return Util::format_char(w, spec, val);
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
struct FormatValue<void>
{
    template <typename T>
    errc operator()(Writer& w, FormatSpec const& spec, T const& val) const {
        return FormatValue<typename std::decay<T>::type>{}(w, spec, val);
    }
};

template <
    typename WriterT,
    typename T,
    typename = typename std::enable_if< std::is_base_of<Writer, typename std::remove_reference<WriterT>::type>::value >::type
>
errc format_value(WriterT&& w, FormatSpec const& spec, T const& value) {
    return FormatValue<>{}(w, spec, value);
}

namespace impl {

class Types
{
public:
    using value_type = uintptr_t; // uint64_t; // uint32_t;

    static const int kBitsPerArg = 4;
    static const int kMaxArgs    = CHAR_BIT * sizeof(value_type) / kBitsPerArg;
    static const int kMaxTypes   = 1 << kBitsPerArg;
    static const int kTypeMask   = kMaxTypes - 1;

    enum EType {
        T_NONE,
        T_FORMATSPEC,
        T_OTHER,
        T_BOOL,
        T_STRING,
        T_PVOID,
        T_PCHAR,
        T_CHAR,
        T_SCHAR,  // XXX: promote to int?
        T_SSHORT, // XXX: promote to int?
        T_SINT,
        T_SLONGLONG,
        T_ULONGLONG,
        T_DOUBLE, // includes 'float'

        T_LAST,
    };
    static_assert(T_LAST <= kMaxTypes, "invalid value for kBitsPerArg");

    value_type const types = 0;

    Types() = default;
    Types(Types const&) = default;

    template <typename Arg1, typename ...Args>
    explicit Types(Arg1 const& arg1, Args const&... args) : types(Make(arg1, args...))
    {
    }

    EType operator[](int index) const
    {
        if (index < 0 || index >= kMaxArgs)
            return T_NONE;
        return static_cast<EType>((types >> (kBitsPerArg * index)) & kTypeMask);
    }

private:
    // XXX:
    // Keep in sync with Arg::Arg() below!!!
    template <typename T>
    static unsigned GetId(T                  const&) { return TreatAsString<T>::value ? T_STRING : T_OTHER; }
    static unsigned GetId(bool               const&) { return T_BOOL; }
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
        static_assert(1 + sizeof...(An) <= kMaxArgs, "too many arguments");
        return (Make(an...) << kBitsPerArg) | GetId(a1);
    }
};

class Arg
{
public:
    using Func = errc (*)(Writer& w, FormatSpec const& spec, void const* value);

    template <typename T>
    static errc FormatValue_fn(Writer& w, FormatSpec const& spec, void const* value)
    {
        return fmtxx::format_value(w, spec, *static_cast<T const*>(value));
    }

    struct Other { void const* value; Func func; };
    struct String { char const* data; size_t size; };

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

    template <typename T> Arg(T const& v, /*TreatAsString*/ std::false_type) : other{ &v, &FormatValue_fn<T> } {}
    template <typename T> Arg(T const& v, /*TreatAsString*/ std::true_type) : string{v.data(), v.size()} {}

    // XXX:
    // Keep in sync with Types::GetId() above!!!
    template <typename T>
    Arg(T                  const& v) : Arg(v, TreatAsString<T>{}) {}
    Arg(bool               const& v) : bool_(v) {}
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

template <typename ...Args>
struct PackedArgs
{
    Arg const arr[sizeof...(Args)];
    Types const types;

    PackedArgs(Args const&... args) : arr{args...}, types{args...} {}
};

template <>
struct PackedArgs<>
{
    Arg const* const arr = nullptr;
    Types const types = {};

    PackedArgs() = default;
};

template <typename ...Args>
PackedArgs<Args...> pack_args(Args const&... args)
{
    return PackedArgs<Args...>(args...);
}

FMTXX_API errc DoFormat      (Writer& w,                 StringView format, Arg const* args, Types types);
FMTXX_API errc DoPrintf      (Writer& w,                 StringView format, Arg const* args, Types types);
FMTXX_API errc DoFormat      (std::FILE* file,           StringView format, Arg const* args, Types types);
FMTXX_API errc DoPrintf      (std::FILE* file,           StringView format, Arg const* args, Types types);
FMTXX_API int  DoFileFormat  (std::FILE* file,           StringView format, Arg const* args, Types types);
FMTXX_API int  DoFilePrintf  (std::FILE* file,           StringView format, Arg const* args, Types types);
FMTXX_API int  DoArrayFormat (char* buf, size_t bufsize, StringView format, Arg const* args, Types types);
FMTXX_API int  DoArrayPrintf (char* buf, size_t bufsize, StringView format, Arg const* args, Types types);

} // namespace impl

template <typename ...Args>
inline errc format(Writer& w, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoFormat(w, format, pa.arr, pa.types);
}

template <typename ...Args>
inline errc printf(Writer& w, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoPrintf(w, format, pa.arr, pa.types);
}

template <typename ...Args>
inline errc format(std::FILE* file, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoFormat(file, format, pa.arr, pa.types);
}

template <typename ...Args>
inline errc printf(std::FILE* file, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoPrintf(file, format, pa.arr, pa.types);
}

template <typename ...Args>
inline int fformat(std::FILE* file, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoFileFormat(file, format, pa.arr, pa.types);
}

template <typename ...Args>
inline int fprintf(std::FILE* file, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoFilePrintf(file, format, pa.arr, pa.types);
}

template <typename ...Args>
inline int snformat(char* buf, size_t bufsize, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoArrayFormat(buf, bufsize, format, pa.arr, pa.types);
}

template <typename ...Args>
inline int snprintf(char* buf, size_t bufsize, StringView format, Args const&... args)
{
    auto const pa = fmtxx::impl::pack_args(args...);
    return fmtxx::impl::DoArrayPrintf(buf, bufsize, format, pa.arr, pa.types);
}

template <size_t N, typename ...Args>
inline int snformat(char (&buf)[N], StringView format, Args const&... args)
{
    return fmtxx::snformat(&buf[0], N, format, args...);
}

template <size_t N, typename ...Args>
inline int snprintf(char (&buf)[N], StringView format, Args const&... args)
{
    return fmtxx::snprintf(&buf[0], N, format, args...);
}

} // namespace fmtxx
