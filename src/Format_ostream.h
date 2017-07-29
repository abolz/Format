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

#include "Format_core.h"

#include <ostream>
#include <utility>

namespace fmtxx {

namespace impl {

class StreamBuf : public std::streambuf
{
    Writer& w_;

public:
    FMTXX_API explicit StreamBuf(Writer& w);
    FMTXX_API ~StreamBuf();

protected:
    FMTXX_API int_type overflow(int_type ch = traits_type::eof()) override;
    FMTXX_API std::streamsize xsputn(char const* str, std::streamsize len) override;
};

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

template <typename T>
struct StreamValue<T, void>
{
    static_assert(IsStreamable<T>::value,
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

FMTXX_API ErrorCode DoFormat(std::ostream& os, cxx::string_view format, Arg const* args, Types types);
FMTXX_API ErrorCode DoPrintf(std::ostream& os, cxx::string_view format, Arg const* args, Types types);

} // namespace fmtxx::impl

template <typename ...Args>
inline ErrorCode format(std::ostream& os, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFormat(os, format, arr, impl::Types::make(args...));
}

template <typename ...Args>
inline ErrorCode printf(std::ostream& os, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoPrintf(os, format, arr, impl::Types::make(args...));
}

inline ErrorCode format(std::ostream& os, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFormat(os, format, args.args_, args.types_);
}

inline ErrorCode printf(std::ostream& os, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoPrintf(os, format, args.args_, args.types_);
}

} // namespace fmtxx

#endif // FMTXX_FORMAT_OSTREAM_H
