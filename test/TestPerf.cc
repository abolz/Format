#include "Format.h"

#define HAVE_PRINTF 0
#define HAVE_FMTLIB 1
#define HAVE_TINYFORMAT 0

#if HAVE_FMTLIB
#define FMT_SHARED 1
#include "fmt/format.h"
#include "fmt/ostream.h"
#endif
#if HAVE_TINYFORMAT
#include "tinyformat/tinyformat.h"
#endif

#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <limits>
#include <random>
#include <typeinfo>
#include <vector>

#define NO_COMP             0 //1:profile
#define NO_SYNC_WITH_STDIO  0
#define NO_IOBUF            0

using Clock = std::chrono::high_resolution_clock;

struct Times {
#if HAVE_PRINTF
    double t_printf = 0.0;
#endif
#if HAVE_FMTLIB
    double t_fmt    = 0.0;
#endif
#if HAVE_TINYFORMAT
    double t_tiny   = 0.0;
#endif
    double t_fmtxx  = 0.0;
};

static std::vector<Times> timing_results;

static void PrintAvgTimes()
{
    Times avg;

    for (auto t : timing_results)
    {
#if HAVE_PRINTF
        avg.t_printf += t.t_printf;
#endif
#if HAVE_FMTLIB
        avg.t_fmt    += t.t_fmt;
#endif
#if HAVE_TINYFORMAT
        avg.t_tiny   += t.t_tiny;
#endif
        avg.t_fmtxx  += t.t_fmtxx;
    }

    // auto const ref = avg.t_printf;
    auto const ref = avg.t_fmt;

    fprintf(stderr, "--------------------------------------------------------------------------------\n");
#if HAVE_PRINTF
    fprintf(stderr, "printf:  x%.2f\n", ref / avg.t_printf);
#endif
#if HAVE_FMTLIB
    fprintf(stderr, "fmt:     x%.2f\n", ref / avg.t_fmt);
#endif
#if HAVE_TINYFORMAT
    fprintf(stderr, "tiny:    x%.2f\n", ref / avg.t_tiny);
#endif
    fprintf(stderr, "fmtxx:   x%.2f\n", ref / avg.t_fmtxx);
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename VSNPrintf>
inline int printf_buffered_impl(VSNPrintf snpf, char const* fmt, va_list args)
{
    static const size_t kStackBufSize = 500;

    char stack_buf[kStackBufSize];
    int n;

    va_list args_copy;
    va_copy(args_copy, args);
    n = snpf(stack_buf, kStackBufSize, fmt, args_copy);
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

        snpf(buf.ptr, count + 1, fmt, args);

        fwrite(buf.ptr, sizeof(char), count, stdout);
    }

    return n;
}

template <typename VSNPrintf>
inline int printf_buffered(VSNPrintf snpf, char const* fmt, ...)
{
    int n;

    va_list args;
    va_start(args, fmt);
    n = printf_buffered_impl(snpf, fmt, args);
    va_end(args);

    return n;
}

//#define PRINTF printf
#define PRINTF(...) \
    printf_buffered([](auto... args) { return vsnprintf(args...); }, __VA_ARGS__)

#define STB_PRINTF(...) \
    printf_buffered([](auto... args) { return stbsp_vsnprintf(args...); }, __VA_ARGS__)

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static std::mt19937 rng(123456);

// static void WarmUp()
// {
//     std::uniform_int_distribution<int> dist;
//     for (int i = 0; i < 5000000; ++i) {
//         auto x = dist(rng);
//         static_cast<void>(x);
//     }
// }

