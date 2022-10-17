#ifndef __TASK_H__
    #define __TASK_H__

#ifdef USE_STDCPP_LIB
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
    DISABLED,
};

class TaskBase
{
public:
    TaskBase() {}
    virtual ~TaskBase() {};

    template<typename Ret, typename ...Args>
    constexpr static void* extract_raw_function_pointer(Ret func(Args...))
    { return (void*)func; }
    template<typename Callable>
    constexpr static void* extract_raw_function_pointer(Callable callable)
    {
        auto funcptr = &std::remove_reference<Callable>::type::operator();
        return (void* &)funcptr;
    }

    // return the size of this task
    virtual std::size_t size() const { return sizeof(TaskBase); }
    // return the task type
    virtual TaskType type() const { return TaskType::DISABLED; }
    // execute this task with the arguments inside
    virtual void exec() {};
    // return the function pointer of the task
    virtual void* faddr() const { return nullptr; }
    // copy this task to the specified destination
    virtual void copy(void* dst) const { new(dst) TaskBase(*this); };
    
    // TimeoutTask<>: get the remaining time of the task
    virtual uint16_t getTimeLeft() const { return 0; }
    // TimeoutTask<>: set the remaining time of the task
    virtual void setTimeLeft(uint16_t ms) { }

    // LongTimeoutTask<>: get the schedule time of the task
    virtual Time getScheduleTime() const { return 0; }
    // LongTimeoutTask<>: set the schedule time of the task
    virtual void setScheduleTime(const Time& time) { }

    // EventTask<>: update keeper of the task
    virtual void updateKeeper() { }
    // EventTask<>: set the keeper of the task
    virtual void setKeeper(TaskBase** keeper) { }

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

class DisabledTask : public TaskBase
{
private:
    std::size_t m_size;
public:
    DisabledTask(std::size_t org_size) : m_size(org_size) {}

    std::size_t size() const final { return m_size; }
    TaskType type() const final { return TaskType::DISABLED; }
};

template<template<class> class Derived, typename Callable>
class TaskMixin : public TaskBase
{
private:
    using StoreType = typename function_traits<Callable>::store_type;
    using Arguments = typename function_traits<Callable>::arguments;    
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
    void* faddr() const final { return TaskBase::extract_raw_function_pointer(m_func); }
    void copy(void *dst) const final { new(dst) Derived<Callable>(*static_cast<const Derived<Callable>*>(this)); }

    template<template<class> class Similar>
    Similar<Callable> transform() const
    {
        return Similar<Callable>(m_func).setArgs(m_args);
    }
};

template<typename Callable>
class Task : public TaskMixin<Task, Callable>
{
public:
    using TaskMixin<Task, Callable>::TaskMixin;

    TaskType type() const final { return TaskType::DEFAULT_TASK; }
};

template<typename Callable>
class TimeoutTask : public TaskMixin<TimeoutTask, Callable>
{
private:
    uint16_t m_time = 0;
public:
    using TaskMixin<TimeoutTask, Callable>::TaskMixin;

    TaskType type() const final { return TaskType::TIMEOUT; }
    uint16_t getTimeLeft() const final { return m_time; }
    void setTimeLeft(uint16_t ms) final { m_time = ms; }
};

template<typename Callable>
class LongTimeoutTask : public TaskMixin<LongTimeoutTask, Callable>
{
private:
    Time m_schedule = 0;
public:
    using TaskMixin<LongTimeoutTask, Callable>::TaskMixin;

    TaskType type() const final { return TaskType::LONGTIMEOUT; }
    Time getScheduleTime() const final { return m_schedule; }
    void setScheduleTime(const Time& time) final { m_schedule = time; }
};

template<typename Callable>
class EventTask : public TaskMixin<EventTask, Callable>
{
private:
    TaskBase** m_keeper = nullptr;
public:
    using TaskMixin<EventTask, Callable>::TaskMixin;

    TaskType type() const final { return TaskType::EVENT; }
    void updateKeeper() final { if(m_keeper) *m_keeper = this; }
    void setKeeper(TaskBase** keeper) final { m_keeper = keeper; }
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