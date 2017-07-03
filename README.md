# Format

[![Build Status](https://travis-ci.org/effzeh/Format.svg?branch=master)](https://travis-ci.org/effzeh/Format)
[![Build status](https://ci.appveyor.com/api/projects/status/fcfasjqpo6s2n34n?svg=true)](https://ci.appveyor.com/project/effzeh/format)

String formatting library.

Uses a format-specification syntax similar to Python's
[str.format](https://docs.python.org/3/library/string.html#formatstrings) or
C's [printf](http://en.cppreference.com/w/cpp/io/c/fprintf).
By default, it can format strings, numbers (integral and floating point types)
and all types which have a `std::ostream` insertion operator. The
[double-conversion](https://github.com/google/double-conversion) library is
used for fast binary-to-decimal conversions.

## {fmt}

This library started in an attempt to reduce compile times of the excellent
[{fmt}](http://fmtlib.net/latest/index.html) library by
[Victor Zverovich](https://github.com/vitaut), which provides more
features than this library, is stable and more thoroughly tested and documented,
compiles with older compilers, provides a header-only mode, provides formatting
of wide-character strings and is used in a large number of real-world projects.

And most importantly, [here](http://fmtlib.net/Text%20Formatting.html) is a
proposal to include the fmt-library into the standard library:
[C++ formatting library proposal (discussion group)](https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/4wOU-1_3D0A)

**Before you continue reading, try [this](https://github.com/fmtlib/fmt).**

If, however, you need faster compile times, don't care about header-only,
don't care about wide-character strings, or need faster floating-point
formatting, this library might be an option.

*Note: Compile times using the fmt-library have decreased in the latest releases
and will continue to decrease in future releases.
[Faster floating-point formatting](https://github.com/fmtlib/fmt/issues/147) is
on the TODO-list.*

## API

### `Format.h`

```c++
namespace fmtxx {

enum struct errc {
    success = 0,
    conversion_error,
    index_out_of_range,
    invalid_argument,
    invalid_format_string,
    io_error,
    not_supported,
    value_out_of_range,
};

enum struct Align : unsigned char {
    Default,
    Left,
    Right,
    Center,
    PadAfterSign,
};

enum struct Sign : unsigned char {
    Default, // = Minus
    Minus,   // => '-' if negative, nothing otherwise
    Plus,    // => '-' if negative, '+' otherwise
    Space,   // => '-' if negative, fill-char otherwise
};

// Contains the fields of the format-specification (see below).
struct FormatSpec {
    std::string_view style;
    int   width = 0;
    int   prec  = -1;
    char  fill  = ' ';
    Align align = Align::Default;
    Sign  sign  = Sign::Default;
    bool  hash  = false;
    bool  zero  = false;
    char  tsep  = '\0';
    char  conv  = '\0';
};

// Base class for formatting buffers. Can be implemented to provide formatting
// into arbitrary buffers or streams.
class Writer {
public:
    virtual ~Writer();
    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
};

// Implements the Writer interface to format into std::FILE's.
class FILEWriter;

// Implements the Writer interface to format into user-provided arrays.
class ArrayWriter;

// Determine whether objects of type T should be handled as strings.
template <typename T>
struct TreatAsString : std::false_type {};

// Specialize this to enable formatting of user-defined types.
// Or include "Format_ostream.h" if objects of type T can be formatted using
// operator<<(std::ostream, T const&).
template <typename T>
struct FormatValue {
    errc operator()(Writer& w, FormatSpec const& spec, T const& value) const;
};

// The Format-methods use a python-style format string (see below).
// The Printf-methods use a printf-style format string.

template <typename ...Args>
errc format(Writer& w, std::string_view format, Args const&... args);

template <typename ...Args>
errc format(std::FILE* file, std::string_view format, Args const&... args);

template <typename ...Args>
int fformat(std::FILE* file, std::string_view format, Args const&... args);

template <typename ...Args>
int snformat(char* buf, size_t bufsize, std::string_view format, Args const&... args);

template <size_t N, typename ...Args>
int snformat(char (&buf)[N], std::string_view format, Args const&... args);

template <typename ...Args>
errc printf(Writer& w, std::string_view format, Args const&... args);

template <typename ...Args>
errc printf(std::FILE* file, std::string_view format, Args const&... args);

template <typename ...Args>
int fprintf(std::FILE* file, std::string_view format, Args const&... args);

template <typename ...Args>
int snprintf(char* buf, size_t bufsize, std::string_view format, Args const&... args);

template <size_t N, typename ...Args>
int snprintf(char (&buf)[N], std::string_view format, Args const&... args);

} // namespace fmtxx
```

**NOTE**: The formatting functions currently only accept at most 16 arguments.

### `Format_string.h`

```c++
namespace fmtxx {

class StringWriter;

class TreatAsString<std::string     > : std::true_type {};
class TreatAsString<std::string_view> : std::true_type {}; // if available

struct StringFormatResult {
    std::string str;
    errc ec = errc::success;
};

template <typename ...Args>
errc format(std::string& str, std::string_view format, Args const&... args);

template <typename ...Args>
errc printf(std::string& str, std::string_view format, Args const&... args);

template <typename ...Args>
StringFormatResult string_format(std::string_view format, Args const&... args);

template <typename ...Args>
StringFormatResult string_printf(std::string_view format, Args const&... args);

} // namespace fmtxx
```

### `Format_ostream.h`

Including this header provides support for formatting user-defined types for
which `operator<<(std::ostream&, T)` is defined.

```c++
namespace fmtxx {

class StreamWriter;

template <typename ...Args>
errc format(std::ostream& os, std::string_view format, Args const&... args);

template <typename ...Args>
errc printf(std::ostream& os, std::string_view format, Args const&... args);

} // namespace fmtxx
```

### `Format_pretty.h`

Provides support for pretty-printing arbitrary containers and tuples.
[(Example)](https://github.com/effzeh/Format/blob/master/test/Example4.cc)

```c++
namespace fmtxx {

template <typename T>
/*unspecified*/ pretty(T const& object);

} // namespace fmtxx
```

## Format String Syntax

### Format string syntax

    '{' [arg-index] ['*' [format-spec-index]] [':' [format-spec]] ['!' [style]] '}'

* `arg-index`

    Positional arguments. Like Rust's
    [std::fmt](https://doc.rust-lang.org/std/fmt/#positional-parameters):

    Each formatting argument is allowed to specify which value argument it's
    referencing, and if omitted it is assumed to be "the next argument". For
    example, the format string `"{} {} {}"` would take three parameters, and
    they would be formatted in the same order as they're given. The format
    string `"{2} {1} {0}"`, however, would format arguments in reverse order.

    Things can get a little tricky once you start intermingling the two types
    of positional specifiers. The "next argument" specifier can be thought of
    as an iterator over the argument. Each time a "next argument" specifier is
    seen, the iterator advances. This leads to behavior like this:

    ```c++
    format(stdout, "{1} {} {0} {}", 1, 2); // => "2 1 1 2"
    ```

    The internal iterator over the argument has not been advanced by the time
    the first `{}` is seen, so it prints the first argument. Then upon reaching
    the second `{}`, the iterator has advanced forward to the second argument.

    Essentially, parameters which explicitly name their argument do not affect
    parameters which do not name an argument in terms of positional specifiers.

* `format-spec-index`

    For dynamic formatting specifications. The formatting argument must be of
    type `FormatSpec` and will be used for formatting.

    If ommitted defaults to "next argument".

    Note: The `format-spec` field (below) overrides the values given in the
    formatting argument.

* `format-spec`

    Format specification. See below.

* `style`

    An arbitrary sequence of characters (except '`}`'). Can be used for
    formatting user-defined types.

### Format specification syntax

    [[fill] align] [flags] [width] ['.' [precision]] [conv]

* `[fill] align`

    If a valid `align` value is specified, it can be preceded by a `fill`
    character that can be any character and defaults to a space if omitted.

    The valid `align` values are:

    - `>`: Align right (default).
    - `<`: Align left.
    - `^`: Center.
    - `=`: For numbers, padding is placed after the sign (if any) but before
           the digits. Ignored otherwise.

    Note: Unless a minimum field width is specified (see below), the `fill`
    and `align` values are meaningless.

* `flags`

    Flags may appear in any order.

    - `-`: A '`-`' sign is displayed only if the value is negative (default).
    - `+`: A '`-`' sign is displayed for negative values, a '`+`' sign is
           displayed for positive values.
    - `space`: A '`-`' sign is displayed for negative values, a `fill`
               character is displayed for positive values.
    - `#`: Same meaning as in printf.
    - `0`: Pad with '`0`'s after the sign. Same as specifying '`0=`' as the
           `fill` and `align` parameters.
    - `'`: Display thousands-separators. For non-decimal presentation types
           separators will be inserted every 4 digits.
    - `_`: Same as `'`, but uses an underscore as the separator.
    - `,`: Same as `'`, but uses a comma as the separator.

    Note: Separators are inserted before padding is applied.

* `width`


    Minimum field width; an integer. Same meaning as in printf.
    The default is 0.

    **NOTE**: The maximum supported minimum field width currently is 4096.

* `precision`

    An integer.
    For string, integral and floating-point values has the same meaning as in
    printf.
    Ignored otherwise.

    Note: Precision is applied before padding.

    **NOTE**: The maximum supported precision is `INT_MAX`.

    **NOTE**: For integral types the maximum supported precision is 256.

    **NOTE**: For floating-point types the maximum supported precision is 1074.

* `conv`

    Any character except digits, '`!`' and '`}`' and any of the `flags`.

    Specifies how the argument should be presented. Defaults to `s` if omitted
    (or if the conversion does not apply to the type of the argument being
    formatted).

    Strings (`std::string[_view]`) and C-Strings (`char const*`):

    - `s`: Nothing special.
    - `q`: Quoted (like [std::quoted](http://en.cppreference.com/w/cpp/io/manip/quoted))

    Pointers (`void const*`):

    - `s`: Same as `p`.
    - `S`: Same as `P`.
    - `p`: Base 16, using lower-case digits, prefixed with '`0x`'. The default
           precision is `2*sizeof(void*)`.
    - `P`: Base 16, using upper-case digits, prefixed with '`0X`'. The default
           precision is `2*sizeof(void*)`.
    - `i`, `d`, `u`, `o`, `x`, `X`, `b`, `B`: As integer.

    `bool`:

    - `s`: Same as `t`.
    - `t`: As string: "`true`" or "`false`".
    - `y`: As string: "`yes`" or "`no`".
    - `o`: As string: "`on`" or "`off`".

    `char`:

    - `s`: Same as `c`.
    - `c`: Print the character.
    - `i`, `d`, `u`, `o`, `x`, `X`, `b`, `B`: As integer.

    Integers:

    - `s`: Same as `d`.
    - `i`: Same as `d`.
    - `d`: Display as a signed decimal integer.
    - `u`: Unsigned; base 10.
    - `o`: Unsigned; base 8, prefixed with '`0`' if `#` is specified (and
           value is non-zero).
    - `x`: Unsigned; base 16, using lower-case digits, prefixed with '`0x`'
           if `#` is specified.
    - `X`: Unsigned; base 16. using upper-case digits, prefixed with '`0X`'
           if `#` is specified.
    - `b`: Unsigned; base 2, prefixed with '`0b`' if `#` is specified.
    - `B`: Unsigned; base 2, prefixed with '`0B`' if `#` is specified.

    Floating-point types:

    - `s`: Short decimal representation (either fixed- or exponential-form)
           which is guaranteed to round-trip.
    - `S`: Same as `s`, but uses '`E`' instead of '`e`' to indicate the
           exponent (if any).
    - `f`: Same as printf `%f`.
    - `F`: Same as printf `%F`.
    - `e`: Same as printf `%e`.
    - `E`: Same as printf `%E`.
    - `g`: Same as printf `%g`.
    - `G`: Same as printf `%G`.
    - `a`: Same as printf `%a`.
    - `A`: Same as printf `%A`.
    - `x`: Same as `a`, except the result is normalized (i.e. the leading
           digit will be '`1`') and a prefix is only printed if `#` is
           specified.
    - `X`: Same as `A`, except the result is normalized (i.e. the leading
           digit will be '`1`') and a prefix is only printed if `#` is
           specified.
