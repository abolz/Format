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
        ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const
        {
            StreamBuf buf;
            std::ostream os{&buf};
            os << val;
            if (os.bad()) // shouldn't happen here...
                return ErrorCode::io_error;
            if (os.fail())
                return ErrorCode::conversion_error;
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
        int_type overflow(int_type ch = traits_type::eof()) override
        {
            if (traits_type::eq_int_type(ch, traits_type::eof()))
                return 0;
            if (Failed(w_.put(static_cast<char>(ch))))
                return traits_type::eof(); // error
            return ch;
        }

        std::streamsize xsputn(char const* str, std::streamsize len) override
        {
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
        ErrorCode operator()(Writer& w, FormatSpec const& /*spec*/, T const& val) const
        {
            StreamBuf buf{w};
            std::ostream os{&buf};
            os << val;
            if (os.bad())
                return ErrorCode::io_error;
            if (os.fail())
                return ErrorCode::conversion_error;
            return ErrorCode::success;
        }
    };
#endif
}

} // namespace fmtxx
