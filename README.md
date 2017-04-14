# Format

[![Build Status](https://travis-ci.org/effzeh/Format.svg?branch=master)](https://travis-ci.org/effzeh/Format)
[![Build status](https://ci.appveyor.com/api/projects/status/fcfasjqpo6s2n34n?svg=true)](https://ci.appveyor.com/project/effzeh/format)

C++ string formatting library

Uses a format-specification syntax similar to Python's
[str.format](https://docs.python.org/3/library/string.html#formatstrings).
By default, it can format strings, numbers (integral and floating point types)
and all types which have a `std::ostream` insertion operator. The
[double-conversion](https://github.com/google/double-conversion) library is
used for fast binary-to-decimal conversions.

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

By default, user-defined types can be handled if they have a `std::ostream`
insertion operator:

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

It is possible to customize formatting by providing a specialization of
`FormatValue`:

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
    errc operator()(FormatBuffer& os, FormatSpec const& spec, Vector2D const& value) const
    {
        if (spec.conv == 'p' || spec.conv == 'P')
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

Formatting to other buffers or streams is supported by implementing the
`FormatBuffer` interface:

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
struct fmtxx::IsString<std::vector<char>> : std::true_type {};

int main()
{
    VectorBuffer buf;

    fmtxx::Format(buf, "{:5}", -123);
        // buf.vec = {' ', '-', '1', '2', '3'}

    fmtxx::Format(stdout, "{}\n", buf.vec);
        // " -123"
}
```

## API

```c++
namespace fmtxx {

enum class errc {
    success                 =  0,
    invalid_format_string   = -1,
    invalid_argument        = -2,
    io_error                = -3,
    index_out_of_range      = -4,
};

// Contains the fields of the format-specification (see below).
struct FormatSpec {
    std::string_view style;
    int  width = 0;
    int  prec  = -1;
    char fill  = ' ';
    char align = '>';
    char sign  = '-';
    bool hash  = false;
    bool zero  = false;
    char tsep  = '\0';
    char conv  = 's';
};

// Base class for formatting buffers. Can be implemented to provide formatting
// arbitrary buffers or streams.
class FormatBuffer {
    virtual bool Put(char c) = 0;
    virtual bool Write(char const* str, size_t len) = 0;
    virtual bool Pad(char c, size_t count) = 0;
};

struct StringBuffer : public FormatBuffer {
    std::string& os;
    explicit StringBuffer(std::string& v);
};

struct FILEBuffer : public FormatBuffer {
    std::FILE* os;
    explicit FILEBuffer(std::FILE* v);
};

struct StreamBuffer : public FormatBuffer {
    std::ostream& os;
    explicit StreamBuffer(std::ostream& v);
};

struct CharArrayBuffer : public FormatBuffer {
    char* next;
    char* const last;
    explicit CharArrayBuffer(char* f, char* l);

    template <size_t N>
    explicit CharArrayBuffer(char (&buf)[N]);
};

// Enables formatting of user-defined types. The default implementation uses
// std::ostringstream to convert the argument into a string, and then appends
// this string to the FormatBuffer.
template <typename T>
struct FormatValue {
    errc operator()(FormatBuffer& fb, FormatSpec const& spec, T const& value) const;
};

// Appends the formatted arguments to the given output stream.
template <typename ...Args>
errc Format(FormatBuffer& fb, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given string.
template <typename ...Args>
errc Format(std::string& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Format(std::FILE* os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given stream.
template <typename ...Args>
errc Format(std::ostream& os, std::string_view format, Args const&... args);

// Appends the formatted arguments to the given buffer.
// This is short for:
//  Buffer fb { ... };
//  Format(fb, format, args...);
//
template <typename Buffer, typename ...Args>
errc FormatTo(Buffer fb, std::string_view format, Args const&... args);

}
```

## Format String Syntax

Format string syntax:
`'{' [arg-index] ['*' format-spec-index] [':' format-spec] [',' style] '}'`

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

    XXX: For dynamic format-spec's.

* `format-spec`

    Format specification. See below.

* `style`

    XXX: For formatting user-defined types.

Format specification syntax:
`[[fill] align] [flags] [width] ['.' precision] [conv]`

* `[fill]align`

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

    Minimum field width. The default is `0`.

* `precision`

    For string and floating-point values has the same meaning as in printf.
    Ignored otherwise.

* `conv`

    Specifies how the argument should be presented. Defaults to `s` if omitted
    or if the conversion does not apply to the type of the argument being
    formatted.

    Strings (`std::string[_view]`) and C-Strings (`char const*`):

    - `s`: Nothing special.

    `bool`:

    - `s`: As string: "`true`" or "`false`".

    `char`:

    - `s`: Same as `c`.
    - `S`: Same as `c`.
    - `c`: Print the character.

    Integers:

    - `s`: Same as `d`
    - `i`: Same as `d`.
    - `d`: Display as a signed decimal integer.
    - `u`: Unsigned; base 10.
    - `b`: Unsigned; base 2, prefixed with '`0b`' if `#` is specified.
    - `B`: Unsigned; base 2, prefixed with '`0B`' if `#` is specified.
    - `o`: Unsigned; base 8, prefixed with '`0`' if `#` is specified (and
           value is non-zero).
    - `x`: Unsigned; base 16, using lower-case digits, prefixed with '`0x`'
           if `#` is specified.
    - `X`: Unsigned; base 16. using upper-case digits, prefixed with '`0X`'
           if `#` is specified.

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

## Limitations

- Only up to 16 arguments may be provided.
- ...

## Copying

Distributed under the MIT license.
