#ifndef __TIME_H__
    #define __TIME_H__

#ifdef USE_STDCPP_LIB
    #include <tuple>
    #include <cstdint>
#else
    #include "no_stdcpp_lib.h"
#endif

/*  
    Time Singleton: Provide the type to represent time, 
    and holds the current time from booted up and offset to the real time
*/
class Time
{
private:
    uint32_t m_1;
    uint16_t m_2;

    static int64_t s_offset;    // offset to the real time in ms
    static Time& getInstance() { static Time self(0); return self; }   // need -fno-threadsafe-statics flag to compile, we dont need thread-safe in avr anyway
public:
    Time() : m_1(0), m_2(0) {}
    Time(uint64_t t) : m_1(t>>16), m_2(t&0xFFFF) {}
    
    operator uint64_t() { return ((uint64_t)m_1<<16) + m_2; }

    static Time absolute() { return getInstance(); }
    static Time now() { return getInstance() + s_offset; }
    static int64_t getOffset() { return s_offset; }
    static void setOffset(int64_t offset) { s_offset = offset; }

    // note: the absolute time cannot be modified except by timer ISR, 
    // and the timer ISR should only increase the absolute time by tick()
    static void tick(int16_t ms=1) { getInstance() = getInstance() + ms; }
    
    // from unix timestamp to civil date. reference: http://howardhinnant.github.io/date_algorithms.html
    
    std::tuple<uint16_t, uint8_t, uint8_t> getDate()
    {
        const uint64_t days = *this/1000/60/60/24 + 719468;   // days since 0000-03-01
        const uint8_t eras = days/146097;                     // eras since 0000-03-01
        const uint32_t day_of_eras = days%146097;             // nth day of this era [0, 146096]
        const uint16_t year_of_eras = (day_of_eras - day_of_eras/1460 + day_of_eras/36524 - day_of_eras/146096)/365; // nth year of this era [0, 399]
        const uint16_t year = year_of_eras + 400*eras;        // current year
        const uint16_t day_of_year = day_of_eras - (365*year_of_eras + year_of_eras/4 - year_of_eras/100); // nth day of this year [0, 365]
        const uint8_t month_p = (5*day_of_year + 2)/153;        // [0, 11]
        const uint8_t day = day_of_year - (153*month_p + 2)/5 + 1; // nth day of this month [1, 31]
        const uint8_t month = month_p + (month_p>9 ? -9 : 3); // current month [1, 12]
        return std::make_tuple(year, month, day);
    }

    uint8_t getWeekday()    // [Sun 0, Sat 6] 
    {
        const uint32_t days = *this/1000/60/60/24;  // days since 1970-01-01
        return (days+4)%7;
    }

    uint8_t getNthWeek()
    {
        const uint64_t days = *this/1000/60/60/24 + 719468;   // days since 0000-03-01
        const uint32_t day_of_eras = days%146097;             // nth day of this era [0, 146096]
        const uint16_t year_of_eras = (day_of_eras - day_of_eras/1460 + day_of_eras/36524 - day_of_eras/146096)/365; // nth year of this era [0, 399]
        const uint16_t day_of_year = day_of_eras - (365*year_of_eras + year_of_eras/4 - year_of_eras/100); // nth day of this year [0, 365]
        return day_of_year/7 + 1;
    }

    uint8_t getHours()
    {
        return (*this/1000/60/60)%24;
    }

    uint8_t getMinutes()
    {
        return (*this/1000/60)%60;
    }

    uint8_t getSeconds()
    {
        return (*this/1000)%60;
    }

};

#endif