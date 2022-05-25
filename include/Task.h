#ifndef __TASK_H__
    #define __TASK_H__

#include <cstdint>
#include <tuple>
#include "function_traits.h"

enum class TaskType : uint8_t
{
    DEFAULT = 0,
    TIMEOUT,
    INTERVAL,
    RUNNED
};

class TaskBase
{
public:
    TaskType type = TaskType::DEFAULT;
    TaskBase() {}
    virtual ~TaskBase() {};
    virtual std::size_t size() const { return sizeof(TaskBase); }
    virtual void exec() {};
    virtual void* faddr() const { return nullptr; }
    // in place new, stdc++ library for avr8 does not provide any new/delete opearator
    void* operator new(std::size_t size, void *ptr)
    {
        return ptr;
    }
};

template<typename Function>
class Task : public TaskBase
{
private:
    using ThisType = Task<Function>;
    using Arguments = typename function_traits<Function>::arguments;
    Arguments m_args;
    Function* m_func;
public:
    Task() {}
    Task(Function func) : m_func(func) {}
    Task(Function func, Arguments args) : m_func(func), m_args(args) {}
    ~Task() {}
    std::size_t size() const { return sizeof(ThisType); }
    void* faddr() const { return reinterpret_cast<void*>(m_func); }
    void exec() { std::apply(m_func, m_args); }
    Task& setFunc(Function* func) { m_func = func; return *this; }
    Task& setArgs(Arguments args) { m_args = args; return *this; }
};

template<typename Function>
constexpr Task<Function> make_task(Function* func) { return Task<Function>(*func); }
#endif