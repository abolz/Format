#if 1

#include "Format.h"
#include <iostream>

int main()
{
    fmtxx::Format(std::cout, "{1} {} {0} {}\n", 1, 2);
        // "2 1 1 2"
    fmtxx::Format(std::cout, "{0:d} {0:x} {0:o} {0:b}\n", 42);
        // "42 2a 52 101010"
    fmtxx::Format(std::cout, "{:-<16}\n", "left");
        // "left------------"
    fmtxx::Format(std::cout, "{:.^16}\n", "center");
        // ".....center....."
    fmtxx::Format(std::cout, "{:~>16}\n", "right");
        // "~~~~~~~~~~~right"
    fmtxx::Format(std::cout, "{:s}\n", 3.1415927);
        // "3.1415927"
        // Uses fast binary-to-decimal conversions (double-conversion)
        // and an output format similar to printf("%g")
}

#endif

#if 0

#include "Format.h"
#include <cmath>

struct Vector2D {
    float x;
    float y;
};

template <>
struct fmtxx::FormatValue<Vector2D>
{
    errc operator()(FormatBuffer& os, FormatSpec const& spec, Vector2D const& value) const
    {
        if (spec.conv == 'p' || spec.conv == 'P')
        {
            auto r   = std::hypot(value.x, value.y);
            auto phi = std::atan2(value.y, value.x);

            return Format(os, "(r={:.3g}, phi={:.3g})", r, phi);
        }

        return Format(os, "({}, {})", value.x, value.y);
    }
};

int main()
{
    Vector2D vec { 3.0, 4.0 };

    fmtxx::Format(stdout, "{}\n", vec);
        // "(3, 4)"
    fmtxx::Format(stdout, "{:p}\n", vec);
        // "(r=5, phi=0.927)"
}

#endif

#if 0

#include "Format.h"
#include <vector>

struct VectorBuffer : public fmtxx::FormatBuffer
{
    std::vector<char>& os;
    explicit VectorBuffer(std::vector<char>& v) : os(v) {}

    bool Put(char c) override {
        os.push_back(c);
        return true;
    }
    bool Write(char const* str, size_t len) override {
        os.insert(os.end(), str, str + len);
        return true;
    }
    bool Pad(char c, size_t count) override {
        os.resize(os.size() + count, c);
        return true;
    }
};

template <>
struct fmtxx::FormatValue<std::vector<char>>
{
    errc operator()(FormatBuffer& fb, FormatSpec const& spec, std::vector<char> const& vec) const 
    {
        // The "{*}" means: 
        // The formatting specification is given as an argument ('spec' here).
        return fmtxx::Format(fb, "{*}", spec, std::string_view(vec.data(), vec.size()));
    }
};

int main()
{
    std::vector<char> vec;
    fmtxx::FormatTo(VectorBuffer{vec}, "{:5}", -123);
        // out = {' ', '-', '1', '2', '3'}

    fmtxx::Format(stdout, "{}\n", vec);
        // " -123"
}

#endif
