#pragma once

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
            if (Failed(w_.put(static_cast<char>(ch))))
                return traits_type::eof(); // error
            return ch;
        }

        std::streamsize xsputn(char const* str, std::streamsize len) override {
            assert(len >= 0);
            if (len == 0)
                return 0;
            if (Failed(w_.write(str, static_cast<size_t>(len))))
                return 0; // error
            return len;
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

private:
    errc Put(char c) override;
    errc Write(char const* str, size_t len) override;
    errc Pad(char c, size_t count) override;
};

inline StreamWriter::StreamWriter(std::ostream& v)
    : os(v)
    , se(v)
{
}

inline StreamWriter::~StreamWriter()
{
}

inline errc StreamWriter::Put(char c)
{
    using traits_type = std::ostream::traits_type;

    if (!se)
        return errc::io_error;

    if (traits_type::eq_int_type(os.rdbuf()->sputc(c), traits_type::eof()))
    {
        os.setstate(std::ios_base::badbit);
        return errc::io_error;
    }

    return errc::success;
}

inline errc StreamWriter::Write(char const* str, size_t len)
{
    if (!se)
        return errc::io_error;

    auto const kMaxLen = static_cast<size_t>( std::numeric_limits<std::streamsize>::max() );

    while (len > 0)
    {
        auto const n = len < kMaxLen ? len : kMaxLen;
        auto const k = static_cast<std::streamsize>(n);
        if (k != os.rdbuf()->sputn(str, k))
        {
            os.setstate(std::ios_base::badbit);
            return errc::io_error;
        }
        str += n;
        len -= n;
    }

    return errc::success;
}

inline errc StreamWriter::Pad(char c, size_t count)
{
    if (!se)
        return errc::io_error;

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
            return errc::io_error;
        }
        count -= n;
    }

    return errc::success;
}

template <typename ...Args>
errc format(std::ostream& os, StringView format, Args const&... args)
{
    StreamWriter w{os};
    return fmtxx::format(w, format, args...);
}

template <typename ...Args>
errc printf(std::ostream& os, StringView format, Args const&... args)
{
    StreamWriter w{os};
    return fmtxx::printf(w, format, args...);
}

} // namespace fmtxx
