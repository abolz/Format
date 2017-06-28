#include "ext/Catch/include/catch_with_main.hpp"

#include "Format.h"
#include "Format_memory.h"
#include "Format_ostream.h"
#include "Format_pretty.h"
#include "Format_string.h"

#include <cfloat>
#include <clocale>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

struct FormatterResult
{
    std::string str;
    fmtxx::errc ec;
};

template <typename Fn>
struct ArrayFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        char buf[1024 * 8];
        fmtxx::ArrayWriter w { buf };
        const auto ec = Fn{}(w, format, args...);
        return { std::string(w.data(), w.size()), ec };
    }
};

//template <typename Fn>
//struct StringFormatter
//{
//    template <typename ...Args>
//    FormatterResult operator ()(std::string_view format, Args const&... args) const
//    {
//        std::string os;
//        const auto ec = Fn{}(os, format, args...);
//        return { os, ec };
//    }
//};

//template <typename Fn>
//struct StreamFormatter
//{
//    template <typename ...Args>
//    FormatterResult operator ()(std::string_view format, Args const&... args) const
//    {
//        std::ostringstream os;
//        const auto ec = Fn{}(os, format, args...);
//        return { os.str(), ec };
//    }
//};

//#ifdef __linux__
//template <typename Fn>
//struct FILEFormatter
//{
//    template <typename ...Args>
//    FormatterResult operator ()(std::string_view format, Args const&... args) const
//    {
//        char buf[1024 * 8] = {0};
//        FILE* f = fmemopen(buf, sizeof(buf), "w");
//        const auto ec = Fn{}(f, format, args...);
//        fclose(f); // flush!
//        return { std::string(buf), ec };
//    }
//};
//#endif

//template <typename Fn>
//struct MemoryFormatter
//{
//    template <typename ...Args>
//    FormatterResult operator()(std::string_view format, Args const&... args) const
//    {
//        fmtxx::MemoryWriter<> w;
//        const auto ec = Fn{}(w, format, args...);
//        return { w.str(), ec };
//    }
//};

//template <typename Fn>
//struct OutputIteratorFormatter
//{
//    template <typename ...Args>
//    FormatterResult operator()(std::string_view format, Args const&... args) const
//    {
//        std::string s;
//        auto w = fmtxx::make_output_iterator_writer(std::back_inserter(s));
//        auto const ec = Fn{}(w, format, args...);
//        return { s, ec };
//    }
//};

template <typename Formatter, typename ...Args>
static std::string FormatArgs1(std::string_view format, Args const&... args)
{
    FormatterResult res = Formatter{}(format, args...);
    //assert(res.ec == fmtxx::errc::success);
    return res.str;
}

struct FormatFn {
    template <typename Buffer, typename ...Args>
    auto operator()(Buffer& fb, std::string_view format, Args const&... args) const {
        return fmtxx::format(fb, format, args...);
    }
};

struct PrintfFn {
    template <typename Buffer, typename ...Args>
    auto operator()(Buffer& fb, std::string_view format, Args const&... args) const {
        return fmtxx::printf(fb, format, args...);
    }
};

