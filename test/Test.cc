#include "../src/Format.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cfloat>
#include <clocale>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <cstdlib>

//------------------------------------------------------------------------------
// Clean me up, Scotty!
//------------------------------------------------------------------------------

static constexpr size_t kBufferSize = 8 * 1024;

using fmtxx::StringFormatResult;

struct ToCharsFormatter
{
    template <typename ...Args>
    static StringFormatResult do_format(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize];
        const auto res = fmtxx::format_to_chars(buf, buf + kBufferSize, format, args...);
        return { std::string(buf, res.next), res.ec };
    }

    template <typename ...Args>
    static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize];
        const auto res = fmtxx::printf_to_chars(buf, buf + kBufferSize, format, args...);
        return { std::string(buf, res.next), res.ec };
    }
};

#ifdef __linux__
struct FILEFormatter
{
    template <typename ...Args>
    static StringFormatResult do_format(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize] = {0};
        FILE* f = fmemopen(buf, kBufferSize, "w");
        const auto ec = fmtxx::format(f, format, args...);
        fclose(f); // flush!
        return { std::string(buf), ec };
    }

    template <typename ...Args>
    static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize] = {0};
        FILE* f = fmemopen(buf, kBufferSize, "w");
        const auto ec = fmtxx::printf(f, format, args...);
        fclose(f); // flush!
        return { std::string(buf), ec };
    }
};
#endif

struct ArrayFormatter
{
    template <typename ...Args>
    static StringFormatResult do_format(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize];
        fmtxx::ArrayWriter w{buf};
        const auto ec = fmtxx::format(w, format, args...);
        return { std::string(w.data(), w.size()), ec };
    }

    template <typename ...Args>
    static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
    {
        char buf[kBufferSize];
        fmtxx::ArrayWriter w{buf};
        const auto ec = fmtxx::printf(w, format, args...);
        return { std::string(w.data(), w.size()), ec };
    }
};

struct StringFormatter
{
   template <typename ...Args>
   static StringFormatResult do_format(cxx::string_view format, Args const&... args)
   {
       return fmtxx::string_format(format, args...);
   }

   template <typename ...Args>
   static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
   {
       return fmtxx::string_printf(format, args...);
   }
};

struct StreamFormatter
{
   template <typename ...Args>
   static StringFormatResult do_format(cxx::string_view format, Args const&... args)
   {
       std::ostringstream os;
       const auto ec = fmtxx::format(os, format, args...);
       return { os.str(), ec };
   }

   template <typename ...Args>
   static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
   {
       std::ostringstream os;
       const auto ec = fmtxx::printf(os, format, args...);
       return { os.str(), ec };
   }
};

#if 0
struct MemoryFormatter
{
   template <typename ...Args>
   static StringFormatResult do_format(cxx::string_view format, Args const&... args)
   {
       fmtxx::MemoryWriter<> w;
       const auto ec = fmtxx::format(w, format, args...);
       return { std::string(w.data(), w.size()), ec };
   }

   template <typename ...Args>
   static StringFormatResult do_printf(cxx::string_view format, Args const&... args)
   {
       fmtxx::MemoryWriter<> w;
       const auto ec = fmtxx::printf(w, format, args...);
       return { std::string(w.data(), w.size()), ec };
   }
};
#endif

template <typename Formatter>
struct FormatFn
{
    template <typename ...Args>
    static StringFormatResult apply(cxx::string_view format, Args const&... args)
    {
        return Formatter::do_format(format, args...);
    }
};

template <typename Formatter>
struct PrintfFn
{
    template <typename ...Args>
    static StringFormatResult apply(cxx::string_view format, Args const&... args)
    {
        return Formatter::do_printf(format, args...);
    }
};

template <template <typename> class Fn, typename ...Args>
static std::string FormatArgsTemplate(cxx::string_view format, Args const&... args)
{
    auto const res = Fn< ToCharsFormatter >::apply(format, args...);
//  REQUIRE(fmtxx::ErrorCode{} == res.ec);

#ifdef __linux__
    {
        auto const x = Fn< FILEFormatter >::apply(format, args...);
        REQUIRE(res.ec == x.ec);
        REQUIRE(res.str == x.str);
    }
#endif
    {
        auto const x = Fn< ArrayFormatter >::apply(format, args...);
        REQUIRE(res.ec == x.ec);
        REQUIRE(res.str == x.str);
    }
    {
        auto const x = Fn< StreamFormatter >::apply(format, args...);
        REQUIRE(res.ec == x.ec);
        REQUIRE(res.str == x.str);
    }
    {
        auto const x = Fn< StringFormatter >::apply(format, args...);
        REQUIRE(res.ec == x.ec);
        REQUIRE(res.str == x.str);
    }
#if 0
    {
        auto const x = Fn< MemoryFormatter >::apply(format, args...);
        REQUIRE(res.ec == x.ec);
        REQUIRE(res.str == x.str);
    }
#endif

    return res.str;
}

template <typename ...Args>
static std::string FormatArgs(cxx::string_view format, Args const&... args)
{
    return FormatArgsTemplate<FormatFn>(format, args...);
}

