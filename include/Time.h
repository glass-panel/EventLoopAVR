#ifndef __TIME_H__
    #define __TIME_H__

#include <cstdint>
#include "compile_time.h"
/*
    In order to provide time for functions that only care about ms,
    and the functions that care about a long time also reduce the size,
    We use uint32 to represent the minutes passed from 1970-01-01 00:00:00.
    And an uint16 to represent the microseconds, so it's max is 60000, just below uint16's limits. 
*/

/*  
    Time Singleton: Provide the type to represent time, 
    and holds the current time from booted up and offset to the real time
*/
class Time
{
private:
    uint32_t m_minutes;
    uint16_t m_microseconds;

    inline static int64_t s_offset = __TIME_UNIX__*1000LL;    // offset to the real time in ms, default is the compile time
    static Time& getInstance() { static Time self(0); return self; }   // need -fno-threadsafe-statics flag to compile, we dont need thread-safe in avr anyway
public:
    Time() : m_minutes(0), m_microseconds(0) {}
    Time(uint64_t t) : m_minutes(t/60000), m_microseconds(t%60000) {}
    
    operator uint64_t() { return m_minutes*60000 + m_microseconds; }

    static Time absolute() { return getInstance(); }
    static Time now() { return getInstance() + s_offset; }
    static int64_t getOffset() { return s_offset; }
    static void setOffset(int64_t offset) { s_offset = offset; }

    // note: the absolute time cannot be modified except by timer ISR, 
    // and the timer ISR should only increase the absolute time by tick()
    static void tick(int16_t ms=1) { getInstance() = getInstance() + ms; }
    
    uint32_t getMinutes() { return m_minutes; }
    uint16_t getMicroseconds() { return m_microseconds; }
};

#endif