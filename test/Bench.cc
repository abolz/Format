#if 0
#include "Format.h"
//#include "Format_memory.h"
//#include "Format_ostream.h"
//#include "Format_pretty.h"
//#include "Format_string.h"
//#include <cstdio>
//#include <iosfwd>
//#include <memory>
//#include <string>
//#include <string_view>
//#include <type_traits>
int main() {}
#else

#include "Format.h"
//#include "Format_memory.h"
//#include "Format_ostream.h"
//#include "Format_pretty.h"
#include "Format_string.h"

#include "benchmark/benchmark.h"

#include <cstdint>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>

template <typename ...Args>
static void Format(char const* format, Args const&... args)
{
    char buf[8 * 1024];
    int const size = fmtxx::snformat(buf, format, args...);
    assert(size >= 0);
    std::fwrite(buf, 1, static_cast<int>(size), stderr);

    //fmtxx::MemoryWriter<> w;
    //fmtxx::format(w, format, args...);
    //std::fwrite(w.data(), 1, w.size(), stderr);

    //fmtxx::format(std::cerr, format, args...);

    //fmtxx::format(stderr, format, args...);

    //auto const str = fmtxx::sformat(format, args...);
    //std::fwrite(str.data(), 1, str.size(), stderr);
}

#if 1

static void warm_up(benchmark::State& state)
{
    while (state.KeepRunning())
        Format("{}", 123);
}
BENCHMARK(warm_up);
BENCHMARK(warm_up);
BENCHMARK(warm_up);

#endif

#if 0 // -------------------------------------------------------------- TEST_INT

#define TEST_INT(NAME, T, FORMAT) \
    static void test_##NAME(benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_int_distribution<T> dist; \
\
        while (state.KeepRunning()) \
            Format(FORMAT, dist(rng)); \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_INT(int32,             int32_t,    "{}");
TEST_INT(int32_pad08,       int32_t,    "{:08}");
TEST_INT(int32_pad32,       int32_t,    "{:32}");
TEST_INT(int32_pad64,       int32_t,    "{:64}");
TEST_INT(int32_pad32_l,     int32_t,    "{:<32}");
TEST_INT(int32_pad64_l,     int32_t,    "{:<64}");
TEST_INT(int32_x,           int32_t,    "{:x}");
TEST_INT(int32_x_pad08,     int32_t,    "{:08x}");
TEST_INT(int32_x_pad32,     int32_t,    "{:32x}");
TEST_INT(int32_x_pad64,     int32_t,    "{:64x}");

TEST_INT(int64,             int64_t,    "{}");
TEST_INT(int64_pad08,       int64_t,    "{:08}");
TEST_INT(int64_pad32,       int64_t,    "{:32}");
TEST_INT(int64_pad64,       int64_t,    "{:64}");
TEST_INT(int64_pad32_l,     int64_t,    "{:<32}");
TEST_INT(int64_pad64_l,     int64_t,    "{:<64}");
TEST_INT(int64_x,           int64_t,    "{:x}");
TEST_INT(int64_x_pad08,     int64_t,    "{:08x}");
TEST_INT(int64_x_pad32,     int64_t,    "{:32x}");
TEST_INT(int64_x_pad64,     int64_t,    "{:64x}");

TEST_INT(uint32,            uint32_t,   "{}");
TEST_INT(uint32_x,          uint32_t,   "{:x}");

TEST_INT(uint64,            uint64_t,   "{}");
TEST_INT(uint64_x,          uint64_t,   "{:x}");

#endif

#if 0 // -------------------------------------------------------- TEST_MULTI_INT

#define ARGS_1  "hello {}"
#define ARGS_2  ARGS_1 ARGS_1
#define ARGS_4  ARGS_2 ARGS_2
#define ARGS_8  ARGS_4 ARGS_4
#define ARGS_16 ARGS_8 ARGS_8

#if 0

