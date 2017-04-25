#if 0
cl /EHsc Test.cc Format.cc /std:c++latest /Fe: Test.exe
g++ Test.cc Format.cc
#endif

#include "ext/Catch/include/catch_with_main.hpp"

#include "Format.h"

#include <cfloat>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <clocale>

struct FormatterResult
{
    std::string str;
    fmtxx::errc ec;
};

template <typename Fn>
struct StringFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        std::string os;
        const auto ec = Fn{}(os, format, args...);
        return { os, ec };
    }
};

template <typename Fn>
struct StreamFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        std::ostringstream os;
        const auto ec = Fn{}(os, format, args...);
        return { os.str(), ec };
    }
};

#ifdef __linux__
template <typename Fn>
struct FILEFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        char buf[1000] = {0};
        FILE* f = fmemopen(buf, sizeof(buf), "w");
        const auto ec = Fn{}(f, format, args...);
        fclose(f); // flush!
        return { std::string(buf), ec };
    }
};
#endif

template <typename Fn>
struct CharArrayFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        char buf[500];
        fmtxx::CharArray os { buf };
        const auto ec = Fn{}(os, format, args...);
        return { std::string(buf, os.next), ec };
    }
};

template <typename Formatter, typename ...Args>
static std::string FormatArgs1(std::string_view format, Args const&... args)
{
    FormatterResult res = Formatter{}(format, args...);
    assert(res.ec == fmtxx::errc::success);
    return res.str;
}

struct FormatFn {
    template <typename Buffer, typename ...Args>
    auto operator()(Buffer& fb, std::string_view format, Args const&... args) const {
        return fmtxx::Format(fb, format, args...);
    }
};

struct PrintfFn {
    template <typename Buffer, typename ...Args>
    auto operator()(Buffer& fb, std::string_view format, Args const&... args) const {
        return fmtxx::Printf(fb, format, args...);
    }
};

template <typename Fn, typename ...Args>
static std::string FormatArgsTemplate(std::string_view format, Args const&... args)
{
    std::string const s1 = FormatArgs1<StringFormatter<Fn>>(format, args...);

    std::string const s2 = FormatArgs1<StreamFormatter<Fn>>(format, args...);
    if (s2 != s1)
        return "[[[[ formatter mismatch ]]]]";

#ifdef __linux__
    std::string const s3 = FormatArgs1<FILEFormatter<Fn>>(format, args...);
    if (s3 != s1)
        return "[[[[ formatter mismatch ]]]]";
#endif

    std::string const s4 = FormatArgs1<CharArrayFormatter<Fn>>(format, args...);
    if (s4 != s1)
        return "[[[[ formatter mismatch ]]]]";

    return s1;
}

template <typename ...Args>
static std::string FormatArgs(std::string_view format, Args const&... args)
{
    return FormatArgsTemplate<FormatFn>(format, args...);
}

template <typename ...Args>
static std::string PrintfArgs(std::string_view format, Args const&... args)
{
    return FormatArgsTemplate<PrintfFn>(format, args...);
}

template <typename ...Args>
static fmtxx::errc FormatErr(std::string_view format, Args const&... args)
{
    std::string sink;
    return fmtxx::Format(sink, format, args...);
}

TEST_CASE("Invalid", "1")
{
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr(std::string_view("{*}", 1), 0, 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr(std::string_view("{*}", 2), 0, 0));
    REQUIRE(fmtxx::errc::invalid_argument      == FormatErr("{*}", 1));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{*", fmtxx::FormatSpec{}, 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{*}}", fmtxx::FormatSpec{}, 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr(std::string_view("{}", 1), 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr(std::string_view("{1}", 2), 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1.", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1.1", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1.1f", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{ ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1 ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1: ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1 ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1. ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1.1 ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{1:1.1f ", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{-1: >10.2f}", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{:*10}", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{-10}", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{{}", 1)); // stray '}'
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{}}", 1)); // stray '}'
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("}", 1, 1, 1, 1, 1));
    REQUIRE(fmtxx::errc::index_out_of_range    == FormatErr("{1}", 1));
    REQUIRE(fmtxx::errc::index_out_of_range    == FormatErr("{1}{2}", 1, 2));
    REQUIRE(fmtxx::errc::index_out_of_range    == FormatErr("{0}{2}", 1, 2));
    REQUIRE(fmtxx::errc::index_out_of_range    == FormatErr("{10}", 1));
    REQUIRE(fmtxx::errc::index_out_of_range    == FormatErr("{2147483647}", 1));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{2147483648}", 0));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{99999999999}", 1));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{:99999999999.0}", 1));
    REQUIRE(fmtxx::errc::invalid_format_string == FormatErr("{:.", 0));
}

TEST_CASE("General", "0")
{
    SECTION("Format")
    {
        REQUIRE("Hello"                           == FormatArgs("Hello",                                    0));
        REQUIRE("Count to 10"                     == FormatArgs("Count to {0}",                             10));
        REQUIRE("Bring me a beer"                 == FormatArgs("Bring me a {}",                            "beer"));
        REQUIRE("From 0 to 10"                    == FormatArgs("From {} to {}",                            0, 10));
        REQUIRE("From 0 to 10"                    == FormatArgs("From {1} to {0}",                          10, 0));
        REQUIRE("dec:42 hex:2a oct:52 bin:101010" == FormatArgs("dec:{0:d} hex:{0:x} oct:{0:o} bin:{0:b}",  42));
        REQUIRE("left<<<<<<<<<<<<"                == FormatArgs("{:<<16}",                                  "left"));
        REQUIRE(".....center....."                == FormatArgs("{:.^16}",                                  "center"));
        REQUIRE(">>>>>>>>>>>right"                == FormatArgs("{:>>16}",                                  "right"));
        REQUIRE("2 1 1 2"                         == FormatArgs("{1} {} {0} {}",                            1, 2));
    }
    SECTION("Printf")
    {
        REQUIRE("Hello"                           == PrintfArgs("Hello",                                    0));
        REQUIRE("Bring me a beer"                 == PrintfArgs("Bring me a %s",                            "beer"));
        REQUIRE("From 0 to 10"                    == PrintfArgs("From %s to %s",                            0, 10));
        REQUIRE("dec:42 hex:2a oct:52 bin:101010" == PrintfArgs("dec:%1$d hex:%1$x oct:%1$o bin:%1$b",      42));
        REQUIRE("left            "                == PrintfArgs("%-16s",                                    "left"));
    }
}

