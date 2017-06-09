// Distributed under the MIT license. See the end of the file for details.

#pragma once

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include "Format.h"

namespace fmtxx {

template <typename T>
struct PrettyPrinter
{
    T const& object;
    explicit PrettyPrinter(T const& object) : object(object)
    {
    }
};

template <typename T>
PrettyPrinter<T> pretty(T const& object) {
    return PrettyPrinter<T>(object);
}

template <typename T>
struct FormatValue<PrettyPrinter<T>> {
    errc operator()(Writer& w, FormatSpec const& spec, PrettyPrinter<T> const& value) const;
};

} // namespace fmtxx

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

#include <iterator>
#include <type_traits>
#include <utility>

namespace fmtxx {
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
void PrettyPrint(Writer& w, T const& value);

inline void PrettyPrint(Writer& w, std::string_view val)
{
    w.Put('"');
    w.Write(val.data(), val.size());
    w.Put('"');
}

inline void PrettyPrint(Writer& w, std::string const& val)
{
    return pp::PrettyPrint(w, std::string_view(val));
}

inline void PrettyPrint(Writer& w, char const* val)
{
    return pp::PrettyPrint(w, std::string_view(val));
}

inline void PrettyPrint(Writer& w, char* val)
{
    return pp::PrettyPrint(w, std::string_view(val));
}

template <typename T>
void PrintTuple(Writer& /*w*/, T const& /*object*/, std::integral_constant<size_t, 0>)
{
}

template <typename T>
void PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, 1>)
{
    // print the last tuple element
    pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - 1>(object));
}

template <typename T, size_t N>
void PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, N>)
{
    // print the next tuple element
    pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - N>(object));

    w.Put(',');
    w.Put(' ');

    // print the remaining tuple elements
    pp::PrintTuple(w, object, std::integral_constant<size_t, N - 1>());
}

template <typename T>
void DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::true_type)
{
    w.Put('{');

    pp::PrintTuple(w, object, typename std::tuple_size<T>::type());

    w.Put('}');
}

template <typename T>
void DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::false_type)
{
    FormatValue<T>{}(w, {}, object);
}

template <typename T>
void DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::true_type)
{
    w.Put('[');

    auto I = begin(object);
    auto E = end(object);
    if (I != E)
    {
        for (;;)
        {
            pp::PrettyPrint(w, *I);
            if (++I == E)
                break;
            w.Put(',');
            w.Put(' ');
        }
    }

    w.Put(']');
}

template <typename T>
void DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::false_type)
{
    pp::DispatchTuple(w, object, IsTuple<T>{});
}

template <typename T>
void PrettyPrint(Writer& w, T const& object)
{
    pp::DispatchContainer(w, object, IsContainer<T>{});
}

} // namespace pp

template <typename T>
errc FormatValue<PrettyPrinter<T>>::operator()(Writer& w, FormatSpec const& /*spec*/, PrettyPrinter<T> const& value) const
{
    pp::PrettyPrint(w, value.object);
    return errc::success;
}

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
