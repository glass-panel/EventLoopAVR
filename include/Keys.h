#ifndef __KEYS_H__
    #define __KEYS_H__

#include <cstdint>
#include "Pin.h"

constexpr uint16_t MaxDoubleClickInterval = 250;    // ms, max time between two clicks to be considered a double click
constexpr uint16_t MinPressDuration = 700;          // ms, min time to be considered a press

enum class KeyState : uint8_t
{
    Release     = 0b00000000,	// key is released
    Click       = 0b00000001,	// key is pressed and debounced
    DoubleClick = 0b00000010,	// key is pressed after realeased click
    Press       = 0b00000100,	// key is pressed, debounced and holded
    Debouncing  = 0b00010000,	// key is debouncing
    PreDoubleClick = 0b00100000,	// key is released after a single click
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


/*
    State Machine for a single key
    For readability,
    DO NOT assign a key_state with number directly, use the enum class KeyState instead
    And operate the state bit ONLY by the condition which it belongs to though some states will coincide with each other
*/
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
        key_cntms = 0;                                          // no need to count time when Pressing
    if(key_state == 0)                                          // Release and No WaitingDoubleClick
        key_cntms = 0;                                          // no need to count time when Released totally

    if((key_state==0 || key_state==(uint8_t)KeyState::PreDoubleClick) && pin_cur)            // [Release] -> [Debouncing]
    {
        key_state |= (uint8_t)KeyState::Debouncing;
        key_cntms = 0;                                          // reset counter for Debouncing
    }
    if(hasKeyState(KeyState::Debouncing, key_state) &&  pin_cur &&  key_cntms>=20) // [Debouncing] -> [Click]
    {   
        key_state &= ~(uint8_t)KeyState::Debouncing;            // cancel Debouncing state
        if(hasKeyState(KeyState::PreDoubleClick, key_state))
        {
            key_state |= (uint8_t)KeyState::DoubleClick;
            key_state &= ~(uint8_t)KeyState::PreDoubleClick;
        }
        key_state |= (uint8_t)KeyState::Click;						
        key_cntms = 0;                                          // reset counter for Click
    }
    if(hasKeyState(KeyState::Debouncing, key_state) && !pin_cur && key_cntms>=20) // [Debouncing] -> [Release]
    {   
        key_state &= ~(uint8_t)KeyState::Debouncing;            // cancel Debouncing state
        key_cntms = 0;                                          // reset counter for Release
    }
    if(hasKeyState(KeyState::PreDoubleClick, key_state) && !pin_cur && key_cntms > MaxDoubleClickInterval) // No further action, [PreDoubleClick] -> [Release] totally
        key_state &= ~(uint8_t)KeyState::PreDoubleClick;
    if(hasKeyState(KeyState::Click, key_state) && !pin_cur)     // [Click] -> [Release]
    {
        if(m_data[key_index].onClick && !hasKeyState(KeyState::Press, key_state) && !hasKeyState(KeyState::DoubleClick, key_state))  // release->press->release: onClick
            m_data[key_index].onClick(key_index);
        key_state &= ~(uint8_t)KeyState::Click;                 // clear Click state        
        key_state |= (uint8_t)KeyState::PreDoubleClick;         // SingleCilckRelease(Release after Clicked)
        key_cntms = 0;                                          // reset counter for Press
    }
    if(hasKeyState(KeyState::DoubleClick, key_state) && !pin_cur)   // [DoubleClick] -> [Release]
    {
        if(m_data[key_index].onDoubleClick && !hasKeyState(KeyState::Press, key_state))
            m_data[key_index].onDoubleClick(key_index);
        key_state &= ~(uint8_t)KeyState::DoubleClick;           // clear DoubleClick state
        key_state &= ~(uint8_t)KeyState::PreDoubleClick;        // no PreDoubleClick after DoubleClick
        key_cntms = 0;                                          // reset counter for Press
    }
    if(hasKeyState(KeyState::Press, key_state) && !pin_cur)     // [Press] -> [Release]
    {
        if(m_data[key_index].onPress)
            m_data[key_index].onPress(key_index);
        key_state &= ~(uint8_t)KeyState::Press;                 // clear Press state
        key_state &= ~(uint8_t)KeyState::PreDoubleClick;        // no PreDoubleClick after Press
        key_cntms = 0;                                          // reset counter for Press
    }
    if(hasKeyState(KeyState::Click, key_state) && pin_cur && key_cntms>=MinPressDuration) // [Click] -> [Press]                           
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