TEST_CASE("String", "1")
{
    REQUIRE(""  == FormatArgs(""));
    REQUIRE("x" == FormatArgs("x"));
    REQUIRE("{" == FormatArgs("{{"));
    REQUIRE("}" == FormatArgs("}}"));

    REQUIRE("hello %" == PrintfArgs("hello %%"));
    REQUIRE("% hello" == PrintfArgs("%% hello"));
    REQUIRE("hello % hello" == PrintfArgs("hello %% hello"));

    REQUIRE("x" == FormatArgs("{}", 'x'));
    REQUIRE("x" == FormatArgs("{:.0}", 'x'));

    REQUIRE("     xxx" == FormatArgs("{:8}", "xxx"));
    REQUIRE("     xxx" == FormatArgs("{:>8}", "xxx"));
    REQUIRE("xxx     " == FormatArgs("{:<8}", "xxx"));
    REQUIRE("  xxx   " == FormatArgs("{:^8}", "xxx"));

    REQUIRE(":Hello, world!:"       == FormatArgs(":{}:",         "Hello, world!"));
	REQUIRE(":  Hello, world!:"     == FormatArgs(":{:15}:",      "Hello, world!"));
	REQUIRE(":Hello, wor:"          == FormatArgs(":{:.10}:",     "Hello, world!"));
	REQUIRE(":Hello, world!:"       == FormatArgs(":{:<10}:",     "Hello, world!"));
	REQUIRE(":Hello, world!  :"     == FormatArgs(":{:<15}:",     "Hello, world!"));
	REQUIRE(":Hello, world!:"       == FormatArgs(":{:.15}:",     "Hello, world!"));
	REQUIRE(":     Hello, wor:"     == FormatArgs(":{:15.10}:",   "Hello, world!"));
	REQUIRE(":Hello, wor     :"     == FormatArgs(":{:<15.10}:",  "Hello, world!"));

    REQUIRE(":Hello, world!:"       == PrintfArgs(":%s:",         "Hello, world!"));
	REQUIRE(":  Hello, world!:"     == PrintfArgs(":%15s:",      "Hello, world!"));
	REQUIRE(":Hello, wor:"          == PrintfArgs(":%.10s:",     "Hello, world!"));
	REQUIRE(":Hello, world!:"       == PrintfArgs(":%-10s:",     "Hello, world!"));
	REQUIRE(":Hello, world!  :"     == PrintfArgs(":%-15s:",     "Hello, world!"));
	REQUIRE(":Hello, world!:"       == PrintfArgs(":%.15s:",     "Hello, world!"));
	REQUIRE(":     Hello, wor:"     == PrintfArgs(":%15.10s:",   "Hello, world!"));
	REQUIRE(":Hello, wor     :"     == PrintfArgs(":%-15.10s:",  "Hello, world!"));

    std::string str = "hello hello hello hello hello hello hello hello hello hello ";
    REQUIRE("hello hello hello hello hello hello hello hello hello hello " == FormatArgs("{}", str));

    REQUIRE(">---<" == FormatArgs(">{}<", "---"));
    REQUIRE("<--->" == FormatArgs("<{}>", "---"));
    REQUIRE(">---<" == FormatArgs(">{0}<", "---"));
    REQUIRE("<--->" == FormatArgs("<{0}>", "---"));
    REQUIRE(">---<" == FormatArgs(">{0:s}<", "---"));
    REQUIRE("<--->" == FormatArgs("<{0:s}>", "---"));

    REQUIRE(">--->" == FormatArgs(">{0:}<s}>", "---"));
    REQUIRE("<---<" == FormatArgs("<{0:}>s}<", "---"));
    REQUIRE("^---^" == FormatArgs("^{0:}^s}^", "---"));

    REQUIRE("(null)"     == FormatArgs("{}",      (char*)0));
    REQUIRE("(null)"     == FormatArgs("{}",      (char const*)0));
    REQUIRE("(null)"     == FormatArgs("{:.3}",   (char const*)0));
    REQUIRE("(null)"     == FormatArgs("{:.10}",  (char const*)0));
    REQUIRE("(null)"     == FormatArgs("{:3.3}",  (char const*)0));
    REQUIRE("    (null)" == FormatArgs("{:10.3}", (char const*)0));

    REQUIRE("(null)"     == PrintfArgs("%s",      (char*)0));
    REQUIRE("(null)"     == PrintfArgs("%s",      (char const*)0));
    REQUIRE("(null)"     == PrintfArgs("%.3s",   (char const*)0));
    REQUIRE("(null)"     == PrintfArgs("%.10s",  (char const*)0));
    REQUIRE("(null)"     == PrintfArgs("%3.3s",  (char const*)0));
    REQUIRE("    (null)" == PrintfArgs("%10.3s", (char const*)0));

    std::string spad = std::string(128, ' ');
    REQUIRE(spad.c_str() == FormatArgs("{:128}", ' '));
}

