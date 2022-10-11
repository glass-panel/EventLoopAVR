#ifndef __EVENTLOOP_H__
    #define __EVENTLOOP_H__

#ifdef USE_STDCPP_LIB
    #include <cstdint>
    #include <type_traits>
#else
    #include "no_stdcpp_lib.h"
#endif

#include "function_traits.h"
#include "Time.h"
#include "Task.h"
#include "CircularTaskQueue.h"

struct EventLoopHelperFunctions
{
    EventLoopHelperFunctions(
        uint8_t (*_preQueueProcess)(uint16_t) = nullptr, 
        uint8_t (*_postQueueProcess)(uint16_t) = nullptr,
        void (*_onTaskAllocationFailed)(void*) = nullptr 
    ) : preQueueProcess(_preQueueProcess),
        postQueueProcess(_postQueueProcess),
        onTaskAllocationFailed(_onTaskAllocationFailed)
    {}

    uint8_t (*preQueueProcess)(uint16_t) = nullptr;
    uint8_t (*postQueueProcess)(uint16_t) = nullptr;
    void (*onTaskAllocationFailed)(void*) = nullptr;
};

template<std::size_t taskbuf_size=768>
class EventLoop
{
private:
    CircularTaskQueue<taskbuf_size> m_task_queue;
    TaskBase* m_cur_begin;
    TaskBase* m_delimiter;
    TaskBase* m_next_end;

    const EventLoopHelperFunctions* m_helper_functions;
    void runCurrentQueue(int16_t passed_ms);

public:
    static constexpr std::size_t TASK_BUFFER_SIZE = taskbuf_size;

    EventLoop(EventLoopHelperFunctions* helper_functions=nullptr) : 
    m_task_queue(),
    m_cur_begin(m_task_queue.begin()),
    m_delimiter(m_task_queue.begin()),
    m_next_end(m_task_queue.begin()),
    m_helper_functions(helper_functions)
    { }
    EventLoop(EventLoop &another) = delete;
    EventLoop& operator=(EventLoop &another) = delete;

    void setHelperFunctions(const EventLoopHelperFunctions* helper_functions) { m_helper_functions = helper_functions; }
    
    // DEBUG
    CircularTaskQueue<taskbuf_size>* __debugGetTaskQueue() { return &m_task_queue; }
    TaskBase* __debugGetCurBegin() { return m_cur_begin; }
    TaskBase* __debugGetDelimiter() { return m_delimiter; }
    TaskBase* __debugGetNextEnd() { return m_next_end; }

    TaskBase* nextTick(const TaskBase* ptr);

    template<typename Callable>
    TaskBase* nextTick(const Task<Callable>& task) { return nextTick(&task); }

    template<typename Ret, typename ...Args>
    TaskBase* nextTick(Ret func(Args...), Args... args)
    { return nextTick(make_task(func).setArgs({args...})); }

    template<typename Callable, typename ...Args, typename = decltype(&Callable::operator())>   // template for lambda
    TaskBase* nextTick(Callable callable, Args... args) 
    { return nextTick(make_task(callable).setArgs({args...})); }
    

    template<typename Callable>
    TaskBase* setTimeout(const Task<Callable>& task, uint32_t ms);

    template<typename Ret, typename ...Args>
    TaskBase* setTimeout(Ret func(Args...), uint32_t ms, Args... args)
    { return setTimeout(make_task(func).setArgs({args...}), ms); }
    
    template<typename Callable, typename ...Args, typename = decltype(&Callable::operator())>   // template for lambda
    TaskBase* setTimeout(Callable callable, uint32_t ms, Args... args) 
    { return setTimeout(make_task(callable).setArgs({args...}), ms); }

    void disableTask(TaskBase* task)
    { m_task_queue.disable(task); }

    void clearTimeout(void* faddr);
    template<typename Ret, typename ...Args>
    void clearTimeout(Ret func(Args...))
    { clearTimeout((void*)func); }

    TaskBase* findTimeout(void* addr);
    template<typename Ret, typename ...Args>
    TaskBase* findTimeout(Ret func(Args...))
    { return findTimeout(reinterpret_cast<void*>(func)); }


    template<typename Callable>
    TaskBase* bindEventHandler(TaskBase* &event_handler, const Task<Callable>& task);

    template<typename Ret, typename ...Args>
    TaskBase* bindEventHandler(TaskBase* &event_handler, Ret func(Args...), Args... args)
    { return bindEventHandler(event_handler, make_task(func).setArgs({args...})); }

    template<typename Callable, typename ...Args, typename = decltype(&Callable::operator())>   // template for lambda
    TaskBase* bindEventHandler(TaskBase* &event_handler, Callable callable, Args... args) 
    { return bindEventHandler(event_handler, make_task(callable).setArgs({args...})); }


    void clearEventHandler(TaskBase* &taskptr);

