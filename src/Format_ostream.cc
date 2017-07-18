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

ErrorCode fmtxx::impl::DoFormat(std::ostream& os, std__string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return fmtxx::impl::DoFormat(w, format, args, types);
    }

    return ErrorCode::io_error;
}

ErrorCode fmtxx::impl::DoPrintf(std::ostream& os, std__string_view format, Arg const* args, Types types)
{
    std::ostream::sentry const ok(os);
    if (ok)
    {
        StreamWriter w{os};
        return fmtxx::impl::DoPrintf(w, format, args, types);
    }

    return ErrorCode::io_error;
}
