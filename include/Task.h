#ifndef __TASK_H__
    #define __TASK_H__

#ifdef USE_STDCPP_LIB
    #include <type_traits>
    #include <tuple>
    #include <cstdint>
    #include <cstdlib>
#else
    #include "no_stdcpp_lib.h"
#endif

#include "Time.h"
#include "function_traits.h"

enum class TaskType : uint8_t
{
    DEFAULT_TASK = 0,   // That FXXKING Ardunio IDE pre-defined DEFAULT as 1 !!!
    TIMEOUT,
    LONGTIMEOUT,
    EVENT,
    INTERVAL,
    DISABLED,
};

class TaskInterface
{
public:
    TaskInterface() {};
    virtual ~TaskInterface() {};

    // for normal function
    template<typename Ret, typename ...Args>
    static constexpr void* extract_raw_function_pointer(Ret func(Args...))
    { return (void*)func; }
    // for member function
    template<typename Ret, typename Class, typename ...Args>
    static constexpr void* extract_raw_function_pointer(Ret (Class::* func)(Args...))
    { return (void* &)func; }
    // for const member function
    template<typename Ret, typename Class, typename ...Args>
    static constexpr void* extract_raw_function_pointer(Ret (Class::* func)(Args...) const)
    { return (void* &)func; }
    // for lambda function
    template<typename Callable>
    static void* extract_raw_function_pointer(const Callable&)
    {
        auto funcptr = &Callable::operator();
        return (void* &)funcptr;
    }
    // return the size of this task
    virtual std::size_t size() const = 0;
    // return the task type
    virtual TaskType type() const = 0;
    // execute this task with the arguments inside
    virtual void exec() {};
    // return the function pointer of the task
    virtual void* faddr() const { return nullptr; }
    // copy this task to the specified destination
    virtual void copy(void* dst) const { };
    
    // TimeoutTask<> || IntervalTask<>: get the remaining time of the task
    virtual uint16_t getTimeLeft() const { return 0; }
    // TimeoutTask<> || IntervalTask<>: set the remaining time of the task
    virtual void setTimeLeft(uint16_t ms) { }

    // LongTimeoutTask<>: get the schedule time of the task
    virtual Time getScheduleTime() const { return 0; }
    // LongTimeoutTask<>: set the schedule time of the task
    virtual void setScheduleTime(const Time& time) { }

    // EventTask<>: update keeper of the task
    virtual void updateKeeper() { }
    // EventTask<>: set the keeper of the task
    virtual void setKeeper(TaskInterface** keeper) { }

    // IntervalTask<>: get interval time
    virtual uint16_t getInterval() const { return 0; }
    // IntervalTask<>: set interval time
    virtual void setInterval(uint16_t interval) { }

    // in place new, stdc++ library for avr8 does not provide any new/delete opearator
    void* operator new(std::size_t size, void *ptr)
    {
        return ptr;
    }
    void operator delete(void* ptr, std::size_t)
    {
        free(ptr);
    }
};

class DisabledTask : public TaskInterface
{
private:
    std::size_t m_size;
public:
    DisabledTask(std::size_t org_size) : m_size(org_size) {}

    std::size_t size() const final { return m_size; }
    TaskType type() const final { return TaskType::DISABLED; }
};

namespace task_impl
{

template<template<class> class Derived, typename Callable, typename Base>
class TaskMixin : public Base
{
private:
    using ClassType = typename function_traits<Callable>::class_type;
    using StoreType = typename  std::conditional<
                                    std::is_same<ClassType, Callable>::value, 
                                    typename function_traits<Callable>::class_type,
                                    typename function_traits<Callable>::pointer
                                >::type;
    using Arguments = typename  std::conditional<
                                    std::is_same<ClassType, Callable>::value, 
                                    typename function_traits<Callable>::arguments,
                                    typename function_traits<Callable>::this_arguments
                                >::type;    
    Arguments m_args;
    StoreType m_func;
public:
    // default empty constructor gets error when using lambda
    TaskMixin() {}
    TaskMixin(Callable func) : m_func(func)
    {}
    TaskMixin(Callable func, Arguments args) : m_func(func), m_args(args)
    {}
    ~TaskMixin() {}