#define TEST_MULTI_INT4(NAME, T, FORMAT) \
    static void test_##NAME (benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_int_distribution<T> dist; \
\
        while (state.KeepRunning()) \
            Format(FORMAT, dist(rng), dist(rng), dist(rng), dist(rng)); \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_MULTI_INT4(int32_multi4, int32_t, ARGS_4);
TEST_MULTI_INT4(int64_multi4, int64_t, ARGS_4);

#define TEST_MULTI_INT8(NAME, T, FORMAT) \
    static void test_##NAME (benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_int_distribution<T> dist; \
\
        while (state.KeepRunning()) \
            Format(FORMAT, dist(rng), dist(rng), dist(rng), dist(rng), \
                           dist(rng), dist(rng), dist(rng), dist(rng)); \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_MULTI_INT8(int32_multi8, int32_t, ARGS_8);
TEST_MULTI_INT8(int64_multi8, int64_t, ARGS_8);

#endif

#define TEST_MULTI_INT16(NAME, T, FORMAT) \
    static void test_##NAME (benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_int_distribution<T> dist; \
\
        while (state.KeepRunning()) \
            Format(FORMAT, dist(rng), dist(rng), dist(rng), dist(rng), \
                           dist(rng), dist(rng), dist(rng), dist(rng), \
                           dist(rng), dist(rng), dist(rng), dist(rng), \
                           dist(rng), dist(rng), dist(rng), dist(rng)); \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_MULTI_INT16(int32_multi16, int32_t, ARGS_16);
TEST_MULTI_INT16(int64_multi16, int64_t, ARGS_16);

#endif

#if 0 // ---------------------------------------------------- TEST_MULTI_INT_x16

#define TEST_MULTI_INT_x16(NAME, T, FORMAT) \
    static void test_##NAME (benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_int_distribution<T> dist; \
\
        while (state.KeepRunning()) \
        { \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
            Format(FORMAT, dist(rng)); \
        } \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_MULTI_INT_x16(int32_multi_x16, int32_t, "hello {}");
TEST_MULTI_INT_x16(int64_multi_x16, int64_t, "hello {}");

#endif

#if 0 // ------------------------------------------------------------ TEST_FLOAT

static void test_genfloats(benchmark::State& state)
{
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist(0, std::numeric_limits<double>::max());

    while (state.KeepRunning())
        benchmark::DoNotOptimize( dist(rng) );
}
BENCHMARK(test_genfloats);
BENCHMARK(test_genfloats);
BENCHMARK(test_genfloats);