    uint8_t runOnce(int16_t passed_ms)
    {
        uint8_t status = 0;
        if(m_helper_functions && m_helper_functions->preQueueProcess)
            status = m_helper_functions->preQueueProcess(m_task_queue.getLength());
        runCurrentQueue(passed_ms);
        if(m_helper_functions && m_helper_functions->postQueueProcess)
            status = m_helper_functions->postQueueProcess(m_task_queue.getLength());
        return status;
    }
    void run()
    {
        Time prev = Time::absolute();
        while (m_cur_begin != m_next_end)
        {
            Time now = Time::absolute();
            runOnce(now-prev);
            prev = now;
        }
    };
};

// execute the task in the next queue
template<std::size_t taskbuf_size>
TaskBase* EventLoop<taskbuf_size>::nextTick(const TaskBase* ptr)
{
    auto p = m_task_queue.push(ptr);
    m_next_end = m_task_queue.end();
    if(!p && m_helper_functions && m_helper_functions->onTaskAllocationFailed)
        m_helper_functions->onTaskAllocationFailed(ptr->faddr());
    return p;
}

// delay a task for ms milliseconds
template<std::size_t taskbuf_size>
template<typename Callable>
TaskBase* EventLoop<taskbuf_size>::setTimeout(const Task<Callable>& task, uint32_t ms)
{
    TaskBase *p = nullptr;
    if(ms < 0xFFFF)
    {
        p = m_task_queue.push(task.template transform<TimeoutTask>());
        if(p)
            p->setTimeLeft(ms);
    }
    else 
    {
        p = m_task_queue.push(task.template transform<LongTimeoutTask>());
        if(p)
            p->setScheduleTime(Time::absolute()+ms);
    }
    m_next_end = m_task_queue.end();
    if(!p && m_helper_functions && m_helper_functions->onTaskAllocationFailed)
        m_helper_functions->onTaskAllocationFailed(task.faddr());
    return p;
}

// clear the timeout task by the function pointer
template<std::size_t taskbuf_size>
void EventLoop<taskbuf_size>::clearTimeout(void* faddr)
{
    // when runOnce() iterating current task queue, the timeout task iterated will be move to
    // the next queue. So the specified timeout task will exist once after where the clearTimeout() 
    // called, which is m_cur_begin.
    for(TaskBase* ptr = m_cur_begin; ptr != m_next_end; ptr = m_task_queue.next(ptr))
        if((ptr->type() == TaskType::TIMEOUT || ptr->type() == TaskType::LONGTIMEOUT ) && ptr->faddr() == faddr)    
            m_task_queue.disable(ptr);
}

// find the timeout task by the function pointer, if not found, return nullptr
template<std::size_t taskbuf_size>
TaskBase* EventLoop<taskbuf_size>::findTimeout(void* addr)
{
    for(TaskBase* ptr = m_cur_begin; ptr != m_next_end; ptr = m_task_queue.next(ptr))
        if((ptr->type() == TaskType::TIMEOUT || ptr->type() == TaskType::LONGTIMEOUT ) && ptr->faddr() == addr)    
            return ptr;
    return nullptr;
}



template<std::size_t taskbuf_size>
template<typename Callable>
TaskBase* EventLoop<taskbuf_size>::bindEventHandler(TaskBase* &event_handler, const Task<Callable> &task)
{
    if(event_handler)
        clearEventHandler(event_handler);   // remove old binding first
    auto p = m_task_queue.push(task.template transform<EventTask>());
    if(p)
        p->setKeeper(&event_handler);
    else if(m_helper_functions && m_helper_functions->onTaskAllocationFailed)
        m_helper_functions->onTaskAllocationFailed(task.faddr());
    
    event_handler = p;
    m_next_end = m_task_queue.end();
    return p;
}

template<std::size_t taskbuf_size>
void EventLoop<taskbuf_size>::clearEventHandler(TaskBase* &taskptr)
{
    if(taskptr && taskptr->type() == TaskType::EVENT)
        m_task_queue.disable(taskptr);
    taskptr = nullptr;
}

// run the current queue
template<std::size_t taskbuf_size>
void EventLoop<taskbuf_size>::runCurrentQueue(int16_t passed_ms)
{
    TaskBase *p = m_cur_begin;
    while(p != m_delimiter)
    {
        switch (p->type()) 
        {
        case TaskType::DEFAULT_TASK:
            p->exec();
            break;
        case TaskType::TIMEOUT:
            if((int32_t)p->getTimeLeft() <= passed_ms)
                p->exec();
            else
            {
                auto next = nextTick(p);
                if(next)
                    next->setTimeLeft(p->getTimeLeft()-passed_ms);
            }
            break;
        case TaskType::LONGTIMEOUT:
            if(p->getScheduleTime() <= Time::absolute())
                p->exec();
            else
                nextTick(p);
            break;
        case TaskType::EVENT:
        {
            auto next = nextTick(p);
            if(next)
                next->updateKeeper();
        }
        default:
            break;
        }
        p = m_task_queue.next(p);
        if(m_task_queue.getTruncated()==reinterpret_cast<char*>(m_delimiter))
            m_delimiter = p; // when the queue is truncated, the delimiter, which equals to p
        m_task_queue.pop();
        m_cur_begin = p;
    }   
    // after: m_cur_begin == m_delimiter
    m_delimiter = m_next_end;
}

#endif