    Derived<Callable>& setFunc(Callable func) { m_func = func; return *static_cast<Derived<Callable>*>(this); }
    Derived<Callable>& setArgs(Arguments args) { m_args = args; return *static_cast<Derived<Callable>*>(this); }
    void exec() final { std::apply(m_func, m_args); }
    
    std::size_t size() const final { return sizeof(Derived<Callable>); }
    void* faddr() const final { return TaskInterface::extract_raw_function_pointer(m_func); }
    void copy(void *dst) const final { new(dst) Derived<Callable>(*static_cast<const Derived<Callable>*>(this)); }

    template<template<class> class Similar>
    constexpr Similar<Callable> transform() const
    {
        return Similar<Callable>(m_func).setArgs(m_args);
    }
};

class DefaultTaskBase : public TaskInterface
{
public:
    TaskType type() const final { return TaskType::DEFAULT_TASK; }
};

class TimeoutTaskBase : public TaskInterface
{
private:
    uint16_t m_time = 0;
public:
    TaskType type() const final { return TaskType::TIMEOUT; }
    uint16_t getTimeLeft() const final { return m_time; }
    void setTimeLeft(uint16_t ms) final { m_time = ms; }
};

class LongTimeoutTaskBase : public TaskInterface
{
private:
    Time m_schedule = 0;
public:
    TaskType type() const final { return TaskType::LONGTIMEOUT; }
    Time getScheduleTime() const final { return m_schedule; }
    void setScheduleTime(const Time& time) final { m_schedule = time; }
};

class EventTaskBase : public TaskInterface
{
private:
    TaskInterface** m_keeper = nullptr;
public:
    TaskType type() const final { return TaskType::EVENT; }
    void updateKeeper() final { if(m_keeper) *m_keeper = this; }
    void setKeeper(TaskInterface** keeper) final { m_keeper = keeper; }
};

class IntervalTaskBase : public TaskInterface
{
private:
    uint16_t m_interval = 0;
    uint16_t m_time = 0;
public:
    TaskType type() const final { return TaskType::INTERVAL; }
    uint16_t getInterval() const final { return m_interval; }
    void setInterval(uint16_t ms) final { m_interval = ms; }
    uint16_t getTimeLeft() const final { return m_time; }
    void setTimeLeft(uint16_t ms) final { m_time = ms; }
};

};

template<typename Callable>
class Task : public task_impl::TaskMixin<Task, Callable, task_impl::DefaultTaskBase>
{
public:
    using task_impl::TaskMixin<Task, Callable, task_impl::DefaultTaskBase>::TaskMixin;
};

template<typename Callable>
class TimeoutTask : public task_impl::TaskMixin<TimeoutTask, Callable, task_impl::TimeoutTaskBase>
{
public:
    using task_impl::TaskMixin<TimeoutTask, Callable, task_impl::TimeoutTaskBase>::TaskMixin;
};

template<typename Callable>
class LongTimeoutTask : public task_impl::TaskMixin<LongTimeoutTask, Callable, task_impl::LongTimeoutTaskBase>
{

public:
    using task_impl::TaskMixin<LongTimeoutTask, Callable, task_impl::LongTimeoutTaskBase>::TaskMixin;
};

template<typename Callable>
class EventTask : public task_impl::TaskMixin<EventTask, Callable, task_impl::EventTaskBase>
{
public:
    using task_impl::TaskMixin<EventTask, Callable, task_impl::EventTaskBase>::TaskMixin;
};

template<typename Callable>
class IntervalTask : public task_impl::TaskMixin<IntervalTask, Callable, task_impl::IntervalTaskBase>
{
public:
    using task_impl::TaskMixin<IntervalTask, Callable, task_impl::IntervalTaskBase>::TaskMixin;
};

template<typename Callable>
constexpr Task<Callable> make_task(Callable func) 
{ 
    return Task<Callable>(func); 
}

template<typename Function>
constexpr Task<Function*> make_task(Function* func) 
{ 
    return Task<Function*>(func); 
}


#endif