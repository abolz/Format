#define FMTXX_SHARED 1
#include "Format.h"

#define FMT_SHARED 1
#include "fmt/format.h"
#include "fmt/ostream.h"

#include <cstdarg>
#include <cstdio>
#include <climits>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#define NO_COMP             0 //1:profile
#define NO_SYNC_WITH_STDIO  0
#define NO_IOBUF            0

using Clock = std::chrono::steady_clock;

struct Times {
    double t_printf = 0.0;
    //double t_tiny   = 0.0;
    double t_fmt    = 0.0;
    double t_fmtxx  = 0.0;
};

static std::vector<Times> timing_results;

static void PrintAvgTimes()
{
    Times avg;

    for (auto t : timing_results)
    {
        avg.t_printf += t.t_printf;
        //avg.t_tiny   += t.t_tiny;
        avg.t_fmt    += t.t_fmt;
        avg.t_fmtxx  += t.t_fmtxx;
    }

    fprintf(stderr, "--------------------------------------------------------------------------------\n");
    //fprintf(stderr, "tiny:    x%.2f\n", avg.t_printf / avg.t_tiny);
    fprintf(stderr, "fmt:     x%.2f\n", avg.t_printf / avg.t_fmt);
    fprintf(stderr, "fmtxx:   x%.2f\n", avg.t_printf / avg.t_fmtxx);
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static int printf_buffered_impl(char const* fmt, va_list args)
{
    static const size_t kStackBufSize = 500;

    char stack_buf[kStackBufSize];
    int n;

    va_list args_copy;
    va_copy(args_copy, args);
    n = vsnprintf(stack_buf, kStackBufSize, fmt, args_copy);
    va_end(args_copy);

    if (n < 0) // io error
        return n;

    const size_t count = static_cast<size_t>(n);
    if (count < kStackBufSize)
    {
        fwrite(stack_buf, sizeof(char), count, stdout);
    }
    else
    {
        struct Malloced {
            char* ptr;
            Malloced(size_t n) : ptr((char*)::operator new(n)) {}
           ~Malloced() { ::operator delete(ptr); }
        };

        Malloced buf(count + 1);

        vsnprintf(buf.ptr, count + 1, fmt, args);

        fwrite(buf.ptr, sizeof(char), count, stdout);
    }

    return n;
}

static int printf_buffered(char const* fmt, ...)
{
    int n;

    va_list args;
    va_start(args, fmt);
    n = printf_buffered_impl(fmt, args);
    va_end(args);

    return n;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename Distribution, typename F>
static double GenerateNumbers(int n, Distribution& dist, F func)
{
    std::mt19937 rng;

    const auto start = Clock::now();
    while (n-- > 0)
    {
        func(dist(rng));
    }
    const auto stop = Clock::now();
    const auto sec = std::chrono::duration<double>(stop - start).count();

    return sec;
}

template <typename Distribution>
static void RunTest(int n, Distribution& dist, char const* format_printf, char const* format_fmtxx, char const* format_fmt = nullptr)
{
    if (format_fmt == nullptr)
        format_fmt = format_fmtxx;

    fprintf(stderr, "Test <%s> %s\n", typeid(typename Distribution::result_type).name(), format_fmtxx);

    Times times;

#if NO_COMP
    times.t_printf  = 1.0;
    //times.t_tiny    = 1.0;
    times.t_fmt     = 1.0;
#else
  times.t_printf  = GenerateNumbers(n, dist, [=](auto i) { printf(format_printf, i); });
    //times.t_printf  = GenerateNumbers(n, dist, [=](auto i) { fprintf(stdout, format_printf, i); });
//  times.t_printf  = GenerateNumbers(n, dist, [=](auto i) { printf_buffered(format_printf, i); });
	//times.t_printf = 1.0;

	//times.t_tiny    = GenerateNumbers(n, dist, [=](auto i) { tinyformat::printf(format_printf, i); });
//  times.t_tiny    = 1.0;

    times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(format_fmt, i); });
//  times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(stdout, format_fmt, i); });
//  times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(std::cout, format_fmt, i); });
#endif
#if 0
    times.t_fmtxx   = GenerateNumbers(n, dist, [=](auto i) { fmtxx::Format(stdout, format_fmtxx, i); });
#endif
#if 0
    times.t_fmtxx   = GenerateNumbers(n, dist, [=](auto i) { fmtxx::Format(std::cout, format_fmtxx, i); });
#endif
#if 1
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        char buf[500];
        const auto res = fmtxx::Format(buf, buf + 500, format_fmtxx, i);
        std::fwrite(buf, 1, static_cast<size_t>(res.next - buf), stdout);
    });
