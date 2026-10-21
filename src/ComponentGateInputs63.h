#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include "NpcCallback63.h"
namespace ComponentGateInputs63 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
struct Type{uint32_t table,method;};
constexpr Type Types[]={{0x30a03c0,0x2242420},{0x309f940,0x2226590},{0x309ec00,0x2226020},{0x30a7690,0x22e2ed0},{0x30a1370,0x22425a0},{0x3099310,0x21e8600},{0x30a7970,0x22e2ff0},{0x30a4ca0,0x226a460}};
inline bool Known(uint64_t image,uint64_t table,uint64_t* method=nullptr){for(auto& t:Types)if(table==image+t.table){if(method)*method=image+t.method;return true;}return false;}
template<class T>T Get(const void* p,size_t at){T value{};memcpy(&value,static_cast<const uint8_t*>(p)+at,sizeof(value));return value;}
struct Snapshot {
 uint64_t component{},context{},table{},method{},owner{},primary{},entity{},entityTable{},proxy{},member{},boundsTransform{};
 uint32_t sourceFrame{},primaryFrame{},errors{},childCount{},childrenRead{};uint64_t geometryMask{};
 bool identity{},cameraRead{},entityRead{},proxyRead{},memberRead{},backlinksStable{},transformRead{};
 uint8_t descriptor[0xc0]{},entityRaw[0x100]{},proxyRaw[0xd8]{},memberRaw[0xd0]{},pose[64]{},transform[64]{};
};
inline void CaptureInto(Snapshot& s,Reader read,uint64_t image,uint64_t source,uint64_t expectedContext,uint64_t expectedOwner,uint64_t component,uint64_t context){
 s={};s.component=component;s.context=context;uint64_t expectedMethod{},again{};
 if(!read(s.descriptor,component,sizeof(s.descriptor)) || !Known(image,s.table=Get<uint64_t>(s.descriptor,0),&expectedMethod) || !read(&s.method,s.table+0x138,8) || s.method!=expectedMethod){s.errors|=1;return;}s.identity=true;
 if(context!=expectedContext || !read(&s.owner,context,8) || s.owner!=expectedOwner || !read(&s.primary,s.owner+0x890,8) || s.primary!=source || !read(&s.sourceFrame,source+0x294,4) || !read(&s.primaryFrame,s.primary+0x294,4) || !read(s.pose,s.primary,64)){s.errors|=2;return;}s.cameraRead=true;
 s.entity=Get<uint64_t>(s.descriptor,0x10);
 if(s.entity){s.entityRead=read(s.entityRaw,s.entity,sizeof(s.entityRaw));if(!s.entityRead){s.errors|=4;return;}s.entityTable=NpcCallback63::CanonicalVtable(s.entity,Get<uint64_t>(s.entityRaw,0));if(s.entityTable!=image+0x3017d60 && s.entityTable!=image+0x3019300){s.errors|=8;return;}
  s.boundsTransform=Get<uint64_t>(s.entityRaw,0xb8);if(s.boundsTransform){s.transformRead=read(s.transform,s.boundsTransform,64);if(!s.transformRead)s.errors|=512;}
  s.proxy=Get<uint64_t>(s.entityRaw,0xf8);if(s.proxy){s.proxyRead=read(s.proxyRaw,s.proxy,sizeof(s.proxyRaw));if(!s.proxyRead)s.errors|=16;}}
 if(s.table==image+0x30a1370){s.member=Get<uint64_t>(s.descriptor,0x40);uint64_t method{};
  s.memberRead=s.member && read(s.memberRaw,s.member,sizeof(s.memberRaw)) && Get<uint64_t>(s.memberRaw,0)==image+0x30a1240 && Get<uint64_t>(s.memberRaw,0x18)==component && read(&method,image+0x30a1240+0xf0,8) && method==image+0x2242740;
  if(!s.memberRead)s.errors|=32;else{uint64_t vector=Get<uint64_t>(s.memberRaw,0xa0);s.childCount=Get<uint16_t>(s.memberRaw,0xaa);
   if(s.childCount>64)s.errors|=64;else for(unsigned i=0;i<s.childCount;++i){uint64_t child{},geometry{};if(!vector || !read(&child,vector+8*i,8) || !child || !read(&geometry,child+8,8)){s.errors|=128;continue;}++s.childrenRead;if(geometry)s.geometryMask|=uint64_t(1)<<i;}}}
 s.backlinksStable=read(&again,component+0x10,8) && again==s.entity && read(&again,context,8) && again==s.owner && read(&again,s.owner+0x890,8) && again==s.primary;
 if(s.entityRead)s.backlinksStable=s.backlinksStable && read(&again,s.entity,8) && NpcCallback63::CanonicalVtable(s.entity,again)==s.entityTable && read(&again,s.entity+0xf8,8) && again==s.proxy;
 if(!s.backlinksStable)s.errors|=256;
}
}
