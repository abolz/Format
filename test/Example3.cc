#include "Format.h"
#include <vector>

struct VectorBuffer : public fmtxx::Writer
{
    std::vector<char> vec;

    bool Put(char c) override {
        vec.push_back(c);
        return true;
    }
    bool Write(char const* str, size_t len) override {
        vec.insert(vec.end(), str, str + len);
        return true;
    }
    bool Pad(char c, size_t count) override {
        vec.resize(vec.size() + count, c);
        return true;
    }
};

int main()
{
    VectorBuffer buf;

    fmtxx::format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}
}
