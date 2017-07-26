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

#include "Format_system_error.h"

#include <string>

using namespace fmtxx;

namespace {

class FormatErrorCategory : public std::error_category
{
public:
    char const* name() const noexcept override;
    std::string message(int ec) const override;
};

char const* FormatErrorCategory::name() const noexcept
{
    return "format error";
}

std::string FormatErrorCategory::message(int ec) const
{
    switch (static_cast<ErrorCode>(ec))
    {
    case ErrorCode::conversion_error:
        return "conversion error";
    case ErrorCode::index_out_of_range:
        return "index out of range";
    case ErrorCode::invalid_argument:
        return "invalid argument";
    case ErrorCode::invalid_format_string:
        return "invalid format string";
    case ErrorCode::io_error:
        return "io error";
    case ErrorCode::not_supported:
        return "not supported";
    case ErrorCode::value_out_of_range:
        return "value out of range";
    }

    return "[[unknown format error]]";
}

} // namespace

std::error_category const& fmtxx::format_error_category()
{
    static FormatErrorCategory cat;
    return cat;
}

std::error_code fmtxx::FormatValue<std::error_code>::operator()(Writer& w, FormatSpec const& spec, std::error_code const& val) const
{
    // NOTE:
    // This is different from 'ostream << error_code'.
    auto const message = val.message();
    return Util::format_string(w, spec, message.data(), message.size());
}

std::error_code FormatValue<std::error_condition>::operator()(Writer& w, FormatSpec const& spec, std::error_condition const& val) const
{
    auto const message = val.message();
    return Util::format_string(w, spec, message.data(), message.size());
}
