#pragma once
#include "BinocularComponentInput50.h"
#include <array>
#include <cstring>
namespace PrimarySourceRanges52 {
constexpr size_t ViewOffsets[]{0,0x40,0x2e0,0x320,0x360,0x3a0,0x3e0};
struct Bytes {uint8_t views[7][64]{},frustum[0xc0]{},state[0x180]{};};
struct Ownership {Bytes original{},applied{};uint8_t* view{},*state{},*frustum{};bool active{};};
inline void Snapshot(Bytes& b,const Ownership& o){for(unsigned i=0;i<7;++i)memcpy(b.views[i],o.view+ViewOffsets[i],64);memcpy(b.frustum,o.frustum,0xc0);memcpy(b.state,o.state,0x180);}
inline void Store(const Bytes& b,const Ownership& o){for(unsigned i=0;i<7;++i)memcpy(o.view+ViewOffsets[i],b.views[i],64);memcpy(o.frustum,b.frustum,0xc0);memcpy(o.state,b.state,0x180);}
inline bool Apply(Ownership& o,const BinocularComponentInput50::Result& r,const void* planes){
 if(!o.view || !o.state || !o.frustum || !planes)return false;Bytes next{};
 const CameraMath::Matrix m[]{r.pose,r.pose,r.viewProjection,r.projection,UnityCamera::ToGraphics(r.pose),r.inverseProjection,r.inverseVP};
 for(unsigned i=0;i<7;++i)memcpy(next.views[i],m[i].m,64);memcpy(next.frustum,planes,0xc0);
 memcpy(next.state,next.views[2],64);memcpy(next.state+0x40,next.views[0],64);memcpy(next.state+0x80,next.views[3],64);memcpy(next.state+0xc0,planes,0xc0);
 Snapshot(o.original,o);o.applied=next;Store(next,o);o.active=true;return true;
}
inline bool Restore(Ownership& o){if(!o.active)return true;Bytes current{};Snapshot(current,o);bool ours=!memcmp(&current,&o.applied,sizeof(current));if(ours)Store(o.original,o);o.active=false;return ours;}
}
