#pragma once
#include "BinocularComponentInput50.h"
#include <array>
#include <cstring>
namespace BulkCamera84 {
struct Published {uint64_t tick{},sequence{},tracking{};uint32_t frame{};bool valid{};BinocularComponentInput50::Result camera{};alignas(16) uint8_t frustum[0xc0]{};};
struct Input {alignas(16) uint8_t view[0x460]{};alignas(16) uint8_t frustum[0xc0]{};};
static_assert(offsetof(Published,camera)%16==0 && offsetof(Published,frustum)%16==0);
inline bool Fresh(const Published& p,uint64_t now,uint32_t frame){return p.valid && p.tracking && now>=p.tick && now-p.tick<=100 && uint32_t(frame-p.frame)<=4;}
inline bool Build(const Published& p,const uint8_t original[0x460],Input& out){
 if(!p.valid || !original || !CameraMath::Rigid(p.camera.pose))return false;
 const CameraMath::Matrix m[]={p.camera.pose,p.camera.pose,p.camera.viewProjection,p.camera.projection,UnityCamera::ToGraphics(p.camera.pose),p.camera.inverseProjection,p.camera.inverseVP};
 for(const auto& mat:m)for(float x:mat.m)if(!std::isfinite(x))return false;
 for(unsigned i=0;i<0xc0/sizeof(float);++i){float value{};memcpy(&value,p.frustum+i*4,4);if(!std::isfinite(value))return false;}
 memcpy(out.view,original,sizeof(out.view));memcpy(out.frustum,p.frustum,sizeof(out.frustum));constexpr unsigned offsets[]={0,0x40,0x2e0,0x320,0x360,0x3a0,0x3e0};for(unsigned i=0;i<7;++i)memcpy(out.view+offsets[i],m[i].m,64);
 auto pointer=reinterpret_cast<uint64_t>(out.frustum);memcpy(out.view+0x428,&pointer,8);return true;
}
}
