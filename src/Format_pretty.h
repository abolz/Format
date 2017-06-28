// Distributed under the MIT license. See the end of the file for details.

#pragma once
#define FMTXX_FORMAT_PRETTY_H 1

#include "Format.h"

#include <iterator>
#include <type_traits>
#include <utility>

namespace fmtxx {

template <typename T>
struct PrettyPrinter
{
    T const& object;

    explicit PrettyPrinter(T const& object) : object(object) {}
};

template <typename T>
PrettyPrinter<T> pretty(T const& object)
{
    return PrettyPrinter<T>(object);
}

namespace pp {

using std::begin;
using std::end;

struct Any {
    template <typename T> Any(T&&) {}
};

struct IsContainerImpl
{
    template <typename T>
    static auto test(T const& u) -> typename std::is_convertible< decltype(begin(u) == end(u)), bool >::type;
    static auto test(Any) -> std::false_type;
};

template <typename T>
struct IsContainer : decltype( IsContainerImpl::test(std::declval<T>()) )
{
};

struct IsTupleImpl
{
    template <typename T>
    static auto test(T const& u) -> decltype( std::get<0>(u), std::true_type{} );
    static auto test(Any) -> std::false_type;
};

template <typename T>
struct IsTuple : decltype( IsTupleImpl::test(std::declval<T>()) )
{
};

template <typename T>
errc PrettyPrint(Writer& w, T const& value);

inline errc PrintString(Writer& w, std::string_view val)
{
    if (!w.Put('"'))
        return errc::io_error;
    if (!w.Write(val.data(), val.size()))
        return errc::io_error;
    if (!w.Put('"'))
        return errc::io_error;

    return errc::success;
}

inline errc PrettyPrint(Writer& w, char const* val)
{
    return pp::PrintString(w, val != nullptr ? val : "(null)");
}

inline errc PrettyPrint(Writer& w, char* val)
{
    return pp::PrintString(w, val != nullptr ? val : "(null)");
}

template <typename T>
errc PrintTuple(Writer& /*w*/, T const& /*object*/, std::integral_constant<size_t, 0>)
{
    return errc::success;
}

template <typename T>
errc PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, 1>)
{
    // print the last tuple element
    return pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - 1>(object));
}

template <typename T, size_t N>
errc PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, N>)
{
    // print the next tuple element
    auto const ec1 = pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - N>(object));
    if (ec1 != errc::success)
        return ec1;

    if (!w.Put(','))
        return errc::io_error;
    if (!w.Put(' '))
        return errc::io_error;

    // print the remaining tuple elements
    auto const ec2 = pp::PrintTuple(w, object, std::integral_constant<size_t, N - 1>());
    if (ec2 != errc::success)
        return ec2;

    return errc::success;
}

template <typename T>
errc DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::true_type)
{
    if (!w.Put('{'))
        return errc::io_error;

    auto const ec = pp::PrintTuple(w, object, typename std::tuple_size<T>::type());
    if (ec != errc::success)
        return ec;

    if (!w.Put('}'))
        return errc::io_error;

    return errc::success;
}

template <typename T>
errc DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::false_type)
{
    return FormatValue<>{}(w, {}, object);
}

template <typename T>
errc DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::true_type)
{
    if (!w.Put('['))
        return errc::io_error;

    auto I = begin(object);
    auto E = end(object);
    if (I != E)
    {
        for (;;)
        {
            auto const ec = pp::PrettyPrint(w, *I);
            if (ec != errc::success)
                return ec;
            if (++I == E)
                break;
            if (!w.Put(','))
                return errc::io_error;
            if (!w.Put(' '))
                return errc::io_error;
        }
    }

    if (!w.Put(']'))
        return errc::io_error;

    return errc::success;
}

template <typename T>
errc DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::false_type)
{
    return pp::DispatchTuple(w, object, IsTuple<T>{});
}

template <typename T>
errc DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::true_type)
{
    return pp::PrintString(w, std::string_view{object.data(), object.size()});
}

template <typename T>
errc DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::false_type)
{
    return pp::DispatchContainer(w, object, IsContainer<T>{});
}

template <typename T>
errc PrettyPrint(Writer& w, T const& object)
{
    return pp::DispatchString(w, object, TreatAsString<T>{});
}

} // namespace pp

template <typename T>
struct FormatValue<PrettyPrinter<T>> {
    errc operator()(Writer& w, FormatSpec const& /*spec*/, PrettyPrinter<T> const& value) const {
        return pp::PrettyPrint(w, value.object);
    }
};

} // namespace fmtxx

//------------------------------------------------------------------------------
// Copyright (c) 2017 A. Bolz
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
