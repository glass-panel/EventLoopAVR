#ifndef __FUNCTION_TRAITS_H__
    #define __FUNCTION_TRAITS_H__

#ifdef USE_STDCPP_LIB
    #include <tuple>
#else
    #include "no_stdcpp_lib.h"
#endif

template<typename>
struct function_traits;

template<typename FunctorType>
struct function_traits : public function_traits<decltype(&FunctorType::operator())>
{};

template<typename ReturnType, typename ...ArgsType>
struct function_traits<ReturnType (ArgsType...)>
{
    using pointer = ReturnType (*)(ArgsType...);
    using returnType = ReturnType;
    using arguments = std::tuple<ArgsType...>;
};

template<typename ReturnType, typename Class, typename ...ArgsType>
struct function_traits<ReturnType (Class::*)(ArgsType...) const>
{
    using pointer = ReturnType (*)(ArgsType...);
    using returnType = ReturnType;
    using arguments = std::tuple<ArgsType...>;
};

#endif
