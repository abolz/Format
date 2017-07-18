#pragma once

#include "Format_core.h"

#include <cstring>
#include <memory> // std::allocator

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <size_t BufferSize>
class MemoryWriterBase : public Writer
{
public:
    static_assert(BufferSize >= 2, "initial buffer size must be >= 2"); // Required in Grow()

protected:
    char   buf_[BufferSize];
    char*  ptr_      = buf_;
    size_t capacity_ = BufferSize;
    size_t size_     = 0;

public:
    MemoryWriterBase() = default;
    MemoryWriterBase(MemoryWriterBase const&) = delete;
    MemoryWriterBase& operator=(MemoryWriterBase const&) = delete;

    // Returns a pointer to the formatted string.
    // NOTE: not null-terminated!
    char* data() const { return ptr_; }

    // Returns the buffer capacity.
    size_t capacity() const { return capacity_; }

    // Returns the length of the formatted string.
    size_t size() const { return size_; }

    // Returns a reference to the formatted string.
    std__string_view view() const { return std__string_view(data(), size()); }

private:
    ErrorCode Put(char c) override;
    ErrorCode Write(char const* str, size_t len) override;
    ErrorCode Pad(char c, size_t count) override;

    bool Grow(size_t req);

    virtual bool Resize(size_t new_capacity) = 0;
};

template <size_t BufferSize>
ErrorCode MemoryWriterBase<BufferSize>::Put(char c)
{
    assert(size_ <= capacity_);
    assert(size_ + 1 > size_ && "integer overflow");

    if (size_ + 1 > capacity_ && !Grow(1))
        return ErrorCode::io_error;

    assert(size_ + 1 <= capacity_);

    ptr_[size_++] = c;
    return ErrorCode::success;
}

template <size_t BufferSize>
ErrorCode MemoryWriterBase<BufferSize>::Write(char const* str, size_t len)
{
    // TODO:
    // Write as much as possible...

    assert(size_ <= capacity_);
    assert(size_ + len >= size_ && "integer overflow");

    if (size_ + len > capacity_ && !Grow(len))
        return ErrorCode::io_error;

    assert(size_ + len <= capacity_);

    std::memcpy(ptr_ + size_, str, len);
    size_ += len;
    return ErrorCode::success;
}

template <size_t BufferSize>
ErrorCode MemoryWriterBase<BufferSize>::Pad(char c, size_t count)
{
    // TODO:
    // Write as much as possible...

    assert(size_ <= capacity_);
    assert(size_ + count >= size_ && "integer overflow");

    if (size_ + count > capacity_ && !Grow(count))
        return ErrorCode::io_error;

    assert(size_ + count <= capacity_);

    std::memset(ptr_ + size_, static_cast<unsigned char>(c), count);
    size_ += count;
    return ErrorCode::success;
}

template <size_t BufferSize>
bool MemoryWriterBase<BufferSize>::Grow(size_t req)
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

template <size_t BufferSize = 512, typename Alloc = std::allocator<char>>
class MemoryWriter : public MemoryWriterBase<BufferSize>
{
    static_assert(std::is_same<typename Alloc::value_type, char>::value,
        "invalid allocator");

    Alloc alloc_;

public:
    MemoryWriter() = default;
    explicit MemoryWriter(Alloc const& alloc) : alloc_(alloc) {}
    ~MemoryWriter();

private:
    void Deallocate();

    bool Resize(size_t new_capacity) override;
};

template <size_t BufferSize, typename Alloc>
MemoryWriter<BufferSize, Alloc>::~MemoryWriter()
{
    Deallocate();
}

template <size_t BufferSize, typename Alloc>
void MemoryWriter<BufferSize, Alloc>::Deallocate()
{
    if (this->ptr_ == this->buf_)
        return;

    assert(this->ptr_ != nullptr);
    alloc_.deallocate(this->ptr_, this->capacity_);
}

template <size_t BufferSize, typename Alloc>
bool MemoryWriter<BufferSize, Alloc>::Resize(size_t new_capacity)
{
    if (new_capacity > alloc_.max_size())
        return false;

    char* new_ptr = alloc_.allocate(new_capacity);

    if (new_ptr == nullptr)
        return false;

    std::memcpy(new_ptr, this->ptr_, this->size_);

    Deallocate();

    this->ptr_ = new_ptr;
    this->capacity_ = new_capacity;

    return true;
}

} // namespace fmtxx
