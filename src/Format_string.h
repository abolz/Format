#pragma once

#include "Format.h"

#include <string>
#if !FMTXX_USE_STD_STRING_VIEW
#if _MSC_VER || __cplusplus >= 201703
#  include <string_view>
#else
#  include <experimental/string_view>
   namespace std { using std::experimental::string_view; }
#endif
#endif

namespace fmtxx {

template <typename Alloc>
struct TreatAsString< std::basic_string<char, std::char_traits<char>, Alloc> >
    : std::true_type
{
};

#if !FMTXX_USE_STD_STRING_VIEW
template <>
struct TreatAsString< std::string_view >
    : std::true_type
{
};
#endif

class StringWriter : public Writer
{
public:
    std::string& os;

    explicit StringWriter(std::string& v) : os(v) {}

    bool Put(char c) override;
    bool Write(char const* str, size_t len) override;
    bool Pad(char c, size_t count) override;
};

inline bool StringWriter::Put(char c)
{
    os.push_back(c);
    return true;
}

inline bool StringWriter::Write(char const* str, size_t len)
{
    os.append(str, len);
    return true;
}

inline bool StringWriter::Pad(char c, size_t count)
{
    os.append(count, c);
    return true;
}

template <typename ...Args>
errc format(std::string& str, string_view format, Args const&... args)
{
    StringWriter w{str};
    return fmtxx::format(w, format, args...);
}

template <typename ...Args>
errc printf(std::string& str, string_view format, Args const&... args)
{
    StringWriter w{str};
    return fmtxx::printf(w, format, args...);
}

struct StringFormatResult
{
    std::string str;
    errc ec = errc::success;
};

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

} // namespace fmtxx