template <typename ...Args>
static std::string PrintfArgs(cxx::string_view format, Args const&... args)
{
    return FormatArgsTemplate<PrintfFn>(format, args...);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

TEST_CASE("FormatStringChecks_Format")
{
    fmtxx::ArrayWriter w{nullptr, 0};

    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, cxx::string_view("{*}", 1), 0, 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, cxx::string_view("{*}", 2), 0, 0));
    CHECK(fmtxx::ErrorCode::invalid_argument          == fmtxx::format(w, "{*}", 1));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{*", fmtxx::FormatSpec{}, 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{*}}", fmtxx::FormatSpec{}, 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, cxx::string_view("{}", 1), 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, cxx::string_view("{1}", 2), 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1.", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1.1", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1.1f", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{ ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1 ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1: ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1 ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1. ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1.1 ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{1:1.1f ", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{-1: >10.2f}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{:*10}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{-10}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{{}", 1));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{}}", 1));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "}", 1, 1, 1, 1, 1));
    CHECK(fmtxx::ErrorCode::index_out_of_range        == fmtxx::format(w, "{1}", 1));
    CHECK(fmtxx::ErrorCode::index_out_of_range        == fmtxx::format(w, "{1}{2}", 1, 2));
    CHECK(fmtxx::ErrorCode::index_out_of_range        == fmtxx::format(w, "{0}{2}", 1, 2));
    CHECK(fmtxx::ErrorCode::index_out_of_range        == fmtxx::format(w, "{10}", 1));
    CHECK(fmtxx::ErrorCode::index_out_of_range        == fmtxx::format(w, "{2147483647}", 1));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{2147483648}", 1));
    CHECK(fmtxx::ErrorCode{}                          == fmtxx::format(w, "{:2147483647}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{:2147483648}", 0));
    CHECK(fmtxx::ErrorCode{}                          == fmtxx::format(w, "{:.2147483647}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{:.2147483648}", 0));
    CHECK(fmtxx::ErrorCode::invalid_format_string     == fmtxx::format(w, "{:.", 0));
}

TEST_CASE("General_Format")
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

TEST_CASE("General_Printf")
{
    CHECK("Hello"                           == PrintfArgs("Hello",                                    0));
    CHECK("Bring me a beer"                 == PrintfArgs("Bring me a %s",                            "beer"));
    CHECK("From 0 to 10"                    == PrintfArgs("From %s to %s",                            0, 10));
    CHECK("dec:42 hex:2a oct:52 bin:101010" == PrintfArgs("dec:%1$d hex:%1$x oct:%1$o bin:%1$b",      42));
    CHECK("left            "                == PrintfArgs("%-16s",                                    "left"));
    CHECK("2 1 1 2"                         == PrintfArgs("%2$d %d %1$d %d",                          1, 2));
}

TEST_CASE("Strings")
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
}

TEST_CASE("Strings")
{
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
}

TEST_CASE("Strings")
{
    CHECK(">---<" == FormatArgs(">{}<", "---"));
    CHECK("<--->" == FormatArgs("<{}>", "---"));
    CHECK(">---<" == FormatArgs(">{0}<", "---"));
    CHECK("<--->" == FormatArgs("<{0}>", "---"));
    CHECK(">---<" == FormatArgs(">{0:s}<", "---"));
    CHECK("<--->" == FormatArgs("<{0:s}>", "---"));

    CHECK(">--->" == FormatArgs(">{0:}<s}>", "---"));
    CHECK("<---<" == FormatArgs("<{0:}>s}<", "---"));
    CHECK("^---^" == FormatArgs("^{0:}^s}^", "---"));
}

TEST_CASE("Strings")
{
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
}

TEST_CASE("Strings")
{
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

TEST_CASE("Ints")
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

    CHECK("2147483647"            == FormatArgs("{}", INT32_MAX));
    CHECK("-2147483648"           == FormatArgs("{}", INT32_MIN));
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
}

TEST_CASE("Ints")
{
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
}

TEST_CASE("Ints")
{
    CHECK("4294966062" == FormatArgs("{:u}", -1234));
    CHECK("4294966062" == FormatArgs("{: u}", -1234));
    CHECK("4294966062" == FormatArgs("{:+u}", -1234));
    CHECK("4294966062" == FormatArgs("{:-u}", -1234));
    CHECK("18446744073709550382" == FormatArgs("{:u}", -1234ll));
}

