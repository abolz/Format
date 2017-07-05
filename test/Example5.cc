#include "Format_string.h"
#include <iostream>

int main()
{
    fmtxx::FormattingArgs args;

    const std::string str_world = "world";

    args.push_back(42);
    args.push_back("hello");
    args.push_back(str_world);
        // NOTE:
        // This does not compile:
        //      args.push_back(std::string("world"));

    std::cout << fmtxx::string_format("{} {} {}\n", args).str;
        // "42 hello world"
}
