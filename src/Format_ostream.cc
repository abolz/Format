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

#include <algorithm>
#include <limits>

using namespace fmtxx;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

fmtxx::impl::StreamBuf::StreamBuf(Writer& w) : w_(w)
{
}

fmtxx::impl::StreamBuf::~StreamBuf()
{
}

fmtxx::impl::StreamBuf::int_type fmtxx::impl::StreamBuf::overflow(int_type ch)
{
    if (traits_type::eq_int_type(ch, traits_type::eof()))
        return 0;
    if (Failed(w_.put(traits_type::to_char_type(ch))))
        return traits_type::eof();
    return ch;
}

std::streamsize fmtxx::impl::StreamBuf::xsputn(char const* str, std::streamsize len)
{
    assert(len >= 0);
    if (len == 0)
        return 0;
    if (Failed(w_.write(str, static_cast<size_t>(len))))
        return 0;
    return len;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace {

class StreamWriter : public Writer
{
public:
    std::ostream& os;

    explicit StreamWriter(std::ostream& os_) : os(os_) {}

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

    return {};
}

inline ErrorCode StreamWriter::Write(char const* str, size_t len)
{
    assert(len <= static_cast<size_t>(std::numeric_limits<std::streamsize>::max()));

    auto const k = static_cast<std::streamsize>(len);
    if (k != os.rdbuf()->sputn(str, k))
    {
        os.setstate(std::ios_base::badbit);
        return ErrorCode::io_error;
    }

    return {};
}

inline ErrorCode StreamWriter::Pad(char c, size_t count)
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::fill_n(block, kBlockSize, c);

    while (count > 0)
    {
        auto const n = std::min(count, kBlockSize);
        if (Failed ec = StreamWriter::Write(block, n))
            return ec;
        count -= n;
    }

    return {};
}

} // namespace

ErrorCode fmtxx::impl::DoFormat(std::ostream& os, cxx::string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return ::fmtxx::impl::DoFormat(w, format, args, types);
    }

    return ErrorCode::io_error;
}

ErrorCode fmtxx::impl::DoPrintf(std::ostream& os, cxx::string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return ::fmtxx::impl::DoPrintf(w, format, args, types);
    }

    return ErrorCode::io_error;
}
