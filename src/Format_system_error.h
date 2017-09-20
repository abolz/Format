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

#ifndef FMTXX_FORMAT_SYSTEM_ERROR_H
#define FMTXX_FORMAT_SYSTEM_ERROR_H 1

#include "Format.h"

#include <system_error>

namespace std
{
    template <>
    struct is_error_code_enum< ::fmtxx::ErrorCode > : std::true_type {};
}

namespace fmtxx {

FMTXX_API std::error_category const& format_error_category();

inline std::error_code make_error_code(ErrorCode ec)
{
    return std::error_code(static_cast<int>(ec), format_error_category());
}

template <>
struct FormatValue<std::error_code>
{
    FMTXX_API std::error_code operator()(Writer& w, FormatSpec const& spec, std::error_code const& val) const;
};

template <>
struct FormatValue<std::error_condition>
{
    FMTXX_API std::error_code operator()(Writer& w, FormatSpec const& spec, std::error_condition const& val) const;
};

} // namespace fmtxx

#endif // FMTXX_FORMAT_SYSTEM_ERROR_H
