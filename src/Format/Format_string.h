// Distributed under the MIT license. See the end of the file for details.

#pragma once
#define FMTXX_FORMAT_STRING_H 1

#include "Format.h"

#include <string>

namespace fmtxx {

template <typename Alloc>
struct TreatAsString< std::basic_string<char, std::char_traits<char>, Alloc> >
    : std::true_type
{
};

template <typename StringT>
class StringWriter : public Writer
{
public:
    StringT& os;

    explicit StringWriter(StringT& v) : os(v) {}

    bool Put(char c) override;
    bool Write(char const* str, size_t len) override;
    bool Pad(char c, size_t count) override;
};

template <typename StringT>
bool StringWriter<StringT>::Put(char c)
{
    os.push_back(c);
    return true;
}

template <typename StringT>
bool StringWriter<StringT>::Write(char const* str, size_t len)
{
    os.append(str, len);
    return true;
}

template <typename StringT>
bool StringWriter<StringT>::Pad(char c, size_t count)
{
    os.append(count, c);
    return true;
}

template <typename StringT, typename ...Args>
errc sformat(std::string& str, std::string_view format, Args const&... args)
{
    StringWriter<StringT> w{str};
    return fmtxx::format(w, format, args...);
}

template <typename StringT = std::string, typename ...Args>
StringT sformat(std::string_view format, Args const&... args)
{
    StringT str;
    StringWriter<StringT> w{str};
    fmtxx::format(w, format, args...); // Returns success or throws (or aborts)
    return str;
}

template <typename StringT, typename ...Args>
errc sprintf(StringT& str, std::string_view format, Args const&... args)
{
    StringWriter<StringT> w{str};
    return fmtxx::printf(w, format, args...);
}

template <typename StringT = std::string, typename ...Args>
StringT sprintf(std::string_view format, Args const&... args)
{
    StringT str;
    StringWriter<StringT> w{str};
    fmtxx::printf(w, format, args...); // Returns success or throws (or aborts)
    return str;
}

} // namespace fmtxx

//------------------------------------------------------------------------------
// Copyright (c) 2017 A. Bolz
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
