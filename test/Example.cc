#include "../src/Format.h"

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static void Example1()
{
    fmtxx::format(stdout, "{1} {} {0} {}\n", 1, 2);
        // "2 1 1 2"
    fmtxx::format(stdout, "{0:d} {0:x} {0:o} {0:b}\n", 42);
        // "42 2a 52 101010"
    fmtxx::format(stdout, "{:-<16}\n", "left");
        // "left------------"
    fmtxx::format(stdout, "{:.^16}\n", "center");
        // ".....center....."
    fmtxx::format(stdout, "{:~>16}\n", "right");
        // "~~~~~~~~~~~right"
    fmtxx::format(stdout, "{:s}\n", 3.1415927);
        // "3.1415927"
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

struct Vector2D {
    float x;
    float y;
};

namespace fmtxx
{
    template <>
    struct FormatValue<Vector2D>
    {
        ErrorCode operator()(Writer& w, FormatSpec const& spec, Vector2D const& value) const
        {
            if (spec.conv == 'p' || spec.conv == 'P')
            {
                auto r   = std::hypot(value.x, value.y);
                auto phi = std::atan2(value.y, value.x);

                return fmtxx::format(w, "(r={:.3g}, phi={:.3g})", r, phi);
            }

            return fmtxx::format(w, "({}, {})", value.x, value.y);
        }
    };
}

static void Example2()
{
    Vector2D vec { 3.0, 4.0 };

    fmtxx::format(std::cout, "{}\n", vec);
        // "(3, 4)"
    fmtxx::format(std::cout, "{:p}\n", vec);
        // "(r=5, phi=0.927)"
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

struct VectorBuffer : public fmtxx::Writer
{
    std::vector<char> vec;

private:
    fmtxx::ErrorCode Put(char c) override
    {
        vec.push_back(c);
        return fmtxx::ErrorCode::success;
    }

    fmtxx::ErrorCode Write(char const* str, size_t len) override
    {
        vec.insert(vec.end(), str, str + len);
        return fmtxx::ErrorCode::success;
    }

    fmtxx::ErrorCode Pad(char c, size_t count) override
    {
        vec.resize(vec.size() + count, c);
        return fmtxx::ErrorCode::success;
    }
};

static void Example3()
{
    VectorBuffer buf;

    fmtxx::format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static void Example4()
{
    std::vector<int> vec = {1, 2, 3, 4, 5};

    fmtxx::format(stdout, "{}\n", fmtxx::pretty(vec));
        // [1, 2, 3, 4, 5]

    std::map<std::string, int> map = {
        {"eins", 1},
        {"zwei", 2},
        {"drei", 3},
    };

    fmtxx::format(stdout, "{}\n", fmtxx::pretty(map));
        // [{"drei", 3}, {"eins", 1}, {"zwei", 2}]
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static void Example5()
{
    fmtxx::FormatArgs args;

    const std::string str_world = "world";

    args.push_back(42);
    args.push_back("hello");
    args.push_back(str_world);
        // NOTE:
        // This does not compile:
        // args.push_back(std::string("world"));

    std::cout << fmtxx::string_format("{} {} {}\n", args).str;
        // "42 hello world"
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

int main()
{
    Example1();
    Example2();
    Example3();
    Example4();
    Example5();
}