TEST_CASE("Ints")
{
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

TEST_CASE("Floats")
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
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
    CHECK("1p-1022"   == FormatArgs("{:x}", std::numeric_limits<double>::min()));
    CHECK("1p-1074"   == FormatArgs("{:x}", std::numeric_limits<double>::denorm_min()));
    CHECK("1P-1074"   == FormatArgs("{:X}", std::numeric_limits<double>::denorm_min()));
    CHECK("0x1p-1022" == FormatArgs("{:#x}", std::numeric_limits<double>::min()));
    CHECK("0x1p-1074" == FormatArgs("{:#x}", std::numeric_limits<double>::denorm_min()));
    CHECK("0X1P-1074" == FormatArgs("{:#X}", std::numeric_limits<double>::denorm_min()));
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
    CHECK("0"       == FormatArgs("{:s}",  0.0));
    CHECK("-0"      == FormatArgs("{:s}",  -0.0));
    CHECK("0p+0"    == FormatArgs("{:x}",  0.0));
    CHECK("-0p+0"   == FormatArgs("{:x}",  -0.0));
    CHECK("0x0p+0"  == FormatArgs("{:#x}",  0.0));
    CHECK("-0x0p+0" == FormatArgs("{:#x}",  -0.0));
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
    double NanVal = std::numeric_limits<double>::quiet_NaN();
    CHECK("nan" == FormatArgs("{:s}", NanVal));
    CHECK("NAN" == FormatArgs("{:S}", NanVal));
    CHECK("nan" == FormatArgs("{:x}", NanVal));
    CHECK("NAN" == FormatArgs("{:X}", NanVal));
    CHECK("nan" == FormatArgs("{:s}", -NanVal));
    CHECK("NAN" == FormatArgs("{:S}", -NanVal));
    CHECK("nan" == FormatArgs("{:x}", -NanVal));
    CHECK("NAN" == FormatArgs("{:X}", -NanVal));
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
    CHECK("1.000000e+00" == FormatArgs("{:e}",    1.0));
    CHECK("1e+00"        == FormatArgs("{:.e}",   1.0));
    CHECK("1e+00"        == FormatArgs("{:.0e}",  1.0));
    CHECK("1.0e+00"      == FormatArgs("{:.1e}",  1.0));
    CHECK("1.00e+00"     == FormatArgs("{:.2e}",  1.0));
    CHECK("1.000000e+00" == FormatArgs("{:#e}",   1.0));
    CHECK("1.e+00"       == FormatArgs("{:#.0e}", 1.0));
    CHECK("1.0e+00"      == FormatArgs("{:#.1e}", 1.0));
    CHECK("1.00e+00"     == FormatArgs("{:#.2e}", 1.0));
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
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
}

TEST_CASE("Floats")
{
    {
        enum { kMaxFloatPrec = 1074 };
        enum { kBufSize = 309 + (309 - 1) / 3 + 1 + kMaxFloatPrec };
        char buf[kBufSize + 1/*null*/] = {0};
        {
            fmtxx::ArrayWriter w { buf, kBufSize };
            fmtxx::ErrorCode ec = fmtxx::format(w, "{:'.1074f}", std::numeric_limits<double>::max());
            CHECK(fmtxx::ErrorCode{} == ec);
            CHECK(kBufSize == w.size());
        }
        {
            // Precision is clipped.
            fmtxx::ArrayWriter w { buf, kBufSize };
            fmtxx::ErrorCode ec = fmtxx::format(w, "{:'.1075f}", std::numeric_limits<double>::max());
            CHECK(fmtxx::ErrorCode{} == ec);
            CHECK(kBufSize == w.size());
        }
    }
}

TEST_CASE("Pointers_1")
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