TEST_CASE("Ints", "1")
{
    REQUIRE("2 1 1 2" == FormatArgs("{1} {} {0} {}", 1, 2));

    static const int V = 0x12345;

    REQUIRE("74565"     == PrintfArgs("%s",     V));
    REQUIRE("74565"     == PrintfArgs("%hhs",     V));
    REQUIRE("74565"     == PrintfArgs("%hs",     V));
    REQUIRE("74565"     == PrintfArgs("%ls",     V));
    REQUIRE("74565"     == PrintfArgs("%lls",     V));
    REQUIRE("74565"     == PrintfArgs("%js",     V));
    REQUIRE("74565"     == PrintfArgs("%zs",     V));
    REQUIRE("74565"     == PrintfArgs("%ts",     V));
    REQUIRE("74565"     == PrintfArgs("%Ls",     V));

    REQUIRE("74565"     == FormatArgs("{}",     V));
    REQUIRE("-74565"    == FormatArgs("{}",    -V));
    REQUIRE(" 74565"    == FormatArgs("{: }",   V));
    REQUIRE("-74565"    == FormatArgs("{: }",  -V));
    REQUIRE("74565"     == FormatArgs("{:-}",   V));
    REQUIRE("-74565"    == FormatArgs("{:-}",  -V));
    REQUIRE("+74565"    == FormatArgs("{:+}",   V));
    REQUIRE("-74565"    == FormatArgs("{:+}",  -V));

    REQUIRE("hello 74565     " == FormatArgs("hello {:<10}",    V));
    REQUIRE("hello -74565    " == FormatArgs("hello {:<10}",   -V));
    REQUIRE("hello  74565    " == FormatArgs("hello {:< 10}",   V));
    REQUIRE("hello -74565    " == FormatArgs("hello {:< 10}",  -V));
    REQUIRE("hello 74565     " == FormatArgs("hello {:<-10}",   V));
    REQUIRE("hello -74565    " == FormatArgs("hello {:<-10}",  -V));
    REQUIRE("hello +74565    " == FormatArgs("hello {:<+10}",   V));
    REQUIRE("hello -74565    " == FormatArgs("hello {:<+10}",  -V));

    REQUIRE("     74565" == FormatArgs("{:>10}",    V));
    REQUIRE("    -74565" == FormatArgs("{:>10}",   -V));
    REQUIRE("     74565" == FormatArgs("{:> 10}",   V));
    REQUIRE("    -74565" == FormatArgs("{:> 10}",  -V));
    REQUIRE("     74565" == FormatArgs("{:>-10}",   V));
    REQUIRE("    -74565" == FormatArgs("{:>-10}",  -V));
    REQUIRE("    +74565" == FormatArgs("{:>+10}",   V));
    REQUIRE("    -74565" == FormatArgs("{:>+10}",  -V));

    REQUIRE("  74565   " == FormatArgs("{:^10}",    V));
    REQUIRE("  -74565  " == FormatArgs("{:^10}",   -V));
    REQUIRE("   74565  " == FormatArgs("{:^ 10}",   V));
    REQUIRE("  -74565  " == FormatArgs("{:^ 10}",  -V));
    REQUIRE("  74565   " == FormatArgs("{:^-10}",   V));
    REQUIRE("  -74565  " == FormatArgs("{:^-10}",  -V));
    REQUIRE("  +74565  " == FormatArgs("{:^+10}",   V));
    REQUIRE("  -74565  " == FormatArgs("{:^+10}",  -V));

    REQUIRE("0000074565" == FormatArgs("{: <010}",    V));
    REQUIRE("-000074565" == FormatArgs("{: <010}",   -V));
    REQUIRE(" 000074565" == FormatArgs("{: < 010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: < 010}",  -V));
    REQUIRE("0000074565" == FormatArgs("{: <-010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: <-010}",  -V));
    REQUIRE("+000074565" == FormatArgs("{: <+010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: <+010}",  -V));

    REQUIRE("0000074565" == PrintfArgs("%-010s",    V));
    REQUIRE("-000074565" == PrintfArgs("%-010s",   -V));
    REQUIRE(" 000074565" == PrintfArgs("%- 010s",   V));
    REQUIRE("-000074565" == PrintfArgs("%- 010s",  -V));
    REQUIRE("0000074565" == PrintfArgs("%--010s",   V));
    REQUIRE("-000074565" == PrintfArgs("%--010s",  -V));
    REQUIRE("+000074565" == PrintfArgs("%-+010s",   V));
    REQUIRE("-000074565" == PrintfArgs("%-+010s",  -V));
    REQUIRE("+000074565" == PrintfArgs("%-+ 010s",   V)); // If the space and + flags both appear, the space flag is ignored.
    REQUIRE("-000074565" == PrintfArgs("%-+ 010s",  -V)); // If the space and + flags both appear, the space flag is ignored.
    REQUIRE("+000074565" == PrintfArgs("%- +010s",   V)); // If the space and + flags both appear, the space flag is ignored.
    REQUIRE("-000074565" == PrintfArgs("%- +010s",  -V)); // If the space and + flags both appear, the space flag is ignored.

    REQUIRE("0000074565" == FormatArgs("{: =010}",    V));
    REQUIRE("-000074565" == FormatArgs("{: =010}",   -V));
    REQUIRE(" 000074565" == FormatArgs("{: = 010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: = 010}",  -V));
    REQUIRE("0000074565" == FormatArgs("{: =-010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: =-010}",  -V));
    REQUIRE("+000074565" == FormatArgs("{: =+010}",   V));
    REQUIRE("-000074565" == FormatArgs("{: =+010}",  -V));

    REQUIRE("0000074565" == FormatArgs("{:010}",     V));
    REQUIRE("-000074565" == FormatArgs("{:010}",    -V));
    REQUIRE("0745650000" == FormatArgs("{:0< 10}",    V));
    REQUIRE("-745650000" == FormatArgs("{:0< 10}",   -V));

    REQUIRE("2147483647"            == FormatArgs("{}", INT_MAX));
    REQUIRE("-2147483648"           == FormatArgs("{}", INT_MIN));
    REQUIRE("9223372036854775807"   == FormatArgs("{}", INT64_MAX));
    REQUIRE("-9223372036854775808"  == FormatArgs("{}", INT64_MIN));

    REQUIRE("1"    == FormatArgs("{:x}", (signed char) 1));
    REQUIRE("ff"   == FormatArgs("{:x}", (signed char)-1));
    REQUIRE("1"    == FormatArgs("{:x}", (signed short) 1));
    REQUIRE("ffff" == FormatArgs("{:x}", (signed short)-1));

    REQUIRE("12345"    == FormatArgs("{:x}",      V));
    REQUIRE("fffedcbb" == FormatArgs("{:x}",     -V));
    REQUIRE("00012345" == FormatArgs("{:08x}",    V));
    REQUIRE("fffedcbb" == FormatArgs("{:08x}",   -V));

#if LONG_MAX != INT_MAX
    REQUIRE("12345"            ==  FormatArgs("{:x}",      (signed long)V));
    REQUIRE("fffffffffffedcbb" ==  FormatArgs("{:x}",     -(signed long)V));
    REQUIRE("00012345"         ==  FormatArgs("{:08x}",    (signed long)V));
    REQUIRE("fffffffffffedcbb" ==  FormatArgs("{:08x}",   -(signed long)V));
#endif

    REQUIRE("12345"            == FormatArgs("{:x}",   (signed long long) V));
    REQUIRE("fffffffffffedcbb" == FormatArgs("{:x}",   (signed long long)-V));
    REQUIRE("12345"            == FormatArgs("{:X}",   (signed long long) V));
    REQUIRE("FFFFFFFFFFFEDCBB" == FormatArgs("{:X}",   (signed long long)-V));

    REQUIRE("1'234'567'890" == FormatArgs("{:'13}", 1234567890));
    REQUIRE("  123'456'789" == FormatArgs("{:'13}", 123456789));
    REQUIRE("   12'345'678" == FormatArgs("{:'13}", 12345678));
    REQUIRE("    1'234'567" == FormatArgs("{:'13}", 1234567));
    REQUIRE("      123'456" == FormatArgs("{:'13}", 123456));
    REQUIRE("       12'345" == FormatArgs("{:'13}", 12345));
    REQUIRE("        1'234" == FormatArgs("{:'13}", 1234));
    REQUIRE("          123" == FormatArgs("{:'13}", 123));
    REQUIRE("           12" == FormatArgs("{:'13}", 12));
    REQUIRE("            1" == FormatArgs("{:'13}", 1));
    REQUIRE("1_234_567_890" == FormatArgs("{:_13}", 1234567890));
    REQUIRE("  123_456_789" == FormatArgs("{:_13}", 123456789));
    REQUIRE("   12_345_678" == FormatArgs("{:_13}", 12345678));
    REQUIRE("    1_234_567" == FormatArgs("{:_13}", 1234567));
    REQUIRE("      123_456" == FormatArgs("{:_13}", 123456));
    REQUIRE("       12_345" == FormatArgs("{:_13}", 12345));
    REQUIRE("        1_234" == FormatArgs("{:_13}", 1234));
    REQUIRE("          123" == FormatArgs("{:_13}", 123));
    REQUIRE("           12" == FormatArgs("{:_13}", 12));
    REQUIRE("            1" == FormatArgs("{:_13}", 1));
    REQUIRE("18_446_744_073_709_551_615" == FormatArgs("{:_}", UINT64_MAX));

    REQUIRE("1234'5678" == FormatArgs("{:'9x}", 0x12345678));
    REQUIRE(" 123'4567" == FormatArgs("{:'9x}", 0x1234567));
    REQUIRE("  12'3456" == FormatArgs("{:'9x}", 0x123456));
    REQUIRE("   1'2345" == FormatArgs("{:'9x}", 0x12345));
    REQUIRE("     1234" == FormatArgs("{:'9x}", 0x1234));
    REQUIRE("      123" == FormatArgs("{:'9x}", 0x123));
    REQUIRE("       12" == FormatArgs("{:'9x}", 0x12));
    REQUIRE("        1" == FormatArgs("{:'9x}", 0x1));

    REQUIRE("7777_7777" == FormatArgs("{:_9o}", 077777777));
    REQUIRE(" 777_7777" == FormatArgs("{:_9o}", 07777777));
    REQUIRE("  77_7777" == FormatArgs("{:_9o}", 0777777));
    REQUIRE("   7_7777" == FormatArgs("{:_9o}", 077777));
    REQUIRE("     7777" == FormatArgs("{:_9o}", 07777));
    REQUIRE("      777" == FormatArgs("{:_9o}", 0777));
    REQUIRE("       77" == FormatArgs("{:_9o}", 077));
    REQUIRE("        7" == FormatArgs("{:_9o}", 07));
    REQUIRE("        0" == FormatArgs("{:_9o}", 0));

    REQUIRE("1111_1111" == FormatArgs("{:_9b}", 0xFF));
    REQUIRE(" 111_1111" == FormatArgs("{:_9b}", 0x7F));
    REQUIRE("  11_1111" == FormatArgs("{:_9b}", 0x3F));
    REQUIRE("   1_1111" == FormatArgs("{:_9b}", 0x1F));
    REQUIRE("     1111" == FormatArgs("{:_9b}", 0x0F));
    REQUIRE("      111" == FormatArgs("{:_9b}", 0x07));
    REQUIRE("       11" == FormatArgs("{:_9b}", 0x03));
    REQUIRE("        1" == FormatArgs("{:_9b}", 0x01));
    REQUIRE("        0" == FormatArgs("{:_9b}", 0x00));
    REQUIRE("1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111" == FormatArgs("{:_b}", UINT64_MAX));

    REQUIRE("4294966062" == FormatArgs("{:u}", -1234));
    REQUIRE("4294966062" == FormatArgs("{: u}", -1234));
    REQUIRE("4294966062" == FormatArgs("{:+u}", -1234));
    REQUIRE("4294966062" == FormatArgs("{:-u}", -1234));
    REQUIRE("18446744073709550382" == FormatArgs("{:u}", -1234ll));

    REQUIRE("0"          == FormatArgs("{:x}",  0));
    REQUIRE("0"          == FormatArgs("{:b}",  0));
    REQUIRE("0"          == FormatArgs("{:o}",  0));
    REQUIRE("1"          == FormatArgs("{:x}",  1));
    REQUIRE("1"          == FormatArgs("{:b}",  1));
    REQUIRE("1"          == FormatArgs("{:o}",  1));
    REQUIRE("0x0"        == FormatArgs("{:#x}", 0));
    REQUIRE("0b0"        == FormatArgs("{:#b}", 0));
    REQUIRE("0"          == FormatArgs("{:#o}", 0));
    REQUIRE("0x1"        == FormatArgs("{:#x}", 1));
    REQUIRE("0b1"        == FormatArgs("{:#b}", 1));
    REQUIRE("01"         == FormatArgs("{:#o}", 1));
    REQUIRE("0x00000000" == FormatArgs("{:#010x}", 0));
    REQUIRE("0b00000000" == FormatArgs("{:#010b}", 0));
    REQUIRE("0000000000" == FormatArgs("{:#010o}", 0));
    REQUIRE("0x00000001" == FormatArgs("{:#010x}", 1));
    REQUIRE("0b00000001" == FormatArgs("{:#010b}", 1));
    REQUIRE("0000000001" == FormatArgs("{:#010o}", 1));
    REQUIRE("       0x0" == FormatArgs("{:#10x}",  0));
    REQUIRE("       0b0" == FormatArgs("{:#10b}",  0));
    REQUIRE("         0" == FormatArgs("{:#10o}",  0));
    REQUIRE("       0x1" == FormatArgs("{:#10x}",  1));
    REQUIRE("       0b1" == FormatArgs("{:#10b}",  1));
    REQUIRE("        01" == FormatArgs("{:#10o}",  1));
}

