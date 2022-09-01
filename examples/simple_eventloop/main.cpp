#include "../../include/EventLoopAVR.h"

EventLoop<256> eventloop;   // create an eventloop with 256bytes queue

const EventLoopHelperFunctions helper_functions = { // helper functions for eventloop
    nullptr,                                        // preQueueProcess
    [](uint16_t totalTaskCount){                    // postQueueProcess
        if(!totalTaskCount)
            eventloop.nextTick([](){});             // push an empty task to the queue to keep the eventloop running
        return (uint8_t)0;
    },
    nullptr,                                        // onTaskAllocationFailed
};

void everytime(int a, int b)    // an useless function
{
    int c = a+b;
    int d = a-b;
    eventloop.nextTick(everytime, d, c);    // push itself back to the eventloop again, with different arguments
}

int main()
{
    //eventloop.setHelperFunctions(&helper_functions);
    eventloop.nextTick(make_task(everytime).setArgs(std::make_tuple(1, 2)));   // give a task to the eventloop, with arguments filled
    // Be aware:
    // we haven't provide any clock source for Time::tick(), 
    // so the eventloop cannot handle the setTimeout() or similar functions

    eventloop.run();    // let the eventloop run!

    return 0;
}
