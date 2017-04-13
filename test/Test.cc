#if 0
cl /EHsc Test.cc Format.cc /std:c++latest /Fe: Test.exe
g++ Test.cc Format.cc
#endif

#include "Format.h"

#include <cfloat>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <clocale>

static int n_errors = 0;

//namespace fmtxx {
//    template <>
//    struct IsString<std::string> { enum { value = true }; };
//}

struct FormatterResult
{
    std::string str;
    fmtxx::errc ec;
};

struct StringFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        std::string os;
        const auto ec = fmtxx::Format(os, format, args...);
        return { os, ec };
    }
};

struct StreamFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        std::ostringstream os;
        const auto ec = fmtxx::Format(os, format, args...);
        return { os.str(), ec };
    }
};

#ifdef __linux__
struct FILEFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        char buf[1000] = {0};
        FILE* f = fmemopen(buf, sizeof(buf), "w");
        const auto ec = fmtxx::Format(f, format, args...);
        fclose(f); // flush!
        return { std::string(buf), ec };
    }
};
#endif

struct CharArrayFormatter
{
    template <typename ...Args>
    FormatterResult operator ()(std::string_view format, Args const&... args) const
    {
        char buf[500];
        fmtxx::CharArrayBuffer os { buf };
        const auto ec = fmtxx::Format(os, format, args...);
        return { std::string(buf, os.next), ec };
    }
};

template <typename Formatter, typename ...Args>
static bool expect_equal(char const* expected, std::string_view format, Args const&... args)
{
    Formatter formatter {};
    FormatterResult res = formatter(format, args...);

    if (res.ec != fmtxx::errc::success)
    {
        fprintf(stderr, "FAIL: invalid format string\n");
        return false;
    }

    if (expected != res.str)
    {
        ++n_errors;
        fprintf(stderr, "FAIL: '%s' != '%s'", expected, res.str.c_str());
        return false;
    }

    return true;
}

template <typename Formatter, typename ...Args>
static bool expect_equal_printf(std::string_view format, char conv, Args const&... args)
{
    std::string f0 = "%" + std::string(format) + conv;
    std::string f1 = "{:" + std::string(format) + conv + "}";

    // fprintf(stderr, "expect_equal_printf(\"%s\", \"%s\")\n", f0.c_str(), f1.c_str());

    Formatter formatter {};
    FormatterResult res = formatter(f1, args...);

    if (res.ec != fmtxx::errc::success)
    {
        fprintf(stderr, "FAIL: invalid format string\n");
        return false;
    }

    static size_t const kPrintfBufSize = 2000;
    char printf_buf[kPrintfBufSize];
    int n = snprintf(printf_buf, kPrintfBufSize, f0.c_str(), args...);
    if (n < 0 || static_cast<size_t>(n) >= kPrintfBufSize)
    {
        fprintf(stderr, "FAIL: snprintf error\n");
        return false;
    }

    auto const expected = std::string_view(printf_buf);
    if (expected != res.str)
    {
        ++n_errors;
        fprintf(stderr, "FAIL: '%.*s' != '%s'", static_cast<int>(expected.size()), expected.data(), res.str.c_str());
        return false;
    }

    return true;
}

#define EXPECT_EQUAL(EXPECTED, FORMAT, ...)             \
    if (!expect_equal<Formatter>(EXPECTED, FORMAT, __VA_ARGS__))   \
    {                                                   \
        fprintf(stderr, "    line %d\n", __LINE__);              \
    }                                                   \
    /**/

#define EXPECT_EQUAL_0(EXPECTED, FORMAT)                \
    if (!expect_equal<Formatter>(EXPECTED, FORMAT))                \
    {                                                   \
        fprintf(stderr, "    line %d\n", __LINE__);              \
    }                                                   \
    /**/

#define EXPECT_EQUAL_PRINTF(PRINTF_FORMAT, FORMAT, ...)             \
    if (!expect_equal_printf<Formatter>(PRINTF_FORMAT, FORMAT, __VA_ARGS__))   \
    {                                                   \
        fprintf(stderr, "    line %d\n", __LINE__);              \
    }                                                   \
    /**/

template <typename Formatter, typename ...Args>
static bool expect_errc(fmtxx::errc expected_err, std::string_view format, Args const&... args)
{
    Formatter formatter {};
    FormatterResult res = formatter(format, args...);

    if (res.ec != expected_err)
    {
        ++n_errors;
        fprintf(stderr, "FAIL: expected error code %d, got %d", (int)expected_err, (int)res.ec);
        return false;
    }

    return true;
}

#define EXPECT_ERRC(EXPECTED_ERR, FORMAT, ...)             \
    if (!expect_errc<Formatter>(EXPECTED_ERR, FORMAT, __VA_ARGS__)) { \
        fprintf(stderr, "    line %d\n", __LINE__);                  \
    }                                                       \
    /**/

template <typename Formatter>
static void test_format_specs()
{
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, std::string_view("{*}", 1), 0, 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, std::string_view("{*}", 2), 0, 0);
    EXPECT_ERRC(fmtxx::errc::success, "{*}", fmtxx::FormatSpec{}, 0);
    EXPECT_EQUAL("0", "{*}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERRC(fmtxx::errc::invalid_argument, "{*}", 1);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{*", fmtxx::FormatSpec{}, 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{*}}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERRC(fmtxx::errc::success, "{*}}}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, std::string_view("{}", 1), 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, std::string_view("{1}", 2), 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1.", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1.1", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1.1f", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{ ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1 ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1: ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1 ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1. ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1.1 ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{1:1.1f ", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{-1: >10.2f}", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:*10}", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{-10}", 0);
    EXPECT_ERRC(fmtxx::errc::success, "{:-10}", 0); // sign + width!
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{{}", 1); // stray '}'
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{}}", 1); // stray '}'
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "}", 1, 1, 1, 1, 1);
//  EXPECT_ERRC(fmtxx::errc::switching_indexing, "{0} {}", 1, 1);
//  EXPECT_ERRC(fmtxx::errc::switching_indexing, "{} {0}", 1, 1);
//  EXPECT_ERRC(fmtxx::errc::switching_indexing, "{0} {0} {}", 1, 1, 1);
//  EXPECT_ERRC(fmtxx::errc::switching_indexing, "{} {} {0}", 1, 1, 1);
    EXPECT_ERRC(fmtxx::errc::index_out_of_range, "{1}", 1);
    EXPECT_ERRC(fmtxx::errc::index_out_of_range, "{1}{2}", 1, 2);
    EXPECT_ERRC(fmtxx::errc::index_out_of_range, "{0}{2}", 1, 2);
    EXPECT_ERRC(fmtxx::errc::index_out_of_range, "{10}", 1);
    EXPECT_ERRC(fmtxx::errc::index_out_of_range, "{2147483647}", 1);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string/*overflow*/, "{2147483648}", 0);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string/*overflow*/, "{99999999999}", 1);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string/*overflow*/, "{:99999999999.0}", 1);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:.", 0);

    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:$.$f}", 5, 2, 3.1415);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:5.$f}", 2, 3.1415);
    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:$.2f}", 5, 3.1415);

//// $ parsed as the type...
////    EXPECT_ERRC(fmtxx::errc::invalid_format_string, "{:$}", 5, 'x');
}

template <typename T> struct incomplete;

