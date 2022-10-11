#ifndef __NO_STDCPP_LIB_H__
    #define __NO_STDCPP_LIB_H__

#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "stdlib.h"

/*
    This header file implements a subset of stl metaprogramming templates which are required by the whole framework.
    Some of the features are implemented roughly, so they might not work as same as the original stl ones.
*/


namespace stl_metaprog_alternative
{
    /*
        Implementation of std::tuple 
        ref: https://stackoverflow.com/questions/4041447/how-is-stdtuple-implemented
    */

    template<size_t i, typename T>
    struct TupleLeaf
    {
        T value;
        TupleLeaf() {}
        TupleLeaf(T v) : value(v) {}

    };

    template<size_t i, typename... Ts>
    struct TupleImpl
    {
        static constexpr size_t _size = 0;
    };
    
    template<size_t i>  // empty tuple
    struct TupleImpl<i>
    {
        static constexpr size_t _size = 0;
    };

    template<size_t i, typename Thead, typename... Ttail>
    struct TupleImpl<i, Thead, Ttail...> : 
    public TupleLeaf<i, Thead>, 
    public TupleImpl<i+1, Ttail...>
    {
        TupleImpl() {}

        TupleImpl(Thead head, Ttail... tail) : 
            TupleLeaf<i, Thead>(head), 
            TupleImpl<i+1, Ttail...>(tail...)
        {}

        static constexpr size_t _size = 1 + TupleImpl<i+1, Ttail...>::_size;
    };
    
    template<typename... Ts>
    using tuple = struct TupleImpl<0, Ts...>;
    
    template<typename Tuple>
    struct tuple_size
    {
        using value_type = size_t;
        static constexpr size_t value = Tuple::_size;
    };

    template<typename... Ts>
    constexpr tuple<Ts...> make_tuple(Ts... args)
    {
        return tuple<Ts...>(args...);
    }

    template<size_t i, typename Thead, typename... Ttail>
    constexpr Thead& get(TupleImpl<i, Thead, Ttail...>& t)
    {
        return t.TupleLeaf<i, Thead>::value;
    }

    /* --- end std::tuple implementation --- */

    /*
        Implementation of std::index_sequence
        from: https://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence
    */

    template <size_t... Ints>
    struct index_sequence
    {
        using type = index_sequence;
        using value_type = size_t;
        static constexpr size_t size() noexcept { return sizeof...(Ints); }
    };

    // --------------------------------------------------------------

    template <class Sequence1, class Sequence2>
    struct _merge_and_renumber;

    template <size_t... I1, size_t... I2>
    struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
      : index_sequence<I1..., (sizeof...(I1)+I2)...>
    { };

    // --------------------------------------------------------------

    template <size_t N>
    struct make_index_sequence
      : _merge_and_renumber<typename make_index_sequence<N/2>::type,
                            typename make_index_sequence<N - N/2>::type>
    { };

    template<> struct make_index_sequence<0> : index_sequence<> { };
    template<> struct make_index_sequence<1> : index_sequence<0> { };

    /* --- end std::index_sequence implementation --- */

    /*
        Implementation of std::apply
        ref: https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
    */

    template<typename Callable, typename ArgsTuple, size_t... Is>
    auto applyImpl(Callable f, ArgsTuple& t, index_sequence<Is...>) -> decltype(f(get<Is>(t)...))
    {
        return f(get<Is>(t)...);
    }

    template<typename Callable, typename ArgsTuple>
    constexpr auto apply(Callable f, ArgsTuple& t) -> decltype(applyImpl(f, t, make_index_sequence<tuple_size<ArgsTuple>::value>{}))
    {
        return applyImpl(f, t, make_index_sequence<tuple_size<ArgsTuple>::value>{});
    }

    /* --- end std::apply implementation --- */

    /*
        Implementation of std::enable_if
        ref: https://en.cppreference.com/w/cpp/types/enable_if
    */

    template<bool B, class T = void>
    struct enable_if {};
    
    template<class T>
    struct enable_if<true, T> { typedef T type; };

    /* --- end std::enable_if implementation --- */

    /*
        Implementation of std::remove_reference
    */

    template<typename T>
    struct remove_reference { using type = T; };

    template<typename T> 
    struct remove_reference<T&> { using type = T; };

    template<typename T>
    struct remove_reference<T&&> { using type = T; };

    /* --- end std::remove_reference implementation --- */

    /*
        Implementation of std::forward
    */

    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    /* --- end std::forward implementation --- */
}

namespace std
{
    using size_t = ::size_t;

    template<typename... Ts>
    using tuple = stl_metaprog_alternative::tuple<Ts...>;

    template<typename Tuple>
    using tuple_size = stl_metaprog_alternative::tuple_size<Tuple>;

    template<typename... Ts>
    constexpr tuple<Ts...> make_tuple(Ts... args)
    {
        return stl_metaprog_alternative::make_tuple(args...);
    }

    template<size_t i, typename Thead, typename... Ttail>
    constexpr Thead& get(stl_metaprog_alternative::TupleImpl<i, Thead, Ttail...>& t)
    {
        return stl_metaprog_alternative::get<i>(t);
    }

    template<typename Callable, typename ArgsTuple>
    constexpr auto apply(Callable f, ArgsTuple& t) -> decltype(stl_metaprog_alternative::apply(f, t))
    {
        return stl_metaprog_alternative::apply(f, t);
    }
    
    template<size_t... Ints>
    using index_sequence = stl_metaprog_alternative::index_sequence<Ints...>;

    template<size_t N>
    using make_index_sequence = stl_metaprog_alternative::make_index_sequence<N>;

    template<bool B, class T = void>
    using enable_if = stl_metaprog_alternative::enable_if<B, T>;

    template<typename T>
    using remove_reference = stl_metaprog_alternative::remove_reference<T>;

    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    
}

#endif