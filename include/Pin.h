#ifndef __PIN_H__
    #define __PIN_H__

#include <avr/io.h>
#include "no_stdcpp_lib.h"

/*
    SFRs are defined by interpret the address number as *(volatile uint8_t*)
    The complier sees it as a "non-constant" pointer, 
    because the pointer is generated from a none const expression (though it's a static number eventually) and doesn't have an address.
    So, it's not possible to use the SFRs in template's parameter directly.
    To achieve that, we need to use the wrapper struct to wrap the SFR to provide it an address.
    With the wrapper, we can use the SFR in template's parameter.

    related: https://arduino.stackexchange.com/questions/56304/how-do-i-directly-access-a-memory-mapped-register-of-avr-with-c
             https://www.tablix.org/~avian/blog/archives/2019/10/specializing_c_templates_on_avr_pins/
             https://zh.cppreference.com/w/cpp/language/template_parameters      
*/
struct sfr_wrapper
{
    volatile uint8_t& sfr;
    sfr_wrapper(volatile uint8_t& sfr_ref) : sfr(sfr_ref) {}
};

template<const sfr_wrapper& Wrapper, uint8_t Index>
struct PinT
{
static_assert(Index>=0 && Index<8, "Pin: Index must be in range [0, 7]");
    static volatile bool get() { return (Wrapper.sfr & (1<<Index)) != 0; }
    static void set(bool value) { if(value) Wrapper.sfr |= (1<<Index); else Wrapper.sfr &= ~(1<<Index); }
};

struct Pin
{
    volatile uint8_t& port;
    uint8_t index;
    Pin(volatile uint8_t& port, uint8_t index) : port(port), index(index) {}
    bool get() { return (port & (1<<index)) != 0; }
    void set(bool value) { if(value) port |= (1<<index); else port &= ~(1<<index); }
};

#endif