template <typename Formatter>
static void test_0()
{
    EXPECT_EQUAL("Hello",                           "Hello",                                    0);
    EXPECT_EQUAL("Count to 10",                     "Count to {0}",                             10);
    EXPECT_EQUAL("Bring me a beer",                 "Bring me a {}",                            "beer");
    EXPECT_EQUAL("From 0 to 10",                    "From {} to {}",                            0, 10);
    EXPECT_EQUAL("From 0 to 10",                    "From {1} to {0}",                          10, 0);
    EXPECT_EQUAL("dec:42 hex:2a oct:52 bin:101010", "dec:{0:d} hex:{0:x} oct:{0:o} bin:{0:b}",  42);
    EXPECT_EQUAL("left<<<<<<<<<<<<",                "{:<<16}",                                  "left");
    EXPECT_EQUAL(".....center.....",                "{:.^16}",                                  "center");
    EXPECT_EQUAL(">>>>>>>>>>>right",                "{:>>16}",                                  "right");
    EXPECT_EQUAL("2 1 1 2",                         "{1} {} {0} {}",                            1, 2);
}

template <typename Formatter>
static void test_strings()
{
    EXPECT_EQUAL_0("", "");
    EXPECT_EQUAL_0("x", "x");

    EXPECT_EQUAL("x", "{}", 'x');
    EXPECT_EQUAL("x", "{:.0}", 'x');

    EXPECT_EQUAL_0("{", "{{");
    EXPECT_EQUAL_0("}", "}}");

    EXPECT_EQUAL("     xxx", "{:8}", "xxx");
    EXPECT_EQUAL("     xxx", "{:>8}", "xxx");
    EXPECT_EQUAL("xxx     ", "{:<8}", "xxx");
    EXPECT_EQUAL("  xxx   ", "{:^8}", "xxx");

    EXPECT_EQUAL(":Hello, world!:",    ":{}:",         "Hello, world!");
	EXPECT_EQUAL(":  Hello, world!:",  ":{:15}:",      "Hello, world!");
	EXPECT_EQUAL(":Hello, wor:",       ":{:.10}:",     "Hello, world!");
	EXPECT_EQUAL(":Hello, world!:",    ":{:<10}:",     "Hello, world!");
	EXPECT_EQUAL(":Hello, world!  :",  ":{:<15}:",     "Hello, world!");
	EXPECT_EQUAL(":Hello, world!:",    ":{:.15}:",     "Hello, world!");
	EXPECT_EQUAL(":     Hello, wor:",  ":{:15.10}:",   "Hello, world!");
	EXPECT_EQUAL(":Hello, wor     :",  ":{:<15.10}:",  "Hello, world!");

    std::string str = "hello hello hello hello hello hello hello hello hello hello ";
    EXPECT_EQUAL("hello hello hello hello hello hello hello hello hello hello ", "{}", str);

    EXPECT_EQUAL(">---<", ">{}<", "---");
    EXPECT_EQUAL("<--->", "<{}>", "---");
    EXPECT_EQUAL(">---<", ">{0}<", "---");
    EXPECT_EQUAL("<--->", "<{0}>", "---");
//    EXPECT_EQUAL(">---<", ">{0:}<", "---");
//    EXPECT_EQUAL("<--->", "<{0:}>", "---");
    EXPECT_EQUAL(">---<", ">{0:s}<", "---");
    EXPECT_EQUAL("<--->", "<{0:s}>", "---");

    EXPECT_EQUAL(">--->", ">{0:}<s}>", "---");
    EXPECT_EQUAL("<---<", "<{0:}>s}<", "---");
    EXPECT_EQUAL("^---^", "^{0:}^s}^", "---");

    EXPECT_EQUAL("(null)", "{}", (char*)0);
    EXPECT_EQUAL("(null)", "{}", (char const*)0);
    EXPECT_EQUAL("(null)", "{:.3}", (char const*)0);
    EXPECT_EQUAL("(null)", "{:.10}", (char const*)0);
    EXPECT_EQUAL("(null)", "{:3.3}", (char const*)0);
    EXPECT_EQUAL("    (null)", "{:10.3}", (char const*)0);

    std::string spad = std::string(128, ' ');
    EXPECT_EQUAL(spad.c_str(), "{:128}", ' ');

#if 0
    EXPECT_EQUAL(R"(Hello)",            "{:x}",     "Hello");
    EXPECT_EQUAL(R"(Hello)",            "{:X}",     "Hello");
    EXPECT_EQUAL(R"(He\x09\x0allo)",    "{:x}",     "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09\x0Allo)",    "{:X}",     "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09)",           "{:.3x}",   "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09)",           "{:.3X}",   "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09\x0a)",       "{:.4x}",   "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09\x0A)",       "{:.4X}",   "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09\x0allo)",    "{:.17x}",  "He\t\nllo");
    EXPECT_EQUAL(R"(He\x09\x0Allo)",    "{:.17X}",  "He\t\nllo");
    EXPECT_EQUAL(R"(He\011\012llo)", "{:o}", "He\t\nllo");
#endif
}

