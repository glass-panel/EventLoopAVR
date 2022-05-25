#ifndef __EVENTLOOPAVR_H__
    #define __EVENTLOOPAVR_H__

/*
    Headers below must be independent from global_variables.h also platform dependent.
    And declare no non-static global variables except Time Singleton.
*/
#include "Pin.h"
#include "Keys.h"
#include "EventLoop.h"
#include "PipeIO.h"
#include "Time.h"

#endif