TEST_CASE("Floats", "1")
{
    REQUIRE(
        "243546080556034731077856379609316893158278902575447060151047"
        "212703405344938119816206067372775299130836050315842578309818"
        "316450894337978612745889730079163798234256495613858256849283"
        "467066859489192118352020514036083287319232435355752493038825"
        "828481044358810649108367633313557305310641892225870327827273"
        "41408256.000000" == FormatArgs("{:f}", 2.4354608055603473e+307));

    static const double PI  = 3.1415926535897932384626433832795;

    REQUIRE("0.000000"  == FormatArgs("{:f}",   0.0));
    REQUIRE("-0.000000" == FormatArgs("{:f}",  -0.0));
    REQUIRE(" 0.000000" == FormatArgs("{: f}",  0.0));
    REQUIRE("-0.000000" == FormatArgs("{: f}", -0.0));
    REQUIRE("+0.000000" == FormatArgs("{:+f}",  0.0));
    REQUIRE("-0.000000" == FormatArgs("{:+f}", -0.0));

    REQUIRE("0"            == FormatArgs("{:.0f}", 0.0));
    REQUIRE("0.0"          == FormatArgs("{:.1f}", 0.0));
    REQUIRE("0.000000e+00" == FormatArgs("{:e}", 0.0));
    REQUIRE("0e+00"        == FormatArgs("{:.0e}", 0.0));
    REQUIRE("0.0e+00"      == FormatArgs("{:.1e}", 0.0));

    REQUIRE("3.141593"  == FormatArgs("{:f}", PI));
    REQUIRE("-3.141593" == FormatArgs("{:f}", -PI));
    REQUIRE("3.14"      == FormatArgs("{:.2f}", PI));
    REQUIRE("-3.14"     == FormatArgs("{:.2f}", -PI));
    REQUIRE("3.142"     == FormatArgs("{:.3f}", PI));
    REQUIRE("-3.142"    == FormatArgs("{:.3f}", -PI));

    REQUIRE("      3.141593" == FormatArgs("{:14f}",        PI));
    REQUIRE("     -3.141593" == FormatArgs("{:14f}",       -PI));
    REQUIRE("3.141593::::::" == FormatArgs("{::<14f}",      PI));
    REQUIRE("-3.141593:::::" == FormatArgs("{::<14f}",     -PI));
    REQUIRE("*3.141593*****" == FormatArgs("{:*< 14f}",     PI));
    REQUIRE("-3.141593*****" == FormatArgs("{:*< 14f}",    -PI));
    REQUIRE("+3.141593~~~~~" == FormatArgs("{:~<+14f}",     PI));
    REQUIRE("-3.141593~~~~~" == FormatArgs("{:~<+14f}",    -PI));
    REQUIRE("~~~~~~3.141593" == FormatArgs("{:~>14f}",      PI));
    REQUIRE("~~~~~-3.141593" == FormatArgs("{:~>14f}",     -PI));
    REQUIRE("~~~~~~3.141593" == FormatArgs("{:~> 14f}",     PI));
    REQUIRE("~~~~~-3.141593" == FormatArgs("{:~> 14f}",    -PI));
    REQUIRE("   3.141593   " == FormatArgs("{: ^ 14f}",     PI));
    REQUIRE("  -3.141593   " == FormatArgs("{: ^ 14f}",    -PI));
    REQUIRE("...3.141593..." == FormatArgs("{:.^ 14f}",     PI));
    REQUIRE("..-3.141593..." == FormatArgs("{:.^ 14f}",    -PI));
    REQUIRE("..+3.141593..." == FormatArgs("{:.^+14f}",     PI));
    REQUIRE("..-3.141593..." == FormatArgs("{:.^+14f}",    -PI));

    // zero flag means align sign left
    REQUIRE("0000003.141593" == FormatArgs("{:014f}",      PI));
    REQUIRE("-000003.141593" == FormatArgs("{:014f}",     -PI));
    REQUIRE("+000003.141593" == FormatArgs("{:+014f}",     PI));
    REQUIRE("-000003.141593" == FormatArgs("{:+014f}",    -PI));
    REQUIRE(" 000003.141593" == FormatArgs("{: 014f}",     PI));
    REQUIRE("-000003.141593" == FormatArgs("{: 014f}",    -PI));
    REQUIRE("3.141593000000" == FormatArgs("{:0<14f}",     PI));
    REQUIRE("-3.14159300000" == FormatArgs("{:0<14f}",    -PI));
    REQUIRE("+3.14159300000" == FormatArgs("{:0<+14f}",    PI));
    REQUIRE("-3.14159300000" == FormatArgs("{:0<+14f}",   -PI));
    REQUIRE("......3.141593" == FormatArgs("{:.=14f}",     PI));
    REQUIRE("-.....3.141593" == FormatArgs("{:.=14f}",    -PI));
    REQUIRE("+.....3.141593" == FormatArgs("{:.=+14f}",    PI));
    REQUIRE("-.....3.141593" == FormatArgs("{:.=+14f}",   -PI));
    REQUIRE("......3.141593" == FormatArgs("{:.= 14f}",    PI));
    REQUIRE("-.....3.141593" == FormatArgs("{:.= 14f}",   -PI));
    REQUIRE("3.141593......" == FormatArgs("{:.<14f}",     PI));
    REQUIRE("-3.141593....." == FormatArgs("{:.<14f}",    -PI));
    REQUIRE("+3.141593....." == FormatArgs("{:.<+14f}",    PI));
    REQUIRE("-3.141593....." == FormatArgs("{:.<+14f}",   -PI));
    REQUIRE(".3.141593....." == FormatArgs("{:.< 14f}",    PI));
    REQUIRE("-3.141593....." == FormatArgs("{:.< 14f}",   -PI));

    REQUIRE("0.010000" == FormatArgs("{:f}", 0.01));

    REQUIRE("1.000000"     == FormatArgs("{:f}",  1.0));
    REQUIRE("1.000000e+00" == FormatArgs("{:e}",  1.0));
    REQUIRE("1.000000E+00" == FormatArgs("{:E}",  1.0));
    REQUIRE("1"            == FormatArgs("{:g}",  1.0));

    REQUIRE("1.200000"     == FormatArgs("{:f}",  1.2));
    REQUIRE("1.200000e+00" == FormatArgs("{:e}",  1.2));
    REQUIRE("1.200000E+00" == FormatArgs("{:E}",  1.2));
    REQUIRE("1.2"          == FormatArgs("{:g}",  1.2));

    REQUIRE("1.234568"         == FormatArgs("{:'f}", 1.23456789));
    REQUIRE("12.345679"        == FormatArgs("{:'f}", 12.3456789));
    REQUIRE("123.456789"       == FormatArgs("{:'f}", 123.456789));
    REQUIRE("1'234.567890"     == FormatArgs("{:'f}", 1234.56789));
    REQUIRE("12'345.678900"    == FormatArgs("{:'f}", 12345.6789));
    REQUIRE("123'456.789000"   == FormatArgs("{:'f}", 123456.789));
    REQUIRE("1'234'567.890000" == FormatArgs("{:'f}", 1234567.89));

    REQUIRE("123456.789000" == FormatArgs("{:f}",  123456.789));
    REQUIRE("1.234568e+05"  == FormatArgs("{:e}",  123456.789));
    REQUIRE("1.235e+05"     == FormatArgs("{:.3e}",  123456.789));
    REQUIRE("1.234568E+05"  == FormatArgs("{:E}",  123456.789));
    REQUIRE("123457"        == FormatArgs("{:g}",  123456.789));
    REQUIRE("1.23e+05"      == FormatArgs("{:.3g}",  123456.789));
    REQUIRE("    1.23e+05"  == FormatArgs("{:12.3g}",  123456.789));
    REQUIRE("1.23e+05    "  == FormatArgs("{:<12.3g}",  123456.789));
    REQUIRE("  1.23e+05  "  == FormatArgs("{:^12.3g}",  123456.789));
    REQUIRE("   -1.23e+05"  == FormatArgs("{:-12.3g}",  -123456.789));
    REQUIRE("-1.23e+05   "  == FormatArgs("{:<-12.3g}",  -123456.789));
    REQUIRE(" -1.23e+05  "  == FormatArgs("{:^-12.3g}",  -123456.789));

    REQUIRE("12345.678900" == FormatArgs("{:f}",  12345.6789));
    REQUIRE("1.234568e+04" == FormatArgs("{:e}",  12345.6789));
    REQUIRE("1.235e+04"    == FormatArgs("{:.3e}",  12345.6789));
    REQUIRE("1.234568E+04" == FormatArgs("{:E}",  12345.6789));
    REQUIRE("12345.7"      == FormatArgs("{:g}",  12345.6789));
    REQUIRE("1.23e+04"     == FormatArgs("{:.3g}",  12345.6789));

    REQUIRE("0"                    == FormatArgs("{:s}", 0.0));
    REQUIRE("10"                   == FormatArgs("{:s}", 10.0));
    REQUIRE("10"                   == FormatArgs("{:S}", 10.0));
    REQUIRE("-0"                   == FormatArgs("{:s}", -0.0));
    REQUIRE("0p+0"                 == FormatArgs("{:x}", 0.0));
    REQUIRE("0P+0"                 == FormatArgs("{:X}", 0.0));
    REQUIRE("-0p+0"                == FormatArgs("{:x}", -0.0));
    REQUIRE("1.8p+0"               == FormatArgs("{:x}", 1.5));
    REQUIRE("0x1.8000p+0"          == FormatArgs("{:.4a}", 1.5));
    REQUIRE("1p+1"                 == FormatArgs("{:.0x}", 1.5));
    REQUIRE("0x2p+0"               == FormatArgs("{:.0a}", 1.5));
    REQUIRE("0x1.921fb5a7ed197p+1" == FormatArgs("{:a}", 3.1415927));
    REQUIRE("0X1.921FB5A7ED197P+1" == FormatArgs("{:A}", 3.1415927));
    REQUIRE("0x1.922p+1"           == FormatArgs("{:.3a}", 3.1415927));
    REQUIRE("0x1.9220p+1"          == FormatArgs("{:.4a}", 3.1415927));
    REQUIRE("0x1.921fbp+1"         == FormatArgs("{:.5a}", 3.1415927));
    REQUIRE("      0x1.922p+1"     == FormatArgs("{:16.3a}", 3.1415927));
    REQUIRE("     0x1.9220p+1"     == FormatArgs("{:16.4a}", 3.1415927));
    REQUIRE("    0x1.921fbp+1"     == FormatArgs("{:16.5a}", 3.1415927));
    REQUIRE("0x0000001.922p+1"     == FormatArgs("{:016.3a}", 3.1415927));
    REQUIRE("0x000001.9220p+1"     == FormatArgs("{:016.4a}", 3.1415927));
    REQUIRE("0x00001.921fbp+1"     == FormatArgs("{:016.5a}", 3.1415927));
    REQUIRE("     -0x1.500p+5"     == FormatArgs("{:16.3a}", -42.0));
    REQUIRE("    -0x1.5000p+5"     == FormatArgs("{:16.4a}", -42.0));
    REQUIRE("   -0x1.50000p+5"     == FormatArgs("{:16.5a}", -42.0));
    REQUIRE("-0x000001.500p+5"     == FormatArgs("{:016.3a}", -42.0));
    REQUIRE("-0x00001.5000p+5"     == FormatArgs("{:016.4a}", -42.0));
    REQUIRE("-0x0001.50000p+5"     == FormatArgs("{:016.5a}", -42.0));

    REQUIRE("1p-1022"   == FormatArgs("{:x}", std::numeric_limits<double>::min()));
    REQUIRE("1p-1074"   == FormatArgs("{:x}", std::numeric_limits<double>::denorm_min()));
    REQUIRE("1P-1074"   == FormatArgs("{:X}", std::numeric_limits<double>::denorm_min()));
    REQUIRE("0x1p-1022" == FormatArgs("{:#x}", std::numeric_limits<double>::min()));
    REQUIRE("0x1p-1074" == FormatArgs("{:#x}", std::numeric_limits<double>::denorm_min()));
    REQUIRE("0X1P-1074" == FormatArgs("{:#X}", std::numeric_limits<double>::denorm_min()));

    REQUIRE("1.7976931348623157e+308"  == FormatArgs("{:s}",  std::numeric_limits<double>::max()));
    REQUIRE("1.7976931348623157E+308"  == FormatArgs("{:S}",  std::numeric_limits<double>::max()));
    REQUIRE("-1.7976931348623157e+308" == FormatArgs("{:s}", -std::numeric_limits<double>::max()));
    REQUIRE("2.2250738585072014e-308"  == FormatArgs("{:s}",  std::numeric_limits<double>::min()));
    REQUIRE("-2.2250738585072014e-308" == FormatArgs("{:s}", -std::numeric_limits<double>::min()));
    REQUIRE("-2.2250738585072014E-308" == FormatArgs("{:S}", -std::numeric_limits<double>::min()));
    REQUIRE("5e-324"                   == FormatArgs("{:s}",  std::numeric_limits<double>::denorm_min()));
    REQUIRE("-5e-324"                  == FormatArgs("{:s}", -std::numeric_limits<double>::denorm_min()));
    REQUIRE("                  5e-324" == FormatArgs("{:>24s}",  std::numeric_limits<double>::denorm_min()));
    REQUIRE("                 -5e-324" == FormatArgs("{:>24s}", -std::numeric_limits<double>::denorm_min()));
    REQUIRE("                  5e-324" == FormatArgs("{: =24s}",  std::numeric_limits<double>::denorm_min()));
    REQUIRE("-                 5e-324" == FormatArgs("{: =24s}", -std::numeric_limits<double>::denorm_min()));
    REQUIRE("0000000000000000005e-324" == FormatArgs("{:024s}",  std::numeric_limits<double>::denorm_min()));
    REQUIRE("-000000000000000005e-324" == FormatArgs("{:024s}", -std::numeric_limits<double>::denorm_min()));

    REQUIRE("0"       == FormatArgs("{:s}",  0.0));
    REQUIRE("-0"      == FormatArgs("{:s}",  -0.0));
    REQUIRE("0p+0"    == FormatArgs("{:x}",  0.0));
    REQUIRE("-0p+0"   == FormatArgs("{:x}",  -0.0));
    REQUIRE("0x0p+0"  == FormatArgs("{:#x}",  0.0));
    REQUIRE("-0x0p+0" == FormatArgs("{:#x}",  -0.0));

    REQUIRE("1.0p+0"       == FormatArgs("{:.1x}",   1.0));
    REQUIRE("1.00p+0"      == FormatArgs("{:.2x}",   1.0));
    REQUIRE("0x1.0p+0"     == FormatArgs("{:#.1x}",   1.0));
    REQUIRE("0x1.00p+0"    == FormatArgs("{:#.2x}",   1.0));
    REQUIRE("0X1.0P+0"     == FormatArgs("{:#.1X}",   1.0));
    REQUIRE("0X1.00P+0"    == FormatArgs("{:#.2X}",   1.0));
    REQUIRE("1.badp+1"     == FormatArgs("{:.3x}", 3.4597));
    REQUIRE("1.bad7p+1"    == FormatArgs("{:.4x}", 3.4597));
    REQUIRE("1.bad77p+1"   == FormatArgs("{:.5x}", 3.4597));
    REQUIRE("0X1.BADP+1"   == FormatArgs("{:#.3X}", 3.4597));
    REQUIRE("0X1.BAD7P+1"  == FormatArgs("{:#.4X}", 3.4597));
    REQUIRE("0X1.BAD77P+1" == FormatArgs("{:#.5X}", 3.4597));

    REQUIRE("0x1p+0"    == FormatArgs("{:a}",     1.0));
    REQUIRE("0x1p+0"    == FormatArgs("{:.0a}",   1.0));
    REQUIRE("0x1.0p+0"  == FormatArgs("{:.1a}",   1.0));
    REQUIRE("0x1.00p+0" == FormatArgs("{:.2a}",   1.0));

    double InvVal = std::numeric_limits<double>::infinity();
    REQUIRE("inf"    == FormatArgs("{:s}", InvVal));
    REQUIRE("   inf" == FormatArgs("{:6s}", InvVal));
    REQUIRE("   inf" == FormatArgs("{:06s}", InvVal));
    REQUIRE("INF"    == FormatArgs("{:S}", InvVal));
    REQUIRE("inf"    == FormatArgs("{:x}", InvVal));
    REQUIRE("INF"    == FormatArgs("{:X}", InvVal));
    REQUIRE("-inf"   == FormatArgs("{:s}", -InvVal));
    REQUIRE("  -inf" == FormatArgs("{:6s}", -InvVal));
    REQUIRE("  -inf" == FormatArgs("{:06s}", -InvVal));
    REQUIRE("-INF"   == FormatArgs("{:S}", -InvVal));
    REQUIRE("-inf"   == FormatArgs("{:x}", -InvVal));
    REQUIRE("-INF"   == FormatArgs("{:X}", -InvVal));

    // infinity with sign (and fill)
    REQUIRE("-INF"   == FormatArgs("{:+S}", -InvVal));
    REQUIRE("-INF"   == FormatArgs("{:-S}", -InvVal));
    REQUIRE("-INF"   == FormatArgs("{: S}", -InvVal));
    REQUIRE("-INF"   == FormatArgs("{:.< S}", -InvVal));
    REQUIRE("+INF"   == FormatArgs("{:+S}", InvVal));
    REQUIRE("INF"    == FormatArgs("{:-S}", InvVal));
    REQUIRE(" INF"   == FormatArgs("{: S}", InvVal));
    REQUIRE(".INF"   == FormatArgs("{:.< S}", InvVal));
    REQUIRE("  -INF" == FormatArgs("{:+06S}", -InvVal));
    REQUIRE("  -INF" == FormatArgs("{:-06S}", -InvVal));
    REQUIRE("  -INF" == FormatArgs("{: 06S}", -InvVal));
    REQUIRE("-INF.." == FormatArgs("{:.<06S}", -InvVal));
    REQUIRE("-INF.." == FormatArgs("{:.< 06S}", -InvVal));
    REQUIRE("  +INF" == FormatArgs("{:+06S}", InvVal));
    REQUIRE("   INF" == FormatArgs("{:-06S}", InvVal));
    REQUIRE("   INF" == FormatArgs("{: 06S}", InvVal));
    REQUIRE("INF..." == FormatArgs("{:.<06S}", InvVal));
    REQUIRE(".INF.." == FormatArgs("{:.< 06S}", InvVal));

    double NanVal = std::numeric_limits<double>::quiet_NaN();
    REQUIRE("nan" == FormatArgs("{:s}", NanVal));
    REQUIRE("NAN" == FormatArgs("{:S}", NanVal));
    REQUIRE("nan" == FormatArgs("{:x}", NanVal));
    REQUIRE("NAN" == FormatArgs("{:X}", NanVal));
    REQUIRE("nan" == FormatArgs("{:s}", -NanVal));
    REQUIRE("NAN" == FormatArgs("{:S}", -NanVal));
    REQUIRE("nan" == FormatArgs("{:x}", -NanVal));
    REQUIRE("NAN" == FormatArgs("{:X}", -NanVal));

    REQUIRE("1.000000" == FormatArgs("{:f}",    1.0));
    REQUIRE("1"        == FormatArgs("{:.f}",   1.0));
    REQUIRE("1"        == FormatArgs("{:.0f}",  1.0));
    REQUIRE("1.0"      == FormatArgs("{:.1f}",  1.0));
    REQUIRE("1.00"     == FormatArgs("{:.2f}",  1.0));
    REQUIRE("1.000000" == FormatArgs("{:#f}",   1.0));
    REQUIRE("1."       == FormatArgs("{:#.0f}", 1.0));
    REQUIRE("1.0"      == FormatArgs("{:#.1f}", 1.0));
    REQUIRE("1.00"     == FormatArgs("{:#.2f}", 1.0));

    REQUIRE("1'234.000000" == FormatArgs("{:'f}",    1234.0));
    REQUIRE("1'234"        == FormatArgs("{:'.f}",   1234.0));
    REQUIRE("1'234"        == FormatArgs("{:'.0f}",  1234.0));
    REQUIRE("1'234.0"      == FormatArgs("{:'.1f}",  1234.0));
    REQUIRE("1'234.00"     == FormatArgs("{:'.2f}",  1234.0));
    REQUIRE("1'234.000000" == FormatArgs("{:'#f}",   1234.0));
    REQUIRE("1'234."       == FormatArgs("{:'#.0f}", 1234.0));
    REQUIRE("1'234.0"      == FormatArgs("{:'#.1f}", 1234.0));
    REQUIRE("1'234.00"     == FormatArgs("{:'#.2f}", 1234.0));

    REQUIRE("1'234.000000" == PrintfArgs("%'f",    1234.0));
    REQUIRE("1'234"        == PrintfArgs("%'.f",   1234.0));
    REQUIRE("1'234"        == PrintfArgs("%'.0f",  1234.0));
    REQUIRE("1'234.0"      == PrintfArgs("%'.1f",  1234.0));
    REQUIRE("1'234.00"     == PrintfArgs("%'.2f",  1234.0));
    REQUIRE("1'234.000000" == PrintfArgs("%'#f",   1234.0));
    REQUIRE("1'234."       == PrintfArgs("%'#.0f", 1234.0));
    REQUIRE("1'234.0"      == PrintfArgs("%'#.1f", 1234.0));
    REQUIRE("1'234.00"     == PrintfArgs("%'#.2f", 1234.0));

    REQUIRE("1.000000e+00" == FormatArgs("{:e}",    1.0));
    REQUIRE("1e+00"        == FormatArgs("{:.e}",   1.0));
    REQUIRE("1e+00"        == FormatArgs("{:.0e}",  1.0));
    REQUIRE("1.0e+00"      == FormatArgs("{:.1e}",  1.0));
    REQUIRE("1.00e+00"     == FormatArgs("{:.2e}",  1.0));
    REQUIRE("1.000000e+00" == FormatArgs("{:#e}",   1.0));
    REQUIRE("1.e+00"       == FormatArgs("{:#.0e}", 1.0));
    REQUIRE("1.0e+00"      == FormatArgs("{:#.1e}", 1.0));
    REQUIRE("1.00e+00"     == FormatArgs("{:#.2e}", 1.0));

    REQUIRE("1"       == FormatArgs("{:g}", 1.0));
    REQUIRE("1"       == FormatArgs("{:.g}", 1.0));
    REQUIRE("1"       == FormatArgs("{:.0g}", 1.0));
    REQUIRE("1"       == FormatArgs("{:.1g}", 1.0));
    REQUIRE("1"       == FormatArgs("{:.2g}", 1.0));
    REQUIRE("1.00000" == FormatArgs("{:#g}", 1.0));
    REQUIRE("1."      == FormatArgs("{:#.0g}", 1.0));
    REQUIRE("1."      == FormatArgs("{:#.1g}", 1.0));
    REQUIRE("1.0"     == FormatArgs("{:#.2g}", 1.0));

    REQUIRE("1e+10"       == FormatArgs("{:g}", 1.0e+10));
    REQUIRE("1e+10"       == FormatArgs("{:.g}", 1.0e+10));
    REQUIRE("1e+10"       == FormatArgs("{:.0g}", 1.0e+10));
    REQUIRE("1e+10"       == FormatArgs("{:.1g}", 1.0e+10));
    REQUIRE("1e+10"       == FormatArgs("{:.2g}", 1.0e+10));
    REQUIRE("1.00000e+10" == FormatArgs("{:#g}", 1.0e+10));
    REQUIRE("1.e+10"      == FormatArgs("{:#.0g}", 1.0e+10));
    REQUIRE("1.e+10"      == FormatArgs("{:#.1g}", 1.0e+10));
    REQUIRE("1.0e+10"     == FormatArgs("{:#.2g}", 1.0e+10));

    REQUIRE("0x1.fcac083126e98p+0" == FormatArgs("{:a}", 1.987));
    REQUIRE("0x2p+0"               == FormatArgs("{:.a}", 1.987));
    REQUIRE("0x2p+0"               == FormatArgs("{:.0a}", 1.987));
    REQUIRE("0x2.0p+0"             == FormatArgs("{:.1a}", 1.987));
    REQUIRE("0x1.fdp+0"            == FormatArgs("{:.2a}", 1.987));
    REQUIRE("0x1.fcac083126e98p+0" == FormatArgs("{:#a}", 1.987));
    REQUIRE("0x2.p+0"              == FormatArgs("{:#.a}", 1.987));
    REQUIRE("0x2.p+0"              == FormatArgs("{:#.0a}", 1.987));
    REQUIRE("0x2.0p+0"             == FormatArgs("{:#.1a}", 1.987));
    REQUIRE("0x1.fdp+0"            == FormatArgs("{:#.2a}", 1.987));
}

