#ifndef __GLOBAL_VARIABLES_H__
    #define __GLOBAL_VARIABLES_H__

#include <avr/io.h>
#include <cstdint>
#include <cstdlib>

#include "EventLoopAVR.h"
#include "Uart.h"

#define TIMER1_DIV 64
#define CLOCK_FRQ 16000000UL
#define BAUD_RATE 9600

// declare the wrapper struct for each port, using C++17's inline variable declaration
inline static const sfr_wrapper PORTBW(PORTB), PORTCW(PORTC), PORTDW(PORTD),
                                PINBW(PINB),   PINCW(PINC),   PINDW(PIND);

inline void do_nothing() {}

inline void operator delete(void *ptr, std::size_t size)
{
    free(ptr);
}

inline void operator delete[](void *ptr, std::size_t size)
{
    free(ptr);
}

extern void timer1_init();
using TypeKeys = Keys<PinT<PINCW, 1>, PinT<PINCW, 2>>;
extern TypeKeys keys;
using TypeEventLoop = EventLoop<256, 16>;
extern TypeEventLoop eventloop;

extern void uartRecieveTimeout();
extern void onUartData(PipeUart*, char*);
extern void commandHandler(PipeUart*);
extern void (*oncommand)(PipeUart*);

#endif