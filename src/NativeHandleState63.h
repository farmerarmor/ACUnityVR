#pragma once
#include <cstdint>
// Exact-build native entity+130 handle: +c low24 bits are reference count.
// Native 1cb635a increments; 1c83ab8 decrements then tests 00ffffff.
// Preserve upper flags and live/backlink checks; never modify/pin references.
namespace NativeHandleState63 {
inline bool Live(uint32_t state) noexcept {return (state & 0x80000000u)!=0 && (state & 0x00ffffffu)!=0;}
inline bool SameLifetime(uint32_t a,uint32_t b) noexcept {return Live(a) && Live(b) && ((a^b)&0xff000000u)==0;}
}
