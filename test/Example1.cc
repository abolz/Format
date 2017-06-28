#include "Format.h"
#include <cstdio>

int main()
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
