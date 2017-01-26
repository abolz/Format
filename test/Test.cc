#if 0
cl /EHsc Test.cc Format.cc /std:c++latest /Fe: Test.exe
g++ Test.cc Format.cc
#endif

#include "Format.h"
#include "Format_std.h"

#include <limits>
#include <sstream>
#include <vector>
#include <cmath>
#include <cfloat>
#include <iostream>

#define NO_FLOATS 0

static int n_errors = 0;

//namespace fmtxx {
//    template <>
//    struct IsString<std::string> { enum { value = true }; };
//}

template <typename ...Args>
static bool expect_equal(char const* expected, std::string_view format, Args const&... args)
{
    //std::ostringstream buf;
    //auto err = fmtxx::Format(buf, format, args...);
    //std::string str = buf.str();
    std::string str = fmtxx::Format(format, args...);
    const int err = 0;

    if (err != 0)
    {
        fprintf(stderr, "FAIL: invalid format string\n");
        return false;
    }

    if (expected != str)
    {
        ++n_errors;
        fprintf(stderr, "FAIL: '%s' != '%s'", expected, str.c_str());
        return false;
    }

    return true;
}

#define EXPECT_EQUAL(EXPECTED, FORMAT, ...)             \
    if (!expect_equal(EXPECTED, FORMAT, __VA_ARGS__))   \
    {                                                   \
        fprintf(stderr, "    line %d\n", __LINE__);              \
    }                                                   \
    /**/

#define EXPECT_EQUAL_0(EXPECTED, FORMAT)                \
    if (!expect_equal(EXPECTED, FORMAT))                \
    {                                                   \
        fprintf(stderr, "    line %d\n", __LINE__);              \
    }                                                   \
    /**/

template <typename ...Args>
static bool expect_error(int expected_err, std::string_view format, Args const&... args)
{
    std::ostringstream buf;
    auto err = fmtxx::Format(buf, format, args...);

    if (err != expected_err)
    {
        ++n_errors;
        fprintf(stderr, "FAIL: expected error code %d, got %d", (int)expected_err, (int)err);
        return false;
    }

    return true;
}

#define EXPECT_ERROR(EXPECTED_ERR, FORMAT, ...)             \
    if (!expect_error(EXPECTED_ERR, FORMAT, __VA_ARGS__)) { \
        fprintf(stderr, "    line %d\n", __LINE__);                  \
    }                                                       \
    /**/