TEST_CASE("Pointers", "1")
{
#if 0
#if UINTPTR_MAX == UINT64_MAX
    REQUIRE("0x0000000001020304" == FormatArgs("{}", (void*)0x01020304));
#elif UINTPTR_MAX == UINT32_MAX
    REQUIRE("0x01020304" == FormatArgs("{}", (void*)0x01020304));
#endif
    REQUIRE("-1" == FormatArgs("{:d}", (void*)-1));
#else
    REQUIRE("0x1020304" == FormatArgs("{}", (void*)0x01020304));
#endif

    REQUIRE("(nil)"    == FormatArgs("{}", (void*)0));
    REQUIRE("(nil)"    == FormatArgs("{:3}", (void*)0));
    REQUIRE("(nil)"    == FormatArgs("{:3.3}", (void*)0));
    REQUIRE("   (nil)" == FormatArgs("{:8}", (void*)0));
    REQUIRE("   (nil)" == FormatArgs("{:8.3}", (void*)0));
    REQUIRE("(nil)"    == FormatArgs("{}", nullptr));
}

TEST_CASE("Dynamic", "1")
{
    fmtxx::FormatSpec spec;

    spec.width  = 10;
    spec.prec   = -1;
    spec.fill   = '.';
    spec.align  = fmtxx::Align::Right;
    spec.sign   = fmtxx::Sign::Space;
    spec.zero   = false;
    spec.conv   = 'd';

    REQUIRE(".......123" == FormatArgs("{*}", spec, 123));
    REQUIRE("......-123" == FormatArgs("{*}", spec, -123));
    REQUIRE(".......123" == FormatArgs("{1*}", spec, 123));
    REQUIRE("......-123" == FormatArgs("{1*}", spec, -123));
    REQUIRE(".......123" == FormatArgs("{1*0}", spec, 123));
    REQUIRE("......-123" == FormatArgs("{1*0}", spec, -123));
    REQUIRE(".......123" == FormatArgs("{0*1}", 123, spec));
    REQUIRE("......-123" == FormatArgs("{0*1}", -123, spec));

    REQUIRE("  3.14" == PrintfArgs("%*.*f", 6, 2, 3.1415));
    REQUIRE("  3.14" == PrintfArgs("%6.*f", 2, 3.1415));
    REQUIRE("3.14  " == PrintfArgs("%-6.*f", 2, 3.1415));
    REQUIRE("  3.14" == PrintfArgs("%3$*.*f", 6, 2, 3.1415));
    REQUIRE("  3.14" == PrintfArgs("%1$*2$.*3$f", 3.1415, 6, 2));
    REQUIRE("3.14  " == PrintfArgs("%1$*2$.*3$f", 3.1415, -6, 2));
}

