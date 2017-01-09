#include "Format.h"
#include <iostream>

int main()
{
    fmtxx::Format(std::cout, "{1} {} {0} {}\n", 1, 2);
        // "2 1 1 2"
    fmtxx::Format(std::cout, "{0:d} {0:x} {0:o} {0:b}\n", 42);
        // "42 2a 52 101010"
    fmtxx::Format(std::cout, "{:<<16}\n", "left");
        // "left<<<<<<<<<<<<"
    fmtxx::Format(std::cout, "{:.^16}\n", "center");
        // ".....center....."
    fmtxx::Format(std::cout, "{:>>16}\n", "right");
        // ">>>>>>>>>>>right"
    fmtxx::Format(std::cout, "{:s}\n", 3.1415927);
        // "3.1415927"
        // Uses fast binary-to-decimal conversions (Grisu2)
        // and an output format similar to printf("%g")
}
