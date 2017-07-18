#include "Format_string.h"

using namespace fmtxx;

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

ErrorCode fmtxx::StringWriter::Put(char c)
{
    str.push_back(c);
    return ErrorCode::success;
}

ErrorCode fmtxx::StringWriter::Write(char const* ptr, size_t len)
{
    str.append(ptr, len);
    return ErrorCode::success;
}

ErrorCode fmtxx::StringWriter::Pad(char c, size_t count)
{
    str.append(count, c);
    return ErrorCode::success;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

ErrorCode fmtxx::impl::DoFormat(std::string& str, cxx::string_view format, Arg const* args, Types types)
{
    StringWriter w{str};
    return fmtxx::impl::DoFormat(w, format, args, types);
}

ErrorCode fmtxx::impl::DoPrintf(std::string& str, cxx::string_view format, Arg const* args, Types types)
{
    StringWriter w{str};
    return fmtxx::impl::DoPrintf(w, format, args, types);
}