template <typename Fn, typename ...Args>
static std::string FormatArgsTemplate(std::string_view format, Args const&... args)
{
    std::string const s1 = FormatArgs1<ArrayFormatter<Fn>>(format, args...);

    //std::string const s2 = FormatArgs1<StreamFormatter<Fn>>(format, args...);
    //if (s2 != s1)
    //    return "[[[[ formatter mismatch 1 ]]]]";

//#ifdef __linux__
//    std::string const s3 = FormatArgs1<FILEFormatter<Fn>>(format, args...);
//    if (s3 != s1)
//        return "[[[[ formatter mismatch 2 ]]]]";
//#endif

    //std::string const s4 = FormatArgs1<StringFormatter<Fn>>(format, args...);
    //if (s4 != s1)
    //    return "[[[[ formatter mismatch 3 ]]]]";

    //std::string const s5 = FormatArgs1<MemoryFormatter<Fn>>(format, args...);
    //if (s5 != s1)
    //    return "[[[[ formatter mismatch 4 ]]]]";

    //std::string const s6 = FormatArgs1<OutputIteratorFormatter<Fn>>(format, args...);
    //if (s6 != s1)
    //    return "[[[[ formatter mismatch 5 ]]]]";

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

//--------------------------------------------------------------------------------------------------
// XXX:
// ENABLE THIS AGAIN!!!
//--------------------------------------------------------------------------------------------------
#if FORMAT_STRING_CHECK_POLICY == 2
TEST_CASE("Invalid", "1")
{
    CHECK_THROWS(FormatArgs("{", 0));
    CHECK_THROWS(FormatArgs(std::string_view("{*}", 1), 0, 0));
    CHECK_THROWS(FormatArgs(std::string_view("{*}", 2), 0, 0));
    CHECK_THROWS(FormatArgs("{*}", 1));
    CHECK_THROWS(FormatArgs("{*", fmtxx::FormatSpec{}, 0));
    CHECK_THROWS(FormatArgs("{*}}", fmtxx::FormatSpec{}, 0));
    CHECK_THROWS(FormatArgs(std::string_view("{}", 1), 0));
    CHECK_THROWS(FormatArgs(std::string_view("{1}", 2), 0));
    CHECK_THROWS(FormatArgs("{1", 0));
    CHECK_THROWS(FormatArgs("{1:", 0));
    CHECK_THROWS(FormatArgs("{1:1", 0));
    CHECK_THROWS(FormatArgs("{1:1.", 0));
    CHECK_THROWS(FormatArgs("{1:1.1", 0));
    CHECK_THROWS(FormatArgs("{1:1.1f", 0));
    CHECK_THROWS(FormatArgs("{ ", 0));
    CHECK_THROWS(FormatArgs("{1 ", 0));
    CHECK_THROWS(FormatArgs("{1: ", 0));
    CHECK_THROWS(FormatArgs("{1:1 ", 0));
    CHECK_THROWS(FormatArgs("{1:1. ", 0));
    CHECK_THROWS(FormatArgs("{1:1.1 ", 0));
    CHECK_THROWS(FormatArgs("{1:1.1f ", 0));
    CHECK_THROWS(FormatArgs("{-1: >10.2f}", 0));
    CHECK_THROWS(FormatArgs("{:*10}", 0));
    CHECK_THROWS(FormatArgs("{-10}", 0));
    CHECK_THROWS(FormatArgs("{{}", 1)); // stray '}'
    CHECK_THROWS(FormatArgs("{}}", 1)); // stray '}'
    CHECK_THROWS(FormatArgs("}", 1, 1, 1, 1, 1));
    CHECK_THROWS(FormatArgs("{1}", 1));
    CHECK_THROWS(FormatArgs("{1}{2}", 1, 2));
    CHECK_THROWS(FormatArgs("{0}{2}", 1, 2));
    CHECK_THROWS(FormatArgs("{10}", 1));
    CHECK_THROWS(FormatArgs("{2147483647}", 1));
    CHECK_THROWS(FormatArgs("{2147483648}", 0));
    CHECK_NOTHROW(FormatArgs("{:2147483647}", 0));
    CHECK_THROWS(FormatArgs("{:2147483648}", 0));
    CHECK_NOTHROW(FormatArgs("{:.2147483647}", 0));
    CHECK_THROWS(FormatArgs("{:.2147483648}", 0));
    CHECK_THROWS(FormatArgs("{:.", 0));
}
#endif

TEST_CASE("General", "0")
{
    SECTION("Format")
    {
        CHECK("Hello"                           == FormatArgs("Hello",                                    0));
        CHECK("Count to 10"                     == FormatArgs("Count to {0}",                             10));
        CHECK("Bring me a beer"                 == FormatArgs("Bring me a {}",                            "beer"));
        CHECK("From 0 to 10"                    == FormatArgs("From {} to {}",                            0, 10));
        CHECK("From 0 to 10"                    == FormatArgs("From {1} to {0}",                          10, 0));
        CHECK("dec:42 hex:2a oct:52 bin:101010" == FormatArgs("dec:{0:d} hex:{0:x} oct:{0:o} bin:{0:b}",  42));
        CHECK("left<<<<<<<<<<<<"                == FormatArgs("{:<<16}",                                  "left"));
        CHECK(".....center....."                == FormatArgs("{:.^16}",                                  "center"));
        CHECK(">>>>>>>>>>>right"                == FormatArgs("{:>>16}",                                  "right"));
        CHECK("2 1 1 2"                         == FormatArgs("{1} {} {0} {}",                            1, 2));
    }
    SECTION("Printf")
    {
        CHECK("Hello"                           == PrintfArgs("Hello",                                    0));
        CHECK("Bring me a beer"                 == PrintfArgs("Bring me a %s",                            "beer"));
        CHECK("From 0 to 10"                    == PrintfArgs("From %s to %s",                            0, 10));
        CHECK("dec:42 hex:2a oct:52 bin:101010" == PrintfArgs("dec:%1$d hex:%1$x oct:%1$o bin:%1$b",      42));
        CHECK("left            "                == PrintfArgs("%-16s",                                    "left"));
    }
}

TEST_CASE("String", "1")
{
    CHECK(""  == FormatArgs(""));
    CHECK("x" == FormatArgs("x"));
    CHECK("{" == FormatArgs("{{"));
    CHECK("}" == FormatArgs("}}"));

    CHECK("hello %" == PrintfArgs("hello %%"));
    CHECK("% hello" == PrintfArgs("%% hello"));
    CHECK("hello % hello" == PrintfArgs("hello %% hello"));

    CHECK("x" == FormatArgs("{}", 'x'));
    CHECK("x" == FormatArgs("{:.0}", 'x'));

    CHECK("     xxx" == FormatArgs("{:8}", "xxx"));
    CHECK("     xxx" == FormatArgs("{:>8}", "xxx"));
    CHECK("xxx     " == FormatArgs("{:<8}", "xxx"));
    CHECK("  xxx   " == FormatArgs("{:^8}", "xxx"));

    CHECK(":Hello, world!:"       == FormatArgs(":{}:",         "Hello, world!"));
    CHECK(":  Hello, world!:"     == FormatArgs(":{:15}:",      "Hello, world!"));
    CHECK(":Hello, wor:"          == FormatArgs(":{:.10}:",     "Hello, world!"));
    CHECK(":Hello, world!:"       == FormatArgs(":{:<10}:",     "Hello, world!"));
    CHECK(":Hello, world!  :"     == FormatArgs(":{:<15}:",     "Hello, world!"));
    CHECK(":Hello, world!:"       == FormatArgs(":{:.15}:",     "Hello, world!"));
    CHECK(":     Hello, wor:"     == FormatArgs(":{:15.10}:",   "Hello, world!"));
    CHECK(":Hello, wor     :"     == FormatArgs(":{:<15.10}:",  "Hello, world!"));

    CHECK(":Hello, world!:"       == PrintfArgs(":%s:",         "Hello, world!"));
    CHECK(":  Hello, world!:"     == PrintfArgs(":%15s:",      "Hello, world!"));
    CHECK(":Hello, wor:"          == PrintfArgs(":%.10s:",     "Hello, world!"));
    CHECK(":Hello, world!:"       == PrintfArgs(":%-10s:",     "Hello, world!"));
    CHECK(":Hello, world!  :"     == PrintfArgs(":%-15s:",     "Hello, world!"));
    CHECK(":Hello, world!:"       == PrintfArgs(":%.15s:",     "Hello, world!"));
    CHECK(":     Hello, wor:"     == PrintfArgs(":%15.10s:",   "Hello, world!"));
    CHECK(":Hello, wor     :"     == PrintfArgs(":%-15.10s:",  "Hello, world!"));

    std::string str = "hello hello hello hello hello hello hello hello hello hello ";
    CHECK("hello hello hello hello hello hello hello hello hello hello " == FormatArgs("{}", str));

    CHECK(">---<" == FormatArgs(">{}<", "---"));
    CHECK("<--->" == FormatArgs("<{}>", "---"));
    CHECK(">---<" == FormatArgs(">{0}<", "---"));
    CHECK("<--->" == FormatArgs("<{0}>", "---"));
    CHECK(">---<" == FormatArgs(">{0:s}<", "---"));
    CHECK("<--->" == FormatArgs("<{0:s}>", "---"));

    CHECK(">--->" == FormatArgs(">{0:}<s}>", "---"));
    CHECK("<---<" == FormatArgs("<{0:}>s}<", "---"));
    CHECK("^---^" == FormatArgs("^{0:}^s}^", "---"));

    CHECK("(null)"     == FormatArgs("{}",      (char*)0));
    CHECK("(null)"     == FormatArgs("{}",      (char const*)0));
    CHECK("(null)"     == FormatArgs("{:.3}",   (char const*)0));
    CHECK("(null)"     == FormatArgs("{:.10}",  (char const*)0));
    CHECK("(null)"     == FormatArgs("{:3.3}",  (char const*)0));
    CHECK("    (null)" == FormatArgs("{:10.3}", (char const*)0));

    CHECK("(null)"     == PrintfArgs("%s",      (char*)0));
    CHECK("(null)"     == PrintfArgs("%s",      (char const*)0));
    CHECK("(null)"     == PrintfArgs("%.3s",   (char const*)0));
    CHECK("(null)"     == PrintfArgs("%.10s",  (char const*)0));
    CHECK("(null)"     == PrintfArgs("%3.3s",  (char const*)0));
    CHECK("    (null)" == PrintfArgs("%10.3s", (char const*)0));

    std::string spad = std::string(128, ' ');
    CHECK(spad.c_str() == FormatArgs("{:128}", ' '));

    //CHECK(R"( "hello \"world\"" )"    == FormatArgs(" {:q} ", R"(hello "world")")); // VC bug? https://developercommunity.visualstudio.com/content/problem/67300/stringifying-raw-string-literal.html
    CHECK(" \"hello \\\"world\\\"\" " == FormatArgs(" {:q} ", R"(hello "world")"));
    CHECK(" \"hello \\\"world\\\"\" " == PrintfArgs(" %q ",   "hello \"world\""));
    CHECK(R"("hello")"                == FormatArgs("{:q}", "hello"));
    //CHECK(R"("\"\"hello")"            == FormatArgs("{:q}", R"(""hello)")); // VC bug?
    CHECK("\"\\\"\\\"hello\""         == FormatArgs("{:q}", R"(""hello)"));

    char arr1[] = "hello";
    CHECK("hello" == FormatArgs("{}", arr1));
}

TEST_CASE("Ints", "1")
{
    CHECK("2 1 1 2" == FormatArgs("{1} {} {0} {}", 1, 2));

    static const int V = 0x12345;

    CHECK("74565"     == PrintfArgs("%s",     V));
    CHECK("74565"     == PrintfArgs("%hhs",     V));
    CHECK("74565"     == PrintfArgs("%hs",     V));
    CHECK("74565"     == PrintfArgs("%ls",     V));
    CHECK("74565"     == PrintfArgs("%lls",     V));
    CHECK("74565"     == PrintfArgs("%js",     V));
    CHECK("74565"     == PrintfArgs("%zs",     V));
    CHECK("74565"     == PrintfArgs("%ts",     V));
    CHECK("74565"     == PrintfArgs("%Ls",     V));

    CHECK("123" == PrintfArgs("%5$d", 1, 2, 3, 4, 123));

    CHECK("74565"     == FormatArgs("{}",     V));
    CHECK("-74565"    == FormatArgs("{}",    -V));
    CHECK(" 74565"    == FormatArgs("{: }",   V));
    CHECK("-74565"    == FormatArgs("{: }",  -V));
    CHECK("74565"     == FormatArgs("{:-}",   V));
    CHECK("-74565"    == FormatArgs("{:-}",  -V));
    CHECK("+74565"    == FormatArgs("{:+}",   V));
    CHECK("-74565"    == FormatArgs("{:+}",  -V));

    CHECK("hello 74565     " == FormatArgs("hello {:<10}",    V));
    CHECK("hello -74565    " == FormatArgs("hello {:<10}",   -V));
    CHECK("hello  74565    " == FormatArgs("hello {:< 10}",   V));
    CHECK("hello -74565    " == FormatArgs("hello {:< 10}",  -V));
    CHECK("hello 74565     " == FormatArgs("hello {:<-10}",   V));
    CHECK("hello -74565    " == FormatArgs("hello {:<-10}",  -V));
    CHECK("hello +74565    " == FormatArgs("hello {:<+10}",   V));
    CHECK("hello -74565    " == FormatArgs("hello {:<+10}",  -V));

    CHECK("     74565" == FormatArgs("{:>10}",    V));
    CHECK("    -74565" == FormatArgs("{:>10}",   -V));
    CHECK("     74565" == FormatArgs("{:> 10}",   V));
    CHECK("    -74565" == FormatArgs("{:> 10}",  -V));
    CHECK("     74565" == FormatArgs("{:>-10}",   V));
    CHECK("    -74565" == FormatArgs("{:>-10}",  -V));
    CHECK("    +74565" == FormatArgs("{:>+10}",   V));
    CHECK("    -74565" == FormatArgs("{:>+10}",  -V));

    CHECK("  74565   " == FormatArgs("{:^10}",    V));
    CHECK("  -74565  " == FormatArgs("{:^10}",   -V));
    CHECK("   74565  " == FormatArgs("{:^ 10}",   V));
    CHECK("  -74565  " == FormatArgs("{:^ 10}",  -V));
    CHECK("  74565   " == FormatArgs("{:^-10}",   V));
    CHECK("  -74565  " == FormatArgs("{:^-10}",  -V));
    CHECK("  +74565  " == FormatArgs("{:^+10}",   V));
    CHECK("  -74565  " == FormatArgs("{:^+10}",  -V));

    CHECK("0000074565" == FormatArgs("{: <010}",    V));
    CHECK("-000074565" == FormatArgs("{: <010}",   -V));
    CHECK(" 000074565" == FormatArgs("{: < 010}",   V));
    CHECK("-000074565" == FormatArgs("{: < 010}",  -V));
    CHECK("0000074565" == FormatArgs("{: <-010}",   V));
    CHECK("-000074565" == FormatArgs("{: <-010}",  -V));
    CHECK("+000074565" == FormatArgs("{: <+010}",   V));
    CHECK("-000074565" == FormatArgs("{: <+010}",  -V));

    CHECK("0000074565" == FormatArgs("{: =010}",    V));
    CHECK("-000074565" == FormatArgs("{: =010}",   -V));
    CHECK(" 000074565" == FormatArgs("{: = 010}",   V));
    CHECK("-000074565" == FormatArgs("{: = 010}",  -V));
    CHECK("0000074565" == FormatArgs("{: =-010}",   V));
    CHECK("-000074565" == FormatArgs("{: =-010}",  -V));
    CHECK("+000074565" == FormatArgs("{: =+010}",   V));
    CHECK("-000074565" == FormatArgs("{: =+010}",  -V));

    CHECK("0000074565" == FormatArgs("{:010}",     V));
    CHECK("-000074565" == FormatArgs("{:010}",    -V));
    CHECK("0745650000" == FormatArgs("{:0< 10}",    V));
    CHECK("-745650000" == FormatArgs("{:0< 10}",   -V));

    CHECK("2147483647"            == FormatArgs("{}", INT_MAX));
    CHECK("-2147483648"           == FormatArgs("{}", INT_MIN));
    CHECK("9223372036854775807"   == FormatArgs("{}", INT64_MAX));
    CHECK("-9223372036854775808"  == FormatArgs("{}", INT64_MIN));

    CHECK("1"    == FormatArgs("{:x}", (signed char) 1));
    CHECK("ff"   == FormatArgs("{:x}", (signed char)-1));
    CHECK("1"    == FormatArgs("{:x}", (signed short) 1));
    CHECK("ffff" == FormatArgs("{:x}", (signed short)-1));

    CHECK("12345"    == FormatArgs("{:x}",      V));
    CHECK("fffedcbb" == FormatArgs("{:x}",     -V));
    CHECK("00012345" == FormatArgs("{:08x}",    V));
    CHECK("fffedcbb" == FormatArgs("{:08x}",   -V));

#if LONG_MAX != INT_MAX
    CHECK("12345"            ==  FormatArgs("{:x}",      (signed long)V));
    CHECK("fffffffffffedcbb" ==  FormatArgs("{:x}",     -(signed long)V));
    CHECK("00012345"         ==  FormatArgs("{:08x}",    (signed long)V));
    CHECK("fffffffffffedcbb" ==  FormatArgs("{:08x}",   -(signed long)V));
#endif

    CHECK("12345"            == FormatArgs("{:x}",   (signed long long) V));
    CHECK("fffffffffffedcbb" == FormatArgs("{:x}",   (signed long long)-V));
    CHECK("12345"            == FormatArgs("{:X}",   (signed long long) V));
    CHECK("FFFFFFFFFFFEDCBB" == FormatArgs("{:X}",   (signed long long)-V));

    CHECK("1'234'567'890" == FormatArgs("{:'13}", 1234567890));
    CHECK("  123'456'789" == FormatArgs("{:'13}", 123456789));
    CHECK("   12'345'678" == FormatArgs("{:'13}", 12345678));
    CHECK("    1'234'567" == FormatArgs("{:'13}", 1234567));
    CHECK("      123'456" == FormatArgs("{:'13}", 123456));
    CHECK("       12'345" == FormatArgs("{:'13}", 12345));
    CHECK("        1'234" == FormatArgs("{:'13}", 1234));
    CHECK("          123" == FormatArgs("{:'13}", 123));
    CHECK("           12" == FormatArgs("{:'13}", 12));
    CHECK("            1" == FormatArgs("{:'13}", 1));
    CHECK("1_234_567_890" == FormatArgs("{:_13}", 1234567890));
    CHECK("  123_456_789" == FormatArgs("{:_13}", 123456789));
    CHECK("   12_345_678" == FormatArgs("{:_13}", 12345678));
    CHECK("    1_234_567" == FormatArgs("{:_13}", 1234567));
    CHECK("      123_456" == FormatArgs("{:_13}", 123456));
    CHECK("       12_345" == FormatArgs("{:_13}", 12345));
    CHECK("        1_234" == FormatArgs("{:_13}", 1234));
    CHECK("          123" == FormatArgs("{:_13}", 123));
    CHECK("           12" == FormatArgs("{:_13}", 12));
    CHECK("            1" == FormatArgs("{:_13}", 1));
    CHECK("18_446_744_073_709_551_615" == FormatArgs("{:_}", UINT64_MAX));

    CHECK("1234'5678" == FormatArgs("{:'9x}", 0x12345678));
    CHECK(" 123'4567" == FormatArgs("{:'9x}", 0x1234567));
    CHECK("  12'3456" == FormatArgs("{:'9x}", 0x123456));
    CHECK("   1'2345" == FormatArgs("{:'9x}", 0x12345));
    CHECK("     1234" == FormatArgs("{:'9x}", 0x1234));
    CHECK("      123" == FormatArgs("{:'9x}", 0x123));
    CHECK("       12" == FormatArgs("{:'9x}", 0x12));
    CHECK("        1" == FormatArgs("{:'9x}", 0x1));

    CHECK("7777_7777" == FormatArgs("{:_9o}", 077777777));
    CHECK(" 777_7777" == FormatArgs("{:_9o}", 07777777));
    CHECK("  77_7777" == FormatArgs("{:_9o}", 0777777));
    CHECK("   7_7777" == FormatArgs("{:_9o}", 077777));
    CHECK("     7777" == FormatArgs("{:_9o}", 07777));
    CHECK("      777" == FormatArgs("{:_9o}", 0777));
    CHECK("       77" == FormatArgs("{:_9o}", 077));
    CHECK("        7" == FormatArgs("{:_9o}", 07));
    CHECK("        0" == FormatArgs("{:_9o}", 0));

    CHECK("1111_1111" == FormatArgs("{:_9b}", 0xFF));
    CHECK(" 111_1111" == FormatArgs("{:_9b}", 0x7F));
    CHECK("  11_1111" == FormatArgs("{:_9b}", 0x3F));
    CHECK("   1_1111" == FormatArgs("{:_9b}", 0x1F));
    CHECK("     1111" == FormatArgs("{:_9b}", 0x0F));
    CHECK("      111" == FormatArgs("{:_9b}", 0x07));
    CHECK("       11" == FormatArgs("{:_9b}", 0x03));
    CHECK("        1" == FormatArgs("{:_9b}", 0x01));
    CHECK("        0" == FormatArgs("{:_9b}", 0x00));
    CHECK("1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111" == FormatArgs("{:_b}", UINT64_MAX));

    CHECK("4294966062" == FormatArgs("{:u}", -1234));
    CHECK("4294966062" == FormatArgs("{: u}", -1234));
    CHECK("4294966062" == FormatArgs("{:+u}", -1234));
    CHECK("4294966062" == FormatArgs("{:-u}", -1234));
    CHECK("18446744073709550382" == FormatArgs("{:u}", -1234ll));

    CHECK("0"          == FormatArgs("{:x}",  0));
    CHECK("0"          == FormatArgs("{:b}",  0));
    CHECK("0"          == FormatArgs("{:o}",  0));
    CHECK("1"          == FormatArgs("{:x}",  1));
    CHECK("1"          == FormatArgs("{:b}",  1));
    CHECK("1"          == FormatArgs("{:o}",  1));
    CHECK("0x0"        == FormatArgs("{:#x}", 0));
    CHECK("0b0"        == FormatArgs("{:#b}", 0));
    CHECK("0"          == FormatArgs("{:#o}", 0));
    CHECK("0x1"        == FormatArgs("{:#x}", 1));
    CHECK("0b1"        == FormatArgs("{:#b}", 1));
    CHECK("01"         == FormatArgs("{:#o}", 1));
    CHECK("0x00000000" == FormatArgs("{:#010x}", 0));
    CHECK("0b00000000" == FormatArgs("{:#010b}", 0));
    CHECK("0000000000" == FormatArgs("{:#010o}", 0));
    CHECK("0x00000001" == FormatArgs("{:#010x}", 1));
    CHECK("0b00000001" == FormatArgs("{:#010b}", 1));
    CHECK("0000000001" == FormatArgs("{:#010o}", 1));
    CHECK("       0x0" == FormatArgs("{:#10x}",  0));
    CHECK("       0b0" == FormatArgs("{:#10b}",  0));
    CHECK("         0" == FormatArgs("{:#10o}",  0));
    CHECK("       0x1" == FormatArgs("{:#10x}",  1));
    CHECK("       0b1" == FormatArgs("{:#10b}",  1));
    CHECK("        01" == FormatArgs("{:#10o}",  1));
}

TEST_CASE("Floats", "1")
{
    CHECK(
        "243546080556034731077856379609316893158278902575447060151047"
        "212703405344938119816206067372775299130836050315842578309818"
        "316450894337978612745889730079163798234256495613858256849283"
        "467066859489192118352020514036083287319232435355752493038825"
        "828481044358810649108367633313557305310641892225870327827273"
        "41408256.000000"
            == FormatArgs("{:f}", 2.4354608055603473e+307));
    CHECK(
        "0.0000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000049406564584124654417656879286822137"
        "236505980261432476442558568250067550727020875186529983636163"
        "599237979656469544571773092665671035593979639877479601078187"
        "812630071319031140452784581716784898210368871863605699873072"
        "305000638740915356498438731247339727316961514003171538539807"
        "412623856559117102665855668676818703956031062493194527159149"
        "245532930545654440112748012970999954193198940908041656332452"
        "475714786901472678015935523861155013480352649347201937902681"
        "071074917033322268447533357208324319360923828934583680601060"
        "115061698097530783422773183292479049825247307763759272478746"
        "560847782037344696995336470179726777175851256605511991315048"
        "911014510378627381672509558373897335989936648099411642057026"
        "37090279242767544565229087538682506419718265533447265625"
            == FormatArgs("{:.1074f}", std::numeric_limits<double>::denorm_min()));

    static const double PI  = 3.1415926535897932384626433832795;

    CHECK("0.000000"  == FormatArgs("{:f}",   0.0));
    CHECK("-0.000000" == FormatArgs("{:f}",  -0.0));
    CHECK(" 0.000000" == FormatArgs("{: f}",  0.0));
    CHECK("-0.000000" == FormatArgs("{: f}", -0.0));
    CHECK("+0.000000" == FormatArgs("{:+f}",  0.0));
    CHECK("-0.000000" == FormatArgs("{:+f}", -0.0));

    CHECK("0"            == FormatArgs("{:.0f}", 0.0));
    CHECK("0.0"          == FormatArgs("{:.1f}", 0.0));
    CHECK("0.000000e+00" == FormatArgs("{:e}", 0.0));
    CHECK("0e+00"        == FormatArgs("{:.0e}", 0.0));
    CHECK("0.0e+00"      == FormatArgs("{:.1e}", 0.0));

    CHECK("3.141593"  == FormatArgs("{:f}", PI));
    CHECK("-3.141593" == FormatArgs("{:f}", -PI));
    CHECK("3.14"      == FormatArgs("{:.2f}", PI));
    CHECK("-3.14"     == FormatArgs("{:.2f}", -PI));
    CHECK("3.142"     == FormatArgs("{:.3f}", PI));
    CHECK("-3.142"    == FormatArgs("{:.3f}", -PI));

    CHECK("      3.141593" == FormatArgs("{:14f}",        PI));
    CHECK("     -3.141593" == FormatArgs("{:14f}",       -PI));
    CHECK("3.141593::::::" == FormatArgs("{::<14f}",      PI));
    CHECK("-3.141593:::::" == FormatArgs("{::<14f}",     -PI));
    CHECK("*3.141593*****" == FormatArgs("{:*< 14f}",     PI));
    CHECK("-3.141593*****" == FormatArgs("{:*< 14f}",    -PI));
    CHECK("+3.141593~~~~~" == FormatArgs("{:~<+14f}",     PI));
    CHECK("-3.141593~~~~~" == FormatArgs("{:~<+14f}",    -PI));
    CHECK("~~~~~~3.141593" == FormatArgs("{:~>14f}",      PI));
    CHECK("~~~~~-3.141593" == FormatArgs("{:~>14f}",     -PI));
    CHECK("~~~~~~3.141593" == FormatArgs("{:~> 14f}",     PI));
    CHECK("~~~~~-3.141593" == FormatArgs("{:~> 14f}",    -PI));
    CHECK("   3.141593   " == FormatArgs("{: ^ 14f}",     PI));
    CHECK("  -3.141593   " == FormatArgs("{: ^ 14f}",    -PI));
    CHECK("...3.141593..." == FormatArgs("{:.^ 14f}",     PI));
    CHECK("..-3.141593..." == FormatArgs("{:.^ 14f}",    -PI));
    CHECK("..+3.141593..." == FormatArgs("{:.^+14f}",     PI));
    CHECK("..-3.141593..." == FormatArgs("{:.^+14f}",    -PI));

    // zero flag means align sign left
    CHECK("0000003.141593" == FormatArgs("{:014f}",      PI));
    CHECK("-000003.141593" == FormatArgs("{:014f}",     -PI));
    CHECK("+000003.141593" == FormatArgs("{:+014f}",     PI));
    CHECK("-000003.141593" == FormatArgs("{:+014f}",    -PI));
    CHECK(" 000003.141593" == FormatArgs("{: 014f}",     PI));
    CHECK("-000003.141593" == FormatArgs("{: 014f}",    -PI));
    CHECK("3.141593000000" == FormatArgs("{:0<14f}",     PI));
    CHECK("-3.14159300000" == FormatArgs("{:0<14f}",    -PI));
    CHECK("+3.14159300000" == FormatArgs("{:0<+14f}",    PI));
    CHECK("-3.14159300000" == FormatArgs("{:0<+14f}",   -PI));
    CHECK("......3.141593" == FormatArgs("{:.=14f}",     PI));
    CHECK("-.....3.141593" == FormatArgs("{:.=14f}",    -PI));
    CHECK("+.....3.141593" == FormatArgs("{:.=+14f}",    PI));
    CHECK("-.....3.141593" == FormatArgs("{:.=+14f}",   -PI));
    CHECK("......3.141593" == FormatArgs("{:.= 14f}",    PI));
    CHECK("-.....3.141593" == FormatArgs("{:.= 14f}",   -PI));
    CHECK("3.141593......" == FormatArgs("{:.<14f}",     PI));
    CHECK("-3.141593....." == FormatArgs("{:.<14f}",    -PI));
    CHECK("+3.141593....." == FormatArgs("{:.<+14f}",    PI));
    CHECK("-3.141593....." == FormatArgs("{:.<+14f}",   -PI));
    CHECK(".3.141593....." == FormatArgs("{:.< 14f}",    PI));
    CHECK("-3.141593....." == FormatArgs("{:.< 14f}",   -PI));

    CHECK("0.010000" == FormatArgs("{:f}", 0.01));

    CHECK("1.000000"     == FormatArgs("{:f}",  1.0));
    CHECK("1.000000e+00" == FormatArgs("{:e}",  1.0));
    CHECK("1.000000E+00" == FormatArgs("{:E}",  1.0));
    CHECK("1"            == FormatArgs("{:g}",  1.0));

    CHECK("1.200000"     == FormatArgs("{:f}",  1.2));
    CHECK("1.200000e+00" == FormatArgs("{:e}",  1.2));
    CHECK("1.200000E+00" == FormatArgs("{:E}",  1.2));
    CHECK("1.2"          == FormatArgs("{:g}",  1.2));

    CHECK("1.234568"         == FormatArgs("{:'f}", 1.23456789));
    CHECK("12.345679"        == FormatArgs("{:'f}", 12.3456789));
    CHECK("123.456789"       == FormatArgs("{:'f}", 123.456789));
    CHECK("1'234.567890"     == FormatArgs("{:'f}", 1234.56789));
    CHECK("12'345.678900"    == FormatArgs("{:'f}", 12345.6789));
    CHECK("123'456.789000"   == FormatArgs("{:'f}", 123456.789));
    CHECK("1'234'567.890000" == FormatArgs("{:'f}", 1234567.89));

    CHECK("123456.789000" == FormatArgs("{:f}",  123456.789));
    CHECK("1.234568e+05"  == FormatArgs("{:e}",  123456.789));
    CHECK("1.235e+05"     == FormatArgs("{:.3e}",  123456.789));
    CHECK("1.234568E+05"  == FormatArgs("{:E}",  123456.789));
    CHECK("123457"        == FormatArgs("{:g}",  123456.789));
    CHECK("1.23e+05"      == FormatArgs("{:.3g}",  123456.789));
    CHECK("    1.23e+05"  == FormatArgs("{:12.3g}",  123456.789));
    CHECK("1.23e+05    "  == FormatArgs("{:<12.3g}",  123456.789));
    CHECK("  1.23e+05  "  == FormatArgs("{:^12.3g}",  123456.789));
    CHECK("   -1.23e+05"  == FormatArgs("{:-12.3g}",  -123456.789));
    CHECK("-1.23e+05   "  == FormatArgs("{:<-12.3g}",  -123456.789));
    CHECK(" -1.23e+05  "  == FormatArgs("{:^-12.3g}",  -123456.789));

    CHECK("12345.678900" == FormatArgs("{:f}",  12345.6789));
    CHECK("1.234568e+04" == FormatArgs("{:e}",  12345.6789));
    CHECK("1.235e+04"    == FormatArgs("{:.3e}",  12345.6789));
    CHECK("1.234568E+04" == FormatArgs("{:E}",  12345.6789));
    CHECK("12345.7"      == FormatArgs("{:g}",  12345.6789));
    CHECK("1.23e+04"     == FormatArgs("{:.3g}",  12345.6789));

    CHECK("0"                    == FormatArgs("{:s}", 0.0));
    CHECK("10"                   == FormatArgs("{:s}", 10.0));
    CHECK("10"                   == FormatArgs("{:S}", 10.0));
    CHECK("-0"                   == FormatArgs("{:s}", -0.0));
    CHECK("0p+0"                 == FormatArgs("{:x}", 0.0));
    CHECK("0P+0"                 == FormatArgs("{:X}", 0.0));
    CHECK("-0p+0"                == FormatArgs("{:x}", -0.0));
    CHECK("1.8p+0"               == FormatArgs("{:x}", 1.5));
    CHECK("0x1.8000p+0"          == FormatArgs("{:.4a}", 1.5));
    CHECK("1p+1"                 == FormatArgs("{:.0x}", 1.5));
    CHECK("0x2p+0"               == FormatArgs("{:.0a}", 1.5));
    CHECK("0x1.921fb5a7ed197p+1" == FormatArgs("{:a}", 3.1415927));
    CHECK("0X1.921FB5A7ED197P+1" == FormatArgs("{:A}", 3.1415927));
    CHECK("0x1.922p+1"           == FormatArgs("{:.3a}", 3.1415927));
    CHECK("0x1.9220p+1"          == FormatArgs("{:.4a}", 3.1415927));
    CHECK("0x1.921fbp+1"         == FormatArgs("{:.5a}", 3.1415927));
    CHECK("      0x1.922p+1"     == FormatArgs("{:16.3a}", 3.1415927));
    CHECK("     0x1.9220p+1"     == FormatArgs("{:16.4a}", 3.1415927));
    CHECK("    0x1.921fbp+1"     == FormatArgs("{:16.5a}", 3.1415927));
    CHECK("0x0000001.922p+1"     == FormatArgs("{:016.3a}", 3.1415927));
    CHECK("0x000001.9220p+1"     == FormatArgs("{:016.4a}", 3.1415927));
    CHECK("0x00001.921fbp+1"     == FormatArgs("{:016.5a}", 3.1415927));
    CHECK("     -0x1.500p+5"     == FormatArgs("{:16.3a}", -42.0));
    CHECK("    -0x1.5000p+5"     == FormatArgs("{:16.4a}", -42.0));
    CHECK("   -0x1.50000p+5"     == FormatArgs("{:16.5a}", -42.0));
    CHECK("-0x000001.500p+5"     == FormatArgs("{:016.3a}", -42.0));
    CHECK("-0x00001.5000p+5"     == FormatArgs("{:016.4a}", -42.0));
    CHECK("-0x0001.50000p+5"     == FormatArgs("{:016.5a}", -42.0));

    CHECK("1p-1022"   == FormatArgs("{:x}", std::numeric_limits<double>::min()));
    CHECK("1p-1074"   == FormatArgs("{:x}", std::numeric_limits<double>::denorm_min()));
    CHECK("1P-1074"   == FormatArgs("{:X}", std::numeric_limits<double>::denorm_min()));
    CHECK("0x1p-1022" == FormatArgs("{:#x}", std::numeric_limits<double>::min()));
    CHECK("0x1p-1074" == FormatArgs("{:#x}", std::numeric_limits<double>::denorm_min()));
    CHECK("0X1P-1074" == FormatArgs("{:#X}", std::numeric_limits<double>::denorm_min()));

    CHECK("1.7976931348623157e+308"  == FormatArgs("{:s}",  std::numeric_limits<double>::max()));
    CHECK("1.7976931348623157E+308"  == FormatArgs("{:S}",  std::numeric_limits<double>::max()));
    CHECK("-1.7976931348623157e+308" == FormatArgs("{:s}", -std::numeric_limits<double>::max()));
    CHECK("2.2250738585072014e-308"  == FormatArgs("{:s}",  std::numeric_limits<double>::min()));
    CHECK("-2.2250738585072014e-308" == FormatArgs("{:s}", -std::numeric_limits<double>::min()));
    CHECK("-2.2250738585072014E-308" == FormatArgs("{:S}", -std::numeric_limits<double>::min()));
    CHECK("5e-324"                   == FormatArgs("{:s}",  std::numeric_limits<double>::denorm_min()));
    CHECK("-5e-324"                  == FormatArgs("{:s}", -std::numeric_limits<double>::denorm_min()));
    CHECK("                  5e-324" == FormatArgs("{:>24s}",  std::numeric_limits<double>::denorm_min()));
    CHECK("                 -5e-324" == FormatArgs("{:>24s}", -std::numeric_limits<double>::denorm_min()));
    CHECK("                  5e-324" == FormatArgs("{: =24s}",  std::numeric_limits<double>::denorm_min()));
    CHECK("-                 5e-324" == FormatArgs("{: =24s}", -std::numeric_limits<double>::denorm_min()));
    CHECK("0000000000000000005e-324" == FormatArgs("{:024s}",  std::numeric_limits<double>::denorm_min()));
    CHECK("-000000000000000005e-324" == FormatArgs("{:024s}", -std::numeric_limits<double>::denorm_min()));

    CHECK("0"       == FormatArgs("{:s}",  0.0));
    CHECK("-0"      == FormatArgs("{:s}",  -0.0));
    CHECK("0p+0"    == FormatArgs("{:x}",  0.0));
    CHECK("-0p+0"   == FormatArgs("{:x}",  -0.0));
    CHECK("0x0p+0"  == FormatArgs("{:#x}",  0.0));
    CHECK("-0x0p+0" == FormatArgs("{:#x}",  -0.0));

    CHECK("1.0p+0"       == FormatArgs("{:.1x}",   1.0));
    CHECK("1.00p+0"      == FormatArgs("{:.2x}",   1.0));
    CHECK("0x1.0p+0"     == FormatArgs("{:#.1x}",   1.0));
    CHECK("0x1.00p+0"    == FormatArgs("{:#.2x}",   1.0));
    CHECK("0X1.0P+0"     == FormatArgs("{:#.1X}",   1.0));
    CHECK("0X1.00P+0"    == FormatArgs("{:#.2X}",   1.0));
    CHECK("1.badp+1"     == FormatArgs("{:.3x}", 3.4597));
    CHECK("1.bad7p+1"    == FormatArgs("{:.4x}", 3.4597));
    CHECK("1.bad77p+1"   == FormatArgs("{:.5x}", 3.4597));
    CHECK("0X1.BADP+1"   == FormatArgs("{:#.3X}", 3.4597));
    CHECK("0X1.BAD7P+1"  == FormatArgs("{:#.4X}", 3.4597));
    CHECK("0X1.BAD77P+1" == FormatArgs("{:#.5X}", 3.4597));

    CHECK("0x1p+0"    == FormatArgs("{:a}",     1.0));
    CHECK("0x1p+0"    == FormatArgs("{:.0a}",   1.0));
    CHECK("0x1.0p+0"  == FormatArgs("{:.1a}",   1.0));
    CHECK("0x1.00p+0" == FormatArgs("{:.2a}",   1.0));

    double InvVal = std::numeric_limits<double>::infinity();
    CHECK("inf"    == FormatArgs("{:s}", InvVal));
    CHECK("   inf" == FormatArgs("{:6s}", InvVal));
    CHECK("   inf" == FormatArgs("{:06s}", InvVal));
    CHECK("INF"    == FormatArgs("{:S}", InvVal));
    CHECK("inf"    == FormatArgs("{:x}", InvVal));
    CHECK("INF"    == FormatArgs("{:X}", InvVal));
    CHECK("-inf"   == FormatArgs("{:s}", -InvVal));
    CHECK("  -inf" == FormatArgs("{:6s}", -InvVal));
    CHECK("  -inf" == FormatArgs("{:06s}", -InvVal));
    CHECK("-INF"   == FormatArgs("{:S}", -InvVal));
    CHECK("-inf"   == FormatArgs("{:x}", -InvVal));
    CHECK("-INF"   == FormatArgs("{:X}", -InvVal));

    // infinity with sign (and fill)
    CHECK("-INF"   == FormatArgs("{:+S}", -InvVal));
    CHECK("-INF"   == FormatArgs("{:-S}", -InvVal));
    CHECK("-INF"   == FormatArgs("{: S}", -InvVal));
    CHECK("-INF"   == FormatArgs("{:.< S}", -InvVal));
    CHECK("+INF"   == FormatArgs("{:+S}", InvVal));
    CHECK("INF"    == FormatArgs("{:-S}", InvVal));
    CHECK(" INF"   == FormatArgs("{: S}", InvVal));
    CHECK(".INF"   == FormatArgs("{:.< S}", InvVal));
    CHECK("  -INF" == FormatArgs("{:+06S}", -InvVal));
    CHECK("  -INF" == FormatArgs("{:-06S}", -InvVal));
    CHECK("  -INF" == FormatArgs("{: 06S}", -InvVal));
    CHECK("-INF.." == FormatArgs("{:.<06S}", -InvVal));
    CHECK("-INF.." == FormatArgs("{:.< 06S}", -InvVal));
    CHECK("  +INF" == FormatArgs("{:+06S}", InvVal));
    CHECK("   INF" == FormatArgs("{:-06S}", InvVal));
    CHECK("   INF" == FormatArgs("{: 06S}", InvVal));
    CHECK("INF..." == FormatArgs("{:.<06S}", InvVal));
    CHECK(".INF.." == FormatArgs("{:.< 06S}", InvVal));

    double NanVal = std::numeric_limits<double>::quiet_NaN();
    CHECK("nan" == FormatArgs("{:s}", NanVal));
    CHECK("NAN" == FormatArgs("{:S}", NanVal));
    CHECK("nan" == FormatArgs("{:x}", NanVal));
    CHECK("NAN" == FormatArgs("{:X}", NanVal));
    CHECK("nan" == FormatArgs("{:s}", -NanVal));
    CHECK("NAN" == FormatArgs("{:S}", -NanVal));
    CHECK("nan" == FormatArgs("{:x}", -NanVal));
    CHECK("NAN" == FormatArgs("{:X}", -NanVal));

    CHECK("1.000000" == FormatArgs("{:f}",    1.0));
    CHECK("1"        == FormatArgs("{:.f}",   1.0));
    CHECK("1"        == FormatArgs("{:.0f}",  1.0));
    CHECK("1.0"      == FormatArgs("{:.1f}",  1.0));
    CHECK("1.00"     == FormatArgs("{:.2f}",  1.0));
    CHECK("1.000000" == FormatArgs("{:#f}",   1.0));
    CHECK("1."       == FormatArgs("{:#.0f}", 1.0));
    CHECK("1.0"      == FormatArgs("{:#.1f}", 1.0));
    CHECK("1.00"     == FormatArgs("{:#.2f}", 1.0));

    CHECK("1'234.000000" == FormatArgs("{:'f}",    1234.0));
    CHECK("1'234"        == FormatArgs("{:'.f}",   1234.0));
    CHECK("1'234"        == FormatArgs("{:'.0f}",  1234.0));
    CHECK("1'234.0"      == FormatArgs("{:'.1f}",  1234.0));
    CHECK("1'234.00"     == FormatArgs("{:'.2f}",  1234.0));
    CHECK("1'234.000000" == FormatArgs("{:'#f}",   1234.0));
    CHECK("1'234."       == FormatArgs("{:'#.0f}", 1234.0));
    CHECK("1'234.0"      == FormatArgs("{:'#.1f}", 1234.0));
    CHECK("1'234.00"     == FormatArgs("{:'#.2f}", 1234.0));

    CHECK("1'234.000000" == PrintfArgs("%'f",    1234.0));
    CHECK("1'234"        == PrintfArgs("%'.f",   1234.0));
    CHECK("1'234"        == PrintfArgs("%'.0f",  1234.0));
    CHECK("1'234.0"      == PrintfArgs("%'.1f",  1234.0));
    CHECK("1'234.00"     == PrintfArgs("%'.2f",  1234.0));
    CHECK("1'234.000000" == PrintfArgs("%'#f",   1234.0));
    CHECK("1'234."       == PrintfArgs("%'#.0f", 1234.0));
    CHECK("1'234.0"      == PrintfArgs("%'#.1f", 1234.0));
    CHECK("1'234.00"     == PrintfArgs("%'#.2f", 1234.0));

    CHECK("1.000000e+00" == FormatArgs("{:e}",    1.0));
    CHECK("1e+00"        == FormatArgs("{:.e}",   1.0));
    CHECK("1e+00"        == FormatArgs("{:.0e}",  1.0));
    CHECK("1.0e+00"      == FormatArgs("{:.1e}",  1.0));
    CHECK("1.00e+00"     == FormatArgs("{:.2e}",  1.0));
    CHECK("1.000000e+00" == FormatArgs("{:#e}",   1.0));
    CHECK("1.e+00"       == FormatArgs("{:#.0e}", 1.0));
    CHECK("1.0e+00"      == FormatArgs("{:#.1e}", 1.0));
    CHECK("1.00e+00"     == FormatArgs("{:#.2e}", 1.0));

    CHECK("1"       == FormatArgs("{:g}", 1.0));
    CHECK("1"       == FormatArgs("{:.g}", 1.0));
    CHECK("1"       == FormatArgs("{:.0g}", 1.0));
    CHECK("1"       == FormatArgs("{:.1g}", 1.0));
    CHECK("1"       == FormatArgs("{:.2g}", 1.0));
    CHECK("1.00000" == FormatArgs("{:#g}", 1.0));
    CHECK("1."      == FormatArgs("{:#.0g}", 1.0));
    CHECK("1."      == FormatArgs("{:#.1g}", 1.0));
    CHECK("1.0"     == FormatArgs("{:#.2g}", 1.0));

    CHECK("1e+10"       == FormatArgs("{:g}", 1.0e+10));
    CHECK("1e+10"       == FormatArgs("{:.g}", 1.0e+10));
    CHECK("1e+10"       == FormatArgs("{:.0g}", 1.0e+10));
    CHECK("1e+10"       == FormatArgs("{:.1g}", 1.0e+10));
    CHECK("1e+10"       == FormatArgs("{:.2g}", 1.0e+10));
    CHECK("1.00000e+10" == FormatArgs("{:#g}", 1.0e+10));
    CHECK("1.e+10"      == FormatArgs("{:#.0g}", 1.0e+10));
    CHECK("1.e+10"      == FormatArgs("{:#.1g}", 1.0e+10));
    CHECK("1.0e+10"     == FormatArgs("{:#.2g}", 1.0e+10));

    CHECK("0x1.fcac083126e98p+0" == FormatArgs("{:a}", 1.987));
    CHECK("0x2p+0"               == FormatArgs("{:.a}", 1.987));
    CHECK("0x2p+0"               == FormatArgs("{:.0a}", 1.987));
    CHECK("0x2.0p+0"             == FormatArgs("{:.1a}", 1.987));
    CHECK("0x1.fdp+0"            == FormatArgs("{:.2a}", 1.987));
    CHECK("0x1.fcac083126e98p+0" == FormatArgs("{:#a}", 1.987));
    CHECK("0x2.p+0"              == FormatArgs("{:#.a}", 1.987));
    CHECK("0x2.p+0"              == FormatArgs("{:#.0a}", 1.987));
    CHECK("0x2.0p+0"             == FormatArgs("{:#.1a}", 1.987));
    CHECK("0x1.fdp+0"            == FormatArgs("{:#.2a}", 1.987));

    {
        enum { kMaxFloatPrec = 1074 };
        enum { kBufSize = 309 + (309 - 1) / 3 + 1 + kMaxFloatPrec };
        char buf[kBufSize + 1/*null*/] = {0};
        {
            fmtxx::ArrayWriter w { buf, kBufSize };
            fmtxx::errc ec = fmtxx::format(w, "{:'.1074f}", std::numeric_limits<double>::max());
            REQUIRE(ec == fmtxx::errc::success);
            REQUIRE(w.size() == kBufSize);
        }
        {
            // Precision is clipped.
            fmtxx::ArrayWriter w { buf, kBufSize };
            fmtxx::errc ec = fmtxx::format(w, "{:'.1075f}", std::numeric_limits<double>::max());
            REQUIRE(ec == fmtxx::errc::success);
            REQUIRE(w.size() == kBufSize);
        }
    }
}

TEST_CASE("Pointers", "1")
{
#if UINTPTR_MAX == UINT64_MAX
    CHECK("0x0000000001020304"   == FormatArgs("{}", (void*)0x01020304));
    CHECK("18446744073709551615" == FormatArgs("{:d}", (void*)-1));
    CHECK("18446744073709551615" == FormatArgs("{:u}", (void*)-1));
#elif UINTPTR_MAX == UINT32_MAX
    CHECK("0x01020304" == FormatArgs("{}", (void*)0x01020304));
    CHECK("4294967295" == FormatArgs("{:d}", (void*)-1));
    CHECK("4294967295" == FormatArgs("{:u}", (void*)-1));
#endif
    CHECK("0x1020304" == FormatArgs("{:.0}", (void*)0x01020304));

    CHECK("(nil)"    == FormatArgs("{}", (void*)0));
    CHECK("(nil)"    == FormatArgs("{:3}", (void*)0));
    CHECK("(nil)"    == FormatArgs("{:3.3}", (void*)0));
    CHECK("   (nil)" == FormatArgs("{:8}", (void*)0));
    CHECK("   (nil)" == FormatArgs("{:8.3}", (void*)0));
    CHECK("(nil)"    == FormatArgs("{}", nullptr));
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

    CHECK(".......123" == FormatArgs("{*}", spec, 123));
    CHECK("......-123" == FormatArgs("{*}", spec, -123));
    CHECK(".......123" == FormatArgs("{1*}", spec, 123));
    CHECK("......-123" == FormatArgs("{1*}", spec, -123));
    CHECK(".......123" == FormatArgs("{1*0}", spec, 123));
    CHECK("......-123" == FormatArgs("{1*0}", spec, -123));
    CHECK(".......123" == FormatArgs("{0*1}", 123, spec));
    CHECK("......-123" == FormatArgs("{0*1}", -123, spec));

    CHECK("  3.14" == PrintfArgs("%*.*f", 6, 2, 3.1415));
    CHECK("  3.14" == PrintfArgs("%6.*f", 2, 3.1415));
    CHECK("3.14  " == PrintfArgs("%-6.*f", 2, 3.1415));
    CHECK("  3.14" == PrintfArgs("%3$*.*f", 6, 2, 3.1415));
    CHECK("  3.14" == PrintfArgs("%1$*2$.*3$f", 3.1415, 6, 2));
    CHECK("3.14  " == PrintfArgs("%1$*2$.*3$f", 3.1415, -6, 2));
    CHECK("3.14"   == PrintfArgs("%1$.*2$f", 3.1415, 2));
    CHECK("3.14"   == PrintfArgs("%1.*2$f", 3.1415, 2));
}

struct Foo {
    int value;
};

namespace fmtxx
{
    template <>
    struct FormatValue<Foo> {
        auto operator()(Writer& w, FormatSpec const& spec, Foo const& value) const {
            return fmtxx::format_value(w, spec, value.value);
        }
    };

#if 1
    //
    // XXX:
    // Must be a separate function like vformat(format, map)...
    //
    template <typename K, typename V, typename Pr, typename Alloc>
    struct FormatValue<std::map<K, V, Pr, Alloc>>
    {
        auto operator()(Writer& w, FormatSpec const& spec, std::map<K, V, Pr, Alloc> const& value) const
        {
            //auto const key = spec.key;
            auto const key = spec.style;
            auto const I = value.find(key);
            if (I == value.end()) {
                return fmtxx::format(w, "[[key '{}' does not exist]]", key);
            }
            return FormatValue<>{}(w, spec, I->second);
        }
    };
#endif
}

namespace foo2_ns
{
    struct Foo2 {
        int value;
    };

    inline std::ostream& operator <<(std::ostream& stream, Foo2 const& value) {
        stream.width(6);
        stream.fill('-');
        return stream << value.value;
    }
}

TEST_CASE("Custom", "1")
{
    CHECK("struct Foo '   123'"  == FormatArgs("struct Foo '{:6}'", Foo{123}));
    CHECK("struct Foo2 '  ---123'" == FormatArgs("struct Foo2 '{:8}'", foo2_ns::Foo2{123}));
    CHECK("struct Foo2 '---123'" == FormatArgs("struct Foo2 '{}'", foo2_ns::Foo2{123}));

#if 1
    std::map<std::string_view, int, std::less<>> map = {{"eins", 1}, {"zwei", 2}};
    //
    // XXX:
    // Must be a separate function like vformat(format, map)...
    //
    CHECK("1, 2" == FormatArgs("{0!eins}, {0!zwei}", map));
#if 0
    CHECK("1, 2" == FormatArgs("{0!{eins}}, {0!{zwei}}", map));
    CHECK("1, 2" == FormatArgs("{0!'eins'}, {0!'zwei'}", map));
#endif
#endif
}

TEST_CASE("Chars", "1")
{
    CHECK("A"     == FormatArgs("{}", 'A'));
    CHECK("A"     == FormatArgs("{:s}", 'A'));
    //CHECK("65"    == FormatArgs("{:d}", 'A'));
    //CHECK("41"    == FormatArgs("{:x}", 'A'));
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <vector>

class VectorBuffer : public fmtxx::Writer
{
    std::vector<char>& os;

public:
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

namespace fmtxx
{
    template <>
    struct FormatValue<std::vector<char>> {
        auto operator()(Writer& w, FormatSpec const& spec, std::vector<char> const& vec) const {
            return fmtxx::Util::format_string(w, spec, vec.data(), vec.size());
        }
    };
}

TEST_CASE("Vector", "1")
{
    std::vector<char> os;
    VectorBuffer buf { os };
    fmtxx::format(buf, "{:6}", -1234);
    REQUIRE(os.size() == 6);
    CHECK(os[0] == ' '); // pad
    CHECK(os[1] == '-'); // put
    CHECK(os[2] == '1'); // write...
    CHECK(os[3] == '2');
    CHECK(os[4] == '3');
    CHECK(os[5] == '4');

    std::vector<char> str = { '1', '2', '3', '4' };
    CHECK("1234" == FormatArgs("{}", str));
}

//------------------------------------------------------------------------------

TEST_CASE("FormatPretty1", "1")
{
    std::map<int, std::string_view> map = {
        {0, "null"},
        {1, "eins"},
        {2, "zwei"},
    };

    char buf[1000];
    auto const len = fmtxx::snformat(buf, "  {}  ", fmtxx::pretty(map));
    CHECK(std::string_view(buf, len) == R"(  [{0, "null"}, {1, "eins"}, {2, "zwei"}]  )");

    char arr1[] = "hello";
    CHECK("\"hello\"" == FormatArgs("{}", fmtxx::pretty(arr1)));
    CHECK("(nil)" == FormatArgs("{}", fmtxx::pretty(nullptr)));
}

TEST_CASE("FormatPretty2", "1")
{
    std::map<int, std::string> map = {
        {0, "null"},
        {1, "eins"},
        {2, "zwei"},
    };

    std::string s = fmtxx::string_format("  {}  ", fmtxx::pretty(map));
    CHECK(s == R"(  [{0, "null"}, {1, "eins"}, {2, "zwei"}]  )");

    char arr1[] = "hello";
    CHECK("\"hello\"" == FormatArgs("{}", fmtxx::pretty(arr1)));
    CHECK("(nil)" == FormatArgs("{}", fmtxx::pretty(nullptr)));
}

//------------------------------------------------------------------------------

TEST_CASE("Format", "ArrayWriter")
{
    REQUIRE(3u == fmtxx::snprintf(nullptr, 0, "%s", 123));

    char buf0[1];
    REQUIRE(3u == fmtxx::snprintf(buf0, "%s", 123));
    REQUIRE(buf0[0] == '\0');

    char buf1[3];
    REQUIRE(3u == fmtxx::snprintf(buf1, "%s", 123));
    REQUIRE(buf1[0] == '1');
    REQUIRE(buf1[1] == '2');
    REQUIRE(buf1[2] == '\0');

    char buf2[4];
    REQUIRE(3u == fmtxx::snprintf(buf2, "%s", 123));
    REQUIRE(buf2[0] == '1');
    REQUIRE(buf2[1] == '2');
    REQUIRE(buf2[2] == '3');
    REQUIRE(buf2[3] == '\0');
}

