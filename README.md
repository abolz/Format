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

## API

```c++
namespace fmtxx {

enum struct errc {
    success,
    io_error,
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
struct FormatSpec { {
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
class FormatBuffer {
    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
};

// Enables formatting of user-defined types. The default implementation uses
// std::ostringstream to convert the argument into a string, and then appends
// this string to the FormatBuffer.
template <typename T>
struct FormatValue {
    errc operator()(FormatBuffer& fb, FormatSpec const& spec, T const& value) const;
};

// Specialize this if you want your data-type to be treated as a string.
// T must have member functions data() and size() and their return values must
// be convertible to char const* and size_t resp.
template <typename T>
struct TreatAsString {
    static constexpr bool value = false;
};

// The Format-methods use a python-style format string (see below).
// The Printf-methods use a printf-style format string.

template <typename ...Args>
errc Format(FormatBuffer& fb, std::string_view format, Args const&... args);

template <typename ...Args>
errc Format(std::string& os, std::string_view format, Args const&... args);

template <typename ...Args>
errc Format(std::FILE* os, std::string_view format, Args const&... args);

template <typename ...Args>
errc Format(std::ostream& os, std::string_view format, Args const&... args);

template <typename ...Args>
std::string StringFormat(std::string_view format, Args const&... args);

template <typename ...Args>
errc Printf(FormatBuffer& fb, std::string_view format, Args const&... args);

template <typename ...Args>
errc Printf(std::string& os, std::string_view format, Args const&... args);

template <typename ...Args>
errc Printf(std::FILE* os, std::string_view format, Args const&... args);

template <typename ...Args>
errc Printf(std::ostream& os, std::string_view format, Args const&... args);

template <typename ...Args>
std::string StringPrintf(std::string_view format, Args const&... args);

}
```

**NOTE**: The formatting functions currently only accept at most 16 arguments.

## Format String Syntax

### Format string syntax

    '{' [arg-index] ['*' [format-spec-index]] [':' [format-spec]] [',' [style]] '}'

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
    Format("{1} {} {0} {}", 1, 2); // => "2 1 1 2"
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

    Note: Separators are inserted before padding is applied.

* `width`

    Minimum field width; a positive integer.
    The default is 0.

    **NOTE**: The maximum supported minimum field width is 4096.

* `precision`

    A non-negative integer.
    For string, integral and floating-point values has the same meaning as in
    printf.
    Ignored otherwise.

    **NOTE**: The maximum supported precision is `INT_MAX`.

    **NOTE**: For integral types the maximum supported precision is 256.

    **NOTE**: For floating-point types the maximum supported precision is 1074.

* `conv`

    Any character except digits, '`,`' and '`}`' and any of the `flags`.

    Specifies how the argument should be presented. Defaults to `s` if omitted
    (or if the conversion does not apply to the type of the argument being
    formatted).

    Strings (`std::string[_view]`) and C-Strings (`char const*`):

    - `s`: Nothing special.

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

### Examples

```c++
#include "Format.h"
#include <iostream>

int main()
{
    fmtxx::Format(std::cout, "{1} {} {0} {}\n", 1, 2);
        // "2 1 1 2"
    fmtxx::Format(std::cout, "{0:d} {0:x} {0:o} {0:b}\n", 42);
        // "42 2a 52 101010"
    fmtxx::Format(std::cout, "{:-<16}\n", "left");
        // "left------------"
    fmtxx::Format(std::cout, "{:.^16}\n", "center");
        // ".....center....."
    fmtxx::Format(std::cout, "{:~>16}\n", "right");
        // "~~~~~~~~~~~right"
    fmtxx::Format(std::cout, "{:s}\n", 3.1415927);
        // "3.1415927"
}
```

```c++
#include "Format.h"
#include <sstream> // This is required to use operator<< below!

struct Vector2D {
    float x;
    float y;
};

std::ostream& operator<<(std::ostream& os, Vector2D const& value) {
    os << "(" << value.x << ", " << value.y << ")";
    return os;
}

int main()
{
    Vector2D vec { 3.0, 4.0 };
    fmtxx::Format(stdout, "{}", vec);
        // "(3, 4)"
}
```

```c++
#include "Format.h"
#include <cmath>

struct Vector2D {
    float x;
    float y;
};

template <>
struct fmtxx::FormatValue<Vector2D>
{
    auto operator()(FormatBuffer& os, FormatSpec const& spec, Vector2D const& value) const
    {
        if (spec.conv == 'p' || spec.conv == 'P') // polar coordinates
        {
            auto r   = std::hypot(value.x, value.y);
            auto phi = std::atan2(value.y, value.x);
            return Format(os, "(r={:.3g}, phi={:.3g})", r, phi);
        }

        return Format(os, "({}, {})", value.x, value.y);
    }
};

int main()
{
    Vector2D vec { 3.0, 4.0 };
    fmtxx::Format(stdout, "{}\n", vec);
        // "(3, 4)"
    fmtxx::Format(stdout, "{:p}\n", vec);
        // "(r=5, phi=0.927)"
}
```

```c++
#include "Format.h"
#include <vector>

struct VectorBuffer : public fmtxx::FormatBuffer
{
    std::vector<char> vec;

    bool Put(char c) override {
        vec.push_back(c);
        return true;
    }
    bool Write(char const* str, size_t len) override {
        vec.insert(vec.end(), str, str + len);
        return true;
    }
    bool Pad(char c, size_t count) override {
        vec.resize(vec.size() + count, c);
        return true;
    }
};

// Tell the Format library that vector<char> should be handled as a string.
// Possible because vector<char> has compatible data() and size() members.
template <>
struct fmtxx::TreatAsString<std::vector<char>> : std::true_type {};

int main()
{
    VectorBuffer buf;
    fmtxx::Format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}
    fmtxx::Format(stdout, "{}\n", buf.vec);
        // " -123"
}
```

# Limitations

- Only up to 16 arguments may be provided.
- No wide string support
- ...

# License

MIT
