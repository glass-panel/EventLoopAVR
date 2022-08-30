#ifndef __PIPEIO_H__
    #define __PIPEIO_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum class PipeIOFlags : uint8_t
{
    NONE    = 0b00000000,
    ONDATA  = 0b00000001,
    ONFULL  = 0b00000010,
    RECVBUSY= 0b00000100,
};

using BlockingSendByteFunc = void (*)(char);
template<BlockingSendByteFunc Func>
class PipeIO
{
private:
    char *m_buffer = nullptr;
    char *m_prev = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_length = 0;
    uint8_t m_flags = 0;
public:
    PipeIO(char *buf, std::size_t capacity) : 
    m_buffer(buf), 
    m_prev(buf),
    m_capacity(capacity)
    {}
    // events callbacks
    void (*onData)(PipeIO*, char*) = nullptr;   // ondata callback is expected to reset the ondata flag
    void (*onFull)(PipeIO*) = nullptr;  // onfull callback is expected to clear the buffer and reset the onfull flag

    void checkEvents();

    // properties functions
    char *buffer() { return m_buffer; }
    std::size_t capacity() { return m_capacity; }
    std::size_t length() { return m_length; }
    uint8_t& flags() { return m_flags; }

    // output functions
    void sendByte(char c) { Func(c); }
    void sendString(const char *str);
    void sendInt32(int32_t number, bool hex=false);
    void sendInt64(int64_t number, bool hex=false);
    void sendFloat(float number, uint8_t decimals=2);
    PipeIO<Func>& operator<<(const char c) { sendByte(c); return *this; }
    PipeIO<Func>& operator<<(bool b) { sendByte(b+'0'); return *this; }
    PipeIO<Func>& operator<<(const char *str) { sendString(str); return *this; }
    PipeIO<Func>& operator<<(int32_t number) { sendInt32(number); return *this; }
    PipeIO<Func>& operator<<(int64_t number) { sendInt64(number); return *this; }
    PipeIO<Func>& operator<<(float number) { sendFloat(number); return *this; }
    PipeIO<Func>& operator<<(void* address) { sendInt32((int32_t)address, true); return *this; }
    
    // input functions
    bool buffer_push(char c);
    char buffer_pop();
    void buffer_clear();
};


template<BlockingSendByteFunc Func>
void PipeIO<Func>::sendString(const char *str)
{ 
    while(*str) 
        sendByte(*str++); 
}

template<BlockingSendByteFunc Func>
void PipeIO<Func>::sendInt32(int32_t number, bool hex)
{
    if(number < 0)
    {
        sendByte('-');
        number = -number;
    }
    if(hex)
        sendString("0x");
    char buffer[12] = {0};
    char *ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    do
    {
        *--ptr = "0123456789ABCDEF"[number % (hex? 16:10)];
        number /= 10;
    } while(number);
    sendString(ptr);
}

template<BlockingSendByteFunc Func>
void PipeIO<Func>::sendInt64(int64_t number, bool hex)
{
    if(number < 0)
    {
        sendByte('-');
        number = -number;
    }
    if(hex)
        sendString("0x");
    char buffer[24] = {0};
    char *ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    do
    {
        *--ptr = "0123456789ABCDEF"[number % (hex? 16:10)];
        number /= 10;
    } while(number);
    sendString(ptr);
}

template<BlockingSendByteFunc Func>
void PipeIO<Func>::sendFloat(float number, uint8_t decimals)
{
    sendInt32((int32_t)number);
    int32_t tens = 1;
    number = number<0? -number:number;
    for(int i=0; i<decimals; i++)
    {
        tens *= 10;
        number *= 10;
    }
    sendByte('.');
    sendInt32((int32_t)number%tens);
}

template<BlockingSendByteFunc Func>
bool PipeIO<Func>::buffer_push(char c)
{
    m_flags |= (uint8_t)PipeIOFlags::ONDATA;
    if(m_length < m_capacity)
    {
        m_buffer[m_length++] = c;
        return true;
    }
    else
    {
        m_flags |= (uint8_t)PipeIOFlags::ONFULL;
        return false;
    }
}

template<BlockingSendByteFunc Func>
char PipeIO<Func>::buffer_pop()
{
    if(m_length > 0)
    {
        char c = m_buffer[m_length-1];
        m_buffer[m_length-1] = '\0';
        m_length--;
        return c;
    }
    else
        return 0;
}

template<BlockingSendByteFunc Func>
void PipeIO<Func>::buffer_clear()
{
    m_length = 0;
    memset(m_buffer, 0, m_capacity);
    m_flags = 0;
}

template<BlockingSendByteFunc Func>
void PipeIO<Func>::checkEvents()
{
    if(m_flags & (uint8_t)PipeIOFlags::ONDATA && onData)
    {
        onData(this, m_prev);
        m_prev = m_buffer+m_length; // update the previous pointer
    }
    if(m_flags & (uint8_t)PipeIOFlags::ONFULL && onFull)
    {
        onFull(this);
    }
}

#endif