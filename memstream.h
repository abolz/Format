// Derived from the libc++ strstream implementation.
// Original license follows:
//
//===--------------------------- strstream --------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstring>
#include <istream>
#include <ostream>
#include <string>

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class memstreambuf : public std::streambuf
{
    using base_type = std::streambuf;

public:
    memstreambuf();
    memstreambuf(char* gnext, std::streamsize n, char* pbeg = 0);
    memstreambuf(const char* gnext, std::streamsize n);

    memstreambuf(memstreambuf&&) = delete;
    memstreambuf& operator=(memstreambuf&&) = delete;

    virtual ~memstreambuf();

    void swap(memstreambuf& rhs);

    std::string str() const;

    char const* data() const;

    size_t size() const;

protected:
    virtual int_type overflow(int_type ch = EOF);
    virtual int_type pbackfail(int_type ch = EOF);
    virtual int_type underflow();
    virtual pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
    virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);

private:
    static const std::streamsize kMinAllocationSize = 512; // 1024; // 4096;

    enum Mode : unsigned {
        Mode_None = 0,
        Mode_Allocated = 0x01,
        Mode_Constant = 0x02,
    };
    Mode strmode_;

    void Init(char* gnext, std::streamsize n, char* pbeg);

    static char* Alloc(size_t num_bytes)
    {
        return static_cast<char*>(::malloc(num_bytes));
    }

    static void Dealloc(void* p)
    {
        if (p)
            ::free(p);
    }
};

inline memstreambuf::memstreambuf()
    : strmode_(Mode_None)
{
}

inline memstreambuf::memstreambuf(char* gnext, std::streamsize n, char* pbeg)
    : strmode_()
{
    Init(gnext, n, pbeg);
}

inline memstreambuf::memstreambuf(const char* gnext, std::streamsize n)
    : strmode_(Mode_Constant)
{
    Init(const_cast<char*>(gnext), n, nullptr);
}

#if 0
inline memstreambuf::memstreambuf(memstreambuf&& rhs)
    : base_type(std::move(rhs))
    , strmode_(rhs.strmode_)
{
    rhs.setg(nullptr, nullptr, nullptr);
    rhs.setp(nullptr, nullptr);
}

inline memstreambuf& memstreambuf::operator=(memstreambuf&& rhs)
{
    if (eback() && (strmode_ & Mode_Allocated) != 0)
    {
        Dealloc(eback());
    }

    base_type::operator=(std::move(rhs));
    strmode_ = rhs.strmode_;

    rhs.setg(nullptr, nullptr, nullptr);
    rhs.setp(nullptr, nullptr);

    return *this;
}
#endif

inline memstreambuf::~memstreambuf()
{
    if (eback() && (strmode_ & Mode_Allocated) != 0)
    {
        Dealloc(eback());
    }
}

inline void memstreambuf::swap(memstreambuf& rhs)
{
    base_type::swap(rhs);
    std::swap(strmode_, rhs.strmode_);
}

inline std::string memstreambuf::str() const
{
    return {eback(), pptr()};
}

inline char const* memstreambuf::data() const
{
    return eback();
}

inline size_t memstreambuf::size() const
{
    return static_cast<size_t>(pptr() - eback());
}

inline memstreambuf::int_type memstreambuf::overflow(int_type ch)
{
    if (ch == EOF)
        return int_type(0);

    if (pptr() == epptr())
    {
        size_t const old_size = static_cast<size_t>((epptr() ? epptr() : egptr()) - eback());

        size_t inc = old_size / 2;
        if (inc < kMinAllocationSize)
            inc = kMinAllocationSize;

        for (;;)
        {
//          if (old_size + inc > old_size)
            if (SIZE_MAX - old_size >= inc)
                break; // OK. Does not overflow.
            inc /= 2;
            if (inc == 0)
                return EOF;
        }

        // POST: inc >= 1
        // So there is at least room for a single char...

        size_t const new_size = old_size + inc;

        //
        // XXX:
        // Use realloc?
        //

        char* const buf = Alloc(new_size);

        if (buf == nullptr)
            return int_type(EOF);

        if (old_size != 0)
        {
            assert(eback());
            std::memcpy(buf, eback(), static_cast<size_t>(old_size));
        }

        ptrdiff_t const ninp = gptr()  - eback();
        ptrdiff_t const einp = egptr() - eback();
        ptrdiff_t const nout = pptr()  - pbase();

        if (strmode_ & Mode_Allocated)
        {
            Dealloc(eback());
        }

        setg(buf, buf + ninp, buf + einp);
        setp(buf + einp, buf + new_size);
        pbump(static_cast<int>(nout));

        strmode_ = static_cast<Mode>(strmode_ | Mode_Allocated);
    }

    pptr()[0] = static_cast<char>(ch);
    pbump(1);

    return int_type(static_cast<unsigned char>(ch));
}

