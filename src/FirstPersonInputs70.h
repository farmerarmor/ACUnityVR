#pragma once
#include "UnityCamera.h"
#include <cstdint>
#include <cstring>
namespace FirstPersonInputs70 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
struct Snapshot {uint64_t player{},camera{},skeleton{},proxy{},calcs{},mapping{},boneData{};uint16_t headIndex{0xffff},leftIndex{0xffff},rightIndex{0xffff};Transport::Vector anchor{};bool eyes{},worldBones{};};
template<class T> bool Read(Reader read,uint64_t p,T& v){return p>=0x10000 && read(&v,p,sizeof(v));}
inline bool Same(Reader read,uint64_t p,uint64_t expected){uint64_t v{};return Read(read,p,v) && v==expected;}
inline bool Finite(Transport::Vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline float Distance2(Transport::Vector a,Transport::Vector b){return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);}
inline bool Index(Reader read,uint64_t mapping,unsigned count,uint32_t id,uint16_t& out){
 unsigned lo=0,hi=count;struct Entry{uint32_t id;uint16_t index,pad;};
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;Entry v{};if(!Read(read,mapping+mid*8,v))return false;if(v.id<id)lo=mid+1;else hi=mid;}
 Entry v{};if(lo>=count || !Read(read,mapping+lo*8,v) || v.id!=id || v.index>=348)return false;out=v.index;return true;
}
// CameraManager array identifies the player; validate exact owner/world/backlinks.
inline bool Capture(Reader read,uint64_t image,uint64_t rendererWorld,const CameraMath::Matrix& native,Snapshot& s){
 s={};uint64_t manager{},array{},world{},vt{},components{},infos{};uint16_t count{},componentCount{},mapCount{};
 if(!CameraMath::Rigid(native) || !Read(read,image+0x521aad0,manager) || !Same(read,manager,image+0x304d800) ||
 !Read(read,manager+0x40,array) || !Read(read,manager+0x4a,count) || count!=1 || !Read(read,array,s.camera) || !Same(read,s.camera,image+0x3050b00) ||
 !Read(read,s.camera+0x10,s.player) || !Same(read,s.player,image+0x3017d60) || !Read(read,rendererWorld+0x28,world) || !Same(read,world,image+0x30225e0) ||
 !Same(read,s.player+0x60,world) || !Same(read,world+0xd8,s.player))return false;
 CameraMath::Matrix player{};Transport::Vector look{};
 if(!Read(read,s.player+0x20,player) || !CameraMath::Rigid(player) || !Read(read,s.camera+0x50,look) || !Finite(look) ||
 Distance2(look,{native.m[12],native.m[13],native.m[14]})>0.25f)return false;
 if(!Read(read,s.player+0x78,components) || !Read(read,s.player+0x82,componentCount) || !componentCount || componentCount>128)return false;
 bool cameraMember=false;
 for(unsigned i=0;i<componentCount;++i){uint64_t p{};if(!Read(read,components+i*8,p) || !Read(read,p,vt))return false;
  if(p==s.camera)cameraMember=true;
  if(vt==image+0x2e76630){if(s.skeleton || !Same(read,p+0x10,s.player))return false;s.skeleton=p;}}
 if(!cameraMember || !s.skeleton || !Read(read,s.skeleton+0x270,infos) || !Read(read,infos+0x38,s.mapping) ||
 !Read(read,infos+0x42,mapCount) || !mapCount || mapCount>512 || !Read(read,s.skeleton+0x288,s.calcs) || !Same(read,s.calcs+0x170,infos) ||
 !Read(read,s.calcs+0x178,s.boneData) || !Index(read,s.mapping,mapCount,0x07c159a2,s.headIndex))return false;
 Transport::Pose root{};uint32_t flags{};
 if(!Read(read,s.calcs,root.position) || !Read(read,s.calcs+0x10,root.orientation) || !Read(read,s.calcs+0x8c,flags) || !Finite(root.position) ||
 Distance2(root.position,{player.m[12],player.m[13],player.m[14]})>1.0f)return false;
 auto q=root.orientation;float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;if(!std::isfinite(norm)||std::abs(norm-1)>0.01f)return false;
 Transport::Vector head{};if(!Read(read,s.boneData+s.headIndex*32,head) || !Finite(head))return false;
 s.worldBones=(flags&(1u<<26))!=0;
 auto worldPosition=[&](Transport::Vector v){if(s.worldBones)return v;auto p=CameraMath::Rotate(q,v);return Transport::Vector{root.position.x+p.x,root.position.y+p.y,root.position.z+p.z};};
 s.anchor=worldPosition(head);s.anchor.z+=0.07f;
 Transport::Vector left{},right{};
 if(Index(read,s.mapping,mapCount,0x1ce92143,s.leftIndex) && Index(read,s.mapping,mapCount,0x18100292,s.rightIndex) &&
 Read(read,s.boneData+s.leftIndex*32,left) && Read(read,s.boneData+s.rightIndex*32,right) && Finite(left) && Finite(right)){
  left=worldPosition(left);right=worldPosition(right);
  if(Distance2(left,right)>0.04f || Distance2(left,worldPosition(head))>0.25f || Distance2(right,worldPosition(head))>0.25f)return false;
  s.anchor={(left.x+right.x)*0.5f,(left.y+right.y)*0.5f,(left.z+right.z)*0.5f};s.eyes=true;
 }
 if(!Finite(s.anchor) || Distance2(s.anchor,root.position)>16 || !Read(read,s.player+0xf8,s.proxy) || !Same(read,s.proxy+0x90,s.player))return false;
 return Same(read,image+0x521aad0,manager) && Same(read,manager+0x40,array) && Same(read,array,s.camera) && Same(read,s.camera+0x10,s.player) &&
 Same(read,s.player+0x60,world) && Same(read,world+0xd8,s.player) && Same(read,s.skeleton+0x10,s.player) && Same(read,s.skeleton+0x288,s.calcs) &&
 Same(read,s.calcs+0x170,infos) && Same(read,s.calcs+0x178,s.boneData) && Same(read,infos+0x38,s.mapping) && Same(read,s.player+0xf8,s.proxy);
}
inline bool BodyMatches(Reader read,uint64_t state,uint64_t object,uint64_t leftState,uint64_t rightState,uint64_t player,uint64_t proxy){
 return player && proxy && state && object==proxy && (state==leftState||state==rightState) && Same(read,proxy+0x90,player);
}
inline CameraMath::Matrix Attach(const CameraMath::Matrix& native,const Snapshot& s,float height,float forward){
 auto m=UnityCamera::LevelBase(native);m.m[12]=s.anchor.x+forward*m.m[4];m.m[13]=s.anchor.y+forward*m.m[5];m.m[14]=s.anchor.z+height;return m;
}
}
