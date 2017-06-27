// Distributed under the MIT license. See the end of the file for details.

#pragma once
#define FMTXX_FORMAT_MEMORY_H 1

#include "Format.h"

#include <cassert>
#include <cstring>
#include <memory> // std::allocator

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class MemoryWriterBase : public Writer
{
public:
    enum { kBufSize = 512 };
    static_assert(kBufSize >= 2, "initial buffer size must be >= 2"); // Required in Grow()

protected:
    char   buf_[kBufSize];
    char*  ptr_      = buf_;
    size_t capacity_ = kBufSize;
    size_t size_     = 0;

public:
    MemoryWriterBase() = default;
    MemoryWriterBase(MemoryWriterBase const&) = delete;
    MemoryWriterBase& operator=(MemoryWriterBase const&) = delete;

    bool Put(char c) override;
    bool Write(char const* str, size_t len) override;
    bool Pad(char c, size_t count) override;

    // Returns a pointer to the formatted string.
    // NOTE: not null-terminated!
    char* data() const { return ptr_; }

    // Returns the length of the formatted string.
    size_t size() const { return size_; }

    // Returns a reference to the formatted string.
    std::string_view view() const { return std::string_view(data(), size()); }

private:
    bool Grow(size_t req);

    virtual bool Resize(size_t new_capacity) = 0;
};

inline bool MemoryWriterBase::Put(char c)
{
    assert(size_ <= capacity_);
    assert(size_ + 1 > size_ && "integer overflow");

    if (size_ + 1 > capacity_ && !Grow(1))
        return false;

    assert(size_ + 1 <= capacity_);

    ptr_[size_++] = c;
    return true;
}

inline bool MemoryWriterBase::Write(char const* str, size_t len)
{
    // TODO:
    // Write as much as possible...

    assert(size_ <= capacity_);
    assert(size_ + len >= size_ && "integer overflow");

    if (size_ + len > capacity_ && !Grow(len))
        return false;

    assert(size_ + len <= capacity_);

    std::memcpy(ptr_ + size_, str, len);
    size_ += len;
    return true;
}

inline bool MemoryWriterBase::Pad(char c, size_t count)
{
    // TODO:
    // Write as much as possible...

    assert(size_ <= capacity_);
    assert(size_ + count >= size_ && "integer overflow");

    if (size_ + count > capacity_ && !Grow(count))
        return false;

    assert(size_ + count <= capacity_);

    std::memset(ptr_ + size_, static_cast<unsigned char>(c), count);
    size_ += count;
    return true;
}

inline bool MemoryWriterBase::Grow(size_t req)
{
    assert(size_ + req > capacity_ && "call to Grow() is not required");

    size_t cap = capacity_;
    for (;;)
    {
        size_t n = cap + cap / 2;
        if (n < cap) // integer overflow
            n = size_ + req;
        if (n >= size_ + req) // new buffer size is large enough
            return Resize(n);
        cap = n;
    }
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename Alloc = std::allocator<char>>
class MemoryWriter : public MemoryWriterBase
{
    static_assert(std::is_same<typename Alloc::value_type, char>::value, "invalid allocator");

    Alloc alloc_; // TODO: EBO...

public:
    MemoryWriter() = default;
    MemoryWriter(Alloc const& alloc) : alloc_(alloc) {}
    ~MemoryWriter();

private:
    void Deallocate();

    bool Resize(size_t new_capacity) override;
};

template <typename Alloc>
MemoryWriter<Alloc>::~MemoryWriter()
{
    Deallocate();
}

template <typename Alloc>
void MemoryWriter<Alloc>::Deallocate()
{
    if (ptr_ == buf_)
        return;

    assert(ptr_ != nullptr);
    alloc_.deallocate(ptr_, capacity_);
}

template <typename Alloc>
bool MemoryWriter<Alloc>::Resize(size_t new_capacity)
{
    if (new_capacity > alloc_.max_size())
        return false;

    char* new_ptr = alloc_.allocate(new_capacity);

    if (new_ptr == nullptr)
        return false;

    std::memcpy(new_ptr, ptr_, size_);

    Deallocate();

    ptr_ = new_ptr;
    capacity_ = new_capacity;

    return true;
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
