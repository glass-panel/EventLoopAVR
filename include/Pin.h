#ifndef __PIN_H__
    #define __PIN_H__

#include "no_stdcpp_lib.h"
#include <stdint.h>

template<intptr_t Address, uint8_t Index>
struct PinT
{
static_assert(Index>=0 && Index<8, "Pin: Index must be in range [0, 7]");
    static volatile bool get() { return (*(volatile uint8_t*)Address & (1<<Index)) != 0; }
    static void set(bool value) 
    { 
        if(value) 
            *(volatile uint8_t*)Address |= (1<<Index); 
        else 
            *(volatile uint8_t*)Address &= ~(1<<Index); 
    }
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