TEST_CASE("Dynamic_1")
{
    fmtxx::FormatSpec spec;

    spec.width  = 10;
    spec.prec   = -1;
    spec.fill   = '.';
    spec.align  = fmtxx::Align::right;
    spec.sign   = fmtxx::Sign::space;
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

    CHECK("       123" == FormatArgs("{0:{1}}", 123, 10));
    CHECK("       123" == FormatArgs("{1:{}}", 10, 123));
    CHECK("      0123" == FormatArgs("{:{}.{}}", 10, 4, 123));
    CHECK("      0123" == FormatArgs("{0:{2}.{1}}", 123, 4, 10));
    CHECK("123......." == FormatArgs("{0:.<{1}}", 123, 10));
    CHECK("123......." == FormatArgs("{1:.<{}}", 10, 123));
    CHECK("0123......" == FormatArgs("{:.<{}.{}}", 10, 4, 123));
    CHECK("0123......" == FormatArgs("{0:.<{2}.{1}}", 123, 4, 10));

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
        fmtxx::ErrorCode operator()(Writer& w, FormatSpec const& spec, Foo const& value) const {
            return fmtxx::FormatValue<>{}(w, spec, value.value);
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
        fmtxx::ErrorCode operator()(Writer& w, FormatSpec const& spec, std::map<K, V, Pr, Alloc> const& value) const
        {
            //auto const key = spec.key;
            auto const key = spec.style;
            auto const I = value.find(std::string(key.data(), key.size()));
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

TEST_CASE("Custom_1")
{
    CHECK("struct Foo '   123'"  == FormatArgs("struct Foo '{:6}'", Foo{123}));
    CHECK("struct Foo2 '---123'" == FormatArgs("struct Foo2 '{:8}'", foo2_ns::Foo2{123})); // format-spec is ignored when using operator<< for formatting
    CHECK("struct Foo2 '---123'" == FormatArgs("struct Foo2 '{}'", foo2_ns::Foo2{123}));

#if 1
    std::map<std::string, int> map = {{"eins", 1}, {"zwei", 2}, {"dr}ei", 3}};
    //
    // XXX:
    // Must be a separate function like vformat(format, map)...
    //
    CHECK("1, 2" == FormatArgs("{0!eins}, {0!zwei}", map));
    CHECK("1, 2" == FormatArgs("{0!{eins}}, {0!{zwei}}", map));
    CHECK("1, 2" == FormatArgs("{0!'eins'}, {0!'zwei'}", map));
    CHECK("1, 2, 3" == FormatArgs("{0!'eins'}, {0!'zwei'}, {0!'dr}ei'}", map));
#endif
}

TEST_CASE("Chars_1")
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

private:
    fmtxx::ErrorCode Put(char c) override {
        os.push_back(c);
        return {};
    }

    fmtxx::ErrorCode Write(char const* str, size_t len) override {
        os.insert(os.end(), str, str + len);
        return {};
    }

    fmtxx::ErrorCode Pad(char c, size_t count) override {
        os.resize(os.size() + count, c);
        return {};
    }
};

namespace fmtxx
{
    template <>
    struct FormatValue<std::vector<char>> {
        fmtxx::ErrorCode operator()(Writer& w, FormatSpec const& spec, std::vector<char> const& vec) const {
            return fmtxx::Util::format_string(w, spec, vec.data(), vec.size());
        }
    };
}

TEST_CASE("Vector_1")
{
    std::vector<char> os;
    VectorBuffer buf { os };
    fmtxx::format(buf, "{:6}", -1234);
    REQUIRE(6u == os.size());
    CHECK(' ' == os[0]); // pad
    CHECK('-' == os[1]); // put
    CHECK('1' == os[2]); // write...
    CHECK('2' == os[3]);
    CHECK('3' == os[4]);
    CHECK('4' == os[5]);

    std::vector<char> str = { '1', '2', '3', '4' };
    CHECK("1234" == FormatArgs("{}", str));
}

//------------------------------------------------------------------------------

TEST_CASE("FormatPretty_1")
{
    using Map = std::map<int, std::string>;

    Map map = {
        {0, "null"},
        {1, "eins"},
        {2, "zwei"},
    };

    static_assert(fmtxx::impl::type_traits::IsContainer<Map          >::value, "");
    static_assert(fmtxx::impl::type_traits::IsContainer<Map const&   >::value, "");
    static_assert(fmtxx::impl::type_traits::IsContainer<Map&         >::value, "");
    static_assert(fmtxx::impl::type_traits::IsContainer<Map const&&  >::value, "");
    static_assert(fmtxx::impl::type_traits::IsContainer<Map&&        >::value, "");

    char buf[1000];
    auto const len = fmtxx::snformat(buf, "  {}  ", fmtxx::pretty(map));
    CHECK(R"(  [{0, "null"}, {1, "eins"}, {2, "zwei"}]  )" == std::string(buf, len));

    char arr1[] = "hello";
    CHECK("\"hello\"" == FormatArgs("{}", fmtxx::pretty(arr1)));
    CHECK("(nil)" == FormatArgs("{}", fmtxx::pretty(nullptr)));

    int arr2[] = {1,2,3};
    CHECK("[1-2-3]" == FormatArgs("{!-}", fmtxx::pretty(arr2)));
}

TEST_CASE("FormatPretty_2")
{
    std::map<int, std::string> map = {
        {0, "null"},
        {1, "eins"},
        {2, "zwei"},
    };

    std::string s = fmtxx::string_format("  {}  ", fmtxx::pretty(map)).str;
    CHECK(R"(  [{0, "null"}, {1, "eins"}, {2, "zwei"}]  )" == s);

    char arr1[] = "hello";
    CHECK("\"hello\"" == FormatArgs("{}", fmtxx::pretty(arr1)));
    CHECK("(nil)" == FormatArgs("{}", fmtxx::pretty(nullptr)));
}

TEST_CASE("FormatPretty_3")
{
    std::tuple<int, double, std::string> tup {123, 1.23, "123"};

    //static_assert(fmtxx::impl::IsTuple<decltype(tup)>::value, "Error!");

    std::string s = fmtxx::string_format("  {}  ", fmtxx::pretty(tup)).str;
    CHECK(R"(  {123, 1.23, "123"}  )" == s);
}

//------------------------------------------------------------------------------

TEST_CASE("ArrayWriter_1")
{
    CHECK(3 == fmtxx::snprintf(nullptr, 0, "%s", 123));

    char buf0[1] = {'x'};
    CHECK(3 == fmtxx::snprintf(buf0, "%s", 123));
    CHECK('\0' == buf0[0]);

    char buf1[3] = {'x','x','x'};
    CHECK(3 == fmtxx::snprintf(buf1, "%s", 123));
    CHECK('1' == buf1[0]);
    CHECK('2' == buf1[1]);
    CHECK('\0' == buf1[2]);

    char buf2[4] = {'x','x','x','x'};
    CHECK(3 == fmtxx::snprintf(buf2, "%s", 123));
    CHECK('1' == buf2[0]);
    CHECK('2' == buf2[1]);
    CHECK('3' == buf2[2]);
    CHECK('\0' == buf2[3]);
}

//------------------------------------------------------------------------------

TEST_CASE("FormatArgs_1")
{
    fmtxx::FormatArgs args;

    const std::string str_world = "world";
    int i = 42;
    args.push_back(i);
    args.push_back(42);
    args.push_back("hello");
    //args.push_back(std::string("world"));     // should not compile
    args.push_back(str_world);
    args.push_back(cxx::string_view("hello"));

    auto const s = fmtxx::string_format("{} {} {} {} {}", args).str;
    CHECK("42 42 hello world hello" == s);

    //fmtxx::format(stdout, "", 1, args);           // should not compile
    //fmtxx::format(stdout, "", args, 1);           // should not compile
}

//static void alpha_delta_227529() {}
//struct Base1 { int a1; void func1() {} };
//struct Base2 { int a2; void func2() {} };
//struct S : Base1, Base2 { int i; void func() {} };
//namespace fmtxx { template <> struct TreatAsString<S> : std::true_type {}; }
//static void ShouldNotCompile()
//{
//    //std::cout << alpha_delta_227529;
//    //std::cout << &S::func;
//    //std::cout << &S::i;
//    //std::cout << (int*)nullptr;
//
//    //fmtxx::format(stdout, "", alpha_delta_227529);
//    //fmtxx::format(stdout, "", &S::func);
//    //fmtxx::format(stdout, "", &S::i);
//    //fmtxx::format(stdout, "", (int*)nullptr);
//    //fmtxx::format(stdout, "", S{});
//    fmtxx::format(stdout, "", std::hex);
//
//    fmtxx::ArrayWriter w{nullptr, 0};
//    //fmtxx::FormatValue<>{}(w, {}, alpha_delta_227529);
//    //fmtxx::FormatValue<>{}(w, {}, &S::func);
//    //fmtxx::FormatValue<>{}(w, {}, &S::i);
//    //fmtxx::FormatValue<>{}(w, {}, (int*)nullptr);
//    //fmtxx::FormatValue<>{}(w, {}, S{});
//}

//------------------------------------------------------------------------------
// printf-conformance tests
//------------------------------------------------------------------------------

// From
// https://github.com/BartMassey/printf-tests/blob/master/sources/tests-libc-sprintf.c

/*
 * Copyright (c) 2011 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Kevin Wolf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _MSC_VER
#pragma warning(disable: 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
#endif

// printf-conformance test
#if 0
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wformat"
#endif
#define CHECK_EQUAL_PRINTF(EXPECTED, N, FORMAT, ...)                                \
    {                                                                               \
        std::string printf_result;                                                  \
        printf_result.resize(kBufferSize);                                          \
        int n = ::snprintf(&printf_result[0], kBufferSize, FORMAT, ## __VA_ARGS__); \
        REQUIRE(n >= 0);                                                            \
        REQUIRE(n < kBufferSize);                                                   \
        CHECK(n == N);                                                              \
        printf_result.resize(static_cast<size_t>(n));                               \
        CHECK(EXPECTED == printf_result);                                           \
        std::string out = fmtxx::string_printf(FORMAT, ## __VA_ARGS__).str;         \
        CHECK(EXPECTED == out);                                                     \
    }                                                                               \
    /**/
#else
#define CHECK_EQUAL_PRINTF(EXPECTED, N, FORMAT, ...)                                \
    {                                                                               \
        std::string out = fmtxx::string_printf(FORMAT, ## __VA_ARGS__).str;         \
        CHECK(N == out.size());                                                     \
        CHECK(EXPECTED == out);                                                     \
    }                                                                               \
    /**/
#endif

TEST_CASE("Printf_conformance")
{
    /* Ein String ohne alles */
    CHECK_EQUAL_PRINTF("Hallo heimur", 12, "Hallo heimur")
}

TEST_CASE("Printf_conformance")
{
    /* Einfache Konvertierungen */
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "%s",       "Hallo heimur")
    CHECK_EQUAL_PRINTF("1024",            4, "%d",       1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%d",       -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%i",       1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%i",       -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%u",       1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "%u",       -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "%o",       0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "%o",       -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "%x",       0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "%x",       -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "%X",       0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "%X",       -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "%c",       'x')
    CHECK_EQUAL_PRINTF("%",               1, "%%")
}

TEST_CASE("Printf_conformance")
{
    /* Mit %c kann man auch Nullbytes ausgeben */
    CHECK_EQUAL_PRINTF(std::string("\0", 1),              1, "%c",       '\0')
}

TEST_CASE("Printf_conformance")
{
    /* Vorzeichen erzwingen (Flag +) */
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "%+s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("+1024",           5, "%+d",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%+d",      -1024)
    CHECK_EQUAL_PRINTF("+1024",           5, "%+i",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%+i",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%+u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "%+u",      -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "%+o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "%+o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "%+x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "%+x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "%+X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "%+X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "%+c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Vorzeichenplatzhalter erzwingen (Flag <space>) */
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "% s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF(" 1024",           5, "% d",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "% d",      -1024)
    CHECK_EQUAL_PRINTF(" 1024",           5, "% i",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "% i",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "% u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "% u",      -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "% o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "% o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "% x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "% x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "% X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "% X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "% c",      'x')
}


TEST_CASE("Printf_conformance")
{
    /* Flag + hat Vorrang ueber <space> */
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "%+ s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("+1024",           5, "%+ d",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%+ d",      -1024)
    CHECK_EQUAL_PRINTF("+1024",           5, "%+ i",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%+ i",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%+ u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "%+ u",      -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "%+ o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "%+ o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "%+ x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "%+ x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "%+ X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "%+ X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "%+ c",      'x')
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "% +s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("+1024",           5, "% +d",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "% +d",      -1024)
    CHECK_EQUAL_PRINTF("+1024",           5, "% +i",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "% +i",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "% +u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "% +u",      -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "% +o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "% +o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "% +x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "% +x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "% +X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "% +X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "% +c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Alternative Form */
    CHECK_EQUAL_PRINTF("0777",            4, "%#o",      0777u)
    CHECK_EQUAL_PRINTF("037777777001",   12, "%#o",      -0777u)
    CHECK_EQUAL_PRINTF("0x1234abcd",     10, "%#x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("0xedcb5433",     10, "%#x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("0X1234ABCD",     10, "%#X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("0XEDCB5433",     10, "%#X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("0",               1, "%#o",      0u)
#if 0
    // we always print a prefix
    CHECK_EQUAL_PRINTF("0",               1, "%#x",      0u)
    CHECK_EQUAL_PRINTF("0",               1, "%#X",      0u)
#endif
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Kleiner als Ausgabe */
    CHECK_EQUAL_PRINTF("Hallo heimur",   12, "%1s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("1024",            4, "%1d",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%1d",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%1i",      1024)
    CHECK_EQUAL_PRINTF("-1024",           5, "%1i",      -1024)
    CHECK_EQUAL_PRINTF("1024",            4, "%1u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272",     10, "%1u",      -1024u)
    CHECK_EQUAL_PRINTF("777",             3, "%1o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001",    11, "%1o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd",        8, "%1x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433",        8, "%1x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD",        8, "%1X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433",        8, "%1X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x",               1, "%1c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Groesser als Ausgabe */
    CHECK_EQUAL_PRINTF("               Hallo",  20, "%20s",      "Hallo")
    CHECK_EQUAL_PRINTF("                1024",  20, "%20d",      1024)
    CHECK_EQUAL_PRINTF("               -1024",  20, "%20d",      -1024)
    CHECK_EQUAL_PRINTF("                1024",  20, "%20i",      1024)
    CHECK_EQUAL_PRINTF("               -1024",  20, "%20i",      -1024)
    CHECK_EQUAL_PRINTF("                1024",  20, "%20u",      1024u)
    CHECK_EQUAL_PRINTF("          4294966272",  20, "%20u",      -1024u)
    CHECK_EQUAL_PRINTF("                 777",  20, "%20o",      0777u)
    CHECK_EQUAL_PRINTF("         37777777001",  20, "%20o",      -0777u)
    CHECK_EQUAL_PRINTF("            1234abcd",  20, "%20x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("            edcb5433",  20, "%20x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("            1234ABCD",  20, "%20X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("            EDCB5433",  20, "%20X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("                   x",  20, "%20c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Linksbuendig */
    CHECK_EQUAL_PRINTF("Hallo               ",  20, "%-20s",      "Hallo")
    CHECK_EQUAL_PRINTF("1024                ",  20, "%-20d",      1024)
    CHECK_EQUAL_PRINTF("-1024               ",  20, "%-20d",      -1024)
    CHECK_EQUAL_PRINTF("1024                ",  20, "%-20i",      1024)
    CHECK_EQUAL_PRINTF("-1024               ",  20, "%-20i",      -1024)
    CHECK_EQUAL_PRINTF("1024                ",  20, "%-20u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272          ",  20, "%-20u",      -1024u)
    CHECK_EQUAL_PRINTF("777                 ",  20, "%-20o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001         ",  20, "%-20o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd            ",  20, "%-20x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433            ",  20, "%-20x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD            ",  20, "%-20X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433            ",  20, "%-20X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x                   ",  20, "%-20c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Padding mit 0 */
    CHECK_EQUAL_PRINTF("00000000000000001024",  20, "%020d",      1024)
    CHECK_EQUAL_PRINTF("-0000000000000001024",  20, "%020d",      -1024)
    CHECK_EQUAL_PRINTF("00000000000000001024",  20, "%020i",      1024)
    CHECK_EQUAL_PRINTF("-0000000000000001024",  20, "%020i",      -1024)
    CHECK_EQUAL_PRINTF("00000000000000001024",  20, "%020u",      1024u)
    CHECK_EQUAL_PRINTF("00000000004294966272",  20, "%020u",      -1024u)
    CHECK_EQUAL_PRINTF("00000000000000000777",  20, "%020o",      0777u)
    CHECK_EQUAL_PRINTF("00000000037777777001",  20, "%020o",      -0777u)
    CHECK_EQUAL_PRINTF("0000000000001234abcd",  20, "%020x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("000000000000edcb5433",  20, "%020x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("0000000000001234ABCD",  20, "%020X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("000000000000EDCB5433",  20, "%020X",      -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Padding und alternative Form */
    CHECK_EQUAL_PRINTF("                0777",  20, "%#20o",      0777u)
    CHECK_EQUAL_PRINTF("        037777777001",  20, "%#20o",      -0777u)
    CHECK_EQUAL_PRINTF("          0x1234abcd",  20, "%#20x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("          0xedcb5433",  20, "%#20x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("          0X1234ABCD",  20, "%#20X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("          0XEDCB5433",  20, "%#20X",      -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    CHECK_EQUAL_PRINTF("00000000000000000777",  20, "%#020o",     0777u)
    CHECK_EQUAL_PRINTF("00000000037777777001",  20, "%#020o",     -0777u)
    CHECK_EQUAL_PRINTF("0x00000000001234abcd",  20, "%#020x",     0x1234abcdu)
    CHECK_EQUAL_PRINTF("0x0000000000edcb5433",  20, "%#020x",     -0x1234abcdu)
    CHECK_EQUAL_PRINTF("0X00000000001234ABCD",  20, "%#020X",     0x1234abcdu)
    CHECK_EQUAL_PRINTF("0X0000000000EDCB5433",  20, "%#020X",     -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: - hat Vorrang vor 0 */
    CHECK_EQUAL_PRINTF("Hallo               ",  20, "%0-20s",      "Hallo")
    CHECK_EQUAL_PRINTF("1024                ",  20, "%0-20d",      1024)
    CHECK_EQUAL_PRINTF("-1024               ",  20, "%0-20d",      -1024)
    CHECK_EQUAL_PRINTF("1024                ",  20, "%0-20i",      1024)
    CHECK_EQUAL_PRINTF("-1024               ",  20, "%0-20i",      -1024)
    CHECK_EQUAL_PRINTF("1024                ",  20, "%0-20u",      1024u)
    CHECK_EQUAL_PRINTF("4294966272          ",  20, "%0-20u",      -1024u)
    CHECK_EQUAL_PRINTF("777                 ",  20, "%-020o",      0777u)
    CHECK_EQUAL_PRINTF("37777777001         ",  20, "%-020o",      -0777u)
    CHECK_EQUAL_PRINTF("1234abcd            ",  20, "%-020x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("edcb5433            ",  20, "%-020x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD            ",  20, "%-020X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("EDCB5433            ",  20, "%-020X",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("x                   ",  20, "%-020c",      'x')
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite: Aus Parameter */
    CHECK_EQUAL_PRINTF("               Hallo",  20, "%*s",      20, "Hallo")
    CHECK_EQUAL_PRINTF("                1024",  20, "%*d",      20, 1024)
    CHECK_EQUAL_PRINTF("               -1024",  20, "%*d",      20, -1024)
    CHECK_EQUAL_PRINTF("                1024",  20, "%*i",      20, 1024)
    CHECK_EQUAL_PRINTF("               -1024",  20, "%*i",      20, -1024)
    CHECK_EQUAL_PRINTF("                1024",  20, "%*u",      20, 1024u)
    CHECK_EQUAL_PRINTF("          4294966272",  20, "%*u",      20, -1024u)
    CHECK_EQUAL_PRINTF("                 777",  20, "%*o",      20, 0777u)
    CHECK_EQUAL_PRINTF("         37777777001",  20, "%*o",      20, -0777u)
    CHECK_EQUAL_PRINTF("            1234abcd",  20, "%*x",      20, 0x1234abcdu)
    CHECK_EQUAL_PRINTF("            edcb5433",  20, "%*x",      20, -0x1234abcdu)
    CHECK_EQUAL_PRINTF("            1234ABCD",  20, "%*X",      20, 0x1234abcdu)
    CHECK_EQUAL_PRINTF("            EDCB5433",  20, "%*X",      20, -0x1234abcdu)
    CHECK_EQUAL_PRINTF("                   x",  20, "%*c",      20, 'x')
}

TEST_CASE("Printf_conformance")
{
    /* Praezision / Mindestanzahl von Ziffern */
    CHECK_EQUAL_PRINTF("Hallo heimur",           12, "%.20s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("00000000000000001024",   20, "%.20d",      1024)
    CHECK_EQUAL_PRINTF("-00000000000000001024",  21, "%.20d",      -1024)
    CHECK_EQUAL_PRINTF("00000000000000001024",   20, "%.20i",      1024)
    CHECK_EQUAL_PRINTF("-00000000000000001024",  21, "%.20i",      -1024)
    CHECK_EQUAL_PRINTF("00000000000000001024",   20, "%.20u",      1024u)
    CHECK_EQUAL_PRINTF("00000000004294966272",   20, "%.20u",      -1024u)
    CHECK_EQUAL_PRINTF("00000000000000000777",   20, "%.20o",      0777u)
    CHECK_EQUAL_PRINTF("00000000037777777001",   20, "%.20o",      -0777u)
    CHECK_EQUAL_PRINTF("0000000000001234abcd",   20, "%.20x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("000000000000edcb5433",   20, "%.20x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("0000000000001234ABCD",   20, "%.20X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("000000000000EDCB5433",   20, "%.20X",      -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    /* Feldbreite und Praezision */
    CHECK_EQUAL_PRINTF("               Hallo",   20, "%20.5s",     "Hallo heimur")
    CHECK_EQUAL_PRINTF("               01024",   20, "%20.5d",      1024)
    CHECK_EQUAL_PRINTF("              -01024",   20, "%20.5d",      -1024)
    CHECK_EQUAL_PRINTF("               01024",   20, "%20.5i",      1024)
    CHECK_EQUAL_PRINTF("              -01024",   20, "%20.5i",      -1024)
    CHECK_EQUAL_PRINTF("               01024",   20, "%20.5u",      1024u)
    CHECK_EQUAL_PRINTF("          4294966272",   20, "%20.5u",      -1024u)
    CHECK_EQUAL_PRINTF("               00777",   20, "%20.5o",      0777u)
    CHECK_EQUAL_PRINTF("         37777777001",   20, "%20.5o",      -0777u)
    CHECK_EQUAL_PRINTF("            1234abcd",   20, "%20.5x",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("          00edcb5433",   20, "%20.10x",     -0x1234abcdu)
    CHECK_EQUAL_PRINTF("            1234ABCD",   20, "%20.5X",      0x1234abcdu)
    CHECK_EQUAL_PRINTF("          00EDCB5433",   20, "%20.10X",     -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    /* Praezision: 0 wird ignoriert */
    CHECK_EQUAL_PRINTF("               Hallo",   20, "%020.5s",    "Hallo heimur")
    CHECK_EQUAL_PRINTF("               01024",   20, "%020.5d",     1024)
    CHECK_EQUAL_PRINTF("              -01024",   20, "%020.5d",     -1024)
    CHECK_EQUAL_PRINTF("               01024",   20, "%020.5i",     1024)
    CHECK_EQUAL_PRINTF("              -01024",   20, "%020.5i",     -1024)
    CHECK_EQUAL_PRINTF("               01024",   20, "%020.5u",     1024u)
    CHECK_EQUAL_PRINTF("          4294966272",   20, "%020.5u",     -1024u)
    CHECK_EQUAL_PRINTF("               00777",   20, "%020.5o",     0777u)
    CHECK_EQUAL_PRINTF("         37777777001",   20, "%020.5o",     -0777u)
    CHECK_EQUAL_PRINTF("            1234abcd",   20, "%020.5x",     0x1234abcdu)
    CHECK_EQUAL_PRINTF("          00edcb5433",   20, "%020.10x",    -0x1234abcdu)
    CHECK_EQUAL_PRINTF("            1234ABCD",   20, "%020.5X",     0x1234abcdu)
    CHECK_EQUAL_PRINTF("          00EDCB5433",   20, "%020.10X",    -0x1234abcdu)
}

TEST_CASE("Printf_conformance")
{
    /* Praezision 0 */
    CHECK_EQUAL_PRINTF("",                        0, "%.0s",        "Hallo heimur")
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.0s",      "Hallo heimur")
    CHECK_EQUAL_PRINTF("",                        0, "%.s",         "Hallo heimur")
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.s",       "Hallo heimur")
    CHECK_EQUAL_PRINTF("                1024",   20, "%20.0d",      1024)
    CHECK_EQUAL_PRINTF("               -1024",   20, "%20.d",       -1024)
    CHECK_EQUAL_PRINTF("                1024",   20, "%20.0i",      1024)
    CHECK_EQUAL_PRINTF("               -1024",   20, "%20.i",       -1024)
    CHECK_EQUAL_PRINTF("                1024",   20, "%20.u",       1024u)
    CHECK_EQUAL_PRINTF("          4294966272",   20, "%20.0u",      -1024u)
    CHECK_EQUAL_PRINTF("                 777",   20, "%20.o",       0777u)
    CHECK_EQUAL_PRINTF("         37777777001",   20, "%20.0o",      -0777u)
    CHECK_EQUAL_PRINTF("            1234abcd",   20, "%20.x",       0x1234abcdu)
    CHECK_EQUAL_PRINTF("            edcb5433",   20, "%20.0x",      -0x1234abcdu)
    CHECK_EQUAL_PRINTF("            1234ABCD",   20, "%20.X",       0x1234abcdu)
    CHECK_EQUAL_PRINTF("            EDCB5433",   20, "%20.0X",      -0x1234abcdu)
#if 0
    // not conforming if value == 0
    // but printf's behavior is quite surprising... at least for me
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.d",       0)
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.i",       0)
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.u",       0u)
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.o",       0u)
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.x",       0u)
    CHECK_EQUAL_PRINTF("                    ",   20, "%20.X",       0u)
#endif
}

TEST_CASE("Printf_conformance")
{
    /*
     * Praezision und Feldbreite aus Parameter.
     * + hat Vorrang vor <space>, - hat Vorrang vor 0 (das eh ignoriert wird,
     * weil eine Praezision angegeben ist)
     */
    CHECK_EQUAL_PRINTF("Hallo               ",   20, "% -0+*.*s",    20,  5, "Hallo heimur")
    CHECK_EQUAL_PRINTF("+01024              ",   20, "% -0+*.*d",    20,  5,  1024)
    CHECK_EQUAL_PRINTF("-01024              ",   20, "% -0+*.*d",    20,  5,  -1024)
    CHECK_EQUAL_PRINTF("+01024              ",   20, "% -0+*.*i",    20,  5,  1024)
    CHECK_EQUAL_PRINTF("-01024              ",   20, "% 0-+*.*i",    20,  5,  -1024)
    CHECK_EQUAL_PRINTF("01024               ",   20, "% 0-+*.*u",    20,  5,  1024u)
    CHECK_EQUAL_PRINTF("4294966272          ",   20, "% 0-+*.*u",    20,  5,  -1024u)
    CHECK_EQUAL_PRINTF("00777               ",   20, "%+ -0*.*o",    20,  5,  0777u)
    CHECK_EQUAL_PRINTF("37777777001         ",   20, "%+ -0*.*o",    20,  5,  -0777u)
    CHECK_EQUAL_PRINTF("1234abcd            ",   20, "%+ -0*.*x",    20,  5,  0x1234abcdu)
    CHECK_EQUAL_PRINTF("00edcb5433          ",   20, "%+ -0*.*x",    20, 10,  -0x1234abcdu)
    CHECK_EQUAL_PRINTF("1234ABCD            ",   20, "% -+0*.*X",    20,  5,  0x1234abcdu)
    CHECK_EQUAL_PRINTF("00EDCB5433          ",   20, "% -+0*.*X",    20, 10,  -0x1234abcdu)
}
