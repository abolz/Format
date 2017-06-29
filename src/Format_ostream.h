// Distributed under the MIT license. See the end of the file for details.

#pragma once
#define FMTXX_FORMAT_OSTREAM_H 1

#include "Format.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <sstream>

namespace fmtxx {

namespace impl
{
#if 1
    // Provides access to the underlying write buffer.
    // To avoid having to make a copy of the string in StreamValue below.
    class StreamBuf : public std::stringbuf
    {
    public:
        char* pub_pbase() const { return pbase(); }
        char* pub_pptr() const { return pptr(); }
    };

    template <typename T>
    struct StreamValue<T, void>
    {
        errc operator()(Writer& w, FormatSpec const& spec, T const& val) const
        {
            StreamBuf buf;
            std::ostream os{&buf};
            os << val;
            if (os.bad()) // shouldn't happen here...
                return errc::io_error;
            if (os.fail())
                return errc::conversion_error;
            return Util::format_string(w, spec, buf.pub_pbase(), static_cast<size_t>(buf.pub_pptr() - buf.pub_pbase()));
        }
    };
#else
    class StreamBuf : public std::streambuf
    {
        Writer& w_;

    public:
        explicit StreamBuf(Writer& w) : w_(w) {}

    protected:
        int_type overflow(int_type ch = traits_type::eof()) override {
            if (traits_type::eq_int_type(ch, traits_type::eof()))
                return 0;
            if (w_.Put(static_cast<char>(ch)))
                return ch;
            return traits_type::eof();
        }

        std::streamsize xsputn(char const* str, std::streamsize len) override {
            assert(len >= 0);
            if (len == 0)
                return 0;
            if (w_.Write(str, static_cast<size_t>(len)))
                return len;
            return 0;
        }
    };

    template <typename T>
    struct StreamValue<T, void>
    {
        errc operator()(Writer& w, FormatSpec const& /*spec*/, T const& val) const
        {
            StreamBuf buf{w};
            std::ostream os{&buf};
            os << val;
            if (os.bad())
                return errc::io_errror;
            if (os.fail())
                return errc::conversion_error;
            return errc::success;
        }
    };
#endif
}

class StreamWriter : public Writer
{
public:
    std::ostream& os;
    std::ostream::sentry const se;

    explicit StreamWriter(std::ostream& v);
    ~StreamWriter();

    bool Put(char c) override;
    bool Write(char const* str, size_t len) override;
    bool Pad(char c, size_t count) override;
};

inline StreamWriter::StreamWriter(std::ostream& v)
    : os(v)
    , se(v)
{
}

inline StreamWriter::~StreamWriter()
{
}

inline bool StreamWriter::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (!se)
        return false;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof()))
    {
        os.setstate(std::ios_base::badbit);
        return false;
    }

    return true;
}

inline bool StreamWriter::Write(char const* str, size_t len)
{
    if (!se)
        return false;

    auto const kMaxLen = static_cast<size_t>( std::numeric_limits<std::streamsize>::max() );

    while (len > 0)
    {
        auto const n = len < kMaxLen ? len : kMaxLen;
        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(str, k))
        {
            os.setstate(std::ios_base::badbit);
            return false;
        }
        str += n;
        len -= n;
    }

    return true;
}

inline bool StreamWriter::Pad(char c, size_t count)
{
    if (!se)
        return false;

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
            return false;
        }
        count -= n;
    }

    return true;
}

template <typename ...Args>
errc format(std::ostream& os, string_view format, Args const&... args)
{
    StreamWriter w{os};
    return fmtxx::format(w, format, args...);
}

template <typename ...Args>
errc printf(std::ostream& os, string_view format, Args const&... args)
{
    StreamWriter w{os};
    return fmtxx::printf(w, format, args...);
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
