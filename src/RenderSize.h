#pragma once
#include <cstdint>
namespace RenderSize {
// A full same-frame pair stacks both eyes in one D3D11 texture (max16384 tall).
inline constexpr unsigned MaxWidth=16384,MaxEyeHeight=8192;
inline unsigned eyeWidth=1920,eyeHeight=1080;
inline bool Valid(unsigned width,unsigned height){return width>0 && height>0 && width<=MaxWidth && height<=MaxEyeHeight;}
inline bool Set(unsigned width,unsigned height){if(!Valid(width,height))return false;eyeWidth=width;eyeHeight=height;return true;}
inline bool Matches(unsigned width,unsigned height){return width==eyeWidth && height==eyeHeight;}
inline unsigned Quarter(unsigned value){return (value+3)/4;}
inline bool CloudMatches(unsigned width,unsigned height){return width==Quarter(eyeWidth) && height==Quarter(eyeHeight);}
}
