#if 1

#include "Format/Format.h"
#include <iostream>

int main()
{
    fmtxx::format(std::cout, "{1} {} {0} {}\n", 1, 2);
        // "2 1 1 2"
    fmtxx::format(std::cout, "{0:d} {0:x} {0:o} {0:b}\n", 42);
        // "42 2a 52 101010"
    fmtxx::format(std::cout, "{:-<16}\n", "left");
        // "left------------"
    fmtxx::format(std::cout, "{:.^16}\n", "center");
        // ".....center....."
    fmtxx::format(std::cout, "{:~>16}\n", "right");
        // "~~~~~~~~~~~right"
    fmtxx::format(std::cout, "{:s}\n", 3.1415927);
        // "3.1415927"
}

#endif

#if 0

#include "Format/Format.h"
#include <cmath>

struct Vector2D {
    float x;
    float y;
};

template <>
struct fmtxx::FormatValue<Vector2D>
{
    auto operator()(Writer& os, FormatSpec const& spec, Vector2D const& value) const
    {
        if (spec.conv == 'p' || spec.conv == 'P')
        {
            auto r   = std::hypot(value.x, value.y);
            auto phi = std::atan2(value.y, value.x);

            return fmtxx::format(os, "(r={:.3g}, phi={:.3g})", r, phi);
        }

        return fmtxx::format(os, "({}, {})", value.x, value.y);
    }
};

int main()
{
    Vector2D vec { 3.0, 4.0 };

    fmtxx::format(stdout, "{}\n", vec);
        // "(3, 4)"
    fmtxx::format(stdout, "{:p}\n", vec);
        // "(r=5, phi=0.927)"
}

#endif

#if 0

#include "Format/Format.h"
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

#endif
