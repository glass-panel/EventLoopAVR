#include <avr/interrupt.h>
#include <avr/io.h>
#include "../../include/EventLoopAVR.h"

#define CLOCK_FREQ 16000000
#define TIMER_PRESCALER 64

EventLoop<256> eventloop;   // create an eventloop with 256bytes queue
int64_t Time::s_offset = 0;     // offset to the real time in milliseconds

// timer1 interrupt, interrupt every 1ms
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    static int cnt = 0;
    if(cnt >= 1000) // reset cnt every 1000 interrupts
        cnt = 0;
    cnt++;
    Time::tick();   // call Time::tick() to update the current time
}


int cancelThis()
{
    eventloop.setTimeout(make_task(cancelThis), 60000);
    return 0;
}

int main()
{
    Time::absolute();   // init Time singleton
    
    TCCR1A = 0;                                 // set timer1 interrupt every 1ms
    TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);    // CTC mode: clear timer on compare match, pre-scaler=64
    OCR1A = CLOCK_FREQ/TIMER_PRESCALER/1000;    // compare match register: 1ms
    TIMSK1 = (1<<OCIE1A);                       // enable timer compare interrupt by setting bit OCIE1A in TIMSK1
    sei();                                      // enable interrupts

    eventloop.setTimeout([](int a, int b){
        int c = a+b;
        int d = a-b;
        return c*d;
    }, 2000, 1, 2);                             // set a task to be executed in 2 seconds, with arguments filled

    eventloop.setTimeout(cancelThis, 60000);
    eventloop.clearTimeout(cancelThis);  // cancel the timeout task by function poineter

    eventloop.run();    // let the eventloop run!
    // without the help of postQueueProcess, the eventloop will quit after the final timeout task is executed. 
    return 0;
}