inline memstreambuf::int_type memstreambuf::pbackfail(int_type ch)
{
    if (eback() == gptr())
        return EOF;

    if (ch == EOF)
    {
        gbump(-1);
        return int_type(0);
    }

    if (strmode_ & Mode_Constant)
    {
        if (gptr()[-1] == static_cast<char>(ch))
        {
            gbump(-1);
            return ch;
        }

        return EOF;
    }

    gbump(-1);
    gptr()[0] = static_cast<char>(ch);

    return ch;
}

inline memstreambuf::int_type memstreambuf::underflow()
{
    if (gptr() == egptr())
    {
        if (egptr() >= pptr())
            return EOF;

        setg(eback(), gptr(), pptr());
    }

    return int_type(static_cast<unsigned char>(*gptr()));
}

inline memstreambuf::pos_type memstreambuf::seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which)
{
    bool const pos_in = (which & std::ios::in) != 0;
    bool const pos_out = (which & std::ios::out) != 0;

    bool legal = false;
    switch (way)
    {
    case std::ios::beg:
    case std::ios::end:
        if (pos_in || pos_out)
            legal = true;
        break;
    case std::ios::cur:
        if (pos_in != pos_out)
            legal = true;
        break;
    default:
        assert(!"unreachable");
        break;
    }

    if (pos_in && gptr() == nullptr)
        legal = false;

    if (pos_out && pptr() == nullptr)
        legal = false;

    if (legal)
    {
        char* const seekhigh = epptr() ? epptr() : egptr();

        off_type newoff;
        switch (way)
        {
        case std::ios::beg:
            newoff = 0;
            break;
        case std::ios::cur:
            newoff = (pos_in ? gptr() : pptr()) - eback();
            break;
        case std::ios::end:
            newoff = seekhigh - eback();
            break;
        default:
            assert(!"unreachable");
            newoff = 0;
            break;
        }

        newoff += off;

        if (0 <= newoff && newoff <= seekhigh - eback())
        {
            char* const newpos = eback() + newoff;

            if (pos_in)
            {
                if (newpos > egptr())
                    setg(eback(), newpos, newpos);
                else
                    setg(eback(), newpos, egptr());
            }

            if (pos_out)
            {
                // min(pbase, newpos), newpos, epptr()
                off = epptr() - newpos;

                if (pbase() < newpos)
                    setp(pbase(), epptr());
                else
                    setp(newpos, epptr());

                pbump(static_cast<int>((epptr() - pbase()) - off));
            }

            return newoff;
        }
    }

    return pos_type(-1);
}

inline memstreambuf::pos_type memstreambuf::seekpos(pos_type pos, std::ios_base::openmode which)
{
    bool const pos_in = (which & std::ios::in) != 0;
    bool const pos_out = (which & std::ios::out) != 0;

    if (pos_in || pos_out)
    {
        if (!((pos_in && gptr() == nullptr) || (pos_out && pptr() == nullptr)))
        {
            char* const seekhigh = epptr() ? epptr() : egptr();

            off_type const newoff = pos;
            if (0 <= newoff && newoff <= seekhigh - eback())
            {
                char* const newpos = eback() + newoff;

                if (pos_in)
                {
                    if (newpos > egptr())
                        setg(eback(), newpos, newpos);
                    else
                        setg(eback(), newpos, egptr());
                }

                if (pos_out)
                {
                    // min(pbase, newpos), newpos, epptr()
                    off_type const temp = epptr() - newpos;

                    if (pbase() < newpos)
                        setp(pbase(), epptr());
                    else
                        setp(newpos, epptr());

                    pbump(static_cast<int>((epptr() - pbase()) - temp));
                }

                return newoff;
            }
        }
    }

    return pos_type(-1);
}

