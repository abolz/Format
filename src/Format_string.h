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

#ifndef FMTXX_FORMAT_STRING_H
#define FMTXX_FORMAT_STRING_H 1

#include "Format_core.h"

#include <string>

namespace fmtxx {

namespace impl {

template <typename Alloc>
struct DefaultTreatAsString<std::basic_string<char, std::char_traits<char>, Alloc>>
    : std::true_type
{
};

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
ErrorCode format(std::string& str, cxx::string_view format, ArgPack<Args...> const& args)
{
    return ::fmtxx::impl::DoFormat(str, format, args.array(), args.types());
}

template <typename ...Args>
ErrorCode printf(std::string& str, cxx::string_view format, ArgPack<Args...> const& args)
{
    return ::fmtxx::impl::DoPrintf(str, format, args.array(), args.types());
}

template <typename ...Args>
ErrorCode format(std::string& str, cxx::string_view format, Args const&... args)
{
    return ::fmtxx::format(str, format, ArgPack<Args...>(args...));
}

template <typename ...Args>
ErrorCode printf(std::string& str, cxx::string_view format, Args const&... args)
{
    return ::fmtxx::printf(str, format, ArgPack<Args...>(args...));
}

struct StringFormatResult
{
    std::string str;
    ErrorCode ec = ErrorCode{};

    StringFormatResult() = default;
    StringFormatResult(std::string str_, ErrorCode ec_) : str(std::move(str_)), ec(ec_) {}

    // Test for successful conversion
    explicit operator bool() const { return ec == ErrorCode{}; }
};

template <typename ...Args>
StringFormatResult string_format(cxx::string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::format(r.str, format, args...);
    return r;
}

template <typename ...Args>
StringFormatResult string_printf(cxx::string_view format, Args const&... args)
{
    StringFormatResult r;
    r.ec = ::fmtxx::printf(r.str, format, args...);
    return r;
}

} // namespace fmtxx

#endif
