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

#ifndef FMTXX_FORMAT_OSTREAM_H
#define FMTXX_FORMAT_OSTREAM_H 1

#include "Format.h"

#include <ostream>
#include <utility>

namespace fmtxx {

namespace impl {

class StreamBuf final : public std::streambuf
{
    Writer& w_;

public:
    FMTXX_API explicit StreamBuf(Writer& w);
    FMTXX_API ~StreamBuf();

protected:
    FMTXX_API int_type overflow(int_type ch = traits_type::eof()) override;
    FMTXX_API std::streamsize xsputn(char const* str, std::streamsize len) override;
};

namespace type_traits {

// Test if an insertion operator is defined for objects of type T.
template <typename T, typename = void>
struct IsStreamable
    : std::false_type
{
};

template <typename T>
struct IsStreamable<T, Void_t< decltype(std::declval<std::ostream&>() << std::declval<T const&>()) >>
    : std::true_type
{
};

} // namespace type_traits

template <typename T>
struct DefaultFormatValue<T, Type::other>
{
    static_assert(type_traits::IsStreamable<T>::value,
        "Formatting objects of type T is not supported. "
        "Specialize FormatValue<T> or TreatAsString<T>, or implement operator<<(std::ostream&, T const&).");

    // Ignores all FormatSpec fields...
    // Setting the stream flags like width, fill-char etc. is not guarenteed to work since many
    // implementations of operator<< set some flags themselves (possibly resetting on exit).
    ErrorCode operator()(Writer& w, FormatSpec const& /*spec*/, T const& val) const
    {
        StreamBuf buf{w};
        std::ostream os{&buf};
        os << val;
        if (os.bad())
            return ErrorCode::io_error;
        if (os.fail())
            return ErrorCode::conversion_error;
        return {};
    }
};

FMTXX_API ErrorCode DoFormat(std::ostream& os, string_view format, Arg const* args, Types types);
FMTXX_API ErrorCode DoPrintf(std::ostream& os, string_view format, Arg const* args, Types types);

} // namespace fmtxx::impl

template <typename ...Args>
ErrorCode format(std::ostream& os, string_view format, ArgPack<Args...> const& args)
{
    return ::fmtxx::impl::DoFormat(os, format, args.array(), args.types());
}

template <typename ...Args>
ErrorCode printf(std::ostream& os, string_view format, ArgPack<Args...> const& args)
{
    return ::fmtxx::impl::DoPrintf(os, format, args.array(), args.types());
}

template <typename ...Args>
ErrorCode format(std::ostream& os, string_view format, Args const&... args)
{
    return ::fmtxx::format(os, format, ArgPack<Args...>(args...));
}

template <typename ...Args>
ErrorCode printf(std::ostream& os, string_view format, Args const&... args)
{
    return ::fmtxx::printf(os, format, ArgPack<Args...>(args...));
}

} // namespace fmtxx

#endif // FMTXX_FORMAT_OSTREAM_H