inline void memstreambuf::Init(char* gnext, std::streamsize n, char* pbeg)
{
    if (pbeg == nullptr)
    {
        setg(gnext, gnext, gnext + n);
    }
    else
    {
        setg(gnext, gnext, pbeg);
        setp(pbeg, pbeg + n);
    }
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class imemstream : public std::istream
{
    using base_type = std::istream;

    memstreambuf sb_;

public:
    imemstream(char const* s, int n)
        : base_type(&sb_)
        , sb_(s, n)
    {
    }

    template <size_t N>
    explicit imemstream(char (&s)[N])
        : imemstream(s, static_cast<int>(N))
    {
    }

#if 0
    imemstream(imemstream&& rhs)
        : base_type(std::move(rhs))
        , sb_(std::move(rhs.sb_))
    {
        base_type::set_rdbuf(&sb_);
    }

    imemstream& operator =(imemstream&& rhs)
    {
        base_type::operator=(std::move(rhs));
        sb_ = std::move(rhs.sb_);
        return *this;
    }
#endif

    void swap(imemstream& rhs)
    {
        base_type::swap(rhs);
        sb_.swap(rhs.sb_);
    }

    memstreambuf* rdbuf() const
    {
        return const_cast<memstreambuf*>(&sb_);
    }

    std::string str() const
    {
        return sb_.str();
    }

    char const* data() const
    {
        return sb_.data();
    }

    size_t size() const
    {
        return sb_.size();
    }
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class omemstream : public std::ostream
{
    using base_type = std::ostream;

    memstreambuf sb_;

public:
    omemstream()
        : base_type(&sb_)
        , sb_(nullptr, 0, nullptr)
    {
    }

    omemstream(char* s, int n, std::ios_base::openmode mode = std::ios_base::out)
        : base_type(&sb_)
        , sb_(s, n, s + (mode & std::ios::app ? strlen(s) : 0))
    {
    }

    template <size_t N>
    explicit omemstream(char (&s)[N], std::ios_base::openmode mode = std::ios_base::out)
        : omemstream(s, static_cast<int>(N), mode)
    {
    }

#if 0
    omemstream(omemstream&& rhs)
        : base_type(std::move(rhs))
        , sb_(std::move(rhs.sb_))
    {
        base_type::set_rdbuf(&sb_);
    }

    omemstream& operator =(omemstream&& rhs)
    {
        base_type::operator=(std::move(rhs));
        sb_ = std::move(rhs.sb_);
        return *this;
    }
#endif

    void swap(omemstream& rhs)
    {
        base_type::swap(rhs);
        sb_.swap(rhs.sb_);
    }

    memstreambuf* rdbuf() const
    {
        return const_cast<memstreambuf*>(&sb_);
    }

    std::string str() const
    {
        return sb_.str();
    }

    char const* data() const
    {
        return sb_.data();
    }

    size_t size() const
    {
        return sb_.size();
    }
};

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

class memstream : public std::iostream
{
    using base_type = std::iostream;

    memstreambuf sb_;

public:
    memstream()
        : base_type(&sb_)
        , sb_(nullptr, 0, nullptr)
    {
    }

    memstream(char* s, int n, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        : base_type(&sb_)
        , sb_(s, n, s + (mode & std::ios::app ? strlen(s) : 0))
    {
    }

    template <size_t N>
    explicit memstream(char (&s)[N], std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        : memstream(s, static_cast<int>(N), mode)
    {
    }

#if 0
    memstream(memstream&& rhs)
        : base_type(std::move(rhs))
        , sb_(std::move(rhs.sb_))
    {
        base_type::set_rdbuf(&sb_);
    }

    memstream& operator =(memstream&& rhs)
    {
        base_type::operator=(std::move(rhs));
        sb_ = std::move(rhs.sb_);
        return *this;
    }
#endif

    void swap(memstream& rhs)
    {
        base_type::swap(rhs);
        sb_.swap(rhs.sb_);
    }

    memstreambuf* rdbuf() const
    {
        return const_cast<memstreambuf*>(&sb_);
    }

    std::string str() const
    {
        return sb_.str();
    }

    char const* data() const
    {
        return sb_.data();
    }

    size_t size() const
    {
        return sb_.size();
    }
};
