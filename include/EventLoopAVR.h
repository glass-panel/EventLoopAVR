#ifndef __EVENTLOOPAVR_H__
    #define __EVENTLOOPAVR_H__

/* If you planned to use the full c++ standard library instead of this crappy one, uncomment the next line */
//#define USE_STDCPP_LIB

/*
    Headers below must be platform dependent.
    And declare no non-static global variables except Time Singleton.
*/
#include "Pin.h"
#include "Keys.h"
#include "EventLoop.h"
#include "PipeIO.h"
#include "Time.h"

#endif