struct Foo {
    int value;
};

namespace fmtxx
{
    template <>
    struct FormatValue<Foo> {
        errc operator()(FormatBuffer& os, FormatSpec const& spec, Foo const& value) const {
            return fmtxx::Format(os, "{*}", spec, value.value);
        }
    };

    template <typename K, typename V>
    struct FormatValue<std::unordered_map<K, V>>
    {
        errc operator()(FormatBuffer& os, FormatSpec const& spec, std::unordered_map<K, V> const& value) const
        {
            //auto const key = spec.key;
            auto const key = spec.style;
            auto const I = value.find(key);
            if (I == value.end()) {
                return fmtxx::Format(os, "[[key '{}' does not exist]]", key);
            }
            return fmtxx::Format(os, "{*}", spec, I->second);
        }
    };
}

namespace foo2_ns
{
    struct Foo2 {
        int value;
    };

    inline std::ostream& operator <<(std::ostream& stream, Foo2 const& value) {
        return stream << value.value;
    }
}

TEST_CASE("Custom", "1")
{
    REQUIRE("struct Foo '   123'"  == FormatArgs("struct Foo '{:6}'", Foo{123}));
    REQUIRE("struct Foo2 '   123'" == FormatArgs("struct Foo2 '{:6}'", foo2_ns::Foo2{123}));

    std::unordered_map<std::string_view, int> map = {{"eins", 1}, {"zwei", 2}};
    REQUIRE("1, 2" == FormatArgs("{0,eins}, {0,zwei}", map));
}

