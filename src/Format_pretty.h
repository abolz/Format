#pragma once

#include "Format_core.h"

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
ErrorCode PrettyPrint(Writer& w, T const& value);

inline ErrorCode PrintString(Writer& w, StringView val)
{
    if (Failed ec = w.put('"'))
        return ec;
    if (Failed ec = w.write(val.data(), val.size()))
        return ec;
    if (Failed ec = w.put('"'))
        return ec;

    return ErrorCode::success;
}

inline ErrorCode PrettyPrint(Writer& w, char const* val)
{
    return ::fmtxx::impl::pp::PrintString(w, val != nullptr ? val : "(null)");
}

inline ErrorCode PrettyPrint(Writer& w, char* val)
{
    return ::fmtxx::impl::pp::PrintString(w, val != nullptr ? val : "(null)");
}

template <typename T>
ErrorCode PrintTuple(Writer& /*w*/, T const& /*object*/, std::integral_constant<size_t, 0>)
{
    return ErrorCode::success;
}

template <typename T>
ErrorCode PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, 1>)
{
    return ::fmtxx::impl::pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - 1>(object));
}

template <typename T, size_t N>
ErrorCode PrintTuple(Writer& w, T const& object, std::integral_constant<size_t, N>)
{
    if (Failed ec = ::fmtxx::impl::pp::PrettyPrint(w, std::get<std::tuple_size<T>::value - N>(object)))
        return ec;
    if (Failed ec = w.put(','))
        return ec;
    if (Failed ec = w.put(' '))
        return ec;
    if (Failed ec = ::fmtxx::impl::pp::PrintTuple(w, object, std::integral_constant<size_t, N - 1>()))
        return ec;

    return ErrorCode::success;
}

template <typename T>
ErrorCode DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::true_type)
{
    if (Failed ec = w.put('{'))
        return ec;
    if (Failed ec = ::fmtxx::impl::pp::PrintTuple(w, object, typename std::tuple_size<T>::type()))
        return ec;
    if (Failed ec = w.put('}'))
        return ec;

    return ErrorCode::success;
}

template <typename T>
ErrorCode DispatchTuple(Writer& w, T const& object, /*IsTuple*/ std::false_type)
{
    return ::fmtxx::format_value(w, {}, object);
}

template <typename T>
ErrorCode DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::true_type)
{
    if (Failed ec = w.put('['))
        return ec;

    auto I = begin(object); // using ADL
    auto E = end(object); // using ADL
    if (I != E)
    {
        for (;;)
        {
            if (Failed ec = ::fmtxx::impl::pp::PrettyPrint(w, *I))
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
ErrorCode DispatchContainer(Writer& w, T const& object, /*IsContainer*/ std::false_type)
{
    return ::fmtxx::impl::pp::DispatchTuple(w, object, IsTuple<T>{});
}

template <typename T>
ErrorCode DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::true_type)
{
    return ::fmtxx::impl::pp::PrintString(w, StringView{object.data(), object.size()});
}

template <typename T>
ErrorCode DispatchString(Writer& w, T const& object, /*TreatAsString*/ std::false_type)
{
    return ::fmtxx::impl::pp::DispatchContainer(w, object, IsContainer<T>{});
}

template <typename T>
ErrorCode PrettyPrint(Writer& w, T const& object)
{
    return ::fmtxx::impl::pp::DispatchString(w, object, TreatAsString<T>{});
}

} // namespace pp
} // namespace impl

template <typename T>
struct FormatValue<PrettyPrinter<T>> {
    ErrorCode operator()(Writer& w, FormatSpec const& /*spec*/, PrettyPrinter<T> const& value) const {
        return ::fmtxx::impl::pp::PrettyPrint(w, value.object);
    }
};

} // namespace fmtxx
