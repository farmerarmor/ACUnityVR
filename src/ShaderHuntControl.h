#pragma once
#include <cstddef>
#include <cstdint>
namespace ShaderHuntControl {
enum Key : uint32_t { End=1, Home=2, Next=4, Previous=8, Delete=16 };
inline constexpr size_t None=static_cast<size_t>(-1);
struct State {
    bool enabled{};size_t count{},selected=None;uint32_t held{};
    void Add(){++count;} // New shaders never change this frame's selection.
    uint32_t Poll(uint32_t down){
        uint32_t pressed=down&~held;held=down;
        if(pressed&Home){enabled=false;count=0;selected=None;return Home;}
        if(pressed&End)enabled=true;
        if(enabled && count && selected==None)selected=0;
        if(enabled && count){
            if(pressed&Next)selected=(selected+1)%count;
            if(pressed&Previous)selected=(selected+count-1)%count;
        }
        return enabled?pressed:0;
    }
};
}
