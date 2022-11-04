#ifndef __KEYS_H__
    #define __KEYS_H__

#ifdef USE_STDCPP_LIB
    #include <tuple>
    #include <cstdint>
    #include <utility>
#else
    #include "no_stdcpp_lib.h"
#endif
#include "Pin.h"
#include "Task.h"

constexpr uint16_t MaxDoubleClickInterval = 250;    // ms, max time between two clicks to be considered a double click
constexpr uint16_t MinPressDuration = 700;          // ms, min time to be considered a press

enum class KeyState : uint8_t
{
    Release         = 0,	// key is released
    Click           = 1,	// key is pressed and debounced
    DoubleClick     = 2,	// key is pressed after realeased click
    Press           = 4,	// key is pressed, debounced and holded
    Debouncing      = 8,	// key is debouncing
    PreDoubleClick  = 16,	// key is released after a single click

    OnClickFlag     = 32,
    OnDbClickFlag   = 64,
    OnPressFlag     = 128,
};

inline bool hasKeyState(const KeyState state, const uint8_t cur_state)
{
    if((cur_state&0x0F)==0 && state==KeyState::Release)
        return true;
    return ((uint8_t)state & cur_state) > 0;
}

/*
    KeyData: contains the data of one key
    Be aware: The event handler is executed instantly, not pushed to the event queue. 
    It's upon you to call eventloop.nextTick() in the event handler
*/
class KeyBase
{
protected:
    uint8_t m_state = 0;
    uint16_t m_cntms = 0;
public:
    using EventHandler = TaskInterface*;
    // event handlers
    EventHandler onClick = nullptr;
    EventHandler onDoubleClick = nullptr;
    EventHandler onPress = nullptr;
    
    uint8_t state() const { return m_state; }

    void executeHandlers()
    {
        if(hasKeyState(KeyState::OnClickFlag, m_state))
        {
            if(onClick)
                onClick->exec();
            m_state &= ~(uint8_t)KeyState::OnClickFlag;
        }
        if(hasKeyState(KeyState::OnDbClickFlag, m_state))
        {
            if(onDoubleClick)
                onDoubleClick->exec();
            m_state &= ~(uint8_t)KeyState::OnDbClickFlag;
        }
        if(hasKeyState(KeyState::OnPressFlag, m_state))
        {
            if(onPress)
                onPress->exec();
            m_state &= ~(uint8_t)KeyState::OnPressFlag;
        }
    }
};

template<typename Pin>
class Key : public KeyBase
{
public:
    constexpr Key() {};
    Key(Key&&) = delete;
    Key(const Key&) = delete;
    Key& operator=(Key&&) = delete;
    Key& operator=(const Key&) = delete;
    
    uint8_t state() const { return m_state; }

