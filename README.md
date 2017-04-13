# Format

[![Build Status](https://travis-ci.org/effzeh/Format.svg?branch=master)](https://travis-ci.org/effzeh/Format)

## API

    namespace fmtxx {

    enum struct errc {
        success,
        invalid_format_string,
        invalid_argument,
        io_error,
        index_out_of_range,
    };

    struct FormatSpec {
        std::string_view style;
        int width;
        int prec;
        char fill;
        char align;
        char sign;
        char hash;
        char zero;
        char tsep;
        char conv;
    };

    struct FormatBuffer {
        virtual ~FormatBuffer();
        virtual bool Put(char c);
        virtual bool Write(char const* str, size_t len);
        virtual bool Pad(char c, size_t count);
    };

    // Formatting function for user-defined types.
    template <typename T>
    errc fmtxx__FormatValue(FormatBuffer& fb, FormatSpec const& spec, T const& value);

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

    } // namespace fmtxx

## Syntax

    format_string     := <text> [ format <text> ]*
    format            := '{' [argument_index]
                             ['*' [format_spec_index]]
                             [':' format_spec]
                             [',' style]
                         '}'
    argument_index    := integer
    format_spec_index := integer
    format_spec       := [[fill] align] [flags] [width] ['.' precision] [conv]
    style             := character*
    fill              := character
    align             := '<' | '^' | '>' | '='
    flags             := [sign] ['#'] [thousands_sep] ['0']
    sign              := ' ' | '-' | '+'
    thousands_sep     := ''' | '_'
    width             := integer
    precision         := integer
    conv              := character

If a valid `align` value is specified, it can be preceded by a `fill` character
that can be any character and defaults to a space if omitted.

The valid `align` values are:

Value | Meaning
------|---------
`>`	| Align right within the available space (default).
`<`	| Align left within the available space.
`^`	| Centered within the available space.
`=`	| For numbers: Padding is placed after the sign (if any) but before the digits.

The `flags` may be specified in any order.

The `conv` option specifies how the data should be presented. For built-in
types the valid values are:

For strings:

Value | Meaning
------|--------
`s`   | Nothing special.
`x`   | Prints non-printable (ASCII) characters as a hexadecimal number in the form `\xff`.
`X`   | Prints non-printable (ASCII) characters as a hexadecimal number in the form `\xFF`.

For ints:

Value | Meaning
------|--------
`s`   | Same as `d`
`i`	  | Same as `d`.
`d`	  | Display as a signed decimal integer.
`u`   | Unsigned; base 10.
`b`	  | Unsigned; base 2, prefixed with `0b` if `#` is specified.
`B`   | Unsigned; base 2, prefixed with `0B` if `#` is specified.
`o`	  | Unsigned; base 8, prefixed with `0` if `#` is specified (and value is non-zero).
`x`   | Unsigned; base 16, using lower-case digits, prefixed with `0x` if `#` is specified.
`X`   | Unsigned; base 16. using upper-case digits, prefixed with `0X` if `#` is specified.

For floats:

Value | Meaning
------|--------
`s`   | Short decimal representation (either fixed- or exponential-form) which is guaranteed to round-trip (`prec` is ignored).
`f`   | Same as printf `%f`.
`F`   | Same as printf `%F`.
`e`   | Same as printf `%e`.
`E`   | Same as printf `%E`.
`g`   | Same as printf `%g`.
`G`   | Same as printf `%G`.
`a`   | Same as printf `%a`.
`A`   | Same as printf `%A`.
`x`   | Normalized hexadecimal form, i.e. the leading digit is always `1`, which is guaranteed to round-trip (`prec` is ignored), exponent-char is `p`, prefixed with `0x` the `#` is specified.
`X`   | Normalized hexadecimal form, i.e. the leading digit is always `1`, which is guaranteed to round-trip (`prec` is ignored), exponent-char is `p`, prefixed with `0x` the `#` is specified.
