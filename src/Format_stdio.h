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

#ifndef FMTXX_FORMAT_STDIO_H
#define FMTXX_FORMAT_STDIO_H 1

#include "Format_core.h"

#include <cstdio>

namespace fmtxx {

// Write to std::FILE's, keeping track of the number of characters (successfully) transmitted.
class FMTXX_VISIBILITY_DEFAULT FILEWriter : public Writer
{
    std::FILE* const file_;
    size_t           size_ = 0;

public:
    explicit FILEWriter(std::FILE* v) : file_(v) {
        assert(file_ != nullptr);
    }

    // Returns the FILE stream.
    std::FILE* file() const { return file_; }

    // Returns the number of bytes successfully transmitted (since construction).
    size_t size() const { return size_; }

private:
    FMTXX_API ErrorCode Put(char c) noexcept override;
    FMTXX_API ErrorCode Write(char const* ptr, size_t len) noexcept override;
    FMTXX_API ErrorCode Pad(char c, size_t count) noexcept override;
};

// Write to a user allocated buffer.
// If the buffer overflows, keep track of the number of characters that would
// have been written if the buffer were large enough. This is for compatibility
// with snprintf.
class FMTXX_VISIBILITY_DEFAULT ArrayWriter : public Writer
{
    char*  const buf_     = nullptr;
    size_t const bufsize_ = 0;
    size_t       size_    = 0;

public:
    ArrayWriter(char* buffer, size_t buffer_size) : buf_(buffer), bufsize_(buffer_size) {
        assert(bufsize_ == 0 || buf_ != nullptr);
    }

    template <size_t N>
    explicit ArrayWriter(char (&buf)[N]) : ArrayWriter(buf, N) {}

    // Returns a pointer to the string.
    // The string is null-terminated if finish() has been called.
    char* data() const { return buf_; }

    // Returns the buffer capacity.
    size_t capacity() const { return bufsize_; }

    // Returns the length of the string.
    size_t size() const { return size_; }

    // Returns true if the buffer was too small.
    bool overflow() const { return size_ >= bufsize_; }

    // Returns the string.
    cxx::string_view view() const { return cxx::string_view(data(), size()); }

    // Null-terminate the buffer.
    // Returns the length of the string not including the null-character.
    FMTXX_API size_t finish() noexcept;

private:
    FMTXX_API ErrorCode Put(char c) noexcept override;
    FMTXX_API ErrorCode Write(char const* ptr, size_t len) noexcept override;
    FMTXX_API ErrorCode Pad(char c, size_t count) noexcept override;
};

namespace impl {

FMTXX_API ErrorCode DoFormat(std::FILE* file, cxx::string_view format, Arg const* args, Types types);
FMTXX_API ErrorCode DoPrintf(std::FILE* file, cxx::string_view format, Arg const* args, Types types);

// fprintf compatible formatting functions.
FMTXX_API int DoFileFormat(std::FILE* file, cxx::string_view format, Arg const* args, Types types);
FMTXX_API int DoFilePrintf(std::FILE* file, cxx::string_view format, Arg const* args, Types types);

// snprintf compatible formatting functions.
FMTXX_API int DoArrayFormat(char* buf, size_t bufsize, cxx::string_view format, Arg const* args, Types types);
FMTXX_API int DoArrayPrintf(char* buf, size_t bufsize, cxx::string_view format, Arg const* args, Types types);

} // namespace fmtxx::impl

template <typename ...Args>
inline ErrorCode format(std::FILE* file, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFormat(file, format, arr, impl::Types::make(args...));
}

template <typename ...Args>
inline ErrorCode printf(std::FILE* file, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoPrintf(file, format, arr, impl::Types::make(args...));
}

inline ErrorCode format(std::FILE* file, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFormat(file, format, args.args_, args.types_);
}

inline ErrorCode printf(std::FILE* file, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoPrintf(file, format, args.args_, args.types_);
}

template <typename ...Args>
inline int fformat(std::FILE* file, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFileFormat(file, format, arr, impl::Types::make(args...));
}

template <typename ...Args>
inline int fprintf(std::FILE* file, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoFilePrintf(file, format, arr, impl::Types::make(args...));
}

inline int fformat(std::FILE* file, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFileFormat(file, format, args.args_, args.types_);
}

inline int fprintf(std::FILE* file, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoFilePrintf(file, format, args.args_, args.types_);
}

template <typename ...Args>
inline int snformat(char* buf, size_t bufsize, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoArrayFormat(buf, bufsize, format, arr, impl::Types::make(args...));
}

template <typename ...Args>
inline int snprintf(char* buf, size_t bufsize, cxx::string_view format, Args const&... args)
{
    impl::ArgArray<sizeof...(Args)> arr = {args...};
    return ::fmtxx::impl::DoArrayPrintf(buf, bufsize, format, arr, impl::Types::make(args...));
}

inline int snformat(char* buf, size_t bufsize, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoArrayFormat(buf, bufsize, format, args.args_, args.types_);
}

inline int snprintf(char* buf, size_t bufsize, cxx::string_view format, FormatArgs const& args)
{
    return ::fmtxx::impl::DoArrayPrintf(buf, bufsize, format, args.args_, args.types_);
}

template <size_t N, typename ...Args>
inline int snformat(char (&buf)[N], cxx::string_view format, Args const&... args)
{
    return ::fmtxx::snformat(&buf[0], N, format, args...);
}

template <size_t N, typename ...Args>
inline int snprintf(char (&buf)[N], cxx::string_view format, Args const&... args)
{
    return ::fmtxx::snprintf(&buf[0], N, format, args...);
}

} // namespace fmtxx

#endif // FMTXX_FORMAT_STDIO_H