TEST_CASE("Chars", "1")
{
    REQUIRE("A"     == FormatArgs("{}", 'A'));
    REQUIRE("A"     == FormatArgs("{:s}", 'A'));
    //REQUIRE("65"    == FormatArgs("{:d}", 'A'));
    //REQUIRE("41"    == FormatArgs("{:x}", 'A'));
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <vector>

class VectorBuffer : public fmtxx::FormatBuffer
{
    std::vector<char>& os;

public:
    explicit VectorBuffer(std::vector<char>& v) : os(v) {}

    virtual bool Put(char c) override {
        os.push_back(c);
        return true;
    }

    virtual bool Pad(char c, size_t count) override {
        os.resize(os.size() + count, c);
        return true;
    }

    virtual bool Write(char const* str, size_t len) override {
        os.insert(os.end(), str, str + len);
        return true;
    }
};

namespace fmtxx
{
    template <>
    struct FormatValue<std::vector<char>> {
        errc operator()(FormatBuffer& fb, FormatSpec const& spec, std::vector<char> const& vec) const {
            return fmtxx::Format(fb, "{*}", spec, std::string_view(vec.data(), vec.size()));
        }
    };
}

TEST_CASE("Vector", "1")
{
    std::vector<char> os;
    VectorBuffer buf { os };
    fmtxx::Format(buf, "{:6}", -1234);
    REQUIRE(os.size() == 6);
    REQUIRE(os[0] == ' '); // pad
    REQUIRE(os[1] == '-'); // put
    REQUIRE(os[2] == '1'); // write...
    REQUIRE(os[3] == '2');
    REQUIRE(os[4] == '3');
    REQUIRE(os[5] == '4');

    std::vector<char> str = { '1', '2', '3', '4' };
    REQUIRE("1234" == FormatArgs("{}", str));
}
