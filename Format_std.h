// Distributed under the MIT license. See the end of the file for details.

#pragma once

#include "Format.h"
//#include "memstream.h"

#include <sstream>
#include <tuple>
#include <type_traits>

namespace fmtxx {

template <typename ...Args>
std::string Format(std__string_view format, Args const&... args)
{
//    char stackbuf[500];
//    omemstream os(stackbuf);
    std::ostringstream os;
    fmtxx::Format(os, format, args...);
    return os.str();
}

namespace impl
{
    template <typename S, typename ...Args>
    struct FormatArgs
    {
        const S format_;
        const std::tuple<Args...> args_;

        explicit FormatArgs(S&& format, Args&&... args)
            : format_(std::forward<S>(format))
            , args_(std::forward<Args>(args)...)
        {
        }

        template <size_t... I>
        void insert_impl(std::ostream& os, std::index_sequence<I...>) const {
            fmtxx::Format(os, format_, std::get<I>(args_)...);
        }

        void insert(std::ostream& os) const {
            return insert_impl(os, std::make_index_sequence<sizeof...(Args)>{});
        }
    };

    template <typename S, typename ...Args>
    std::ostream& operator <<(std::ostream& os, FormatArgs<S, Args...> const& args)
    {
        args.insert_impl(os, std::make_index_sequence<sizeof...(Args)>{});
        return os;
    }
}

template <typename S, typename ...Args>
struct impl::FormatArgs<S, Args...> Formatted(S&& format, Args&&... args)
{
    return impl::FormatArgs<S, Args...>(
        std::forward<S>(format), std::forward<Args>(args)... );
}

} // namespace fmtxx

//------------------------------------------------------------------------------
// Copyright 2016 Alexander Bolz
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
