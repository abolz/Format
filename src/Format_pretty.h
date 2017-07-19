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

#include <iterator>     // begin, end
#include <utility>      // declval, forward, get<pair>, tuple_size<pair>

namespace fmtxx {

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

template <typename T>
struct PrettyPrinter
{
    T const& object;

    explicit PrettyPrinter(T const& object) : object(object) {}
};

template <typename T>
inline PrettyPrinter<T> pretty(T const& object)
{
    return PrettyPrinter<T>(object);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace impl {

#if 1 // pre C++14

template <typename ...>
struct AlwaysVoid { using type = void; };

template <typename ...Ts>
using Void_t = typename AlwaysVoid<Ts...>::type;

#else

template <typename ...Ts>
using Void_t = void;

#endif

namespace adl
{
    using std::begin;
    using std::end;

    template <typename T>
    auto adl_begin(T&& t) -> decltype(( begin(std::forward<T>(t)) )) // <- SFINAE
    {
        return begin(std::forward<T>(t));
    }

    template <typename T>
    auto adl_end(T&& t) -> decltype(( end(std::forward<T>(t)) )) // <- SFINAE
    {
        return end(std::forward<T>(t));
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
struct IsContainer<T, Void_t< decltype(adl_begin(std::declval<T>()) == adl_end(std::declval<T>())) >>
    : std::is_convertible<
        decltype(adl_begin(std::declval<T>()) == adl_end(std::declval<T>())),
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
struct IsTuple<T, Void_t< decltype(std::get<0>(std::declval<T>())) >>
    : std::true_type
{
};

struct PP
{
    static ErrorCode PrintString(Writer& w, cxx::string_view val)
    {
        if (Failed ec = w.put('"'))
            return ec;
        if (Failed ec = w.write(val.data(), val.size()))
            return ec;
        if (Failed ec = w.put('"'))
            return ec;

        return ErrorCode::success;
    }

    template <typename T>
    static ErrorCode PrintTuple(Writer& /*w*/, T const& /*object*/, std::integral_constant<size_t, 0>)
    {
        return ErrorCode::success;
    }

    template <typename T>
    static ErrorCode PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, 1>)
    {
        return PrettyPrint(w, std::get<std::tuple_size<T>::value - 1>(object));
    }

    template <typename T, size_t N>
    static ErrorCode PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, N>)
    {
        if (Failed ec = PrettyPrint(w, std::get<std::tuple_size<T>::value - N>(object)))
            return ec;
        if (Failed ec = w.put(','))
            return ec;
        if (Failed ec = w.put(' '))
            return ec;
        if (Failed ec = PrintTuple(w, object, std::integral_constant<size_t, N - 1>()))
            return ec;

        return ErrorCode::success;
    }

    template <typename T>
    static ErrorCode DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::true_type)
    {
        if (Failed ec = w.put('{'))
            return ec;
        if (Failed ec = PrintTuple(w, object, typename std::tuple_size<T>::type()))
            return ec;
        if (Failed ec = w.put('}'))
            return ec;

        return ErrorCode::success;
    }

    template <typename T>
    static ErrorCode DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::false_type)
    {
        return ::fmtxx::format_value(w, {}, object);
    }

    template <typename T>
    static ErrorCode DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::true_type)
    {
        if (Failed ec = w.put('['))
            return ec;

        auto I = adl_begin(object);
        auto E = adl_end(object);
        if (I != E)
        {
            for (;;)
            {
                if (Failed ec = PrettyPrint(w, *I))
                    return ec;
                if (++I == E)
                    break;
                if (Failed ec = w.put(','))
                    return ec;
                if (Failed ec = w.put(' '))
                    return ec;
            }
        }

        if (Failed ec = w.put(']'))
            return ec;

        return ErrorCode::success;
    }

    template <typename T>
    static ErrorCode DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::false_type)
    {
        return DispatchTuple(w, object, typename IsTuple<T>::type{});
    }

    template <typename T>
    static ErrorCode DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::true_type)
    {
        return PrintString(w, cxx::string_view{object.data(), object.size()});
    }

    template <typename T>
    static ErrorCode DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::false_type)
    {
        return DispatchContainer(w, object, typename IsContainer<T>::type{});
    }

    template <typename T>
    static ErrorCode PrettyPrint(Writer& w, T const& object)
    {
        return DispatchString(w, object, typename TreatAsString<T>::type{});
    }

    static ErrorCode PrettyPrint(Writer& w, char const* val)
    {
        return PrintString(w, val != nullptr ? val : "(null)");
    }

    static ErrorCode PrettyPrint(Writer& w, char* val)
    {
        return PrintString(w, val != nullptr ? val : "(null)");
    }
};

} // namespace fmtxx::impl

template <typename T>
struct FormatValue<PrettyPrinter<T>>
{
    ErrorCode operator()(Writer& w, FormatSpec const& /*spec*/, PrettyPrinter<T> const& value) const
    {
        return fmtxx::impl::PP::PrettyPrint(w, value.object);
    }
};

} // namespace fmtxx

#endif // FMTXX_FORMAT_PRETTY_H
