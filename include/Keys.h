#ifndef __KEYS_H__
    #define __KEYS_H__

#include <cstdint>
#include "Pin.h"

enum class KeyState : uint8_t
{
    Release     = 0b00000000,	// key is released
    Click       = 0b00000001,	// key is pressed and debounced
    DoubleClick = 0b00000010,	// key is pressed after realeased click
    Press       = 0b00000100,	// key is pressed, debounced and holded
    Debouncing  = 0b00010000,	// key is debouncing
    WaitingDoubleClickA = 0b00100000,	// key is released after a single click
    WaitingDoubleClickB = 0b01000000,	// key is pressed afeter WaitingDoubleClickA
};

inline bool hasKeyState(const KeyState state, const uint8_t cur_state)
{
    if(cur_state==0 && state==KeyState::Release)
        return true;
    return ((uint8_t)state & cur_state) > 0;
}

/*
    KeyData: contains the data of one key
    Be aware: The event handler is executed instantly, not pushed to the event queue. 
    It's upon you to call eventloop.nextTick() in the event handler
*/
struct KeyData
{
    using EventHandler = void(*)(const uint8_t);
    uint8_t state = 0;
    uint16_t cntms = 0;
    // event handlers
    EventHandler onClick = nullptr;
    EventHandler onDoubleClick = nullptr;
    EventHandler onPress = nullptr;
};

/*
    Keys: holds the data of the keys specified by the PinT<>s in the template,
    and provides state update functions to them.
*/
template<typename ...Pins>
class Keys
{
private:
    KeyData m_data[sizeof...(Pins)];

    template<typename Pa, typename ...Ps>
    inline void updateStateSingle(const uint16_t ms);
public:
    void updateState(uint16_t passedms)
    {
        updateStateSingle<Pins...>(passedms);
    }
    
    KeyData& operator[](std::size_t index)
    {
        return m_data[index];
    }
};


template<typename ...Pins>
template<typename Pa, typename ...Ps>							    // when compiler optimization is enabled, this function will expand the recursion
inline void Keys<Pins...>::updateStateSingle(const uint16_t ms)		// update the state of a single key by recrusive template function
{
    constexpr std::size_t key_index = sizeof...(Pins)-sizeof...(Ps)-1;
    uint8_t &key_state = m_data[key_index].state;
    uint16_t &key_cntms = m_data[key_index].cntms;
    const bool pin_cur = Pa::get();

    key_cntms += ms;
	if(hasKeyState(KeyState::Press, key_state))
        key_cntms = 0;                                              // no need to count time when Pressing
    if(key_state == 0)                                              // Release and No WaitingDoubleClick
        key_cntms = 0;                                              // no need to count time when Released totally

    if(hasKeyState(KeyState::Release, key_state) && pin_cur)            // [Release] -> [Debouncing]
    {   
        if( hasKeyState(KeyState::WaitingDoubleClickA, key_state)  && key_cntms<=50 ) // [WaitingDoubleClickA](Release after Clicked) -> [WaitingDoubleClickB](Debouncing or Release)                                      
        {   
            key_state &= ~(uint8_t)KeyState::WaitingDoubleClickA;   // cancel WaitingDoubleClickA state
            key_state |= (uint8_t)KeyState::WaitingDoubleClickB;
        }   
        key_state |= (uint8_t)KeyState::Debouncing;
        key_cntms = 0;                                              // reset counter for Debouncing
    }
    if(hasKeyState(KeyState::Debouncing, key_state) &&  pin_cur &&  key_cntms>=20) // [Debouncing] -> [Click]
    {   
        if(hasKeyState(KeyState::WaitingDoubleClickB, key_state))       // [WaitingDoubleClickB](Debouncing or Release) -> [DoubleClick]
        {   
            key_state &= ~(uint8_t)KeyState::WaitingDoubleClickB;	// cancel WaitingDoubleClickB state
            key_state |= (uint8_t)KeyState::DoubleClick;			
        }
        key_state &= ~(uint8_t)KeyState::Debouncing;                // cancel Debouncing state
        key_state |= (uint8_t)KeyState::Click;						
        key_cntms = 0;                                              // reset counter for Click
    }
    if(hasKeyState(KeyState::Debouncing, key_state) && !pin_cur && key_cntms>=20) // [Debouncing] -> [Release]
    {   
        key_state &= ~(uint8_t)KeyState::Debouncing;				// cancel Debouncing state
        key_state |= (uint8_t)KeyState::Release;					
        key_cntms = 0;                                              // reset counter for Release
    }
    if(hasKeyState(KeyState::WaitingDoubleClickA, key_state) && !pin_cur && key_cntms > 50 ) // No further action, [WaitingDoubleClickA] -> [Release] totally
        key_state &= ~(uint8_t)KeyState::WaitingDoubleClickA;       
    if(hasKeyState(KeyState::Click, key_state) && !pin_cur)             // [Click] -> [Release]
    {
        if(m_data[key_index].onClick && !hasKeyState(KeyState::Press, key_state) && !hasKeyState(KeyState::DoubleClick, key_state) )  // release->press->release: onClick
            m_data[key_index].onClick(key_index);
        if(hasKeyState(KeyState::Press, key_state))
            key_state &= ~(uint8_t)KeyState::Press;                 // Press -> Release
        key_state &= ~(uint8_t)KeyState::Click;                     // cancel Click state
        key_state |= (uint8_t)KeyState::Release;
        key_state |= (uint8_t)KeyState::WaitingDoubleClickA;        // WaitingDoubleClickA(Release after Clicked)
        key_cntms = 0;                                              // reset counter for Press
    }
	if(hasKeyState(KeyState::DoubleClick, key_state) && !pin_cur)		// [DoubleClick] -> [Release]
	{
        if(m_data[key_index].onDoubleClick)
            m_data[key_index].onDoubleClick(key_index);
		key_state &= ~(uint8_t)KeyState::DoubleClick;				// cancel DoubleClick state
		key_state &= ~(uint8_t)KeyState::WaitingDoubleClickA;		// cancel WaitingDoubleClickA state
	}
    if( hasKeyState(KeyState::Click, key_state) && pin_cur && key_cntms>=500 ) // [Click] -> [Press]                           
    {   
        if(m_data[key_index].onPress)
            m_data[key_index].onPress(key_index);
        key_state |= (uint8_t)KeyState::Press;
        key_cntms = 0;
    }

    // TIME FOR MAGIC!
    if constexpr (sizeof...(Ps) > 0)
        updateStateSingle<Ps...>(ms);
}

#endif