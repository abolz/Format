# Format

[![Build Status](https://travis-ci.org/abolz/Format.svg?branch=master)](https://travis-ci.org/abolz/Format)
[![Build status](https://ci.appveyor.com/api/projects/status/un21yf9tlrfn7ebh/branch/master?svg=true)](https://ci.appveyor.com/project/abolz/format/branch/master)

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

### Format_core.h

```c++
namespace cxx
{
    // A minimal replacement for std::string_view. For compatibility.
    class string_view;
}

namespace fmtxx {

// Error codes returned by the formatting functions.
// A value-initialized ErrorCode indicates success.
enum struct ErrorCode {
    conversion_error        = 1, // Value could not be converted to string
    index_out_of_range      = 2, // Argument index out of range
    invalid_argument        = 3,
    invalid_format_string   = 4,
    io_error                = 5, // Writer failed
    not_supported           = 6, // Conversion not supported
    value_out_of_range      = 7, // Value of argument out of range
};

// Alignment options.
enum struct Align : unsigned char {
    use_default,
    left,
    right,
    center,
    pad_after_sign,
};

// Options for specifying if and how a sign for positive numbers (integral or
// floating-point) should be formatted.
enum struct Sign : unsigned char {
    use_default, // => minus
    minus,       // => '-' if negative, nothing otherwise
    plus,        // => '-' if negative, '+' otherwise
    space,       // => '-' if negative, fill-char otherwise
};

// Contains the fields of the format-specification (see below).
struct FormatSpec {
    cxx::string_view style;
    int   width = 0;
    int   prec  = -1;
    char  fill  = ' ';
    Align align = Align::use_default;
    Sign  sign  = Sign::use_default;
    bool  hash  = false;
    bool  zero  = false;
    char  tsep  = '\0';
    char  conv  = '\0';
};

// Base class for formatting buffers. Can be implemented to provide formatting
// into arbitrary buffers or streams.
class Writer {
protected:
    virtual ~Writer();

    // Write a character to the output stream.
    ErrorCode put(char c);

    // Write a character to the output stream iff it is not the null-character.
    ErrorCode put_nonnull(char c);

    // Insert a range of characters into the output stream.
    ErrorCode write(char const* str, size_t len);

    // Insert a character multiple times into the output stream.
    ErrorCode pad(char c, size_t count);

private:
    virtual ErrorCode Put(char c) = 0;
    virtual ErrorCode Write(char const* str, size_t len) = 0;
    virtual ErrorCode Pad(char c, size_t count) = 0;
};

// Returned by the format_to_chars/printf_to_chars function (below).
// Like std::to_chars.
struct ToCharsResult
{
    char* next;
    ErrorCode ec = ErrorCode{};

    ToCharsResult();
    ToCharsResult(char* next, ErrorCode ec);

    explicit operator bool() const; // Test for successful conversions
};

// Determine whether objects of type T should be handled as strings.
//
// May be specialized for user-defined types to provide more efficient string
// formatting.
template <typename T>
struct TreatAsString : std::false_type {};

// Specialize this to enable formatting of user-defined types.
// Or include "Format_ostream.h" if objects of type T can be formatted using
// operator<<(std::ostream, T const&).
//
// The second template parameter may be used to conditionally enable partial
// specializations.
template <typename T, typename /*Enable*/ = void>
struct FormatValue {
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& value) const;
};

// Dynamically created argument list.
//
// May be passed (as the only argument) to the formatting functions instead of
// the variadic arguments.
class FormatArgs {
public:
    int size() const;
    int max_size() const;

    // Add an argument to this list.
    // PRE: size() < max_size()
    template <typename T>
    void push_back(T&& value);
};

// The format-method uses a python-style format string (see below).
template <typename ...Args>
ErrorCode format(Writer& w, cxx::string_view format, Args const&... args);

// The printf-method uses a printf-style format string.
template <typename ...Args>
ErrorCode printf(Writer& w, cxx::string_view format, Args const&... args);

// Formatting function with an interface like std::to_chars, using python-style
// format strings.
//
// On success, returns a ToCharsResult such that 'ec' equals value-initialized
// ErrorCode and 'next' is the one-past-the-end pointer of the characters
// written.
//
// On error, returns a value of type ToCharsResult holding the error code
// in 'ec' and a copy of the value 'last' in 'next', and leaves the contents
// of the range '[first, last)' in unspecified state.
template <typename ...Args>
ToCharsResult format_to_chars(char* first, char* last, cxx::string_view format, Args const&... args);

// Same as 'format_to_chars' but uses a printf-style format string.
template <typename ...Args>
ToCharsResult printf_to_chars(char* first, char* last, cxx::string_view format, Args const&... args);

} // namespace fmtxx
```

**NOTE**: The formatting functions currently only accept at most 16 arguments.

### Format_stdio.h

```c++
// Write to std::FILE's, keeping track of the number of characters
// (successfully) transmitted.
class FILEWriter : public Writer {
public:
    explicit FILEWriter(std::FILE* v);

    // Returns the FILE stream.
    std::FILE* file() const;

    // Returns the number of bytes successfully transmitted (since construction).
    size_t size() const;
};

// Write to a user allocated buffer.
// If the buffer overflows, keep track of the number of characters that would
// have been written if the buffer were large enough. This is for compatibility
// with snprintf.
class ArrayWriter : public Writer {
public:
    ArrayWriter(char* buffer, size_t buffer_size);

    template <size_t N>
    explicit ArrayWriter(char (&buf)[N]);

    // Returns a pointer to the string.
    // The string is null-terminated if finish() has been called.
    char* data() const;

    // Returns the buffer capacity.
    size_t capacity() const;

    // Returns the length of the string.
    size_t size() const;

    // Returns true if the buffer was too small.
    bool overflow() const;

    // Returns the string.
    cxx::string_view view() const;

    // Null-terminate the buffer.
    // Returns the length of the string not including the null-character.
    size_t finish() noexcept;
};

template <typename ...Args>
ErrorCode format(std::FILE* file, cxx::string_view format, Args const&... args);

template <typename ...Args>
ErrorCode printf(std::FILE* file, cxx::string_view format, Args const&... args);

template <typename ...Args>
int fformat(std::FILE* file, cxx::string_view format, Args const&... args);

template <typename ...Args>
int fprintf(std::FILE* file, cxx::string_view format, Args const&... args);

template <typename ...Args>
int snformat(char* buf, size_t bufsize, cxx::string_view format, Args const&... args);

template <typename ...Args>
int snprintf(char* buf, size_t bufsize, cxx::string_view format, Args const&... args);

template <size_t N, typename ...Args>
int snformat(char (&buf)[N], cxx::string_view format, Args const&... args);

template <size_t N, typename ...Args>
int snprintf(char (&buf)[N], cxx::string_view format, Args const&... args);
```

### Format_string.h

Support for formatting `std::string[_view]`s and for writing to `std::string`s.

```c++
namespace fmtxx {

struct StringFormatResult
{
    std::string str;
    ErrorCode ec = ErrorCode{};

    StringFormatResult();
    StringFormatResult(std::string str, ErrorCode ec);

    explicit operator bool() const; // Test for successful conversion
};

template <typename ...Args>
ErrorCode format(std::string& str, cxx::string_view format, Args const&... args);

template <typename ...Args>
ErrorCode printf(std::string& str, cxx::string_view format, Args const&... args);

template <typename ...Args>
StringFormatResult string_format(cxx::string_view format, Args const&... args);

template <typename ...Args>
StringFormatResult string_printf(cxx::string_view format, Args const&... args);

} // namespace fmtxx
```

### Format_ostream.h

Including this header provides support for formatting user-defined types for
which `operator<<(std::ostream&, T)` is defined.

```c++
namespace fmtxx {

template <typename ...Args>
ErrorCode format(std::ostream& os, cxx::string_view format, Args const&... args);

template <typename ...Args>
ErrorCode printf(std::ostream& os, cxx::string_view format, Args const&... args);

} // namespace fmtxx
```

### Format_system_error.h

Provides support for printing `std::error_code`s and `std::error_condition`s.

### Format_pretty.h

Provides support for pretty-printing arbitrary containers and tuples.

```c++
namespace fmtxx {

template <typename T>
/*unspecified*/ pretty(T const& object);

} // namespace fmtxx
```

For example:

```c++
std::map<std::string, int> m {{"eins", 1}, {"zwei", 2}};

format(stdout, "{}", pretty(m));
    // "[{"eins", 1}, {"zwei", 2}]"
```

### Format.h

Simply includes all the headers above.

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

    **NOTE**: The maximum supported precision is `INT_MAX`.

    **NOTE**: For integral types the maximum supported precision is 300.

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