template <typename Formatter>
static void test_ints()
{
    EXPECT_EQUAL("2 1 1 2", "{1} {} {0} {}", 1, 2);

    static const int V = 0x12345;

    EXPECT_EQUAL("74565",   "{}",     V);
    EXPECT_EQUAL("-74565",  "{}",    -V);
    EXPECT_EQUAL(" 74565",  "{: }",   V);
    EXPECT_EQUAL("-74565",  "{: }",  -V);
    EXPECT_EQUAL("74565",   "{:-}",   V);
    EXPECT_EQUAL("-74565",  "{:-}",  -V);
    EXPECT_EQUAL("+74565",  "{:+}",   V);
    EXPECT_EQUAL("-74565",  "{:+}",  -V);

    EXPECT_EQUAL("hello 74565     ", "hello {:<10}",    V);
    EXPECT_EQUAL("hello -74565    ", "hello {:<10}",   -V);
    EXPECT_EQUAL("hello  74565    ", "hello {:< 10}",   V);
    EXPECT_EQUAL("hello -74565    ", "hello {:< 10}",  -V);
    EXPECT_EQUAL("hello 74565     ", "hello {:<-10}",   V);
    EXPECT_EQUAL("hello -74565    ", "hello {:<-10}",  -V);
    EXPECT_EQUAL("hello +74565    ", "hello {:<+10}",   V);
    EXPECT_EQUAL("hello -74565    ", "hello {:<+10}",  -V);

    EXPECT_EQUAL("     74565", "{:>10}",    V);
    EXPECT_EQUAL("    -74565", "{:>10}",   -V);
    EXPECT_EQUAL("     74565", "{:> 10}",   V);
    EXPECT_EQUAL("    -74565", "{:> 10}",  -V);
    EXPECT_EQUAL("     74565", "{:>-10}",   V);
    EXPECT_EQUAL("    -74565", "{:>-10}",  -V);
    EXPECT_EQUAL("    +74565", "{:>+10}",   V);
    EXPECT_EQUAL("    -74565", "{:>+10}",  -V);

    EXPECT_EQUAL("  74565   ", "{:^10}",    V);
    EXPECT_EQUAL("  -74565  ", "{:^10}",   -V);
    EXPECT_EQUAL("   74565  ", "{:^ 10}",   V);
    EXPECT_EQUAL("  -74565  ", "{:^ 10}",  -V);
    EXPECT_EQUAL("  74565   ", "{:^-10}",   V);
    EXPECT_EQUAL("  -74565  ", "{:^-10}",  -V);
    EXPECT_EQUAL("  +74565  ", "{:^+10}",   V);
    EXPECT_EQUAL("  -74565  ", "{:^+10}",  -V);

    EXPECT_EQUAL("0000074565", "{: <010}",    V);
    EXPECT_EQUAL("-000074565", "{: <010}",   -V);
    EXPECT_EQUAL(" 000074565", "{: < 010}",   V);
    EXPECT_EQUAL("-000074565", "{: < 010}",  -V);
    EXPECT_EQUAL("0000074565", "{: <-010}",   V);
    EXPECT_EQUAL("-000074565", "{: <-010}",  -V);
    EXPECT_EQUAL("+000074565", "{: <+010}",   V);
    EXPECT_EQUAL("-000074565", "{: <+010}",  -V);

    EXPECT_EQUAL("0000074565", "{: =010}",    V);
    EXPECT_EQUAL("-000074565", "{: =010}",   -V);
    EXPECT_EQUAL(" 000074565", "{: = 010}",   V);
    EXPECT_EQUAL("-000074565", "{: = 010}",  -V);
    EXPECT_EQUAL("0000074565", "{: =-010}",   V);
    EXPECT_EQUAL("-000074565", "{: =-010}",  -V);
    EXPECT_EQUAL("+000074565", "{: =+010}",   V);
    EXPECT_EQUAL("-000074565", "{: =+010}",  -V);

    EXPECT_EQUAL("0000074565",  "{:010}",     V);
    EXPECT_EQUAL("-000074565",  "{:010}",    -V);
    EXPECT_EQUAL("0745650000",  "{:0< 10}",    V);
    EXPECT_EQUAL("-745650000",  "{:0< 10}",   -V);

    EXPECT_EQUAL("2147483647", "{}", INT_MAX);
    EXPECT_EQUAL("-2147483648", "{}", INT_MIN);
    EXPECT_EQUAL("9223372036854775807", "{}", INT64_MAX);
    EXPECT_EQUAL("-9223372036854775808", "{}", INT64_MIN);

    EXPECT_EQUAL("1",     "{:x}",   (signed char) 1);
#if LIB_BASE_FORMAT_PROMOTE_TO_INT
    EXPECT_EQUAL("ffffffff", "{:x}", (signed char)-1);
#else
    EXPECT_EQUAL("ff",    "{:x}",   (signed char)-1);
#endif

    EXPECT_EQUAL("1",       "{:x}",   (signed short) 1);
#if LIB_BASE_FORMAT_PROMOTE_TO_INT
    EXPECT_EQUAL("ffffffff",    "{:x}",   (signed short)-1);
#else
    EXPECT_EQUAL("ffff",    "{:x}",   (signed short)-1);
#endif

    EXPECT_EQUAL("12345",       "{:x}",      V);
    EXPECT_EQUAL("fffedcbb",    "{:x}",     -V);
    EXPECT_EQUAL("00012345",    "{:08x}",    V);
    EXPECT_EQUAL("fffedcbb",    "{:08x}",   -V);

#if LONG_MAX != INT_MAX
    EXPECT_EQUAL("12345",               "{:x}",      (signed long)V);
    EXPECT_EQUAL("fffffffffffedcbb",    "{:x}",     -(signed long)V);
    EXPECT_EQUAL("00012345",            "{:08x}",    (signed long)V);
    EXPECT_EQUAL("fffffffffffedcbb",    "{:08x}",   -(signed long)V);
#endif

    EXPECT_EQUAL("12345",               "{:x}",   (signed long long) V);
    EXPECT_EQUAL("fffffffffffedcbb",    "{:x}",   (signed long long)-V);
    EXPECT_EQUAL("12345",               "{:X}",   (signed long long) V);
    EXPECT_EQUAL("FFFFFFFFFFFEDCBB",    "{:X}",   (signed long long)-V);

    EXPECT_EQUAL("1'234'567'890", "{:'13}", 1234567890);
    EXPECT_EQUAL("  123'456'789", "{:'13}", 123456789);
    EXPECT_EQUAL("   12'345'678", "{:'13}", 12345678);
    EXPECT_EQUAL("    1'234'567", "{:'13}", 1234567);
    EXPECT_EQUAL("      123'456", "{:'13}", 123456);
    EXPECT_EQUAL("       12'345", "{:'13}", 12345);
    EXPECT_EQUAL("        1'234", "{:'13}", 1234);
    EXPECT_EQUAL("          123", "{:'13}", 123);
    EXPECT_EQUAL("           12", "{:'13}", 12);
    EXPECT_EQUAL("            1", "{:'13}", 1);
    EXPECT_EQUAL("1_234_567_890", "{:_13}", 1234567890);
    EXPECT_EQUAL("  123_456_789", "{:_13}", 123456789);
    EXPECT_EQUAL("   12_345_678", "{:_13}", 12345678);
    EXPECT_EQUAL("    1_234_567", "{:_13}", 1234567);
    EXPECT_EQUAL("      123_456", "{:_13}", 123456);
    EXPECT_EQUAL("       12_345", "{:_13}", 12345);
    EXPECT_EQUAL("        1_234", "{:_13}", 1234);
    EXPECT_EQUAL("          123", "{:_13}", 123);
    EXPECT_EQUAL("           12", "{:_13}", 12);
    EXPECT_EQUAL("            1", "{:_13}", 1);
    EXPECT_EQUAL("18_446_744_073_709_551_615", "{:_}", UINT64_MAX);

    EXPECT_EQUAL("1234'5678", "{:'9x}", 0x12345678);
    EXPECT_EQUAL(" 123'4567", "{:'9x}", 0x1234567);
    EXPECT_EQUAL("  12'3456", "{:'9x}", 0x123456);
    EXPECT_EQUAL("   1'2345", "{:'9x}", 0x12345);
    EXPECT_EQUAL("     1234", "{:'9x}", 0x1234);
    EXPECT_EQUAL("      123", "{:'9x}", 0x123);
    EXPECT_EQUAL("       12", "{:'9x}", 0x12);
    EXPECT_EQUAL("        1", "{:'9x}", 0x1);

    EXPECT_EQUAL("7777_7777", "{:_9o}", 077777777);
    EXPECT_EQUAL(" 777_7777", "{:_9o}", 07777777);
    EXPECT_EQUAL("  77_7777", "{:_9o}", 0777777);
    EXPECT_EQUAL("   7_7777", "{:_9o}", 077777);
    EXPECT_EQUAL("     7777", "{:_9o}", 07777);
    EXPECT_EQUAL("      777", "{:_9o}", 0777);
    EXPECT_EQUAL("       77", "{:_9o}", 077);
    EXPECT_EQUAL("        7", "{:_9o}", 07);
    EXPECT_EQUAL("        0", "{:_9o}", 0);

    EXPECT_EQUAL("1111_1111", "{:_9b}", 0xFF);
    EXPECT_EQUAL(" 111_1111", "{:_9b}", 0x7F);
    EXPECT_EQUAL("  11_1111", "{:_9b}", 0x3F);
    EXPECT_EQUAL("   1_1111", "{:_9b}", 0x1F);
    EXPECT_EQUAL("     1111", "{:_9b}", 0x0F);
    EXPECT_EQUAL("      111", "{:_9b}", 0x07);
    EXPECT_EQUAL("       11", "{:_9b}", 0x03);
    EXPECT_EQUAL("        1", "{:_9b}", 0x01);
    EXPECT_EQUAL("        0", "{:_9b}", 0x00);
    EXPECT_EQUAL("1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111", "{:_b}", UINT64_MAX);

    EXPECT_EQUAL("4294966062", "{:u}", -1234);
    EXPECT_EQUAL("4294966062", "{: u}", -1234);
    EXPECT_EQUAL("4294966062", "{:+u}", -1234);
    EXPECT_EQUAL("4294966062", "{:-u}", -1234);
    EXPECT_EQUAL("18446744073709550382", "{:u}", -1234ll);

    EXPECT_EQUAL("0",   "{:x}",  0);
    EXPECT_EQUAL("0",   "{:b}",  0);
    EXPECT_EQUAL("0",   "{:o}",  0);
    EXPECT_EQUAL("1",   "{:x}",  1);
    EXPECT_EQUAL("1",   "{:b}",  1);
    EXPECT_EQUAL("1",   "{:o}",  1);
    EXPECT_EQUAL("0x0", "{:#x}", 0);
    EXPECT_EQUAL("0b0", "{:#b}", 0);
    EXPECT_EQUAL("0",   "{:#o}", 0);
    EXPECT_EQUAL("0x1", "{:#x}", 1);
    EXPECT_EQUAL("0b1", "{:#b}", 1);
    EXPECT_EQUAL("01",  "{:#o}", 1);
    EXPECT_EQUAL("0x00000000", "{:#010x}", 0);
    EXPECT_EQUAL("0b00000000", "{:#010b}", 0);
    EXPECT_EQUAL("0000000000", "{:#010o}", 0);
    EXPECT_EQUAL("0x00000001", "{:#010x}", 1);
    EXPECT_EQUAL("0b00000001", "{:#010b}", 1);
    EXPECT_EQUAL("0000000001", "{:#010o}", 1);
    EXPECT_EQUAL("       0x0", "{:#10x}",  0);
    EXPECT_EQUAL("       0b0", "{:#10b}",  0);
    EXPECT_EQUAL("         0", "{:#10o}",  0);
    EXPECT_EQUAL("       0x1", "{:#10x}",  1);
    EXPECT_EQUAL("       0b1", "{:#10b}",  1);
    EXPECT_EQUAL("        01", "{:#10o}",  1);
}

