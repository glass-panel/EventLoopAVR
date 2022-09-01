#ifndef __TASK_H__
    #define __TASK_H__

#include "no_stdcpp_lib.h"
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
    static constexpr bool IsTask = false;
    TaskBase() {}
    virtual ~TaskBase() {};
    
    // return the size of this task
    virtual std::size_t size() const { return sizeof(TaskBase); }
    // return the task type
    virtual TaskType type() const { return TaskType::DISABLED; }
    // disable the task
    virtual void disable() {}
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

template<template<class> class Derived, typename Function>
class TaskMixin : public TaskBase
{
private:
    using Pointer = typename function_traits<Function>::pointer;
    using Arguments = typename function_traits<Function>::arguments;    
    Arguments m_args;
    Pointer m_func;
public:
    TaskMixin() {}
    TaskMixin(Function func) : m_func(func) {}
    TaskMixin(Function func, Arguments args) : m_func(func), m_args(args) {}
    ~TaskMixin() {}

    Derived<Function>& setFunc(Pointer func) { m_func = func; return *static_cast<Derived<Function>*>(this); }
    Derived<Function>& setArgs(Arguments args) { m_args = args; return *static_cast<Derived<Function>*>(this); }
    void exec() override { std::apply(m_func, m_args); }
    void disable() override { m_func = nullptr; }

    std::size_t size() const override { return sizeof(Derived<Function>); }
    void* faddr() const override { return reinterpret_cast<void*>(m_func); }
    void copy(void *dst) const override { new(dst) Derived<Function>(*static_cast<const Derived<Function>*>(this)); }

    template<template<class> class Similar>
    Similar<Function> transform() const
    {
        return Similar<Function>().setFunc(m_func).setArgs(m_args);
    }
};

template<typename Function>
class Task : public TaskMixin<Task, Function>
{
public:
    Task() {}
    Task(Function func) : TaskMixin<Task, Function>(func) {}
    TaskType type() const override { return this->faddr()? TaskType::DEFAULT_TASK : TaskType::DISABLED; }
};

template<typename Function>
class TimeoutTask : public TaskMixin<TimeoutTask, Function>
{
private:
    uint16_t m_time = 0;
public:
    TimeoutTask() {}
    TimeoutTask(Function func) : TaskMixin<TimeoutTask, Function>(func) {}

    TaskType type() const override { return this->faddr()? TaskType::TIMEOUT : TaskType::DISABLED; }
    uint16_t getTimeLeft() const override { return m_time; }
    void setTimeLeft(uint16_t ms) override { m_time = ms; }
};

template<typename Function>
class LongTimeoutTask : public TaskMixin<LongTimeoutTask, Function>
{
private:
    Time m_schedule = 0;
public:
    LongTimeoutTask() {}
    LongTimeoutTask(Function func) : TaskMixin<LongTimeoutTask, Function>(func) {}
    
    TaskType type() const override { return this->faddr()? TaskType::LONGTIMEOUT : TaskType::DISABLED; }
    Time getScheduleTime() const override { return m_schedule; }
    void setScheduleTime(const Time& time) override { m_schedule = time; }
};

template<typename Function>
class EventTask : public TaskMixin<EventTask, Function>
{
private:
    TaskBase** m_keeper = nullptr;
public:
    EventTask() {}
    EventTask(Function func) : TaskMixin<EventTask, Function>(func) {}

    TaskType type() const override { return this->faddr()? TaskType::EVENT : TaskType::DISABLED; }
    void updateKeeper() override { if(m_keeper) *m_keeper = this; }
    void setKeeper(TaskBase** keeper) override { m_keeper = keeper; }
};

template<typename Function>
constexpr Task<Function> make_task(Function* func) 
{ 
    return Task<Function>(*func); 
}

template<typename Functor>
constexpr Task<Functor> make_task(Functor func) 
{ 
    return Task<Functor>(func); 
}

#endif