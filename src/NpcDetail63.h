#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include "NpcCallback63.h"
#include "NpcDecision63.h"
#include "NpcCone63.h"
#include "BulkDraw83.h"
// F8-only snapshots of existing native actor states and priority groups.
// No native calls, hooks, locks, refcount or game-data writes.
namespace NpcDetail63 {
using Candidate=bool(*)(uint64_t);
inline bool Read(void* out,uint64_t p,size_t n) noexcept {if(!out || p<0x10000 || p>0x00007ffffffeffffULL || !n || n>0x10000 || p+n<p)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T> T Get(const uint8_t* p,size_t o){T v{};memcpy(&v,p+o,sizeof(v));return v;}
inline bool Q(uint64_t p,size_t o,uint64_t expected){uint64_t v{};return Read(&v,p+o,8) && v==expected;}
inline bool BulkIdentity(uint64_t image,uint64_t object){uint64_t observed{};return Read(&observed,object,8) && BulkDraw83::CanonicalBulk(object,observed)==image+0x304ab90;}
struct Actor {uint32_t slot{};uint64_t state{},entity{},handle{},proxy{};bool valid{};uint8_t stateRaw[0x360]{},descriptor[0x1b0]{},reference[0xf0]{};};
struct Snapshot {uint32_t status{},count{},capacity{},refused{};uint64_t world{},bulk{},collection{},manager{},pool{},states{},descriptors{},references{},selector{},groups{};uint16_t groupCount{};bool stable{},groupsRead{},cameraRead{},fovRead{};uint8_t selectorRaw[0x88]{},cameraRaw[0x100]{};float cachedFov{};std::vector<Actor> actors;std::vector<uint8_t> groupRaw;};
inline void Collect(Snapshot& out,uint64_t image,uint64_t world,Candidate candidate){
 out={};out.world=world;out.status=1;
 if(!candidate || !Q(world,0,image+0x30225e0) || !Read(&out.bulk,world+0x998,8) || !BulkIdentity(image,out.bulk))return;
 out.status=2;uint64_t vt{};
 if(!Read(&out.collection,out.bulk+0x500,8) || !Read(&vt,out.collection,8) || NpcCone63::CanonicalCollection(out.collection,vt)!=image+0x2f6b880 || !Read(&out.manager,out.collection+0xc0,8) || !Read(&vt,out.manager,8) || NpcDecision63::CanonicalManager(out.manager,vt)!=image+0x2f6bcb0 || !Q(out.manager,0x278,world))return;
 out.status=3;uint8_t pool[0xa8]{},again[0xa8]{};
 if(!Read(&out.pool,out.manager+0x40,8) || !Read(pool,out.pool,sizeof(pool)) || Get<uint64_t>(pool,0)!=image+0x304c640 || Get<uint64_t>(pool,0x50)!=out.pool || Get<uint64_t>(pool,0x58)!=out.manager || Get<uint64_t>(pool,0x70)!=world)return;
 out.count=Get<uint32_t>(pool,0x68);out.capacity=Get<uint32_t>(pool,0x6c)&0x1fffffff;out.states=Get<uint64_t>(pool,0x60);out.status=4;
 if(!out.count || out.count>4096 || out.capacity<out.count || out.capacity>4096 || out.states<0x10000 || !Read(&out.descriptors,out.manager+0x258,8) || !Read(&out.references,out.manager+0x384,8) || out.descriptors<0x10000 || out.references<0x10000)return;
 out.status=5;
 if(!Read(&out.selector,out.bulk+0x4f8,8) || !Read(out.selectorRaw,out.selector,sizeof(out.selectorRaw)) || Get<uint64_t>(out.selectorRaw,0)!=image+0x2f6b980 || Get<uint64_t>(out.selectorRaw,0x80)!=out.bulk)return;
 out.actors.reserve(out.count);
 for(uint32_t i=0;i<out.count;++i){uint64_t state=out.states+uint64_t(i)*0x360,handle{},entity{};uint8_t href[16]{},actorRaw[0x138]{};
  if(!Read(&handle,state+0x10,8) || !Read(href,handle,16) || int32_t(Get<uint32_t>(href,12))>=0)continue;
  entity=Get<uint64_t>(href,0);
  if(!Read(actorRaw,entity,sizeof(actorRaw)) || NpcCallback63::CanonicalVtable(entity,Get<uint64_t>(actorRaw,0))!=image+0x3017d60 || Get<uint64_t>(actorRaw,0x60)!=world || Get<uint64_t>(actorRaw,0x130)!=handle || !candidate(entity))continue;
  Actor a{};a.slot=i;a.state=state;a.entity=entity;a.handle=handle;a.proxy=Get<uint64_t>(actorRaw,0xf8);uint8_t afterHandle[16]{};
  a.valid=Read(a.stateRaw,state,sizeof(a.stateRaw)) && Get<uint64_t>(a.stateRaw,0x10)==handle && Read(a.descriptor,out.descriptors+uint64_t(i)*0x1b0,sizeof(a.descriptor)) && Read(a.reference,out.references+uint64_t(i)*0xf0,sizeof(a.reference)) && Q(state,0x10,handle) && Q(entity,0x130,handle) && Read(afterHandle,handle,16) && memcmp(href,afterHandle,16)==0;
  if(!a.valid)++out.refused;out.actors.push_back(a);
 }
 out.groups=Get<uint64_t>(out.selectorRaw,8);out.groupCount=Get<uint16_t>(out.selectorRaw,0x12);
 if(out.groupCount<=4096 && (!out.groupCount || out.groups>=0x10000)){out.groupRaw.resize(size_t(out.groupCount)*24);out.groupsRead=true;for(size_t at=0;at<out.groupRaw.size();){size_t n=(out.groupRaw.size()-at<0x10000)?out.groupRaw.size()-at:0x10000;if(!Read(out.groupRaw.data()+at,out.groups+at,n)){out.groupsRead=false;break;}at+=n;}}
 uint64_t camera{};out.cameraRead=Read(&camera,image+0x521a768,8) && Read(out.cameraRaw,camera,sizeof(out.cameraRaw)) && Get<uint64_t>(out.cameraRaw,0)==image+0x3040d10;out.fovRead=Read(&out.cachedFov,image+0x4ee0304,4);
 uint8_t selectorAfter[0x88]{};uint64_t managerVt{},collectionVt{};
 out.stable=Q(world,0,image+0x30225e0) && Q(world,0x998,out.bulk) && BulkIdentity(image,out.bulk) && Q(out.bulk,0x500,out.collection) && Q(out.bulk,0x4f8,out.selector) && Q(out.collection,0xc0,out.manager) && Read(&collectionVt,out.collection,8) && NpcCone63::CanonicalCollection(out.collection,collectionVt)==image+0x2f6b880 && Read(&managerVt,out.manager,8) && NpcDecision63::CanonicalManager(out.manager,managerVt)==image+0x2f6bcb0 && Q(out.manager,0x278,world) && Q(out.manager,0x40,out.pool) && Q(out.manager,0x258,out.descriptors) && Q(out.manager,0x384,out.references) && Read(again,out.pool,sizeof(again)) && memcmp(pool,again,8)==0 && memcmp(pool+0x50,again+0x50,0x28)==0 && Read(selectorAfter,out.selector,sizeof(selectorAfter)) && memcmp(out.selectorRaw,selectorAfter,24)==0 && Get<uint64_t>(selectorAfter,0x80)==out.bulk;
 out.status=out.stable?0:6;
}
inline void Hex(FILE* f,const uint8_t* p,size_t n){fputc('"',f);for(size_t i=0;i<n;++i)fprintf(f,"%02x",p[i]);fputc('"',f);}
inline void Write(FILE* f,const Snapshot& s){
 fprintf(f,"{\"event\":\"crowdBegin\",\"schema\":1,\"status\":%u,\"stable\":%s,\"manager\":\"%llx\",\"pool\":\"%llx\",\"selector\":\"%llx\",\"states\":\"%llx\",\"count\":%u,\"capacity\":%u,\"actors\":%zu,\"refused\":%u,\"groupCount\":%u,\"groupsRead\":%s,\"cameraRead\":%s,\"cachedFovRead\":%s,\"cachedFovRaw\":",s.status,s.stable?"true":"false",s.manager,s.pool,s.selector,s.states,s.count,s.capacity,s.actors.size(),s.refused,s.groupCount,s.groupsRead?"true":"false",s.cameraRead?"true":"false",s.fovRead?"true":"false");Hex(f,reinterpret_cast<const uint8_t*>(&s.cachedFov),s.fovRead?4:0);fprintf(f,",\"selectorRaw\":");Hex(f,s.selectorRaw,s.status==0?sizeof(s.selectorRaw):0);fprintf(f,",\"cameraRaw\":");Hex(f,s.cameraRaw,s.cameraRead?sizeof(s.cameraRaw):0);fprintf(f,",\"note\":\"F8-only unlocked reads, exact world/manager/pool/selector identities. Actor state, descriptor and reference raw blocks have no assumed LOD field. Header stability is not atomic consistency. Native FOV cache may differ from an override camera; exact caller registers are recorded separately. No hooks/native calls/game writes.\"}\n");
 if(!s.status && s.stable){for(const auto& a:s.actors){fprintf(f,"{\"event\":\"crowdActor\",\"slot\":%u,\"state\":\"%llx\",\"entity\":\"%llx\",\"handle\":\"%llx\",\"proxy\":\"%llx\",\"valid\":%s,\"mode\":%u,\"activation\":%u,\"stateRaw\":",a.slot,a.state,a.entity,a.handle,a.proxy,a.valid?"true":"false",a.valid?Get<uint32_t>(a.stateRaw,0x80):0,a.valid?Get<uint16_t>(a.stateRaw,0x28):0);Hex(f,a.stateRaw,a.valid?sizeof(a.stateRaw):0);fprintf(f,",\"descriptorRaw\":");Hex(f,a.descriptor,a.valid?sizeof(a.descriptor):0);fprintf(f,",\"referenceRaw\":");Hex(f,a.reference,a.valid?sizeof(a.reference):0);fprintf(f,"}\n");}
 if(s.groupsRead)for(unsigned i=0;i<s.groupCount;++i){fprintf(f,"{\"event\":\"crowdPriorityGroup\",\"index\":%u,\"raw\":",i);Hex(f,s.groupRaw.data()+size_t(i)*24,24);fprintf(f,"}\n");}}
}
}
