#ifndef __FUNCTION_TRAITS_H__
    #define __FUNCTION_TRAITS_H__

#include <tuple>
//#include <functional>

template<typename FunctionType>
struct function_traits {};

template<typename ReturnType, typename ...ArgsType>
struct function_traits<ReturnType (ArgsType...)>
{
    using returnType = ReturnType;
    using arguments = std::tuple<ArgsType...>;
};

/*
template<typename ReturnType, typename ...ArgsType>
struct function_traits<std::function<ReturnType (ArgsType...)>>
{
    using returnType = ReturnType;
    using arguments = std::tuple<ArgsType...>;
};
*/
#endif
