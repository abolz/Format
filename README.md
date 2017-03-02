# Format

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
        virtual bool Put(char c, size_t count);
        virtual bool Write(char const* str, size_t len);
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
    format_spec       := [[fill] align] [sign] ['#'] ['''] ['0'] [width] ['.' precision] [conv]
    style             := character*
    fill              := character
    align             := '<' | '^' | '>' | '='
    sign              := ' ' | '-' | '+'
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

The `conv` option specifies how the data should be represented. For built-in
types and strings, the valid values are:

Value |  Meaning
------|----------
None | The same as 's'.
`s` | For ints: Same as `d`. For floats: a short representation which is guaranteed to round-trip.
`i`	| For ints: Same as `d`.
`d`	| For ints: Display in base 10.
`u` | For ints: Display in base 10.
`b`	| For ints: Display in base 2.
`o`	| For ints: Display in base 8.
`x` | For ints: Display in base 16. For floats: Normalized hexadecimal format, guaranteed to round-trip.
`X` | For ints: Display in base 16. For floats: Normalized hexadecimal format, guaranteed to round-trip.
`f` | For floats: Same as printf `%f`.
`F` | For floats: Same as printf `%F`.
`e` | For floats: Same as printf `%e`.
`E` | For floats: Same as printf `%E`.
`g` | For floats: Same as printf `%g`.
`G` | For floats: Same as printf `%G`.
`a` | For floats: Same as printf `%a`.
`A` | For floats: Same as printf `%A`.
