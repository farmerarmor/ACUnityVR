#pragma once
#include "UnityCamera.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace StationSubmission90 {
inline constexpr uint32_t NativeCaller=0xfed9e7;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*,void*);
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
using Owner=bool(*)();
// Same near-distance and 4-lane cone calculation as native fec3a0. The engine
// stores render scale in position.w and zero in its cached camera position.w.
inline bool HeadVisible(const CameraMath::Matrix& pose,const float position[4],float cosine,float nearSquared=9.f) noexcept {
 if(!CameraMath::Rigid(pose) || !position || !std::isfinite(cosine) || cosine< -1 || cosine>1 || nearSquared!=9.f)return false;
 float delta[4]{position[0]-pose.m[12],position[1]-pose.m[13],position[2]-pose.m[14],position[3]};
 for(float v:delta)if(!std::isfinite(v))return false;
 if(delta[0]*delta[0]+delta[1]*delta[1]<=nearSquared)return true;
 float length=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]+delta[3]*delta[3]);
 if(!std::isfinite(length) || length<=1e-8f)return false;
 return (delta[0]*pose.m[4]+delta[1]*pose.m[5]+delta[2]*pose.m[6])/length>=cosine;
}
bool Configure(uint8_t* image,uint64_t source,uint64_t world,uint64_t manager,Reader,Owner,FILE*,const std::filesystem::path&);
void Publish(uint32_t frame);
uint64_t Invoke(Callback,void*,void*,void*,void*,void*,uint64_t caller);
void Poll(uint64_t sequence);
void Save(uint64_t sequence,uint64_t tracking,uint32_t frame);
void Stop();
}
