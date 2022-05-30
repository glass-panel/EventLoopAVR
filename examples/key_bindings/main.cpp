// ONLY SUPPORT BY C++17 OR HIGHER!

#include <avr/interrupt.h>
#include <avr/io.h>
#include "../../include/EventLoopAVR.h"

#define CLOCK_FREQ 16000000
#define TIMER_PRESCALER 64

static const sfr_wrapper PORTB_W(PORTB), PORTC_W(PORTC);  // warp the ports for template usage
Keys<PinT<PORTB_W, 1>, PinT<PORTB_W, 2>> keys;  // bind PORTB.1 to keys[0] and PORTB.2 to keys[1]
EventLoop<256, 24> eventloop;   // create an eventloop with 256bytes task queue size and 24 timeout slots
int64_t Time::s_offset = 0;     // offset to the real time in milliseconds

const static EventLoopHelperFunctions helper_functions = {
    [](uint16_t totalTaskCount){
        static Time prev = 0;
        Time now = Time::absolute();
        uint16_t passedms = now - prev;
        if(passedms >= 10)
            keys.updateState(passedms); // tell the state machine how long has been passed, recommeded interval is 10ms
        return (uint8_t)0;
    },
    [](uint16_t totalTaskCount){    // postQueueProcess
        if(!totalTaskCount)
            eventloop.nextTick(make_task([](){}));  // push an empty task to the queue to keep the eventloop running
        return (uint8_t)0;
    },
    nullptr,
    nullptr
};

// timer1 interrupt, interrupt every 1ms
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    static int cnt = 0;
    if(cnt >= 1000) // reset cnt every 1000 interrupts
        cnt = 0;
    cnt++;
    Time::tick();   // call Time::tick() to update the current time
}

void reversePinEach1s(bool prev)
{
    PinT<PORTC_W, 0>::set(!prev);   // reverse the state of PORTC.0, according to the previous state
    eventloop.setTimeout(make_task(reversePinEach1s).setArgs({!prev}), 1000);   // keep calling itself every 1s
}

int main()
{
    Time::absolute();   // init Time singleton
    // set timer1 interrupt every 1ms
    TCCR1A = 0;
    TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);    // CTC mode: clear timer on compare match, pre-scaler=64
    OCR1A = CLOCK_FREQ/TIMER_PRESCALER/1000;         // compare match register: 1ms
    TIMSK1 = (1<<OCIE1A);                    // enable timer compare interrupt by setting bit OCIE1A in TIMSK1
    sei();                                   // enable interrupts

    // set port B to input
    DDRB = 0;
    // set port C to output
    DDRC = 0xff;

    eventloop.setHelperFunctions(&helper_functions); 
    keys[0].onClick = [](uint8_t key_idx){  // set onClick callback for keys[0]
        eventloop.nextTick(make_task(reversePinEach1s).setArgs({false}));   // start to reverse PORTC.0 state every 1s
    };

    keys[1].onDoubleClick = [](uint8_t key_idx){    // set onDoubleClick callback for keys[1]
        eventloop.clearTimeout((void*)reversePinEach1s);    // stop reversing PORTC.0 state
    };

    eventloop.run();    // let the eventloop run!
    
    return 0;
}
