// F8-only native player-camera activation lists and actual NPC metadata.
// Uses the existing source boundary. No new hooks, native calls or data writes.
#include "NpcCamera63.h"
#include "NpcCallback63.h"
#include "NpcDetail63.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <share.h>
namespace NpcCamera63 {
namespace {
constexpr unsigned ActiveLimit=8192,NpcLimit=1024,CameraLimit=8,DisabledLimit=4096;
struct Npc {uint64_t entity{},handle{},proxy{},biped{},skeleton{},behaviour{};uint8_t raw[0x138]{},proxyRaw[0xd8]{},transform[64]{};bool valid{},active{},proxyRead{},transformRead{};unsigned disabledMask{};};
struct Camera {uint64_t component{},entity{},vtable{},disabledPointer{};uint8_t raw[0x12d0]{};unsigned rawSize{},count{},capacity{},resolved{},invalid{};bool valid{},listRead{},stable{};std::array<uint64_t,DisabledLimit> handles{},entities{};};
uint8_t* image{};void* source{};void* world{};void* rendererWorld{};FILE* report{};std::filesystem::path directory;
std::atomic<bool> enabled{};bool f8Down{};uint64_t captureId{};unsigned knownCount{},cameraCount{},refused{},overflow{};
std::array<Npc,NpcLimit> npcs{};std::array<Camera,CameraLimit> cameras{};std::array<uint64_t,ActiveLimit> active{};
bool Read(void* out,const void* address,size_t size) noexcept {
 if(!out || !address || !size || size>0x10000)return false;
 __try{memcpy(out,address,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
uint64_t Q(const uint8_t* p,size_t o){uint64_t v{};memcpy(&v,p+o,8);return v;}
uint32_t D(const uint8_t* p,size_t o){uint32_t v{};memcpy(&v,p+o,4);return v;}
uint16_t W(const uint8_t* p,size_t o){uint16_t v{};memcpy(&v,p+o,2);return v;}
bool Owner(void* entity,uint8_t* raw){
 return Read(raw,entity,0x138) && NpcCallback63::CanonicalVtable(reinterpret_cast<uint64_t>(entity),Q(raw,0))==reinterpret_cast<uint64_t>(image)+0x3017d60 && Q(raw,0x60)==reinterpret_cast<uint64_t>(world);
}
bool Handle(uint64_t handle,uint64_t& entity){
 uint8_t raw[16]{};if(!handle || !Read(raw,reinterpret_cast<void*>(handle),16) || int32_t(D(raw,12))>=0)return false;
 entity=Q(raw,0);uint64_t backlink{};
 return entity && Read(&backlink,reinterpret_cast<void*>(entity+0x130),8) && backlink==handle;
}
bool Candidate(uint64_t entity,Npc& n){
 n={};if(!entity || !Owner(reinterpret_cast<void*>(entity),n.raw))return false;
 n.entity=entity;n.handle=Q(n.raw,0x130);uint64_t resolved{};
 if(!Handle(n.handle,resolved) || resolved!=entity)return false;
 uint16_t count=W(n.raw,0x82);uint64_t parts[128]{};
 if(!count || count>128 || !Read(parts,reinterpret_cast<void*>(Q(n.raw,0x78)),size_t(count)*8))return false;
 for(unsigned i=0;i<count;++i){if(!parts[i])continue;uint8_t c[24]{};
  if(!Read(c,reinterpret_cast<void*>(parts[i]),24) || Q(c,0x10)!=entity)return false;
  uint64_t vt=Q(c,0);if(vt==reinterpret_cast<uint64_t>(image)+0x3051e60)n.biped=parts[i];
  if(vt==reinterpret_cast<uint64_t>(image)+0x2e76630)n.skeleton=parts[i];
  if(vt==reinterpret_cast<uint64_t>(image)+0x2f192c0)n.behaviour=parts[i];
 }
 if(!n.biped || !n.skeleton)return false;
 n.proxy=Q(n.raw,0xf8);n.proxyRead=n.proxy && Read(n.proxyRaw,reinterpret_cast<void*>(n.proxy),sizeof(n.proxyRaw));
 if(!n.proxyRead || Q(n.proxyRaw,0x90)!=entity)return false;
 n.transformRead=Q(n.raw,0xb8) && Read(n.transform,reinterpret_cast<void*>(Q(n.raw,0xb8)),64);
 n.valid=true;return true;
}
void Remember(uint64_t entity,bool isActive,unsigned disabledMask){
 Npc fresh{};if(!Candidate(entity,fresh)){++refused;return;}
 unsigned i=0;while(i<knownCount && !(npcs[i].entity==entity && npcs[i].handle==fresh.handle))++i;
 if(i==knownCount){if(knownCount==NpcLimit){++overflow;return;}++knownCount;}
 fresh.active=npcs[i].active || isActive;fresh.disabledMask=npcs[i].disabledMask|disabledMask;npcs[i]=fresh;
}
void AddCamera(uint64_t entity,uint64_t component,uint64_t vt){
 for(unsigned i=0;i<cameraCount;++i)if(cameras[i].component==component)return;
 if(cameraCount==CameraLimit){++overflow;return;}
 auto& c=cameras[cameraCount];c={};c.component=component;c.entity=entity;c.vtable=vt;
 c.rawSize=vt==reinterpret_cast<uint64_t>(image)+0x3050b00?0x12d0:0x120;
 if(!Read(c.raw,reinterpret_cast<void*>(component),c.rawSize) || Q(c.raw,0)!=vt || Q(c.raw,0x10)!=entity){++refused;return;}
 c.valid=true;++cameraCount;
}
void FindCameras(uint64_t entity){
 uint8_t raw[0x138]{};if(!Owner(reinterpret_cast<void*>(entity),raw))return;
 uint16_t count=W(raw,0x82);uint64_t parts[128]{};
 if(!count || count>128 || !Read(parts,reinterpret_cast<void*>(Q(raw,0x78)),size_t(count)*8))return;
 for(unsigned i=0;i<count;++i){uint8_t c[24]{};if(!parts[i] || !Read(c,reinterpret_cast<void*>(parts[i]),24) || Q(c,0x10)!=entity)continue;
  uint64_t vt=Q(c,0);if(vt==reinterpret_cast<uint64_t>(image)+0x304d9e0 || vt==reinterpret_cast<uint64_t>(image)+0x3050b00)AddCamera(entity,parts[i],vt);
 }
}
void Disabled(Camera& c,unsigned index){
 c.disabledPointer=Q(c.raw,0xb0);c.count=W(c.raw,0xba);c.capacity=D(c.raw,0xb8)&0x3fff;
 if(c.count>c.capacity || c.count>DisabledLimit || (c.count && !c.disabledPointer)){++refused;return;}
 c.listRead=!c.count || Read(c.handles.data(),reinterpret_cast<void*>(c.disabledPointer),size_t(c.count)*8);
 if(!c.listRead){++refused;return;}
 for(unsigned j=0;j<c.count;++j){uint64_t e{};if(!Handle(c.handles[j],e)){++c.invalid;continue;}c.entities[j]=e;++c.resolved;Remember(e,false,1u<<index);}
 uint8_t after[16]{},identity[24]{};
 c.stable=Read(after,reinterpret_cast<void*>(c.component+0xb0),16) && memcmp(after,c.raw+0xb0,16)==0 && Read(identity,reinterpret_cast<void*>(c.component),24) && Q(identity,0)==c.vtable && Q(identity,0x10)==c.entity;
}
void Hex(FILE* f,const uint8_t* p,size_t n){fputc('"',f);for(size_t i=0;i<n;++i)fprintf(f,"%02x",p[i]);fputc('"',f);}
void Snapshot(uint64_t sequence,uint64_t tracking,uint32_t frame){
 ++captureId;refused=overflow=0;
 // Rediscover cameras each capture. Previously seen NPCs are retained only by a
 // validated entity/handle pair; stale identities are never treated as valid.
 cameraCount=0;for(unsigned i=0;i<knownCount;++i){npcs[i].active=false;npcs[i].disabledMask=0;npcs[i].valid=false;}
 uint8_t header[16]{},after[16]{};uint64_t listAddress=reinterpret_cast<uint64_t>(world)+0x180+0x4e8;
 uint64_t currentWorld{},currentHandle{},currentOwner{},currentVtable{};
 bool worldMatches=Read(&currentWorld,static_cast<uint8_t*>(rendererWorld)+0x28,8) && currentWorld==reinterpret_cast<uint64_t>(world) && Read(&currentHandle,static_cast<uint8_t*>(source)+0x250,8) && currentHandle && Read(&currentOwner,reinterpret_cast<void*>(currentHandle),8) && currentOwner==currentWorld && Read(&currentVtable,world,8) && currentVtable==reinterpret_cast<uint64_t>(image)+0x30225e0;
 bool headerRead=worldMatches && Read(header,reinterpret_cast<void*>(listAddress),16);
 uint32_t count=headerRead?D(header,8):0,capacity=headerRead?(D(header,12)&0x1fffffff):0;
 uint64_t pointer=headerRead?Q(header,0):0;
 bool listRead=headerRead && count<=capacity && count<=ActiveLimit && (!count || pointer);
 if(listRead && count)listRead=Read(active.data(),reinterpret_cast<void*>(pointer),size_t(count)*8);
 if(listRead)for(unsigned i=0;i<count;++i){FindCameras(active[i]);Remember(active[i],true,0);}
 bool activeStable=listRead && Read(after,reinterpret_cast<void*>(listAddress),16) && memcmp(header,after,16)==0;
 for(unsigned i=0;i<cameraCount;++i)Disabled(cameras[i],i);
 unsigned valid=0;for(unsigned i=0;i<knownCount;++i){auto& n=npcs[i];Npc current{};
  if(worldMatches && Candidate(n.entity,current) && current.handle==n.handle){current.active=n.active;current.disabledMask=n.disabledMask;n=current;++valid;}else n.valid=false;
 }
 uint32_t endFrame{};bool frameRead=Read(&endFrame,static_cast<uint8_t*>(source)+0x294,4);
 std::error_code error;std::filesystem::create_directories(directory,error);if(error)return;
 auto file=directory/(L"camera-"+std::to_wstring(captureId)+L"-frame-"+std::to_wstring(frame)+L".jsonl");
 FILE* f=_wfsopen(file.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"schema\":1,\"captureId\":%llu,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"endFrame\":%u,\"frameRead\":%s,\"imageBase\":\"%llx\",\"world\":\"%llx\",\"activeList\":\"%llx\",\"activePointer\":\"%llx\",\"activeCount\":%u,\"activeCapacity\":%u,\"activeRead\":%s,\"activeStable\":%s,\"cameraCount\":%u,\"knownNpcs\":%u,\"note\":\"F8 read-only metadata at the existing renderer source boundary. Native player-camera +b0 holds entity reference handles, not direct entity pointers. Membership is evidence of native activation state, not an observed setter caller. No native calls/hooks or writes. Previously seen NPC entity+handle identities revalidated; no refcounts modified. Lists are unlocked snapshots; header stability is not an atomic consistency guarantee.\"}\n",captureId,sequence,tracking,frame,endFrame,frameRead?"true":"false",reinterpret_cast<uint64_t>(image),reinterpret_cast<uint64_t>(world),listAddress,pointer,count,capacity,listRead?"true":"false",activeStable?"true":"false",cameraCount,knownCount);
 for(unsigned i=0;i<cameraCount;++i){auto& c=cameras[i];fprintf(f,"{\"event\":\"camera\",\"index\":%u,\"component\":\"%llx\",\"entity\":\"%llx\",\"vtable\":\"%llx\",\"valid\":%s,\"disabledPointer\":\"%llx\",\"count\":%u,\"capacity\":%u,\"resolved\":%u,\"invalid\":%u,\"listRead\":%s,\"stable\":%s,\"raw\":",i,c.component,c.entity,c.vtable,c.valid?"true":"false",c.disabledPointer,c.count,c.capacity,c.resolved,c.invalid,c.listRead?"true":"false",c.stable?"true":"false");Hex(f,c.raw,c.rawSize);fprintf(f,"}\n");
  if(c.listRead)for(unsigned j=0;j<c.count;++j)fprintf(f,"{\"event\":\"disabledHandle\",\"camera\":%u,\"index\":%u,\"handle\":\"%llx\",\"entity\":\"%llx\"}\n",i,j,c.handles[j],c.entities[j]);
 }
 for(unsigned i=0;i<knownCount;++i){auto& n=npcs[i];fprintf(f,"{\"event\":\"npc\",\"index\":%u,\"entity\":\"%llx\",\"handle\":\"%llx\",\"proxy\":\"%llx\",\"biped\":\"%llx\",\"skeleton\":\"%llx\",\"behaviour\":\"%llx\",\"valid\":%s,\"activeListMember\":%s,\"cameraDisabledMask\":%u,\"proxyRead\":%s,\"transformRead\":%s,\"raw\":",i,n.entity,n.handle,n.proxy,n.biped,n.skeleton,n.behaviour,n.valid?"true":"false",n.active?"true":"false",n.disabledMask,n.proxyRead?"true":"false",n.transformRead?"true":"false");Hex(f,n.raw,sizeof(n.raw));fprintf(f,",\"proxyRaw\":");Hex(f,n.proxyRaw,sizeof(n.proxyRaw));fprintf(f,",\"transform\":");Hex(f,n.transform,sizeof(n.transform));fprintf(f,"}\n");}
 NpcDetail63::Snapshot detail;NpcDetail63::Collect(detail,reinterpret_cast<uint64_t>(image),reinterpret_cast<uint64_t>(world),[](uint64_t entity){Npc n{};return Candidate(entity,n);});NpcDetail63::Write(f,detail);
 fprintf(f,"{\"event\":\"end\",\"validNpcs\":%u,\"refusedCandidates\":%u,\"overflow\":%u}\n",valid,refused,overflow);fclose(f);
 fprintf(report,"F8 NPC camera63 capture=%llu sourceFrame=%u endFrame=%u active=%u read=%d stable=%d cameras=%u NPCs=%u valid=%u overflow=%u; read-only source boundary\n",captureId,frame,endFrame,count,listRead,activeStable,cameraCount,knownCount,valid,overflow);fflush(report);
}
}
bool Install(uint8_t* base,void* primary,void* owner,const std::filesystem::path& output,FILE* log){
 image=base;source=primary;rendererWorld=owner;report=log;directory=output/L"npc-camera63";
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x1f143e0,"\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8d\xac\x24\x18\xfe\xff\xff\x48\x81\xec\xe8\x02\x00\x00\x0f\x28\x41\x50\x48\x8b\x01\x0f\x29\xb4\x24\xd0\x02\x00\x00\x4c\x8b\xe9\x66\x0f\x7f\x44\x24\x60\xff\x90\x60\x02\x00\x00\x49\x8b\x45\x00\x49\x8b\xcd\x0f\x28\xf0\xff\x90\x68\x02\x00\x00\x49\x8b\x55\x10",78},{0x1ef9200,"\x48\x89\x5c\x24\x10\x57\x48\x81\xec\x80\x00\x00\x00\x48\x8b\xda\x48\x8b\xf9\xb2\x01\x48\x8b\xcb\xe8\x33\x99\xde\xff",29},{0x1f14d07,"\x84\xdb\x75\x0b\x48\x8b\xd7\x49\x8b\xcd\xe8\xea\x44\xfe\xff\x48\x8b\x9f\x30\x01\x00\x00\xf0\xff\x43",25},{0x1ce1e3b,"\x48\x8d\xb5\xe8\x04\x00\x00\x8b\x4e\x0c\x8b\x46\x08\x81\xe1\xff\xff\xff\x1f\xff\xc0\x3b\xc1\x76\x40\x83\xf9",27}};
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(report,"REFUSED NPC camera63 signature=%zx\n",g.rva);fflush(report);return false;}
 uint64_t graph{},sourceWorld{},backlink{};
 if(!source || !rendererWorld || !Read(&graph,static_cast<uint8_t*>(source)+0x440,8) || !graph || !Read(&sourceWorld,reinterpret_cast<void*>(graph+0x10),8) || sourceWorld!=reinterpret_cast<uint64_t>(rendererWorld) || !Read(&backlink,reinterpret_cast<void*>(graph),8) || backlink!=reinterpret_cast<uint64_t>(source))return false;
 uint64_t gameplay{},handle{},handleOwner{},gameplayVtable{},rendererVtable{};uint32_t handleFlags{};
 if(!Read(&rendererVtable,rendererWorld,8) || rendererVtable!=reinterpret_cast<uint64_t>(base)+0x3094180 ||
 !Read(&gameplay,static_cast<uint8_t*>(rendererWorld)+0x28,8) || !gameplay || !Read(&gameplayVtable,reinterpret_cast<void*>(gameplay),8) || gameplayVtable!=reinterpret_cast<uint64_t>(base)+0x30225e0 ||
 !Read(&handle,static_cast<uint8_t*>(source)+0x250,8) || !handle || !Read(&handleOwner,reinterpret_cast<void*>(handle),8) || handleOwner!=gameplay || !Read(&handleFlags,reinterpret_cast<void*>(handle+12),4) || int32_t(handleFlags)>=0)return false;
 world=reinterpret_cast<void*>(gameplay);
 for(size_t vt:{size_t(0x304d9e0),size_t(0x3050b00)}){uint64_t method{};if(!Read(&method,base+vt+0x220,8) || method!=reinterpret_cast<uint64_t>(base)+0x1f15e60)return false;}
 enabled=true;fprintf(report,"NPC camera63 capture initialized: existing F8 source boundary only; renderer graph world+28 == source handle owner resolves actual gameplay World; native gameplay world+180 entity service / active list+4e8, exact PlayerCamera native activation lists, validated actual NPCs retained by entity+handle. Collector only reads; notification observer separately owns per-NPC vptr copies. Native code/shared vtables and entity flags/camera/LOD decisions unchanged.\n");fflush(report);return true;
}
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame){
#ifdef ACUNITYVR_NPC56_CAMERA_CHECK
 extern bool checkF8;bool down=checkF8;
#else
 bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
#endif
 bool pressed=down&&!f8Down;f8Down=down;if(enabled && pressed)Snapshot(sequence,tracking,frame);
}
void Stop(){enabled=false;if(report){fprintf(report,"NPC camera63 capture stopped; no additional native ranges to restore\n");fflush(report);}}
}
