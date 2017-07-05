#pragma once

#include "Format.h"

#ifndef FMTXX_HAS_INCLUDE
#ifdef __has_include
#define FMTXX_HAS_INCLUDE(X) __has_include(X)
#else
#define FMTXX_HAS_INCLUDE(X) 0
#endif
#endif

#if (_MSC_VER >= 1910 && _HAS_CXX17) || __cplusplus >= 201703
#define FMTXX_HAS_STD_STRING_VIEW 1
#elif __cplusplus > 201103 && FMTXX_HAS_INCLUDE(<experimental/string_view>)
#define FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW 1
#endif

#include <string>
#if FMTXX_HAS_STD_STRING_VIEW
#include <string_view>
#elif FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
#include <experimental/string_view>
#endif

namespace fmtxx {

template <typename Alloc>
struct TreatAsString< std::basic_string<char, std::char_traits<char>, Alloc> >
    : std::true_type
{
};

#if FMTXX_HAS_STD_STRING_VIEW
template <>
struct TreatAsString< std::string_view >
    : std::true_type
{
};
#elif FMTXX_HAS_STD_EXPERIMENTAL_STRING_VIEW
template <>
struct TreatAsString< std::experimental::string_view >
    : std::true_type
{
};
#endif

class StringWriter : public Writer
{
public:
    std::string& os;

    explicit StringWriter(std::string& v) : os(v) {}

private:
    errc Put(char c) override;
    errc Write(char const* str, size_t len) override;
    errc Pad(char c, size_t count) override;
};

inline errc StringWriter::Put(char c)
{
    os.push_back(c);
    return errc::success;
}

inline errc StringWriter::Write(char const* str, size_t len)
{
    os.append(str, len);
    return errc::success;
}

inline errc StringWriter::Pad(char c, size_t count)
{
    os.append(count, c);
    return errc::success;
}

template <typename ...Args>
inline errc format(std::string& str, StringView format, Args const&... args)
{
    StringWriter w{str};
    return fmtxx::format(w, format, args...);
}

template <typename ...Args>
inline errc printf(std::string& str, StringView format, Args const&... args)
{
    StringWriter w{str};
    return fmtxx::printf(w, format, args...);
}

inline errc format(std::string& str, StringView format, FormattingArgs const& args)
{
    StringWriter w{str};
    return fmtxx::format(w, format, args);
}

inline errc printf(std::string& str, StringView format, FormattingArgs const& args)
{
    StringWriter w{str};
    return fmtxx::printf(w, format, args);
}

struct StringFormatResult
{
    std::string str;
    errc ec = errc::success;
};

template <typename ...Args>
inline StringFormatResult string_format(StringView format, Args const&... args)
{
    StringFormatResult r;
    r.ec = fmtxx::format(r.str, format, args...);
    return r;
}

template <typename ...Args>
inline StringFormatResult string_printf(StringView format, Args const&... args)
{
    StringFormatResult r;
    r.ec = fmtxx::printf(r.str, format, args...);
    return r;
}

inline StringFormatResult string_format(StringView format, FormattingArgs const& args)
{
    StringFormatResult r;
    r.ec = fmtxx::format(r.str, format, args);
    return r;
}

inline StringFormatResult string_printf(StringView format, FormattingArgs const& args)
{
    StringFormatResult r;
    r.ec = fmtxx::printf(r.str, format, args);
    return r;
}

} // namespace fmtxx