static void test_format_specs()
{
    EXPECT_ERROR(-1/*invalid*/, "{", 0);
    EXPECT_ERROR(-1/*invalid*/, std::string_view("{*}", 1), 0, 0);
    EXPECT_ERROR(-1/*invalid*/, std::string_view("{*}", 2), 0, 0);
    EXPECT_ERROR(0, "{*}", fmtxx::FormatSpec{}, 0);
    EXPECT_EQUAL("0", "{*}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERROR(-1/*invalid*/, "{*", fmtxx::FormatSpec{}, 0);
    EXPECT_ERROR(-1/*invalid*/, "{*}}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERROR(0, "{*}}}", fmtxx::FormatSpec{}, 0);
    EXPECT_ERROR(-1/*invalid*/, std::string_view("{}", 1), 0);
    EXPECT_ERROR(-1/*invalid*/, std::string_view("{1}", 2), 0);
    EXPECT_ERROR(-1/*invalid*/, "{1", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1.", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1.1", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1.1f", 0);
    EXPECT_ERROR(-1/*invalid*/, "{ ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1 ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1: ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1 ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1. ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1.1 ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{1:1.1f ", 0);
    EXPECT_ERROR(-1/*invalid*/, "{-1: >10.2f}", 0);
    EXPECT_ERROR(-1/*invalid*/, "{:*10}", 0);
    EXPECT_ERROR(-1/*invalid*/, "{-10}", 0);
    EXPECT_ERROR(0, "{:-10}", 0); // sign + width!
    EXPECT_ERROR(-1/*invalid*/, "{{}", 1); // stray '}'
    EXPECT_ERROR(-1/*invalid*/, "{}}", 1); // stray '}'
    EXPECT_ERROR(-1/*invalid*/, "}", 1, 1, 1, 1, 1);
//  EXPECT_ERROR(fmtxx::errc::switching_indexing, "{0} {}", 1, 1);
//  EXPECT_ERROR(fmtxx::errc::switching_indexing, "{} {0}", 1, 1);
//  EXPECT_ERROR(fmtxx::errc::switching_indexing, "{0} {0} {}", 1, 1, 1);
//  EXPECT_ERROR(fmtxx::errc::switching_indexing, "{} {} {0}", 1, 1, 1);
    EXPECT_ERROR(-1/*invalid*/, "{1}", 1);
    EXPECT_ERROR(-1/*invalid*/, "{1}{2}", 1, 2);
    EXPECT_ERROR(-1/*invalid*/, "{0}{2}", 1, 2);
    EXPECT_ERROR(-1/*invalid*/, "{10}", 1);
    EXPECT_ERROR(-1/*invalid*/, "{2147483647}", 1);
    EXPECT_ERROR(-1/*overflow*/, "{2147483648}", 0);
    EXPECT_ERROR(-1/*overflow*/, "{99999999999}", 1);
    EXPECT_ERROR(-1/*overflow*/, "{:99999999999.0}", 1);
    EXPECT_ERROR(-1/*invalid*/, "{:.", 0);

    EXPECT_ERROR(-1/*invalid*/, "{:$.$f}", 5, 2, 3.1415);
    EXPECT_ERROR(-1/*invalid*/, "{:5.$f}", 2, 3.1415);
    EXPECT_ERROR(-1/*invalid*/, "{:$.2f}", 5, 3.1415);

//// $ parsed as the type...
////    EXPECT_ERROR(-1/*invalid*/, "{:$}", 5, 'x');
}

template <typename T> struct incomplete;

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

static void test_strings()
{
    EXPECT_EQUAL_0("", "");
    EXPECT_EQUAL_0("x", "x");

    EXPECT_EQUAL("x", "{}", 'x');

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
}

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

#if 0
    EXPECT_EQUAL("1_234_567_890", "{:13_}", 1234567890);
    EXPECT_EQUAL("  123_456_789", "{:13_}", 123456789);
    EXPECT_EQUAL("   12_345_678", "{:13_}", 12345678);
    EXPECT_EQUAL("    1_234_567", "{:13_}", 1234567);
    EXPECT_EQUAL("      123_456", "{:13_}", 123456);
    EXPECT_EQUAL("       12_345", "{:13_}", 12345);
    EXPECT_EQUAL("        1_234", "{:13_}", 1234);
    EXPECT_EQUAL("          123", "{:13_}", 123);
    EXPECT_EQUAL("           12", "{:13_}", 12);
    EXPECT_EQUAL("            1", "{:13_}", 1);

    EXPECT_EQUAL("1234_5678", "{:9_x}", 0x12345678);
    EXPECT_EQUAL(" 123_4567", "{:9_x}", 0x1234567);
    EXPECT_EQUAL("  12_3456", "{:9_x}", 0x123456);
    EXPECT_EQUAL("   1_2345", "{:9_x}", 0x12345);
    EXPECT_EQUAL("     1234", "{:9_x}", 0x1234);
    EXPECT_EQUAL("      123", "{:9_x}", 0x123);
    EXPECT_EQUAL("       12", "{:9_x}", 0x12);
    EXPECT_EQUAL("        1", "{:9_x}", 0x1);
#endif

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

static void test_floats()
{
#if !NO_FLOATS
    static const double PI  = 3.1415926535897932384626433832795;

    EXPECT_EQUAL("0.000000",  "{:f}",   0.0);
    EXPECT_EQUAL("-0.000000", "{:f}",  -0.0);
    EXPECT_EQUAL(" 0.000000", "{: f}",  0.0);
    EXPECT_EQUAL("-0.000000", "{: f}", -0.0);
    EXPECT_EQUAL("+0.000000", "{:+f}",  0.0);
    EXPECT_EQUAL("-0.000000", "{:+f}", -0.0);

    EXPECT_EQUAL("3.141593", "{:f}", PI);
    EXPECT_EQUAL("-3.141593", "{:f}", -PI);

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

    EXPECT_EQUAL("1.000000",     "{:f}",  1.0);
    EXPECT_EQUAL("1.000000e+00", "{:e}",  1.0);
    EXPECT_EQUAL("1.000000E+00", "{:E}",  1.0);
    EXPECT_EQUAL("1",            "{:g}",  1.0);

    EXPECT_EQUAL("1.200000",     "{:f}",  1.2);
    EXPECT_EQUAL("1.200000e+00", "{:e}",  1.2);
    EXPECT_EQUAL("1.200000E+00", "{:E}",  1.2);
    EXPECT_EQUAL("1.2",          "{:g}",  1.2);

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
#endif

    EXPECT_EQUAL("0",                 "{:s}", 0.0);
    EXPECT_EQUAL("10",              "{:s}", 10.0);
    EXPECT_EQUAL("10",              "{:S}", 10.0);
    EXPECT_EQUAL("-0",                "{:s}", -0.0);
    EXPECT_EQUAL("0p+0",                "{:x}", 0.0);
    EXPECT_EQUAL("0p+0",                "{:X}", 0.0);
    EXPECT_EQUAL("-0p+0",               "{:x}", -0.0);
    EXPECT_EQUAL("1.8p+0",              "{:x}", 1.5);
//    EXPECT_EQUAL("1.8000p+0",           "{:.4x}", 1.5);
//    EXPECT_EQUAL("1p+1",                "{:.0x}", 1.5);
    EXPECT_EQUAL("1.921FB5A7ED197p+1",  "{:x}", 3.1415927);
//    EXPECT_EQUAL("1.922p+1",            "{:.3x}", 3.1415927);
//    EXPECT_EQUAL("1.9220p+1",           "{:.4x}", 3.1415927);
//    EXPECT_EQUAL("1.921fbp+1",          "{:.5x}", 3.1415927);

    EXPECT_EQUAL("1p-1022",               "{:x}", std::numeric_limits<double>::min());
    EXPECT_EQUAL("1p-1074",               "{:x}", std::numeric_limits<double>::denorm_min());
    EXPECT_EQUAL("1p-1074",               "{:X}", std::numeric_limits<double>::denorm_min());
    //EXPECT_EQUAL("1.0p-1022",               "{:#x}", std::numeric_limits<double>::min());
    //EXPECT_EQUAL("1.0p-1074",               "{:#x}", std::numeric_limits<double>::denorm_min());
    //EXPECT_EQUAL("1.0p-1074",               "{:#X}", std::numeric_limits<double>::denorm_min());

    EXPECT_EQUAL("1.7976931348623157e+308",     "{:s}",  std::numeric_limits<double>::max());
    EXPECT_EQUAL("1.7976931348623157e+308",     "{:S}",  std::numeric_limits<double>::max());
    EXPECT_EQUAL("-1.7976931348623157e+308",    "{:s}", -std::numeric_limits<double>::max());
    EXPECT_EQUAL("2.2250738585072014e-308",     "{:s}",  std::numeric_limits<double>::min());
    EXPECT_EQUAL("-2.2250738585072014e-308",    "{:s}", -std::numeric_limits<double>::min());
    EXPECT_EQUAL("-2.2250738585072014e-308",    "{:S}", -std::numeric_limits<double>::min());
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

    EXPECT_EQUAL("1p+0",    "{:x}",     1.0);
//    EXPECT_EQUAL("1.0p+0",    "{:.0x}",   1.0);
//    EXPECT_EQUAL("1.0p+0",  "{:.1x}",   1.0);
//    EXPECT_EQUAL("1.00p+0", "{:.2x}",   1.0);

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

    double NanVal = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQUAL("nan", "{:s}", NanVal);
    EXPECT_EQUAL("NAN", "{:S}", NanVal);
    EXPECT_EQUAL("nan", "{:x}", NanVal);
    EXPECT_EQUAL("NAN", "{:X}", NanVal);
    EXPECT_EQUAL("nan", "{:s}", -NanVal);
    EXPECT_EQUAL("NAN", "{:S}", -NanVal);
    EXPECT_EQUAL("nan", "{:x}", -NanVal);
    EXPECT_EQUAL("NAN", "{:X}", -NanVal);
}

static void test_pointer()
{
    EXPECT_EQUAL("0x1020304", "{}", (void*)0x01020304)

    EXPECT_EQUAL("(nil)", "{}", (void*)0);
    EXPECT_EQUAL("(nil)", "{:3}", (void*)0);
    EXPECT_EQUAL("(nil)", "{:3.3}", (void*)0);
    EXPECT_EQUAL("   (nil)", "{:8}", (void*)0);
    EXPECT_EQUAL("   (nil)", "{:8.3}", (void*)0);

    EXPECT_EQUAL("(nil)", "{}", nullptr);
}

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
    spec.zero   = '\0';
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

inline int fmtxx__FormatValue(std::ostream& out, fmtxx::FormatSpec const& spec, Foo const& value) {
    return fmtxx::Format(out, "{*}", spec, value.value);
}

static void test_custom()
{
    EXPECT_EQUAL("struct Foo '   123'", "struct Foo '{:6}'", Foo{123});
}

static void test_char()
{
    EXPECT_EQUAL("A", "{}", 'A');
    EXPECT_EQUAL("A", "{:s}", 'A');
    //EXPECT_EQUAL("65", "{:d}", 'A');
    //EXPECT_EQUAL("41", "{:x}", 'A');
}

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

static std::string say_hello() {
    return "hello hello hello hello hello hello hello hello hello hello";
}

static void test_tuple()
{
    const auto formatted = fmtxx::Formatted("{} world {} {}", say_hello(), 1, 2.345);

    std::ostringstream stream;
    // Should probably be more like "stream << to_string(Format(...))",
    // but it isn't
    stream << formatted;

    //std::cout << stream.str() << "\n";

    assert(stream.str() == "hello hello hello hello hello hello hello hello hello hello world 1 2.345");
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

int main()
{
    test_format_specs();
    test_0();
    test_strings();
    test_ints();
    test_floats();
    test_pointer();
    test_dynamic();
    test_custom();
    test_char();
    test_wide_strings();
    test_tuple();

    return n_errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
