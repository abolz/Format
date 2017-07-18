#ifndef FMTXX_FORMAT_STRING_H
#define FMTXX_FORMAT_STRING_H 1

#include "Format_core.h"

#ifndef FMTXX_HAS_INCLUDE
#if defined(__has_include)
#define FMTXX_HAS_INCLUDE(X) __has_include(X)
#else
#define FMTXX_HAS_INCLUDE(X) 0
#endif
#endif

#ifndef FMTXX_HAS_STD_STRING_VIEW
#if (__cplusplus >= 201703 && FMTXX_HAS_INCLUDE(<string_view>)) || (_MSC_VER >= 1910 && _HAS_CXX17)
#define FMTXX_HAS_STD_STRING_VIEW 1
#endif
#endif

#ifndef FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
#if __cplusplus > 201103 && FMTXX_HAS_INCLUDE(<experimental/string_view>)
#define FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW 1
#endif
#endif

#include <string>
#if FMTXX_HAS_STD_STRING_VIEW
#include <string_view>
#endif
#if FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
#include <experimental/string_view>
#endif

namespace fmtxx {

#if FMTXX_HAS_STD_STRING_VIEW
template <>
struct TreatAsString< std::string_view >
    : std::true_type
{
};
#endif

#if FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
template <>
struct TreatAsString< std::experimental::string_view >
    : std::true_type
{
};
#endif

template <typename Alloc>
struct TreatAsString< std::basic_string<char, std::char_traits<char>, Alloc> >
    : std::true_type
{
};

namespace impl {

#if FMTXX_HAS_STD_STRING_VIEW
template <>
struct IsSafeRValueType<std::string_view> : std::true_type {};
#endif

#if FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
template <>
struct IsSafeRValueType<std::experimental::string_view> : std::true_type {};
#endif

FMTXX_API ErrorCode DoFormat(std::string& str, cxx::string_view format, Arg const* args, Types types);
FMTXX_API ErrorCode DoPrintf(std::string& str, cxx::string_view format, Arg const* args, Types types);

} // namespace impl

class StringWriter : public Writer
{
public:
    std::string& str;

    explicit StringWriter(std::string& s) : str(s) {}

private:
    FMTXX_API ErrorCode Put(char c) override;
    FMTXX_API ErrorCode Write(char const* ptr, size_t len) override;
    FMTXX_API ErrorCode Pad(char c, size_t count) override;
};

template <typename ...Args>
inline ErrorCode format(std::string& str, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFormat(str, format, arr, impl::Types{args...});
}

template <typename ...Args>
inline ErrorCode printf(std::string& str, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoPrintf(str, format, arr, impl::Types{args...});
}

inline ErrorCode format(std::string& str, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFormat(str, format, args.args_, args.types_);
}

inline ErrorCode printf(std::string& str, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoPrintf(str, format, args.args_, args.types_);
}

struct StringFormatResult
{
    std::string str;
    ErrorCode ec = ErrorCode::success;
};

template <typename ...Args>
inline StringFormatResult string_format(cxx::string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::format(r.str, format, args...);
    return r;
}

template <typename ...Args>
inline StringFormatResult string_printf(cxx::string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::printf(r.str, format, args...);
    return r;
}

inline StringFormatResult string_format(cxx::string_view format, FormatArgs const& args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::format(r.str, format, args);
    return r;
}

inline StringFormatResult string_printf(cxx::string_view format, FormatArgs const& args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::printf(r.str, format, args);
    return r;
}

} // namespace fmtxx

#endif