template <typename Formatter>
static void test_floats_printf(char conv)
{
    // fprintf(stderr, "test_floats_printf('%c')\n", conv);

#ifndef _WIN32
    EXPECT_EQUAL_PRINTF("",        conv, -0.0);
    EXPECT_EQUAL_PRINTF("",        conv, +0.0);
    EXPECT_EQUAL_PRINTF("",        conv, 1.0e-10);
    EXPECT_EQUAL_PRINTF("",        conv, 1.0e-6);
    EXPECT_EQUAL_PRINTF("",        conv, 1.0e-5);
    EXPECT_EQUAL_PRINTF("",        conv, 3.141592);
    EXPECT_EQUAL_PRINTF(".0",      conv, 3.141592);
    EXPECT_EQUAL_PRINTF(".1",      conv, 3.141592);
    EXPECT_EQUAL_PRINTF(".2",      conv, 3.141592);
    EXPECT_EQUAL_PRINTF("",        conv, 3141592.0);
    EXPECT_EQUAL_PRINTF("",        conv, 3141592.0e+47);
    EXPECT_EQUAL_PRINTF("#",       conv, -0.0);
    EXPECT_EQUAL_PRINTF("#",       conv, +0.0);
    EXPECT_EQUAL_PRINTF("#",       conv, 1.0e-10);
    EXPECT_EQUAL_PRINTF("#",       conv, 1.0e-6);
    EXPECT_EQUAL_PRINTF("#",       conv, 1.0e-5);
    EXPECT_EQUAL_PRINTF("#",       conv, 3.141592);
    EXPECT_EQUAL_PRINTF("#.0",     conv, 3.141592);
    EXPECT_EQUAL_PRINTF("#.1",     conv, 3.141592);
    EXPECT_EQUAL_PRINTF("#.2",     conv, 3.141592);
    EXPECT_EQUAL_PRINTF("#",       conv, 3141592.0);
    EXPECT_EQUAL_PRINTF("#",       conv, 3141592.0e+47);
    EXPECT_EQUAL_PRINTF("047",     conv, -0.0);
    EXPECT_EQUAL_PRINTF("047",     conv, +0.0);
    EXPECT_EQUAL_PRINTF("047",     conv, 1.0e-10);
    EXPECT_EQUAL_PRINTF("047",     conv, 1.0e-6);
    EXPECT_EQUAL_PRINTF("047",     conv, 1.0e-5);
    EXPECT_EQUAL_PRINTF("047",     conv, 3.141592);
    EXPECT_EQUAL_PRINTF("047.0",   conv, 3.141592);
    EXPECT_EQUAL_PRINTF("047.1",   conv, 3.141592);
    EXPECT_EQUAL_PRINTF("047.2",   conv, 3.141592);
    EXPECT_EQUAL_PRINTF("047",     conv, 3141592.0);
    EXPECT_EQUAL_PRINTF("047",     conv, 3141592.0e+47);
    EXPECT_EQUAL_PRINTF("+#047",   conv, -0.0);
    EXPECT_EQUAL_PRINTF("+#047",   conv, +0.0);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 1.0e-10);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 1.0e-6);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 1.0e-5);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 3.141592);
    EXPECT_EQUAL_PRINTF("+#047.0", conv, 3.141592);
    EXPECT_EQUAL_PRINTF("+#047.1", conv, 3.141592);
    EXPECT_EQUAL_PRINTF("+#047.2", conv, 3.141592);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 3141592.0);
    EXPECT_EQUAL_PRINTF("+#047",   conv, 3141592.0e+47);
    EXPECT_EQUAL_PRINTF(" #047",   conv, -0.0);
    EXPECT_EQUAL_PRINTF(" #047",   conv, +0.0);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 1.0e-10);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 1.0e-6);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 1.0e-5);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 3.141592);
    EXPECT_EQUAL_PRINTF(" #047.0", conv, 3.141592);
    EXPECT_EQUAL_PRINTF(" #047.1", conv, 3.141592);
    EXPECT_EQUAL_PRINTF(" #047.2", conv, 3.141592);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 3141592.0);
    EXPECT_EQUAL_PRINTF(" #047",   conv, 3141592.0e+47);
#endif
}

