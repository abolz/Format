#include "Format.h"
#include <vector>

struct VectorBuffer : public fmtxx::Writer
{
    std::vector<char> vec;

private:
    fmtxx::ErrorCode Put(char c) override {
        vec.push_back(c);
        return fmtxx::ErrorCode::success;
    }
    fmtxx::ErrorCode Write(char const* str, size_t len) override {
        vec.insert(vec.end(), str, str + len);
        return fmtxx::ErrorCode::success;
    }
    fmtxx::ErrorCode Pad(char c, size_t count) override {
        vec.resize(vec.size() + count, c);
        return fmtxx::ErrorCode::success;
    }
};

int main()
{
    VectorBuffer buf;

    fmtxx::format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}
}
