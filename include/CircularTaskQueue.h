#ifndef __CircularTaskQueue_H__
    #define __CircularTaskQueue_H__

#include "Task.h"

template<std::size_t buffer_size>
class CircularTaskQueue
{
private:
    char m_buffer[buffer_size];
    char *m_buffer_begin = m_buffer;
    char *m_buffer_end = nullptr;
    char *m_begin = nullptr;
    char *m_end = nullptr;
    char *m_truncated = nullptr;
    std::size_t length = 0;

    char* calcAllocAddr(std::size_t size);
public:

    CircularTaskQueue(const CircularTaskQueue<buffer_size> &another) = delete;
    CircularTaskQueue()
    {
        m_buffer_end = m_buffer + buffer_size;
        m_begin = m_buffer_begin;
        m_end = m_buffer_begin;
    }
    ~CircularTaskQueue()
    {
        while(length > 0)   // clear all task
            pop();
    }
    char* getBufferBegin() { return m_buffer_begin; }
    char* getBufferEnd() { return m_buffer_end; }
    char* getTruncated() { return m_truncated; }
    std::size_t getLength() { return length; }

    TaskInterface* begin() { return reinterpret_cast<TaskInterface*>(m_begin); }
    TaskInterface* end() { return reinterpret_cast<TaskInterface*>(m_end); }
    TaskInterface* next(TaskInterface* cur);  

    TaskInterface* push(const TaskInterface* ptr);
    TaskInterface* push(const TaskInterface &task) { return push(&task); }

    void pop();
    void disable(const TaskInterface* ptr);
};

// calculate an available address for a new task, CANNOT be used in ISR
template<std::size_t buffer_size>
char* CircularTaskQueue<buffer_size>::calcAllocAddr(std::size_t size)
{
    char *addr = nullptr;
    if(m_begin <= m_end && m_end + size < m_buffer_end)
    {   // [buffer_begin] <-- 0~n --> [begin] <-- 0~n --> [end] <-- {addr} size~n --> [buffer_end]
        addr = m_end;
        m_end = addr + size;
    }
    else if(m_begin <= m_end && m_end + size >= m_buffer_end && m_buffer_begin + size < m_begin)
    {   // [buffer_begin] <--{addr} size~n --> [begin] <-- 0~n --> [end] <-- 0~size-1 --> [buffer_end]
        addr = m_buffer_begin;
        m_truncated = m_end;
        m_end = addr + size;
    }
    else if(m_begin > m_end && m_end + size < m_begin)
    {   // [buffer_begin] <-- 0~n --> [end] <-- {addr} size~n --> [begin] <-- 0~n --> [truncated] <-- 0-unused --> [buffer_end]
        addr = m_end;
        m_end = addr + size;  
    }
    else // [buffer_begin] <-- 0~n --> [end] <-- {NOT ENOUGH PLACE} 0~size-1 --> [begin] <-- 0~n --> [truncated] <-- 0~unused --> [buffer_end]
        return nullptr;
    return addr;
}

// push a task to the front of the queue
template<std::size_t buffer_size>
TaskInterface* CircularTaskQueue<buffer_size>::push(const TaskInterface* ptr)
{   // we have no type info here so just copy it by size
    const auto addr = calcAllocAddr(ptr->size());
    if(!addr)
        return nullptr;
    ptr->copy(addr);
    length++;
    return reinterpret_cast<TaskInterface*>(addr);
}

// pop the element at the back of the queue
template<std::size_t buffer_size>
void CircularTaskQueue<buffer_size>::pop() 
{
    if(length <= 0)
        return;
    auto todelete = reinterpret_cast<TaskInterface*>(m_begin);
    std::size_t size = todelete->size();
    todelete->~TaskInterface(); // NOT using delete here since we constructed it IN PLACE!
    if(m_truncated != nullptr && m_begin+size >= m_truncated)
    {   // [buffer_begin] <-- 0~n --> [end] <-- 0~n --> [begin] <-- 0~{size} --> [truncated] <-- 0-unused --> [buffer_end]
        m_begin = m_buffer_begin;   // skip the trucated region
        m_truncated = nullptr;
    }
    else
        m_begin = m_begin + size;
    length--;
}

// disable a task in the queue, aka erase it
template<std::size_t buffer_size>
void CircularTaskQueue<buffer_size>::disable(const TaskInterface *ptr)
{
    const auto size = ptr->size();
    ptr->~TaskInterface();
    new((void*)ptr) DisabledTask(size); // in place new a DisableTask replacing the original one
}

// return the next TaskInterface's address in the buffer, considered truncated case
template<std::size_t buffer_size>
TaskInterface* CircularTaskQueue<buffer_size>::next(TaskInterface* ptr)
{
    char* tmp = reinterpret_cast<char*>(ptr) + ptr->size();
    if(this->getTruncated()!=nullptr && tmp>=this->getTruncated())
        tmp = this->getBufferBegin();
    return reinterpret_cast<TaskInterface*>(tmp);
}

#endif