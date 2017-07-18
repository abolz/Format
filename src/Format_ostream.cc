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

#include "Format_ostream.h"

#include <cstring>
#include <limits>

using namespace fmtxx;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace {

class StreamWriter : public Writer
{
public:
    std::ostream& os;

    explicit StreamWriter(std::ostream& os) : os(os) {}

private:
    ErrorCode Put(char c) override;
    ErrorCode Write(char const* str, size_t len) override;
    ErrorCode Pad(char c, size_t count) override;
};

inline ErrorCode StreamWriter::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof()))
    {
        os.setstate(std::ios_base::badbit);
        return ErrorCode::io_error;
    }

    return ErrorCode::success;
}

inline ErrorCode StreamWriter::Write(char const* str, size_t len)
{
    auto const kMaxLen = static_cast<size_t>(std::numeric_limits<std::streamsize>::max());

    while (len > 0)
    {
        auto const n = len < kMaxLen ? len : kMaxLen;
        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(str, k))
        {
            os.setstate(std::ios_base::badbit);
            return ErrorCode::io_error;
        }
        str += n;
        len -= n;
    }

    return ErrorCode::success;
}

inline ErrorCode StreamWriter::Pad(char c, size_t count)
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        auto const n = count < kBlockSize ? count : kBlockSize;
        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(block, k))
        {
            os.setstate(std::ios_base::badbit);
            return ErrorCode::io_error;
        }
        count -= n;
    }

    return ErrorCode::success;
}

} // namespace

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

ErrorCode fmtxx::impl::DoFormat(std::ostream& os, cxx::string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return fmtxx::impl::DoFormat(w, format, args, types);
    }

    return ErrorCode::io_error;
}

ErrorCode fmtxx::impl::DoPrintf(std::ostream& os, cxx::string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return fmtxx::impl::DoPrintf(w, format, args, types);
    }

    return ErrorCode::io_error;
}