template <typename Formatter>
static void test_floats()
{
#if !defined(_WIN32) && !FMTXX_USE_DOUBLE_CONVERSION
    test_floats_printf<Formatter>('f');
    test_floats_printf<Formatter>('e');
    test_floats_printf<Formatter>('g');
    test_floats_printf<Formatter>('a');
#endif

#if FMTXX_USE_DOUBLE_CONVERSION
    EXPECT_EQUAL("24354608055603473107785637960931689315827890257544706015104721270340534493811981620606737277529913083605031584257830981831645089433797861274588973007916379823425649561385825684928346706685948919211835202051403608328731923243535575249303882582848104435881064910836763331355730531064189222587032782727341408256.000000", "{:f}", 2.4354608055603473e+307);

    static const double PI  = 3.1415926535897932384626433832795;

    EXPECT_EQUAL("0.000000",  "{:f}",   0.0);
    EXPECT_EQUAL("-0.000000", "{:f}",  -0.0);
    EXPECT_EQUAL(" 0.000000", "{: f}",  0.0);
    EXPECT_EQUAL("-0.000000", "{: f}", -0.0);
    EXPECT_EQUAL("+0.000000", "{:+f}",  0.0);
    EXPECT_EQUAL("-0.000000", "{:+f}", -0.0);

    EXPECT_EQUAL("0", "{:.0f}", 0.0);
    EXPECT_EQUAL("0.0", "{:.1f}", 0.0);
    EXPECT_EQUAL("0.000000e+00", "{:e}", 0.0);
    EXPECT_EQUAL("0e+00", "{:.0e}", 0.0);
    EXPECT_EQUAL("0.0e+00", "{:.1e}", 0.0);

    EXPECT_EQUAL("3.141593", "{:f}", PI);
    EXPECT_EQUAL("-3.141593", "{:f}", -PI);
    EXPECT_EQUAL("3.14", "{:.2f}", PI);
    EXPECT_EQUAL("-3.14", "{:.2f}", -PI);
    EXPECT_EQUAL("3.142", "{:.3f}", PI);
    EXPECT_EQUAL("-3.142", "{:.3f}", -PI);

    EXPECT_EQUAL("      3.141593",  "{:14f}",        PI);
    EXPECT_EQUAL("     -3.141593",  "{:14f}",       -PI);
    EXPECT_EQUAL("3.141593::::::",  "{::<14f}",      PI);
    EXPECT_EQUAL("-3.141593:::::",  "{::<14f}",     -PI);
    EXPECT_EQUAL("*3.141593*****",  "{:*< 14f}",     PI);
    EXPECT_EQUAL("-3.141593*****",  "{:*< 14f}",    -PI);
    EXPECT_EQUAL("+3.141593~~~~~",  "{:~<+14f}",     PI);
    EXPECT_EQUAL("-3.141593~~~~~",  "{:~<+14f}",    -PI);
    EXPECT_EQUAL("~~~~~~3.141593",  "{:~>14f}",      PI);
    EXPECT_EQUAL("~~~~~-3.141593",  "{:~>14f}",     -PI);
    EXPECT_EQUAL("~~~~~~3.141593",  "{:~> 14f}",     PI);
    EXPECT_EQUAL("~~~~~-3.141593",  "{:~> 14f}",    -PI);
    EXPECT_EQUAL("   3.141593   ",  "{: ^ 14f}",     PI);
    EXPECT_EQUAL("  -3.141593   ",  "{: ^ 14f}",    -PI);
    EXPECT_EQUAL("...3.141593...",  "{:.^ 14f}",     PI);
    EXPECT_EQUAL("..-3.141593...",  "{:.^ 14f}",    -PI);
    EXPECT_EQUAL("..+3.141593...",  "{:.^+14f}",     PI);
    EXPECT_EQUAL("..-3.141593...",  "{:.^+14f}",    -PI);

    // zero flag means align sign left
    EXPECT_EQUAL("0000003.141593",  "{:014f}",       PI);
    EXPECT_EQUAL("-000003.141593",  "{:014f}",      -PI);
    EXPECT_EQUAL("+000003.141593",  "{:+014f}",      PI);
    EXPECT_EQUAL("-000003.141593",  "{:+014f}",     -PI);
    EXPECT_EQUAL(" 000003.141593",  "{: 014f}",      PI);
    EXPECT_EQUAL("-000003.141593",  "{: 014f}",     -PI);
    EXPECT_EQUAL("3.141593000000",  "{:0<14f}",      PI);
    EXPECT_EQUAL("-3.14159300000",  "{:0<14f}",     -PI);
    EXPECT_EQUAL("+3.14159300000",  "{:0<+14f}",     PI);
    EXPECT_EQUAL("-3.14159300000",  "{:0<+14f}",    -PI);
    EXPECT_EQUAL("......3.141593",  "{:.=14f}",     PI);
    EXPECT_EQUAL("-.....3.141593",  "{:.=14f}",    -PI);
    EXPECT_EQUAL("+.....3.141593",  "{:.=+14f}",    PI);
    EXPECT_EQUAL("-.....3.141593",  "{:.=+14f}",   -PI);
    EXPECT_EQUAL("......3.141593",  "{:.= 14f}",    PI);
    EXPECT_EQUAL("-.....3.141593",  "{:.= 14f}",   -PI);
    EXPECT_EQUAL("3.141593......",  "{:.<14f}",     PI);
    EXPECT_EQUAL("-3.141593.....",  "{:.<14f}",    -PI);
    EXPECT_EQUAL("+3.141593.....",  "{:.<+14f}",    PI);
    EXPECT_EQUAL("-3.141593.....",  "{:.<+14f}",   -PI);
    EXPECT_EQUAL(".3.141593.....",  "{:.< 14f}",    PI);
    EXPECT_EQUAL("-3.141593.....",  "{:.< 14f}",   -PI);

    EXPECT_EQUAL("0.010000", "{:f}", 0.01);

    EXPECT_EQUAL("1.000000",     "{:f}",  1.0);
    EXPECT_EQUAL("1.000000e+00", "{:e}",  1.0);
    EXPECT_EQUAL("1.000000E+00", "{:E}",  1.0);
    EXPECT_EQUAL("1",            "{:g}",  1.0);

    EXPECT_EQUAL("1.200000",     "{:f}",  1.2);
    EXPECT_EQUAL("1.200000e+00", "{:e}",  1.2);
    EXPECT_EQUAL("1.200000E+00", "{:E}",  1.2);
    EXPECT_EQUAL("1.2",          "{:g}",  1.2);

    EXPECT_EQUAL("1.234568", "{:'f}", 1.23456789);
    EXPECT_EQUAL("12.345679", "{:'f}", 12.3456789);
    EXPECT_EQUAL("123.456789", "{:'f}", 123.456789);
    EXPECT_EQUAL("1'234.567890", "{:'f}", 1234.56789);
    EXPECT_EQUAL("12'345.678900", "{:'f}", 12345.6789);
    EXPECT_EQUAL("123'456.789000", "{:'f}", 123456.789);
    EXPECT_EQUAL("1'234'567.890000", "{:'f}", 1234567.89);

    EXPECT_EQUAL("123456.789000", "{:f}",  123456.789);
    EXPECT_EQUAL("1.234568e+05",  "{:e}",  123456.789);
    EXPECT_EQUAL("1.235e+05",     "{:.3e}",  123456.789);
    EXPECT_EQUAL("1.234568E+05",  "{:E}",  123456.789);
    EXPECT_EQUAL("123457",        "{:g}",  123456.789);
    EXPECT_EQUAL("1.23e+05",      "{:.3g}",  123456.789);
    EXPECT_EQUAL("    1.23e+05",  "{:12.3g}",  123456.789);
    EXPECT_EQUAL("1.23e+05    ",  "{:<12.3g}",  123456.789);
    EXPECT_EQUAL("  1.23e+05  ",  "{:^12.3g}",  123456.789);
    EXPECT_EQUAL("   -1.23e+05",  "{:-12.3g}",  -123456.789);
    EXPECT_EQUAL("-1.23e+05   ",  "{:<-12.3g}",  -123456.789);
    EXPECT_EQUAL(" -1.23e+05  ",  "{:^-12.3g}",  -123456.789);

    EXPECT_EQUAL("12345.678900",  "{:f}",  12345.6789);
    EXPECT_EQUAL("1.234568e+04",  "{:e}",  12345.6789);
    EXPECT_EQUAL("1.235e+04",     "{:.3e}",  12345.6789);
    EXPECT_EQUAL("1.234568E+04",  "{:E}",  12345.6789);
    EXPECT_EQUAL("12345.7",       "{:g}",  12345.6789);
    EXPECT_EQUAL("1.23e+04",      "{:.3g}",  12345.6789);

#if 0
    EXPECT_EQUAL("12_345.678900",  "{:_f}",  12345.6789);
#endif

    EXPECT_EQUAL("0",                 "{:s}", 0.0);
    EXPECT_EQUAL("10",              "{:s}", 10.0);
    EXPECT_EQUAL("10",              "{:S}", 10.0);
    EXPECT_EQUAL("-0",                "{:s}", -0.0);
    EXPECT_EQUAL("0p+0",                "{:x}", 0.0);
    EXPECT_EQUAL("0P+0",                "{:X}", 0.0);
    EXPECT_EQUAL("-0p+0",               "{:x}", -0.0);
    EXPECT_EQUAL("1.8p+0",              "{:x}", 1.5);
    EXPECT_EQUAL("0x1.8000p+0",           "{:.4a}", 1.5);
    EXPECT_EQUAL("1p+1",                "{:.0x}", 1.5);
    EXPECT_EQUAL("0x2p+0",                "{:.0a}", 1.5);
    EXPECT_EQUAL("0x1.921fb5a7ed197p+1",  "{:a}", 3.1415927);
    EXPECT_EQUAL("0X1.921FB5A7ED197P+1",  "{:A}", 3.1415927);
    EXPECT_EQUAL("0x1.922p+1",            "{:.3a}", 3.1415927);
    EXPECT_EQUAL("0x1.9220p+1",           "{:.4a}", 3.1415927);
    EXPECT_EQUAL("0x1.921fbp+1",          "{:.5a}", 3.1415927);
    EXPECT_EQUAL("      0x1.922p+1",            "{:16.3a}", 3.1415927);
    EXPECT_EQUAL("     0x1.9220p+1",           "{:16.4a}", 3.1415927);
    EXPECT_EQUAL("    0x1.921fbp+1",          "{:16.5a}", 3.1415927);
    EXPECT_EQUAL("0x0000001.922p+1",            "{:016.3a}", 3.1415927);
    EXPECT_EQUAL("0x000001.9220p+1",           "{:016.4a}", 3.1415927);
    EXPECT_EQUAL("0x00001.921fbp+1",          "{:016.5a}", 3.1415927);
    EXPECT_EQUAL("     -0x1.500p+5", "{:16.3a}", -42.0);
    EXPECT_EQUAL("    -0x1.5000p+5", "{:16.4a}", -42.0);
    EXPECT_EQUAL("   -0x1.50000p+5", "{:16.5a}", -42.0);
    EXPECT_EQUAL("-0x000001.500p+5", "{:016.3a}", -42.0);
    EXPECT_EQUAL("-0x00001.5000p+5", "{:016.4a}", -42.0);
    EXPECT_EQUAL("-0x0001.50000p+5", "{:016.5a}", -42.0);

    EXPECT_EQUAL("1p-1022",               "{:x}", std::numeric_limits<double>::min());
    EXPECT_EQUAL("1p-1074",               "{:x}", std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("1P-1074",               "{:X}", std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("0x1p-1022",               "{:#x}", std::numeric_limits<double>::min());
    EXPECT_EQUAL("0x1p-1074",               "{:#x}", std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("0X1P-1074",               "{:#X}", std::numeric_limits<double>::denorm_min());

    EXPECT_EQUAL("1.7976931348623157e+308",     "{:s}",  std::numeric_limits<double>::max());
    EXPECT_EQUAL("1.7976931348623157E+308",     "{:S}",  std::numeric_limits<double>::max());
    EXPECT_EQUAL("-1.7976931348623157e+308",    "{:s}", -std::numeric_limits<double>::max());
    EXPECT_EQUAL("2.2250738585072014e-308",     "{:s}",  std::numeric_limits<double>::min());
    EXPECT_EQUAL("-2.2250738585072014e-308",    "{:s}", -std::numeric_limits<double>::min());
    EXPECT_EQUAL("-2.2250738585072014E-308",    "{:S}", -std::numeric_limits<double>::min());
    EXPECT_EQUAL("5e-324",                    "{:s}",  std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("-5e-324",                   "{:s}", -std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("                  5e-324",    "{:>24s}",  std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("                 -5e-324",    "{:>24s}", -std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("                  5e-324",    "{: =24s}",  std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("-                 5e-324",    "{: =24s}", -std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("0000000000000000005e-324",    "{:024s}",  std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("-000000000000000005e-324",    "{:024s}", -std::numeric_limits<double>::denorm_min());

    EXPECT_EQUAL("0",  "{:s}",  0.0);
    EXPECT_EQUAL("-0", "{:s}",  -0.0);
    EXPECT_EQUAL("0p+0",  "{:x}",  0.0);
    EXPECT_EQUAL("-0p+0", "{:x}",  -0.0);
    EXPECT_EQUAL("0x0p+0",  "{:#x}",  0.0);
    EXPECT_EQUAL("-0x0p+0", "{:#x}",  -0.0);

    EXPECT_EQUAL("1.0p+0",  "{:.1x}",   1.0);
    EXPECT_EQUAL("1.00p+0", "{:.2x}",   1.0);
    EXPECT_EQUAL("0x1.0p+0",  "{:#.1x}",   1.0);
    EXPECT_EQUAL("0x1.00p+0", "{:#.2x}",   1.0);
    EXPECT_EQUAL("0X1.0P+0",  "{:#.1X}",   1.0);
    EXPECT_EQUAL("0X1.00P+0", "{:#.2X}",   1.0);
    EXPECT_EQUAL("1.badp+1",   "{:.3x}", 3.4597);
    EXPECT_EQUAL("1.bad7p+1",  "{:.4x}", 3.4597);
    EXPECT_EQUAL("1.bad77p+1", "{:.5x}", 3.4597);
    EXPECT_EQUAL("0X1.BADP+1",   "{:#.3X}", 3.4597);
    EXPECT_EQUAL("0X1.BAD7P+1",  "{:#.4X}", 3.4597);
    EXPECT_EQUAL("0X1.BAD77P+1", "{:#.5X}", 3.4597);

    EXPECT_EQUAL("0x1p+0",    "{:a}",     1.0);
    EXPECT_EQUAL("0x1p+0",    "{:.0a}",   1.0);
    EXPECT_EQUAL("0x1.0p+0",  "{:.1a}",   1.0);
    EXPECT_EQUAL("0x1.00p+0", "{:.2a}",   1.0);

    double InvVal = std::numeric_limits<double>::infinity();
    EXPECT_EQUAL("inf", "{:s}", InvVal);
    EXPECT_EQUAL("   inf", "{:6s}", InvVal);
    EXPECT_EQUAL("   inf", "{:06s}", InvVal);
    EXPECT_EQUAL("INF", "{:S}", InvVal);
    EXPECT_EQUAL("inf", "{:x}", InvVal);
    EXPECT_EQUAL("INF", "{:X}", InvVal);
    EXPECT_EQUAL("-inf", "{:s}", -InvVal);
    EXPECT_EQUAL("  -inf", "{:6s}", -InvVal);
    EXPECT_EQUAL("  -inf", "{:06s}", -InvVal);
    EXPECT_EQUAL("-INF", "{:S}", -InvVal);
    EXPECT_EQUAL("-inf", "{:x}", -InvVal);
    EXPECT_EQUAL("-INF", "{:X}", -InvVal);

    // infinity with sign (and fill)
    EXPECT_EQUAL("-INF", "{:+S}", -InvVal);
    EXPECT_EQUAL("-INF", "{:-S}", -InvVal);
    EXPECT_EQUAL("-INF", "{: S}", -InvVal);
    EXPECT_EQUAL("-INF", "{:.< S}", -InvVal);
    EXPECT_EQUAL("+INF", "{:+S}", InvVal);
    EXPECT_EQUAL("INF",  "{:-S}", InvVal);
    EXPECT_EQUAL(" INF", "{: S}", InvVal);
    EXPECT_EQUAL(".INF", "{:.< S}", InvVal);
    EXPECT_EQUAL("  -INF", "{:+06S}", -InvVal);
    EXPECT_EQUAL("  -INF", "{:-06S}", -InvVal);
    EXPECT_EQUAL("  -INF", "{: 06S}", -InvVal);
    EXPECT_EQUAL("-INF..", "{:.<06S}", -InvVal);
    EXPECT_EQUAL("-INF..", "{:.< 06S}", -InvVal);
    EXPECT_EQUAL("  +INF", "{:+06S}", InvVal);
    EXPECT_EQUAL("   INF",  "{:-06S}", InvVal);
    EXPECT_EQUAL("   INF", "{: 06S}", InvVal);
    EXPECT_EQUAL("INF...", "{:.<06S}", InvVal);
    EXPECT_EQUAL(".INF..", "{:.< 06S}", InvVal);

    double NanVal = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQUAL("nan", "{:s}", NanVal);
    EXPECT_EQUAL("NAN", "{:S}", NanVal);
    EXPECT_EQUAL("nan", "{:x}", NanVal);
    EXPECT_EQUAL("NAN", "{:X}", NanVal);
    EXPECT_EQUAL("nan", "{:s}", -NanVal);
    EXPECT_EQUAL("NAN", "{:S}", -NanVal);
    EXPECT_EQUAL("nan", "{:x}", -NanVal);
    EXPECT_EQUAL("NAN", "{:X}", -NanVal);

    EXPECT_EQUAL("1.000000", "{:f}",    1.0);
    EXPECT_EQUAL("1",        "{:.f}",   1.0);
    EXPECT_EQUAL("1",        "{:.0f}",  1.0);
    EXPECT_EQUAL("1.0",      "{:.1f}",  1.0);
    EXPECT_EQUAL("1.00",     "{:.2f}",  1.0);
    EXPECT_EQUAL("1.000000", "{:#f}",   1.0);
    EXPECT_EQUAL("1.",       "{:#.0f}", 1.0);
    EXPECT_EQUAL("1.0",      "{:#.1f}", 1.0);
    EXPECT_EQUAL("1.00",     "{:#.2f}", 1.0);

    EXPECT_EQUAL("1'234.000000", "{:'f}",    1234.0);
    EXPECT_EQUAL("1'234",        "{:'.f}",   1234.0);
    EXPECT_EQUAL("1'234",        "{:'.0f}",  1234.0);
    EXPECT_EQUAL("1'234.0",      "{:'.1f}",  1234.0);
    EXPECT_EQUAL("1'234.00",     "{:'.2f}",  1234.0);
    EXPECT_EQUAL("1'234.000000", "{:'#f}",   1234.0);
    EXPECT_EQUAL("1'234.",       "{:'#.0f}", 1234.0);
    EXPECT_EQUAL("1'234.0",      "{:'#.1f}", 1234.0);
    EXPECT_EQUAL("1'234.00",     "{:'#.2f}", 1234.0);

    EXPECT_EQUAL("1.000000e+00", "{:e}",    1.0);
    EXPECT_EQUAL("1e+00",        "{:.e}",   1.0);
    EXPECT_EQUAL("1e+00",        "{:.0e}",  1.0);
    EXPECT_EQUAL("1.0e+00",      "{:.1e}",  1.0);
    EXPECT_EQUAL("1.00e+00",     "{:.2e}",  1.0);
    EXPECT_EQUAL("1.000000e+00", "{:#e}",   1.0);
    EXPECT_EQUAL("1.e+00",       "{:#.0e}", 1.0);
    EXPECT_EQUAL("1.0e+00",      "{:#.1e}", 1.0);
    EXPECT_EQUAL("1.00e+00",     "{:#.2e}", 1.0);

    EXPECT_EQUAL("1",       "{:g}", 1.0);
    EXPECT_EQUAL("1",       "{:.g}", 1.0);
    EXPECT_EQUAL("1",       "{:.0g}", 1.0);
    EXPECT_EQUAL("1",       "{:.1g}", 1.0);
    EXPECT_EQUAL("1",       "{:.2g}", 1.0);
    EXPECT_EQUAL("1.00000", "{:#g}", 1.0);
    EXPECT_EQUAL("1.",      "{:#.0g}", 1.0);
    EXPECT_EQUAL("1.",      "{:#.1g}", 1.0);
    EXPECT_EQUAL("1.0",     "{:#.2g}", 1.0);

    EXPECT_EQUAL("1e+10",       "{:g}", 1.0e+10);
    EXPECT_EQUAL("1e+10",       "{:.g}", 1.0e+10);
    EXPECT_EQUAL("1e+10",       "{:.0g}", 1.0e+10);
    EXPECT_EQUAL("1e+10",       "{:.1g}", 1.0e+10);
    EXPECT_EQUAL("1e+10",       "{:.2g}", 1.0e+10);
    EXPECT_EQUAL("1.00000e+10", "{:#g}", 1.0e+10);
    EXPECT_EQUAL("1.e+10",      "{:#.0g}", 1.0e+10);
    EXPECT_EQUAL("1.e+10",      "{:#.1g}", 1.0e+10);
    EXPECT_EQUAL("1.0e+10",     "{:#.2g}", 1.0e+10);

    EXPECT_EQUAL("0x1.fcac083126e98p+0", "{:a}", 1.987);
    EXPECT_EQUAL("0x2p+0",               "{:.a}", 1.987);
    EXPECT_EQUAL("0x2p+0",               "{:.0a}", 1.987);
    EXPECT_EQUAL("0x2.0p+0",             "{:.1a}", 1.987);
    EXPECT_EQUAL("0x1.fdp+0",            "{:.2a}", 1.987);
    EXPECT_EQUAL("0x1.fcac083126e98p+0", "{:#a}", 1.987);
    EXPECT_EQUAL("0x2.p+0",              "{:#.a}", 1.987);
    EXPECT_EQUAL("0x2.p+0",              "{:#.0a}", 1.987);
    EXPECT_EQUAL("0x2.0p+0",             "{:#.1a}", 1.987);
    EXPECT_EQUAL("0x1.fdp+0",            "{:#.2a}", 1.987);
#endif
}

template <typename Formatter>
static void test_pointer()
{
#if 0
#if UINTPTR_MAX == UINT64_MAX
    EXPECT_EQUAL("0x0000000001020304", "{}", (void*)0x01020304)
#elif UINTPTR_MAX == UINT32_MAX
    EXPECT_EQUAL("0x01020304", "{}", (void*)0x01020304)
#endif
    EXPECT_EQUAL("-1", "{:d}", (void*)-1)
#else
    EXPECT_EQUAL("0x1020304", "{}", (void*)0x01020304);
#endif

    EXPECT_EQUAL("(nil)", "{}", (void*)0);
    EXPECT_EQUAL("(nil)", "{:3}", (void*)0);
    EXPECT_EQUAL("(nil)", "{:3.3}", (void*)0);
    EXPECT_EQUAL("   (nil)", "{:8}", (void*)0);
    EXPECT_EQUAL("   (nil)", "{:8.3}", (void*)0);

    EXPECT_EQUAL("(nil)", "{}", nullptr);
}

template <typename Formatter>
static void test_dynamic()
{
//    EXPECT_EQUAL("    x", "{1:0$}", 5, 'x');
//    EXPECT_EQUAL("    x", "{0:1$}", 'x', 5);
////  EXPECT_EQUAL("    x", "{0:1$}", 'x', char{5}); // error:
//    EXPECT_EQUAL("3.14 ", "{:<*.*f}", 5, 2, 3.1415);
//    EXPECT_EQUAL("3.14 ", "{:<5.*f}", 2, 3.1415);
//    EXPECT_EQUAL("3.14 ", "{:<*.2f}", 5, 3.1415);
//    EXPECT_EQUAL("3.14*", "{:*<*.2f}", 5, 3.1415);
////    EXPECT_EQUAL("3.14$", "{:$<$.2f}", 5, 3.1415);
//
//    EXPECT_EQUAL("000004", "{1:00$}", 6, 4);
//    EXPECT_EQUAL("000004", "{0:01$}", 4, 6);

    fmtxx::FormatSpec spec;

    spec.width  = 10;
    spec.prec   = -1;
    spec.fill   = '.';
    spec.align  = '>';
    spec.sign   = ' ';
    spec.zero   = false;
    spec.conv   = 'd';

    EXPECT_EQUAL(".......123", "{*}", spec, 123);
    EXPECT_EQUAL("......-123", "{*}", spec, -123);
    EXPECT_EQUAL(".......123", "{1*}", spec, 123);
    EXPECT_EQUAL("......-123", "{1*}", spec, -123);
    EXPECT_EQUAL(".......123", "{1*0}", spec, 123);
    EXPECT_EQUAL("......-123", "{1*0}", spec, -123);
    EXPECT_EQUAL(".......123", "{0*1}", 123, spec);
    EXPECT_EQUAL("......-123", "{0*1}", -123, spec);
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

template <typename Formatter>
static void test_custom()
{
    EXPECT_EQUAL("struct Foo '   123'", "struct Foo '{:6}'", Foo{123});
    EXPECT_EQUAL("struct Foo2 '   123'", "struct Foo2 '{:6}'", foo2_ns::Foo2{123});

    std::unordered_map<std::string_view, int> map = {{"eins", 1}, {"zwei", 2}};
    EXPECT_EQUAL("1, 2", "{0,eins}, {0,zwei}", map);
}

template <typename Formatter>
static void test_char()
{
    EXPECT_EQUAL("A", "{}", 'A');
    EXPECT_EQUAL("A", "{:s}", 'A');
    //EXPECT_EQUAL("65", "{:d}", 'A');
    //EXPECT_EQUAL("41", "{:x}", 'A');
}

template <typename Formatter>
static void test_wide_strings()
{
    #if 0
#if !FMTXX_NO_CODECVT
    EXPECT_EQUAL(u8"z\u00df\u6c34", "{}",  L"z\u00df\u6c34");
#ifndef _MSC_VER
    EXPECT_EQUAL(u8"z\u00df\u6c34\U0001f34c", "{}",  u"z\u00df\u6c34\U0001f34c");
    EXPECT_EQUAL(u8"z\u00df\u6c34\U0001f34c", "{}",  U"z\u00df\u6c34\U0001f34c");
#endif
#endif
    #endif
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

template <typename Formatter>
static void test_vector()
{
    std::vector<char> os;
    VectorBuffer buf { os };
    fmtxx::Format(buf, "{:6}", -1234);
    assert(os.size() == 6);
    assert(os[0] == ' '); // pad
    assert(os[1] == '-'); // put
    assert(os[2] == '1'); // write...
    assert(os[3] == '2');
    assert(os[4] == '3');
    assert(os[5] == '4');

    std::vector<char> str = { '1', '2', '3', '4' };
    EXPECT_EQUAL("1234", "{}", str);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename Formatter>
static void test_all()
{
    test_format_specs<Formatter>();
    test_0<Formatter>();
    test_strings<Formatter>();
    test_ints<Formatter>();
    test_floats<Formatter>();
    test_pointer<Formatter>();
    test_dynamic<Formatter>();
    test_custom<Formatter>();
    test_char<Formatter>();
    test_wide_strings<Formatter>();
    test_vector<Formatter>();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

int main()
{
    fprintf(stderr, "StringFormatter...\n");
    test_all<StringFormatter>();

    fprintf(stderr, "StreamFormatter...\n");
    test_all<StreamFormatter>();

#ifdef __linux__
    fprintf(stderr, "FILEFormatter...\n");
    test_all<FILEFormatter>();
#endif

    fprintf(stderr, "CharArrayFormatter...\n");
    test_all<CharArrayFormatter>();

#if 0
    setlocale(LC_NUMERIC, "");

    fmtxx::Format(stderr, "{:'f}\n", 12345.6789);
    fmtxx::Format(stderr, "{:'.20f}\n", 12345.6789);
    fmtxx::Format(stderr, "{:'050f}\n", 12345.6789);
    fmtxx::Format(stderr, "{:'050.20f}\n", 12345.6789);
    fmtxx::Format(stderr, "{:'050.0e}\n", 1e+20);
    fmtxx::Format(stderr, "{:'050.20e}\n", 1e+20);
    fmtxx::Format(stderr, "{:'050.20e}\n", 1.23456789);

    fprintf(stderr, "%'f\n", 12345.67890);
    fprintf(stderr, "%'.20f\n", 12345.67890);
    fprintf(stderr, "%'050f\n", 12345.67890);
    fprintf(stderr, "%'050.20f\n", 12345.67890);
    fprintf(stderr, "%'050.0e\n", 1e+20);
    fprintf(stderr, "%'050.20e\n", 1e+20);
    fprintf(stderr, "%'050.20e\n", 1.23456789);
#endif

    return n_errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
