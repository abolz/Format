# Format

[![Build Status](https://travis-ci.org/abolz/Format.svg?branch=master)](https://travis-ci.org/abolz/Format)
[![Build status](https://ci.appveyor.com/api/projects/status/un21yf9tlrfn7ebh/branch/master?svg=true)](https://ci.appveyor.com/project/abolz/format/branch/master)
[![codecov](https://codecov.io/gh/abolz/Format/branch/master/graph/badge.svg)](https://codecov.io/gh/abolz/Format)

String formatting library (C++11)

Uses a format-specification syntax similar to Python's [str.format](https://docs.python.org/3/library/string.html#formatstrings)
or C's [printf](http://en.cppreference.com/w/cpp/io/c/fprintf). By default, it
can format strings, numbers (integral and floating point types) and all types
which have a `std::ostream` insertion operator. The [double-conversion](https://github.com/google/double-conversion)
library is used for fast binary-to-decimal conversions.

## {fmt}

**XXX**: Rewrite

## API

**XXX**: Rewrite

## Formatting user-defined data types

**XXX**: Rewrite

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

    Sequences containing '`}`' may be escaped using any of the following
    delimiters: '`'`', '`"`', '`{`' and '`}`', '`(`' and '`)`', or
    '`[`' and '`]`'. E.g.

    ```c++
    format(stdout, "{!one}", 1);
        // => style = "one"
    format(stdout, "{!'{one}'}", 1);
        // => style = "{one}"
    format(stdout, "{!('one')", 1);
        // => style = "'one'"
    ```

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

* `width ::= integer | '{' [arg-index] '}'`

    Minimum field width. Same meaning as in printf. The default is 0.

* `precision ::= integer | '{' [arg-index] '}'`

    For string, integral and floating-point values has the same meaning as in
    printf. Ignored otherwise.

    Note: Precision is applied before padding.

    **Note**: The maximum supported precision is `INT_MAX`.

    **Note**: For integral types the maximum supported precision is 300.

    **Note**: For floating-point types the maximum supported precision is 1074.

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