#endif
#if 0
    std::string buf;
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        buf.clear();
        fmtxx::Format(buf, format_fmtxx, i);
        std::fwrite(buf.data(), 1, buf.size(), stdout);
    });
#endif

#if 1
    fprintf(stderr,
        "   printf:  %.2f sec\n"
        //"   tiny:    %.2f sec (x%.2f)\n"
        "   fmt:     %.2f sec (x%.2f)\n"
        "   fmtxx:   %.2f sec (x%.2f)\n",
        times.t_printf,
        //times.t_tiny,  times.t_printf / times.t_tiny,
        times.t_fmt,   times.t_printf / times.t_fmt,
        times.t_fmtxx, times.t_printf / times.t_fmtxx);
#endif

    timing_results.push_back(times);
}

template <typename T>
static void TestInts(char const* format_printf, char const* format_fmtxx, char const* format_fmt = nullptr)
{
    std::uniform_int_distribution<T> dist {
        std::numeric_limits<T>::lowest(),
        std::numeric_limits<T>::max()
    };
    RunTest(5000000, dist, format_printf, format_fmtxx, format_fmt);
}

template <typename T>
static void TestFloats(char const* format_printf, char const* format_fmtxx, char const* format_fmt = nullptr)
{
    std::uniform_real_distribution<T> dist {
        static_cast<T>(0.0),
        static_cast<T>(1.0)
    };
    RunTest(2500000, dist, format_printf, format_fmtxx, format_fmt);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static constexpr size_t kIOBufSize = 4 * 1024 * 1024;
static char kIOBuf[kIOBufSize];

int main()
{
#if !NO_SYNC_WITH_STDIO
    std::ios_base::sync_with_stdio(false);
#endif
#if !NO_IOBUF
    setvbuf(stdout, kIOBuf, _IOFBF, kIOBufSize);
#endif

#if 1
    TestInts<int32_t>("%d",     "{}");
    TestInts<int32_t>("%8d",    "{:8d}");
    TestInts<int32_t>("%24d",   "{:24d}");
    TestInts<int32_t>("%128d",  "{:128d}");
    TestInts<int32_t>("%x",     "{:x}");
    TestInts<int32_t>("%08x",   "{:08x}");
    TestInts<int32_t>("%016x",  "{:016x}");
    TestInts<int32_t>("%0128x", "{:0128x}");

    PrintAvgTimes();
    timing_results.clear();

    //TestInts<uint32_t>("%u",     "{}");
    //TestInts<uint32_t>("%8u",    "{:8d}");
    //TestInts<uint32_t>("%24u",   "{:24d}");
    //TestInts<uint32_t>("%128u",  "{:128d}");
    //TestInts<uint32_t>("%x",     "{:x}");
    //TestInts<uint32_t>("%08x",   "{:08x}");
    //TestInts<uint32_t>("%016x",  "{:016x}");
    //TestInts<uint32_t>("%0128x", "{:0128x}");
    //
    //PrintAvgTimes();
    //timing_results.clear();

    TestInts<int64_t>("%lld",     "{}");
    TestInts<int64_t>("%8lld",    "{:8d}");
    TestInts<int64_t>("%24lld",   "{:24d}");
    TestInts<int64_t>("%128lld",  "{:128d}");
    TestInts<int64_t>("%llx",     "{:x}");
    TestInts<int64_t>("%08llx",   "{:08x}");
    TestInts<int64_t>("%016llx",  "{:016x}");
    TestInts<int64_t>("%0128llx", "{:0128x}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestFloats<float>("%f",     "{:f}");
    TestFloats<float>("%e",     "{:e}");
    TestFloats<float>("%g",     "{:g}");
    TestFloats<float>("%a",     "{:a}");
    TestFloats<float>("%.17f",  "{:.17f}");
    TestFloats<float>("%.17e",  "{:.17e}");
    TestFloats<float>("%.17g",  "{:.17g}");
    TestFloats<float>("%.17a",  "{:.17a}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestFloats<float>("%.12a", "{:x}", "{:.12a}");
    TestFloats<float>("%.17g", "{:s}", "{:.17g}");

    PrintAvgTimes();
    timing_results.clear();
#endif
}