#ifndef __EVENTLOOP_H__
    #define __EVENTLOOP_H__

#include <cstdint>
#include "function_traits.h"
#include "Time.h"
#include "Task.h"
#include "CircularTaskQueue.h"

/*
    Timeout tasks are store in the eventloop's queue
    They won't be execute when iterating the queue cuz they were marked as TIMEOUT
    And checkTimeout() will copy them to the next queue again and again to keep their content in the rotating queue
    Until when the time is come, the checkTimeout() will mark them as DEFAULT type to be executed
*/
struct TimeoutNode
{
    TaskBase *task = nullptr;
    Time when = 0;
};

struct EventLoopHelperFunctions
{
    uint8_t (*preQueueProcess)(uint16_t) = nullptr;
    uint8_t (*postQueueProcess)(uint16_t) = nullptr;
    void (*onTaskAllocationFailed)(void*) = nullptr;
    void (*onTimeoutNodeAllocationFailed)(void*) = nullptr;
};

template<std::size_t taskbuf_size=768, uint16_t timeoutnode_size=20>
class EventLoop
{
private:
    CircularTaskQueue<taskbuf_size> m_task_queue;
    TaskBase* m_cur_begin;
    TaskBase* m_delimiter;
    TaskBase* m_next_end;
    TimeoutNode m_timeout_nodes[timeoutnode_size];

    const EventLoopHelperFunctions* m_helper_functions;
    uint16_t checkTimeout();
    void runCurrentQueue();

public:
    static constexpr std::size_t TASK_BUFFER_SIZE = taskbuf_size;
    static constexpr std::size_t TIMEOUT_NODE_SIZE = timeoutnode_size;
    EventLoop(EventLoop<taskbuf_size, timeoutnode_size> &another) = delete;
    EventLoop(EventLoopHelperFunctions* helper_functions=nullptr) : 
    m_task_queue(),
    m_cur_begin(m_task_queue.begin()),
    m_delimiter(m_task_queue.begin()),
    m_next_end(m_task_queue.begin()),
    m_helper_functions(helper_functions)
    {
        for(uint16_t i = 0; i < timeoutnode_size; i++)
            m_timeout_nodes[i].task = nullptr;
    }

    void setHelperFunctions(const EventLoopHelperFunctions* helper_functions) { m_helper_functions = helper_functions; }
    CircularTaskQueue<taskbuf_size>* debugGetTaskQueue() { return &m_task_queue; }
    TimeoutNode* debugGetTimeoutNodes() { return m_timeout_nodes; }

    template<typename Function>
    TaskBase* nextTick(const Task<Function>& task);
    TaskBase* nextTick(TaskBase* ptr);
    
    template<typename Function>
    TaskBase* setTimeout(const Task<Function>& task, uint16_t ms);    
    bool clearTimeout(void* func_ptr);

    uint8_t runOnce()
    {
        uint8_t status = 0;
        if(m_helper_functions && m_helper_functions->preQueueProcess)
            status = m_helper_functions->preQueueProcess(m_task_queue.getLength());
        checkTimeout();
        runCurrentQueue();
        if(m_helper_functions && m_helper_functions->postQueueProcess)
            status = m_helper_functions->postQueueProcess(m_task_queue.getLength());
        return status;
    }
    void run()
    {
        while (m_cur_begin != m_next_end) 
            runOnce();
    };
};

template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
uint16_t EventLoop<taskbuf_size, timeoutnode_size>::checkTimeout()
{
    uint16_t cnt = 0;
    for(uint16_t i=0; i<timeoutnode_size; i++)
    {
        if(m_timeout_nodes[i].task == nullptr)
            continue;
        if(m_timeout_nodes[i].when <= Time::absolute())
        {
            m_timeout_nodes[i].task->type = TaskType::DEFAULT;  // set the task that complete the timeout to default to let it run
            m_timeout_nodes[i].task = nullptr;
        }
        else
        {   // store the timeout task into the next queue
            m_timeout_nodes[i].task = nextTick(m_timeout_nodes[i].task);   // using memcpy, so no worry about task type
            cnt++;
        }
    }
    return cnt;     // return the number of timeouts
}

// execute the task in the next queue
template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
template<typename Function>
TaskBase* EventLoop<taskbuf_size, timeoutnode_size>::nextTick(const Task<Function>& task)
{
    auto p = m_task_queue.push(task);
    m_next_end = m_task_queue.end();
    return p;
}

// execute the task in the next queue
template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
TaskBase* EventLoop<taskbuf_size, timeoutnode_size>::nextTick(TaskBase* ptr)
{
    auto p = m_task_queue.push(ptr);
    m_next_end = m_task_queue.end();
    return p;
}

// delay a task for ms milliseconds
template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
template<typename Function>
TaskBase* EventLoop<taskbuf_size, timeoutnode_size>::setTimeout(const Task<Function>& task, uint16_t ms)
{
    for(std::size_t i=0; i<timeoutnode_size; i++)   // iterate each slot to find an empty one
        if(m_timeout_nodes[i].task == nullptr)
        {
            auto p = nextTick(task);                // store the task to the next queue
            p->type = TaskType::TIMEOUT;             // remember to set its type to TIMEOUT
            m_timeout_nodes[i].task = p;
            m_timeout_nodes[i].when = Time::absolute() + ms;
            return p;
        }
    return nullptr;
} 

// clear the timeout task by the function pointer
template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
bool EventLoop<taskbuf_size, timeoutnode_size>::clearTimeout(void* func_ptr) 
{
    for(std::size_t i=0; i<timeoutnode_size; i++)
        if(m_timeout_nodes[i].task!=nullptr && m_timeout_nodes[i].task->faddr()==func_ptr)
        {
            m_timeout_nodes[i].task->type = TaskType::RUNNED;   // marked the task in queue runned so it won't be executed when iterating the queue
            m_timeout_nodes[i].task = nullptr;  // then remove it from the timeout slots
            return true;
        }
    return false;
}

// run the current queue
template<std::size_t taskbuf_size, uint16_t timeoutnode_size>
void EventLoop<taskbuf_size, timeoutnode_size>::runCurrentQueue()
{
    TaskBase *p = m_cur_begin;
    while(p != m_delimiter)
    {
        if(p->type == TaskType::DEFAULT)
            p->exec();
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