template <typename Distribution, typename F>
static double GenerateNumbers(int n, Distribution& dist, F func)
{
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

#if HAVE_PRINTF
    times.t_printf  = 1.0;
#endif
#if HAVE_FMTLIB
    times.t_fmt     = 1.0;
#endif
#if HAVE_TINYFORMAT
    times.t_tiny    = 1.0;
#endif

#else
#if HAVE_PRINTF
    //times.t_printf  = GenerateNumbers(n, dist, [=](auto i) { PRINTF(format_printf, i); });
	times.t_printf  = 1.0;
#endif

#if HAVE_FMTLIB
    //times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(format_fmt, i); });
    //times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(stdout, format_fmt, i); });
    //times.t_fmt     = GenerateNumbers(n, dist, [=](auto i) { fmt::print(std::cout, format_fmt, i); });

    // times.t_fmt = GenerateNumbers(n, dist, [&](auto i) {
    //     const auto str = fmt::format(format_fmt, i);
    //     std::fwrite(str.data(), 1, str.size(), stdout);
    // });

    //times.t_fmt = GenerateNumbers(n, dist, [&](auto i) {
    //    fmt::MemoryWriter w;
    //    w.write(format_fmt, i);
    //    std::fwrite(w.data(), 1, w.size(), stdout);
    //});

    times.t_fmt = GenerateNumbers(n, dist, [&](auto i) {
        fmt::MemoryWriter w;
        w << i;
        std::fwrite(w.data(), 1, w.size(), stdout);
    });
#endif

#if HAVE_TINYFORMAT
    times.t_tiny    = GenerateNumbers(n, dist, [=](auto i) { tinyformat::printf(format_printf, i); });
#endif

#endif // NO_COMP

#if 0
    times.t_fmtxx   = GenerateNumbers(n, dist, [=](auto i) { fmtxx::Format(stdout, format_fmtxx, i); });
#endif
#if 0
    times.t_fmtxx   = GenerateNumbers(n, dist, [=](auto i) { fmtxx::Format(std::cout, format_fmtxx, i); });
#endif
#if 0
    times.t_fmtxx   = GenerateNumbers(n, dist, [&](auto i) { std::cout << fmtxx::StringFormat(format_fmtxx, i); });
#endif
#if 0
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        const auto str = fmtxx::StringFormat(format_fmtxx, i);
        std::fwrite(str.data(), 1, str.size(), stdout);
    });
#endif
#if 1
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        fmtxx::FILEWriter{stdout} << i;
        //fmtxx::Format(stdout, "{}", i);
    });
#endif
#if 0
  #if 1
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        char buf[500];
        fmtxx::CharArray os{buf};
        fmtxx::Format(os, format_fmtxx, i);
        std::fwrite(buf, 1, static_cast<size_t>(os.next - buf), stdout);
    });
  #else
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        char buf[500];
        fmtxx::CharArray os{buf};
        fmtxx::Printf(os, format_printf, i);
        std::fwrite(buf, 1, static_cast<size_t>(os.next - buf), stdout);
    });
  #endif
#endif
#if 0
  #if 1
    std::string buf;
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        buf.clear();
        fmtxx::Format(buf, format_fmtxx, i);
        std::fwrite(buf.data(), 1, buf.size(), stdout);
    });
  #else
    std::string buf;
    times.t_fmtxx = GenerateNumbers(n, dist, [&](auto i) {
        buf.clear();
        fmtxx::Printf(buf, format_printf, i);
        std::fwrite(buf.data(), 1, buf.size(), stdout);
    });
  #endif
#endif

    // auto const ref = times.t_printf;
    auto const ref = times.t_fmt;

#if 1
    fprintf(stderr,
#if HAVE_PRINTF
        "   printf:  %.2f sec (x%.2f)\n"
#endif
#if HAVE_FMTLIB
        "   fmt:     %.2f sec (x%.2f)\n"
#endif
#if HAVE_TINYFORMAT
        "   tiny:    %.2f sec (x%.2f)\n"
#endif
        "   fmtxx:   %.2f sec (x%.2f)\n",
#if HAVE_PRINTF
        times.t_printf, ref / times.t_printf,
#endif
#if HAVE_FMTLIB
        times.t_fmt,   ref / times.t_fmt,
#endif
#if HAVE_TINYFORMAT
        times.t_tiny, ref / times.t_tiny,
#endif
        times.t_fmtxx, ref / times.t_fmtxx);
#endif

    timing_results.push_back(times);
}

template <typename T>
static void TestInts(char const* format_printf, char const* format_fmtxx, char const* format_fmt = nullptr)
{
   std::uniform_int_distribution<T> dist { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max() };

#ifndef NDEBUG
    RunTest(500000, dist, format_printf, format_fmtxx, format_fmt);
#else
    RunTest(5000000, dist, format_printf, format_fmtxx, format_fmt);
#endif
}

