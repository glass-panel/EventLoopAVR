#include <avr/interrupt.h>
#include <avr/io.h>
#include "../../include/EventLoopAVR.h"

#define CLOCK_FREQ 16000000
#define TIMER_PRESCALER 64

/*
    Notice here:
    To use PinT<>, we need to know the sfr's address.
    But due to compiler's bug(?), &(*(volatile char*)sfr_address) won't be treated as constexpr
    So we must declare a variable to provide indirect address to template
*/
const intptr_t PINB_ADDRESS = (intptr_t)&PINB,
               PORTC_ADDRESS = (intptr_t)&PORTC;

Keys<PinT<PINB_ADDRESS, 0>, PinT<PINB_ADDRESS, 1>> keys;  // bind PINB.0 to keys[0] and PINB.1 to keys[1]

EventLoop<256> eventloop;                       // create an eventloop with 256bytes queue
int64_t Time::s_offset = 0;                     // offset to the real time in milliseconds


const EventLoopHelperFunctions helper_functions{
    [](uint16_t totalTaskCount){
        static Time prev = 0;
        Time now = Time::absolute();
        uint16_t passed_ms = now - prev;
        if(passed_ms >= 10)
        {
            keys.executeHandlers();             // execute any key events callback if it has
        }
        return (uint8_t)0;
    },
    [](uint16_t totalTaskCount){                // postQueueProcess
        if(!totalTaskCount)
            eventloop.nextTick(make_task([](){}));  // push an empty task to the queue to keep the eventloop running
        return (uint8_t)0;
    },
};

// timer1 interrupt, interrupt every 1ms
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    static int cnt = 0;
    if(cnt >= 1000)                             // reset cnt every 1000 interrupts
        cnt = 0;
    cnt++;
    Time::tick();                               // call Time::tick() to update the current time
    keys.updateState(1);                        // tell the key state machine how long has been passed
}

void reversePinEach1s(bool prev)
{
    PinT<PORTC_ADDRESS, 0>::set(!prev);               // reverse the state of PORTC.0, according to the previous state
    eventloop.setTimeout(reversePinEach1s, 1000, !prev);   // keep calling itself every 1s
}

int main()
{
    Time::absolute();                           // init Time singleton
    
    TCCR1A = 0;                                 // set timer1 interrupt every 1ms
    TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);    // CTC mode: clear timer on compare match, pre-scaler=64
    OCR1A = CLOCK_FREQ/TIMER_PRESCALER/1000;    // compare match register: 1ms
    TIMSK1 = (1<<OCIE1A);                       // enable timer compare interrupt by setting bit OCIE1A in TIMSK1
    sei();                                      // enable interrupts

    DDRB = 0;                                   // set port B to input
    DDRC = 0xff;                                // set port C to output

    eventloop.setHelperFunctions(&helper_functions); 
    eventloop.bindEventHandler(keys[0].onClick, [](){                       // set onClick callback for keys[0]
        eventloop.nextTick(make_task(reversePinEach1s).setArgs({false}));   // start to reverse PORTC.0 state every 1s
    });
    eventloop.bindEventHandler(keys[1].onDoubleClick, [](){                 // set onDoubleClick callback for keys[1]
        eventloop.clearTimeout(reversePinEach1s);                    // stop reversing PORTC.0 state
    });

    eventloop.run();                            // let the eventloop run!
    
    return 0;
}
