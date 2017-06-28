#include "Format_ostream.h"
#include <cmath>
#include <iostream>

struct Vector2D {
    float x;
    float y;
};

template <>
struct fmtxx::FormatValue<Vector2D>
{
    auto operator()(Writer& w, FormatSpec const& spec, Vector2D const& value) const
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

int main()
{
    Vector2D vec { 3.0, 4.0 };

    fmtxx::format(std::cout, "{}\n", vec);
        // "(3, 4)"
    fmtxx::format(std::cout, "{:p}\n", vec);
        // "(r=5, phi=0.927)"
}
