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

#include "Format_stdio.h"

#include <algorithm> // min

using namespace fmtxx;

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

ErrorCode fmtxx::FILEWriter::Put(char c) noexcept
{
    if (EOF == std::fputc(c, file_))
        return ErrorCode::io_error;

    size_ += 1;
    return {};
}

ErrorCode fmtxx::FILEWriter::Write(char const* ptr, size_t len) noexcept
{
    size_t n = std::fwrite(ptr, 1, len, file_);

    // Count the number of characters successfully transmitted.
    // This is unlike ArrayWriter, which counts characters that would have been written on success.
    // (FILEWriter and ArrayWriter are for compatibility with fprintf and snprintf, resp.)
    size_ += n;
    return n == len ? ErrorCode{} : ErrorCode::io_error;
}

ErrorCode fmtxx::FILEWriter::Pad(char c, size_t count) noexcept
{
    size_t const kBlockSize = 32;

    char block[kBlockSize];
    std::memset(block, static_cast<unsigned char>(c), kBlockSize);

    while (count > 0)
    {
        auto const n = std::min(count, kBlockSize);
        if (Failed ec = FILEWriter::Write(block, n))
            return ec;
        count -= n;
    }

    return {};
}

size_t fmtxx::ArrayWriter::finish() noexcept
{
    if (size_ < bufsize_)
        buf_[size_] = '\0';
    else if (bufsize_ > 0)
        buf_[bufsize_ - 1] = '\0';

    return size_;
}

ErrorCode fmtxx::ArrayWriter::Put(char c) noexcept
{
    if (size_ < bufsize_)
        buf_[size_] = c;

    size_ += 1;
    return {};
}

ErrorCode fmtxx::ArrayWriter::Write(char const* ptr, size_t len) noexcept
{
    if (size_ < bufsize_)
        std::memcpy(buf_ + size_, ptr, std::min(len, bufsize_ - size_));

    size_ += len;
    return {};
}

ErrorCode fmtxx::ArrayWriter::Pad(char c, size_t count) noexcept
{
    if (size_ < bufsize_)
        std::memset(buf_ + size_, static_cast<unsigned char>(c), std::min(count, bufsize_ - size_));

    size_ += count;
    return {};
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

ErrorCode fmtxx::impl::DoFormat(std::FILE* file, cxx::string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return ::fmtxx::impl::DoFormat(w, format, args, types);
}

ErrorCode fmtxx::impl::DoPrintf(std::FILE* file, cxx::string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};
    return ::fmtxx::impl::DoPrintf(w, format, args, types);
}

int fmtxx::impl::DoFileFormat(std::FILE* file, cxx::string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(::fmtxx::impl::DoFormat(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::DoFilePrintf(std::FILE* file, cxx::string_view format, Arg const* args, Types types)
{
    FILEWriter w{file};

    if (Failed(::fmtxx::impl::DoPrintf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    return static_cast<int>(w.size());
}

int fmtxx::impl::DoArrayFormat(char* buf, size_t bufsize, cxx::string_view format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(::fmtxx::impl::DoFormat(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    w.finish();
    return static_cast<int>(w.size());
}

int fmtxx::impl::DoArrayPrintf(char* buf, size_t bufsize, cxx::string_view format, Arg const* args, Types types)
{
    ArrayWriter w{buf, bufsize};

    if (Failed(::fmtxx::impl::DoPrintf(w, format, args, types)))
        return -1;
    if (w.size() > INT_MAX)
        return -1;

    w.finish();
    return static_cast<int>(w.size());
}
