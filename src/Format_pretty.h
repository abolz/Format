#pragma once

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

inline errc PrintString(Writer& w, string_view val)
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
    return fmtxx::pp::PrintString(w, val != nullptr ? val : "(null)");
}

inline errc PrettyPrint(Writer& w, char* val)
{
    return fmtxx::pp::PrintString(w, val != nullptr ? val : "(null)");
}

template <typename T>
errc PrintTuple(Writer& /*w*/, T const& /*object*/, std::integral_constant<size_t, 0>)
{
    return errc::success;
}

template <typename T>
errc PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, 1>)
{
    return fmtxx::pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - 1>(object));
}

template <typename T, size_t N>
errc PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, N>)
{
    auto const ec1 = fmtxx::pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - N>(object));
    if (ec1 != errc::success)
        return ec1;

    if (!w.Put(','))
        return errc::io_error;
    if (!w.Put(' '))
        return errc::io_error;

    auto const ec2 = fmtxx::pp::PrintTuple(w, object, std::integral_constant<size_t, N - 1>());
    if (ec2 != errc::success)
        return ec2;

    return errc::success;
}

template <typename T>
errc DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::true_type)
{
    if (!w.Put('{'))
        return errc::io_error;

    auto const ec = fmtxx::pp::PrintTuple(w, object, typename std::tuple_size<T>::type());
    if (ec != errc::success)
        return ec;

    if (!w.Put('}'))
        return errc::io_error;

    return errc::success;
}

template <typename T>
errc DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::false_type)
{
    return fmtxx::format_value(w, {}, object);
}

template <typename T>
errc DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::true_type)
{
    if (!w.Put('['))
        return errc::io_error;

    auto I = begin(object); // using ADL
    auto E = end(object); // using ADL
    if (I != E)
    {
        for (;;)
        {
            auto const ec = fmtxx::pp::PrettyPrint(w, *I);
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
    return fmtxx::pp::DispatchTuple(w, object, IsTuple<T>{});
}

template <typename T>
errc DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::true_type)
{
    return fmtxx::pp::PrintString(w, string_view{object.data(), object.size()});
}

template <typename T>
errc DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::false_type)
{
    return fmtxx::pp::DispatchContainer(w, object, IsContainer<T>{});
}

template <typename T>
errc PrettyPrint(Writer& w, T const& object)
{
    return fmtxx::pp::DispatchString(w, object, TreatAsString<T>{});
}

} // namespace pp

template <typename T>
struct FormatValue<PrettyPrinter<T>> {
    errc operator()(Writer& w, FormatSpec const& /*spec*/, PrettyPrinter<T> const& value) const {
        return fmtxx::pp::PrettyPrint(w, value.object);
    }
};

} // namespace fmtxx