#define TEST_FLOAT(NAME, T, FORMAT) \
    static void test_##NAME (benchmark::State& state) \
    { \
        std::mt19937 rng; \
        std::uniform_real_distribution<T> dist(0, std::numeric_limits<T>::max()); \
\
        while (state.KeepRunning()) \
            Format(FORMAT, dist(rng)); \
    } \
    BENCHMARK(test_##NAME); \
    /**/

TEST_FLOAT(double,      double,     "{}");
//TEST_FLOAT(double_x,    double,     "{:x}");
//TEST_FLOAT(double_f,    double,     "{:f}");
//TEST_FLOAT(double_e,    double,     "{:e}");
//TEST_FLOAT(double_g,    double,     "{:g}");
//TEST_FLOAT(double_a,    double,     "{:a}");
TEST_FLOAT(double_f_17, double,     "{:.17f}");
TEST_FLOAT(double_e_17, double,     "{:.17e}");
TEST_FLOAT(double_g_17, double,     "{:.17g}");
TEST_FLOAT(double_a_17, double,     "{:.17a}");

#endif

#define STRING_1 "Hello"
#define STRING_2 "I'm sorry Dave. I'm afraid I can't do that."
#define STRING_3 \
    "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod " \
    "tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At " \
    "vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, " \
    "no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit " \
    "amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut " \
    "labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam " \
    "et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata " \
    "sanctus est Lorem ipsum dolor sit amet."

#if 1 // ----------------------------------------------------------- TEST_STRING

#define TEST_STRING(NAME, FORMAT, ...) \
    static void test_##NAME (benchmark::State& state) \
    { \
        while (state.KeepRunning()) \
            Format(FORMAT, __VA_ARGS__); \
    } \
    BENCHMARK(test_##NAME);
    /**/

TEST_STRING(str_1,      "{}",       STRING_1);
TEST_STRING(str_2,      "{}",       STRING_2);
TEST_STRING(str_3,      "{}",       STRING_3);
TEST_STRING(str_1_2,    "{} {}",    STRING_1, STRING_1);
TEST_STRING(str_2_2,    "{} {}",    STRING_2, STRING_2);
TEST_STRING(str_3_2,    "{} {}",    STRING_3, STRING_3);
TEST_STRING(str_1_3,    "{0} {0}",  STRING_1);
TEST_STRING(str_2_3,    "{0} {0}",  STRING_2);
TEST_STRING(str_3_3,    "{0} {0}",  STRING_3);
TEST_STRING(stdstr_1,   "{}",       std::string{STRING_1});
TEST_STRING(stdstr_2,   "{}",       std::string{STRING_2});
TEST_STRING(stdstr_3,   "{}",       std::string{STRING_3});
TEST_STRING(stdstr_1_2, "{} {}",    std::string{STRING_1}, std::string{STRING_1});
TEST_STRING(stdstr_2_2, "{} {}",    std::string{STRING_2}, std::string{STRING_2});
TEST_STRING(stdstr_3_2, "{} {}",    std::string{STRING_3}, std::string{STRING_3});
TEST_STRING(stdstr_1_3, "{0} {0}",  std::string{STRING_1});
TEST_STRING(stdstr_2_3, "{0} {0}",  std::string{STRING_2});
TEST_STRING(stdstr_3_3, "{0} {0}",  std::string{STRING_3});

#endif

#if 1 // --------------------------------------------------------- TEST_CUSTOM_1

struct MyString {
    char const* str;
};

namespace fmtxx {
    template <>
    struct FormatValue<MyString> {
        errc operator()(Writer& w, FormatSpec const& spec, MyString const& s) const {
            return fmtxx::Util::format_string(w, spec, s.str);
        }
    };
}

#define TEST_CUSTOM_1(NAME, FORMAT, ...) \
    static void test_##NAME (benchmark::State& state) \
    { \
        while (state.KeepRunning()) \
            Format(FORMAT, __VA_ARGS__); \
    } \
    BENCHMARK(test_##NAME);
    /**/

TEST_CUSTOM_1(custom_str_1,      "{}",       MyString{STRING_1});
TEST_CUSTOM_1(custom_str_2,      "{}",       MyString{STRING_2});
TEST_CUSTOM_1(custom_str_3,      "{}",       MyString{STRING_3});
TEST_CUSTOM_1(custom_str_1_2,    "{} {}",    MyString{STRING_1}, MyString{STRING_1});
TEST_CUSTOM_1(custom_str_2_2,    "{} {}",    MyString{STRING_2}, MyString{STRING_2});
TEST_CUSTOM_1(custom_str_3_2,    "{} {}",    MyString{STRING_3}, MyString{STRING_3});
TEST_CUSTOM_1(custom_str_1_3,    "{0} {0}",  MyString{STRING_1});
TEST_CUSTOM_1(custom_str_2_3,    "{0} {0}",  MyString{STRING_2});
TEST_CUSTOM_1(custom_str_3_3,    "{0} {0}",  MyString{STRING_3});

#endif

int main(int argc, char* argv[])
{
    const size_t iobuf_size = 64 * 1024 * 1024;
    char* iobuf = static_cast<char*>(malloc(iobuf_size));

    std::ios_base::sync_with_stdio(false);
    setvbuf(stderr, iobuf, _IOFBF, iobuf_size);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;
    benchmark::RunSpecifiedBenchmarks();

    return 0;
}

#endif