template <typename T>
static void TestFloats(T min, T max, char const* format_printf, char const* format_fmtxx, char const* format_fmt = nullptr)
{
    //fprintf(stderr, "TestFloats(%g, %g)\n", min, max);

    std::uniform_real_distribution<T> dist { min, max };

#ifndef NDEBUG
    RunTest(500000, dist, format_printf, format_fmtxx, format_fmt);
#else
    RunTest(1000000, dist, format_printf, format_fmtxx, format_fmt);
#endif
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

#if 0
//----------------------
// when using clang (on linux), the first run takes almost twice as long as the following...
// figure out whats going on...
//
// occurs only for ints...
// branch prediction? clang does not generate a jump table...
//----------------------
    TestInts<uint32_t>("%16u",    "{:16d}");
    // TestInts<uint32_t>("%16u",    "{:16d}");
    // TestInts<uint32_t>("%16u",    "{:16d}");
    // TestInts<uint32_t>("%16u",    "{:16d}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 1 // ints
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
#endif

#if 0
    TestInts<uint32_t>("%u",     "{}");
    TestInts<uint32_t>("%8u",    "{:8d}");
    TestInts<uint32_t>("%24u",   "{:24d}");
    TestInts<uint32_t>("%128u",  "{:128d}");
    TestInts<uint32_t>("%x",     "{:x}");
    TestInts<uint32_t>("%08x",   "{:08x}");
    TestInts<uint32_t>("%016x",  "{:016x}");
    TestInts<uint32_t>("%0128x", "{:0128x}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestInts<uint32_t>("%u",     "{:'}",      "{:n}");
    TestInts<uint32_t>("%8u",    "{:'8d}",    "{:8n}");
    TestInts<uint32_t>("%24u",   "{:'24d}",   "{:24n}");
    TestInts<uint32_t>("%128u",  "{:'128d}",  "{:128n}");
    TestInts<uint64_t>("%llu",     "{:'}",      "{:n}");
    TestInts<uint64_t>("%8llu",    "{:'8d}",    "{:8n}");
    TestInts<uint64_t>("%24llu",   "{:'24d}",   "{:24n}");
    TestInts<uint64_t>("%128llu",  "{:'128d}",  "{:128n}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 1
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
    TestInts<int64_t>("%'lld",     "{:'}");
    TestInts<int64_t>("%'8lld",    "{:'8d}");
    TestInts<int64_t>("%'24lld",   "{:'24d}");
    TestInts<int64_t>("%'128lld",  "{:'128d}");
    TestInts<int64_t>("%'llx",     "{:'x}");
    TestInts<int64_t>("%'08llx",   "{:'08x}");
    TestInts<int64_t>("%'016llx",  "{:'016x}");
    TestInts<int64_t>("%'0128llx", "{:'0128x}");

    PrintAvgTimes();
    timing_results.clear();
#endif
#endif // ints

#if !FMTXX_NO_FLOAT
#if 1 // floats
#if 0
    TestFloats(0.0,      1.0e-10,  "%.10f",  "{:.10f}");
    TestFloats(1.0e-10,  1.0e-20,  "%.20f",  "{:.20f}");
    TestFloats(1.0e-20,  1.0e-40,  "%.40f",  "{:.40f}");
    TestFloats(1.0e-40,  1.0e-80,  "%.80f",  "{:.80f}");
    TestFloats(1.0e-295, 1.0e-305, "%.305f", "{:.305f}");
    TestFloats(0.0,      1.0e-308, "%.308f", "{:.308f}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestFloats(0.0,      1.0,      "%.17e", "{:.17e}");
    TestFloats(1.0,      1.0e+20,  "%.17e", "{:.17e}");
    TestFloats(1.0e+20,  1.0e+40,  "%.17e", "{:.17e}");
    TestFloats(1.0e+40,  1.0e+60,  "%.17e", "{:.17e}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestFloats(0.0,      1.0,      "%.17g", "{:.17g}");
    TestFloats(1.0,      1.0e+20,  "%.17g", "{:.17g}");
    TestFloats(1.0e+20,  1.0e+40,  "%.17g", "{:.17g}");
    TestFloats(1.0e+40,  1.0e+60,  "%.17g", "{:.17g}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 1
    TestFloats(0.0,      1.0,      "%.17g", "{}", "{:.17g}");
    TestFloats(1.0,      1.0e+20,  "%.17g", "{}", "{:.17g}");
    TestFloats(1.0e+20,  1.0e+40,  "%.17g", "{}", "{:.17g}");
    TestFloats(1.0e+40,  1.0e+60,  "%.17g", "{}", "{:.17g}");

    PrintAvgTimes();
    timing_results.clear();
#endif

#if 0
    TestFloats(0.0,      1.0,      "%a", "{:a}");
    TestFloats(1.0,      1.0e+20,  "%a", "{:a}");
    TestFloats(1.0e+20,  1.0e+40,  "%a", "{:a}");
    TestFloats(1.0e+40,  1.0e+60,  "%a", "{:a}");

    PrintAvgTimes();
    timing_results.clear();
#endif
#endif // floats
#endif // !FMTXX_NO_FLOAT
}
