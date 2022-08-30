#ifndef __EVENTLOOP_H__
    #define __EVENTLOOP_H__

#include "no_stdcpp_lib.h"
#include "function_traits.h"
#include "Time.h"
#include "Task.h"
#include "CircularTaskQueue.h"

struct EventLoopHelperFunctions
{
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

    template<typename Function>
    TaskBase* nextTick(const Task<Function>& task) { return nextTick(&task); }

    template<typename Function, typename ...Args, 
             typename = decltype(&Function::operator())>
    TaskBase* nextTick(Function func, Args&&... args) 
    {   
        //static_assert(is_task<Function>::value, "Not based of");
        return nextTick(make_task(func).setArgs({args...})); 
    }
    
    template<typename Function>
    TaskBase* setTimeout(const Task<Function>& task, uint32_t ms);
    template<typename Function, typename ...Args, 
             typename = decltype(&Function::operator())>
    TaskBase* setTimeout(Function func, uint32_t ms, Args&&... args) 
    { return setTimeout(make_task(func).setArgs({args...}), ms); }

    template<typename Function>
    void clearTimeout(Function func);
    template<typename Function>
    TaskBase* findTimeout(Function func);

    template<typename Function>
    TaskBase* bindEventHandler(TaskBase* &event_handler, const Task<Function>& task);
    template<typename Function, typename ...Args, 
             typename = decltype(&Function::operator())>
    TaskBase* bindEventHandler(TaskBase* &event_handler, Function func, Args&&... args) 
    { return bindEventHandler(event_handler, make_task(func).setArgs({args...})); }
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
template<typename Function>
TaskBase* EventLoop<taskbuf_size>::setTimeout(const Task<Function>& task, uint32_t ms)
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
template<typename Function>
void EventLoop<taskbuf_size>::clearTimeout(Function func) 
{
    // when runOnce() iterating current task queue, the timeout task iterated will be move to
    // the next queue. So the specified timeout task will exist once after where the clearTimeout() 
    // called, which is m_cur_begin.
    void* func_ptr = (void*)(+func);
    for(TaskBase* ptr = m_cur_begin; ptr != m_next_end; ptr = m_task_queue.next(ptr))
        if((ptr->type() == TaskType::TIMEOUT || ptr->type() == TaskType::LongTimeoutTask ) && ptr->faddr() == func_ptr)    
            ptr->disable();
}

// find the timeout task by the function pointer, if not found, return nullptr
template<std::size_t taskbuf_size>
template<typename Function>
TaskBase* EventLoop<taskbuf_size>::findTimeout(Function func)
{
    // the timeout task will only be stored and get executed after m_cur_begin
    void* func_ptr = (void*)(+func);
    for(TaskBase* ptr = m_cur_begin; ptr != m_next_end; ptr = m_task_queue.next(ptr))
        if((ptr->type() == TaskType::TIMEOUT || ptr->type() == TaskType::LongTimeoutTask ) && ptr->faddr() == func_ptr)    
            return ptr;
    return nullptr;
}

template<std::size_t taskbuf_size>
template<typename Function>
TaskBase* EventLoop<taskbuf_size>::bindEventHandler(TaskBase* &event_handler, const Task<Function> &task)
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
        taskptr->disable();
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
        case TaskType::DEFAULT:
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
        case TaskType::LongTimeoutTask:
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