    void updateState(uint16_t passed_ms)
    {
        const bool pin_cur = Pin::get();
        m_cntms += passed_ms;
        if(hasKeyState(KeyState::Press, m_state))
            m_cntms = 0;                                          // no need to count time when Pressing
        if((m_state&0x1F) == 0)                             // Release and No WaitingDoubleClick
            m_cntms = 0;                                          // no need to count time when Released totally

        if(hasKeyState(KeyState::Release, m_state) && pin_cur)            // [Release] -> [Debouncing]
        {
            m_state |= (uint8_t)KeyState::Debouncing;
            m_cntms = 0;                                          // reset counter for Debouncing
        }
        if(hasKeyState(KeyState::Debouncing, m_state) && pin_cur && m_cntms>=20) // [Debouncing] -> [Click]
        {   
            m_state &= ~(uint8_t)KeyState::Debouncing;            // cancel Debouncing state
            if(hasKeyState(KeyState::PreDoubleClick, m_state))
            {
                m_state |= (uint8_t)KeyState::DoubleClick;
                m_state &= ~(uint8_t)KeyState::PreDoubleClick;
            }
            m_state |= (uint8_t)KeyState::Click;						
            m_cntms = 0;                                          // reset counter for Click
        }
        if(hasKeyState(KeyState::Debouncing, m_state) && !pin_cur && m_cntms>=20) // [Debouncing] -> [Release]
        {   
            m_state &= ~(uint8_t)KeyState::Debouncing;            // cancel Debouncing state
            m_cntms = 0;                                          // reset counter for Release
        }
        if(hasKeyState(KeyState::PreDoubleClick, m_state) && !pin_cur && m_cntms > MaxDoubleClickInterval) // No further action, [PreDoubleClick] -> [Release] totally
            m_state &= ~(uint8_t)KeyState::PreDoubleClick;
        if(hasKeyState(KeyState::Click, m_state) && !pin_cur)     // [Click] -> [Release]
        {
            m_state |= (uint8_t)KeyState::OnClickFlag;
            m_state &= ~(uint8_t)KeyState::Click;                 // clear Click state        
            m_state |= (uint8_t)KeyState::PreDoubleClick;         // SingleCilckRelease(Release after Clicked)
            m_cntms = 0;                                          // reset counter for Press
        }
        if(hasKeyState(KeyState::DoubleClick, m_state) && !pin_cur)   // [DoubleClick] -> [Release]
        {
            m_state |= (uint8_t)KeyState::OnDbClickFlag;
            m_state &= ~(uint8_t)KeyState::DoubleClick;           // clear DoubleClick state
            m_state &= ~(uint8_t)KeyState::PreDoubleClick;        // no PreDoubleClick after DoubleClick
            m_cntms = 0;                                          // reset counter for Press
        }
        if(hasKeyState(KeyState::Press, m_state) && !pin_cur)     // [Press] -> [Release]
        {
            m_state &= ~(uint8_t)KeyState::Press;                 // clear Press state
            m_state &= ~(uint8_t)KeyState::PreDoubleClick;        // no PreDoubleClick after Press
            m_cntms = 0;                                          // reset counter for Press
        }
        if(hasKeyState(KeyState::Click, m_state) && pin_cur && m_cntms>=MinPressDuration) // [Click] -> [Press]                           
        {   
            m_state |= (uint8_t)KeyState::OnPressFlag;
            m_state |= (uint8_t)KeyState::Press;
            m_cntms = 0;
        }
    }

};

/*
    Keys: holds the data of the keys specified by the PinT<>s in the template,
    and provides state update functions to them.
*/
template<typename ...Pins>
class Keys : private Key<Pins>...
{
private:
    KeyBase* m_refs[sizeof...(Pins)];

#if __cplusplus < 201703L
    template<std::size_t I, std::size_t... Is>
    void updateStateRecrusive(uint16_t passed_ms, std::tuple<Key<Pins>*...>& selfptr, std::index_sequence<I, Is...>)
    {
        std::get<sizeof...(Is)>(selfptr)->updateState(passed_ms);
        updateStateRecrusive(passed_ms, selfptr, std::make_index_sequence<sizeof...(Is)>{});
    }

    template<std::size_t I=0>
    void updateStateRecrusive(uint16_t passed_ms, std::tuple<Key<Pins>*...>& selfptr, std::index_sequence<I>)
    {
        std::get<I>(selfptr)->updateState(passed_ms);
    }
#endif

public:
    static constexpr uint8_t KeyNumber = sizeof...(Pins);
    Keys() : Key<Pins>()..., m_refs{static_cast<KeyBase*>(static_cast<Key<Pins>*>(this))...} {}
    Keys(Keys&&) = delete;
    Keys(const Keys&) = delete;
    Keys& operator=(Keys&&) = delete;
    Keys& operator=(const Keys&) = delete;

    void updateState(uint16_t passed_ms)
    {
#if __cplusplus >= 201703L
        ( this->Key<Pins>::updateState(passed_ms) , ... );
#else
        auto tmp = std::make_tuple(static_cast<Key<Pins>*>(this)...);
        updateStateRecrusive(passed_ms, tmp, std::make_index_sequence<sizeof...(Pins)>{});
#endif
    }

    void executeHandlers()
    {
#if __cplusplus >= 201703L
        ( this->Key<Pins>::executeHandlers() , ... );
#else
        for(uint8_t i=0; i<sizeof...(Pins); i++)
            m_refs[i]->executeHandlers();
#endif
    }

    KeyBase& operator[](std::size_t index)
    {
        return *m_refs[index];
    }
};

#endif