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

#include "Format_core.h"

#include <iterator> // begin, end
#include <utility>  // declval, forward, get<pair>, tuple_size<pair>

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename T>
struct PrettyPrinter
{
    T object;
};

template <typename T>
inline PrettyPrinter<T> pretty(T&& object)
{
    // NB:
    // Forward lvalue-references, copy rvalue-references.

    return PrettyPrinter<T>{std::forward<T>(object)};
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

namespace adl
{
    using std::begin;
    using std::end;

    template <typename T>
    auto adl_begin(T&& val) -> decltype(( begin(std::forward<T>(val)) )) // <- SFINAE
    {
        return begin(std::forward<T>(val));
    }

    template <typename T>
    auto adl_end(T&& val) -> decltype(( end(std::forward<T>(val)) )) // <- SFINAE
    {
        return end(std::forward<T>(val));
    }
}

using adl::adl_begin;
using adl::adl_end;

template <typename T, typename = void>
struct IsContainer
    : std::false_type
{
};

template <typename T>
struct IsContainer<T, Void_t< decltype(::fmtxx::impl::adl_begin(std::declval<T>()) == ::fmtxx::impl::adl_end(std::declval<T>())) >>
    : std::is_convertible<
        decltype(::fmtxx::impl::adl_begin(std::declval<T>()) == ::fmtxx::impl::adl_end(std::declval<T>())),
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
    //  4. Otherwise:       => Use FormatValue<T>
    //
    template <typename T>
    using PrintAs =
        typename std::conditional<
            TreatAsString<typename std::decay<T>::type>::value,
            AsString,
            typename std::conditional<
                IsContainer<T>::value, // NOTE: No decay<T> here!
                AsContainer,
                typename std::conditional< IsTuple<typename std::decay<T>::type>::value, AsTuple, AsOther >::type
            >::type
        >::type;

    static ErrorCode Print(Writer& w, FormatSpec const& /*spec*/, cxx::string_view val, AsString)
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
        auto const sep = spec.style.empty() ? ", " : spec.style;

        if (Failed ec = w.put('['))
            return ec;

        auto I = ::fmtxx::impl::adl_begin(val);
        auto E = ::fmtxx::impl::adl_end(val);
        if (I != E)
        {
            for (;;)
            {
                if (Failed ec = PrettyPrint(w, spec, *I))
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
        using std::get; // Use ADL!

        return PrettyPrint(w, spec, get<std::tuple_size<T>::value - 1>(val));
    }

    template <typename T, size_t N>
    static ErrorCode PrintTuple(Writer& w, FormatSpec const& spec, T const& val, std::integral_constant<size_t, N>)
    {
        using std::get; // Use ADL!

        auto const sep = spec.style.empty() ? ", " : spec.style;

        if (Failed ec = PrettyPrint(w, spec, get<std::tuple_size<T>::value - N>(val)))
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
        return FormatValue<>{}(w, spec, val);
    }

    template <typename T>
    static ErrorCode PrettyPrint(Writer& w, FormatSpec const& spec, T const& val)
    {
        return Print(w, spec, val, PrintAs<T const&>{}); // NOTE: T const&, the reference is important!
    }

    static ErrorCode PrettyPrint(Writer& w, FormatSpec const& spec, char const* const& val)
    {
        return Print(w, spec, val != nullptr ? val : "(null)", AsString{});
    }

    static ErrorCode PrettyPrint(Writer& w, FormatSpec const& spec, char* const& val)
    {
        return Print(w, spec, val != nullptr ? val : "(null)", AsString{});
    }
};

} // namespace fmtxx::impl

template <typename T>
struct FormatValue<PrettyPrinter<T>>
{
    ErrorCode operator()(Writer& w, FormatSpec const& spec, PrettyPrinter<T> const& value) const
    {
        return ::fmtxx::impl::PP::PrettyPrint(w, spec, value.object);
    }
};

} // namespace fmtxx

#endif // FMTXX_FORMAT_PRETTY_H
