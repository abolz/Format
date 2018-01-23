// Copyright (c) 2017 Alexander Bolz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef FMTXX_FORMAT_PRETTY_H
#define FMTXX_FORMAT_PRETTY_H 1

#include "Format.h"

#include <iterator> // begin, end
#include <utility>  // declval, forward, get<pair>, tuple_size<pair>

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl
{
    template <typename T>
    struct PrettyPrinter
    {
        T object;
    };
}

template <typename T>
inline impl::PrettyPrinter<T> pretty(T&& object)
{
    // NB:
    // Forward lvalue-references, copy rvalue-references.

    return impl::PrettyPrinter<T>{std::forward<T>(object)};
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

namespace type_traits {

using std::begin;
using std::end;

template <typename T, typename = void>
struct IsContainer
    : std::false_type
{
};

template <typename T>
struct IsContainer<T, Void_t< decltype(begin(std::declval<T>()) == end(std::declval<T>())) >>
    : std::is_convertible<
        decltype(begin(std::declval<T>()) == end(std::declval<T>())),
        bool
      >
{
};

template <typename T, typename = void>
struct IsTuple
    : std::false_type
{
};

template <typename T>
struct IsTuple<T, Void_t< typename std::tuple_size<T>::type >>
    : std::true_type
{
};

} // namespace type_traits

struct PP
{
    struct AsString {};
    struct AsContainer {};
    struct AsTuple {};
    struct AsOther {};

    //
    // Determine how to print objects of type T.
    //
    //  1. Is a string?     => Print as string (quoted)
    //      (includes C-strings)
    //  2. Is a container?  => Recursively print as array "[x, y, z]"
    //  3. Is a tuple?      => Recursively print as tuple "{x, y, z}"
    //  4. Otherwise:       => Use FormatPretty<T>
    //
    template <typename T>
    using PrintAs =
        typename std::conditional<
            TreatAsString<typename std::decay<T>::type>::value,
            AsString,
            typename std::conditional<
                type_traits::IsContainer<T>::value, // NOTE: No decay<T> here!
                AsContainer,
                typename std::conditional<
                    type_traits::IsTuple<typename std::decay<T>::type>::value,
                    AsTuple,
                    AsOther
                >::type
            >::type
        >::type;

    // Recursively calls FormatPretty<T>::operator().
    // The default implementation of FormatPretty<T>::operator() then calls PP::Print(w, spec, val, PRINT_AS).
    template <typename T>
    static ErrorCode Print(Writer& w, FormatSpec const& spec, T const& val);

    static ErrorCode Print(Writer& w, FormatSpec const& spec, char const* const& val)
    {
        return Print(w, spec, val != nullptr ? val : "(null)", AsString());
    }

    static ErrorCode Print(Writer& w, FormatSpec const& spec, char* const& val)
    {
        return Print(w, spec, val != nullptr ? val : "(null)", AsString());
    }

    static ErrorCode Print(Writer& w, FormatSpec const& /*spec*/, string_view val, AsString)
    {
        if (Failed ec = w.put('"'))
            return ec;
        if (Failed ec = w.write(val.data(), val.size()))
            return ec;
        if (Failed ec = w.put('"'))
            return ec;

        return {};
    }

    template <typename T>
    static ErrorCode Print(Writer& w, FormatSpec const& spec, T const& val, AsContainer)
    {
        using std::begin; // using ADL!
        using std::end;   // using ADL!

        string_view const sep = spec.style.empty() ? ", " : spec.style;

        if (Failed ec = w.put('['))
            return ec;

        auto I = begin(val);
        auto E = end(val);
        if (I != E)
        {
            for (;;)
            {
                if (Failed ec = Print(w, spec, *I)) // Recursive!!!
                    return ec;
                if (++I == E)
                    break;
                if (Failed ec = w.write(sep.data(), sep.size()))
                    return ec;
            }
        }

        if (Failed ec = w.put(']'))
            return ec;

        return {};
    }

    template <typename T>
    static ErrorCode PrintTuple(Writer& /*w*/, FormatSpec const& /*spec*/, T const& /*val*/, std::integral_constant<size_t, 0>)
    {
        return {};
    }

    template <typename T>
    static ErrorCode PrintTuple(Writer& w, FormatSpec const& spec, T const& val, std::integral_constant<size_t, 1>)
    {
        using std::get; // using ADL!

        return Print(w, spec, get<std::tuple_size<T>::value - 1>(val)); // Recursive!!!
    }

    template <typename T, size_t N>
    static ErrorCode PrintTuple(Writer& w, FormatSpec const& spec, T const& val, std::integral_constant<size_t, N>)
    {
        using std::get; // using ADL!

        string_view const sep = spec.style.empty() ? ", " : spec.style;

        if (Failed ec = Print(w, spec, get<std::tuple_size<T>::value - N>(val))) // Recursive!!!
            return ec;
        if (Failed ec = w.write(sep.data(), sep.size()))
            return ec;
        if (Failed ec = PrintTuple(w, spec, val, std::integral_constant<size_t, N - 1>()))
            return ec;

        return {};
    }

    template <typename T>
    static ErrorCode Print(Writer& w, FormatSpec const& spec, T const& val, AsTuple)
    {
        if (Failed ec = w.put('{'))
            return ec;
        if (Failed ec = PrintTuple(w, spec, val, typename std::tuple_size<T>::type()))
            return ec;
        if (Failed ec = w.put('}'))
            return ec;

        return {};
    }

    template <typename T>
    static ErrorCode Print(Writer& w, FormatSpec const& spec, T const& val, AsOther)
    {
        // T is not a string, nor a container/array, nor a tuple; and FormatPretty has not
        // been specialized for T.
        // Fall back to FormatValue and see what happens.
        return FormatValue<>{}(w, spec, val);
    }
};

} // namespace impl

// Specialize this to pretty-print custom types.
template <typename T = void, typename /*Enable*/ = void>
struct FormatPretty
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const
    {
        // Note:
        // The "T const&" is important here, so that the IsContainer works correctly for built-in arrays.
        return impl::PP::Print(w, spec, val, impl::PP::PrintAs<T const&>());
    }
};

template <>
struct FormatPretty<void>
{
    template <typename T>
    ErrorCode operator()(Writer& w, FormatSpec const& spec, T const& val) const
    {
        return FormatPretty<T>{}(w, spec, val);
    }
};

namespace impl
{
    template <typename T>
    ErrorCode PP::Print(Writer& w, FormatSpec const& spec, T const& val)
    {
        // Call the custom pretty-printing method.
        //
        // If FormatPretty<T> is specialized for type T, this will call operator() of this
        // specialization, and that's it.
        // If FormatPretty<T> is not specialized, the default implementation just dispatches to
        // impl::PP::Print (the 4 argument version, so there is no infinite recursion here).
        return FormatPretty<T>{}(w, spec, val);
    }
}

template <typename T>
struct FormatValue<impl::PrettyPrinter<T>>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, impl::PrettyPrinter<T> const& value) const
    {
        return impl::PP::Print(w, spec, value.object);
    }
};

} // namespace fmtxx

#endif // FMTXX_FORMAT_PRETTY_H
