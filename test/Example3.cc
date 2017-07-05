#include "Format.h"
#include <vector>

struct VectorBuffer : public fmtxx::Writer
{
    std::vector<char> vec;

private:
    fmtxx::errc Put(char c) override {
        vec.push_back(c);
        return fmtxx::errc::success;
    }
    fmtxx::errc Write(char const* str, size_t len) override {
        vec.insert(vec.end(), str, str + len);
        return fmtxx::errc::success;
    }
    fmtxx::errc Pad(char c, size_t count) override {
        vec.resize(vec.size() + count, c);
        return fmtxx::errc::success;
    }
};

int main()
{
    VectorBuffer buf;

    fmtxx::format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}
}
