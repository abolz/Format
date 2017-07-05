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

class string_view // A minimal replacement for std::string_view
{
    char const* data_ = nullptr;
    size_t      size_ = 0;

public:
    using pointer  = char const*;
    using iterator = char const*;

public:
    string_view() = default;
    string_view(pointer p, size_t len) : data_(p), size_(len) {}

    // Construct from null-terminated C-string.
    string_view(pointer c_str)
        : data_(c_str)
        , size_(c_str ? ::strlen(c_str) : 0u)
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
    string_view(T const& str)
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

inline bool operator==(string_view lhs, string_view rhs) {
    return lhs.size() == rhs.size() && ::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool operator!=(string_view lhs, string_view rhs) {
    return !(lhs == rhs);
}

inline bool operator<(string_view lhs, string_view rhs) {
    int c = ::strncmp(lhs.data(), rhs.data(), lhs.size() < rhs.size() ? lhs.size() : rhs.size());
    return c < 0 || (c == 0 && lhs.size() < rhs.size());
}

inline bool operator>(string_view lhs, string_view rhs) {
    return rhs < lhs;
}

inline bool operator<=(string_view lhs, string_view rhs) {
    return !(rhs < lhs);
}

inline bool operator>=(string_view lhs, string_view rhs) {
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
    string_view style; // XXX: Points into the format string. Only valid while formatting arguments...
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

    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
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

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;

    // Returns the FILE stream.
    std::FILE* file() const { return file_; }

    // Returns the number of bytes successfully transmitted (since construction).
    size_t size() const { return size_; }
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

    FMTXX_API bool Put(char c) noexcept override;
    FMTXX_API bool Write(char const* str, size_t len) noexcept override;
    FMTXX_API bool Pad(char c, size_t count) noexcept override;

    // Returns a pointer to the string.
    // The string is null-terminated if Finish() has been called.
    char* data() const { return buf_; }

    // Returns the buffer capacity.
    size_t capacity() const { return bufsize_; }

    // Returns the length of the string.
    size_t size() const { return size_; }

    // Returns the string.
    string_view view() const { return string_view(data(), size()); }

    // Null-terminate the buffer.
    // Returns the length of the string not including the null-character.
    FMTXX_API size_t Finish() noexcept;
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
struct TreatAsString< string_view > : std::true_type {};

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
        T_DOUBLE,     // 14 includes 'float' and 'long double'
        T_FORMATSPEC, // 15
    };

    value_type const types = 0;

    Types() = default;
    Types(Types const&) = default;

    template <typename Arg1, typename ...Args>
    explicit Types(Arg1 const& arg1, Args const&... args) : types(Make(arg1, args...))
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

    template <typename T>
    Arg(T const& v, /*TreatAsString*/ std::false_type) : other{ &v, &FormatValue_fn<T> } {}

    template <typename T>
    Arg(T const& v, /*TreatAsString*/ std::true_type) : string{v.data(), v.size()} {}

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

FMTXX_API errc DoFormat      (Writer& w,                 string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf      (Writer& w,                 string_view format, Types types, Arg const* args);
FMTXX_API errc DoFormat      (std::FILE* file,           string_view format, Types types, Arg const* args);
FMTXX_API errc DoPrintf      (std::FILE* file,           string_view format, Types types, Arg const* args);
FMTXX_API int  DoFileFormat  (std::FILE* file,           string_view format, Types types, Arg const* args);
FMTXX_API int  DoFilePrintf  (std::FILE* file,           string_view format, Types types, Arg const* args);
FMTXX_API int  DoArrayFormat (char* buf, size_t bufsize, string_view format, Types types, Arg const* args);
FMTXX_API int  DoArrayPrintf (char* buf, size_t bufsize, string_view format, Types types, Arg const* args);

// HACK
template <typename ...Args>
using ArgArray = typename std::conditional<sizeof...(Args) != 0, Arg[], Arg*>::type;

} // namespace impl

template <typename ...Args>
inline errc format(Writer& w, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormat(w, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline errc printf(Writer& w, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintf(w, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline errc format(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFormat(file, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline errc printf(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoPrintf(file, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline int fformat(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFileFormat(file, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline int fprintf(std::FILE* file, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoFilePrintf(file, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline int snformat(char* buf, size_t bufsize, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoArrayFormat(buf, bufsize, format, fmtxx::impl::Types{args...}, arr);
}

template <typename ...Args>
inline int snprintf(char* buf, size_t bufsize, string_view format, Args const&... args)
{
    fmtxx::impl::ArgArray<Args...> arr = {args...};
    return fmtxx::impl::DoArrayPrintf(buf, bufsize, format, fmtxx::impl::Types{args...}, arr);
}

template <size_t N, typename ...Args>
inline int snformat(char (&buf)[N], string_view format, Args const&... args)
{
    return fmtxx::snformat(&buf[0], N, format, args...);
}

template <size_t N, typename ...Args>
inline int snprintf(char (&buf)[N], string_view format, Args const&... args)
{
    return fmtxx::snprintf(&buf[0], N, format, args...);
}

} // namespace fmtxx
