// Bounded observer of actual NPC activation notifications via writable entity
// vptrs. Immutable game code/shared tables are never patched. Native callback
// always receives unchanged four GPR arguments and its integer result is returned.
#include "NpcCallback63.h"
#include "NpcInputs63.h"
#include "NativeHandleState63.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <share.h>
namespace NpcCallback63 {
namespace {
constexpr unsigned TargetLimit=256,RecordLimit=512,StackLimit=12,MethodCount=53,ActivationSlot=0x138/8;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
struct Target {uint64_t entity{},handle{},proxy{};uint32_t handleToken{};void* original{};void* clone[MethodCount+1]{};std::atomic<bool> installed{};};
struct Record {uint64_t id{},entity{},proxy{},caller{},before{},after{},tick{};uint32_t frameBefore{},frameAfter{},thread{};uint16_t stackCount{};bool afterRead{},frameRead{};uint64_t stack[StackLimit]{};NpcInputs63::Snapshot inputs{};};
std::array<Target,TargetLimit> targets{};std::atomic<unsigned> targetCount{},inFlight{};std::atomic<bool> enabled{};
std::array<Record,RecordLimit> records{},snapshot{};SRWLOCK recordsLock=SRWLOCK_INIT;
uint8_t* image{};void* source{};uint64_t world{};Callback native{};std::filesystem::path directory;FILE* report{};uint64_t total{},captureId{};bool f8Down{};unsigned overflow{};
constexpr std::array<size_t,MethodCount> Methods{0x1cbcc00,0x2708870,0x1ccd7a0,0x1cc65d0,0x1e6d4e0,0x1cd02b0,0x1cd3250,0x1cddf00,0x1cd9f70,0x1cda0d0,0x8085e0,0x1e6d4e0,0xb25e90,0x1e6d4e0,0x1cd0070,0x8085e0,0x1ce1ff0,0x1cd9b90,0x1cd9a00,0x1cdaef0,0x1ce2e40,0x1cd21c0,0x1ce34a0,0x1ce3550,0x1cd8490,0x1cd25d0,0x1ce05d0,0x1cdb4e0,0x1cdb330,0x1ceaab0,0x1e6d4e0,0x1cdccd0,0x1cd9840,0x1cdae70,0x1cda220,0x1cda500,0x1ce07a0,0x1ce08e0,0x1ce8d80,0x1ce9250,0x1e6d4e0,0x1ceae00,0x1ceaee0,0x1cd0b70,0x1cd0b10,0x1cd0a40,0x1ccb0d0,0x1ccbab0,0xb25e90,0x1cdb080,0x1cd8d60,0x1ce76b0,0x1cd7fb0};
struct Lock {Lock(){AcquireSRWLockExclusive(&recordsLock);}~Lock(){ReleaseSRWLockExclusive(&recordsLock);}};
struct Flight {Flight(){++inFlight;}~Flight(){--inFlight;}};
bool Read(void* out,const void* address,size_t size) noexcept {
 if(!out || !address || !size || size>0x10000)return false;
 __try{memcpy(out,address,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
uint64_t Q(const uint8_t* p,size_t o){uint64_t v{};memcpy(&v,p+o,8);return v;}
uint32_t D(const uint8_t* p,size_t o){uint32_t v{};memcpy(&v,p+o,4);return v;}
uint16_t W(const uint8_t* p,size_t o){uint16_t v{};memcpy(&v,p+o,2);return v;}
Target* Find(uint64_t entity,uint64_t table){
 unsigned count=targetCount.load(std::memory_order_acquire);
 for(unsigned i=0;i<count;++i){auto& t=targets[i];if(t.entity==entity && table==reinterpret_cast<uint64_t>(t.clone+1) && t.installed.load(std::memory_order_acquire))return &t;}return nullptr;
}
bool Identity(const Target& t,uint64_t& flags,uint32_t* currentState=nullptr){
 uint8_t e[0x138]{},h[16]{};uint64_t proxyOwner{};
 if(!Read(e,reinterpret_cast<void*>(t.entity),sizeof(e)) || Q(e,0)!=reinterpret_cast<uint64_t>(t.clone+1) || Q(e,0x60)!=world || Q(e,0xf8)!=t.proxy || Q(e,0x130)!=t.handle ||
 !Read(h,reinterpret_cast<void*>(t.handle),16) || Q(h,0)!=t.entity || !NativeHandleState63::SameLifetime(D(h,12),t.handleToken) || !Read(&proxyOwner,reinterpret_cast<void*>(t.proxy+0x90),8) || proxyOwner!=t.entity)return false;
 if(currentState)*currentState=D(h,12);flags=Q(e,0x88);return true;
}
void Put(Record& r){Lock lock;if(!enabled.load())return;r.id=++total;records[(r.id-1)%RecordLimit]=r;}
__declspec(noinline) uint64_t __fastcall Observer(void* a,void* b,void* c,void* d){
 if(!enabled.load(std::memory_order_acquire))return native(a,b,c,d);
 Flight flight;Record record{};uint64_t vt{},flags{};Target* t{};
 if(Read(&vt,a,8))t=Find(reinterpret_cast<uint64_t>(a),vt);
 // The bit7 setter performs this virtual call after writing the new bit7 state.
 // Other users of the same notification are forwarded without a stack capture.
 uint64_t caller=reinterpret_cast<uint64_t>(_ReturnAddress());
 bool take=t && caller==reinterpret_cast<uint64_t>(image)+0x1ce2b97 && Identity(*t,flags);
 if(take){NpcInputs63::Capture(record.inputs,reinterpret_cast<uint64_t>(image),world,t->entity,t->handle,t->handleToken);record.entity=t->entity;record.proxy=t->proxy;record.caller=caller;record.before=flags;record.tick=GetTickCount64();record.thread=GetCurrentThreadId();record.frameRead=Read(&record.frameBefore,static_cast<uint8_t*>(source)+0x294,4);
  void* stack[StackLimit]{};record.stackCount=RtlCaptureStackBackTrace(0,StackLimit,stack,nullptr);for(unsigned i=0;i<record.stackCount;++i)record.stack[i]=reinterpret_cast<uint64_t>(stack[i]);}
 // No locks, SEH, file I/O or metadata writes surround the original native call.
 uint64_t result=native(a,b,c,d);
 if(take){record.afterRead=Read(&record.after,static_cast<uint8_t*>(a)+0x88,8);record.frameRead=Read(&record.frameAfter,static_cast<uint8_t*>(source)+0x294,4) && record.frameRead;Put(record);}
 return result;
}
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool Replace(Target& t,bool attach) noexcept {
 __try {return InterlockedCompareExchangePointer(reinterpret_cast<void**>(t.entity),attach?static_cast<void*>(t.clone+1):t.original,attach?t.original:static_cast<void*>(t.clone+1))==(attach?t.original:static_cast<void*>(t.clone+1));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool HeadVisible(const uint8_t* raw,const uint8_t* planes){
 float lo[4]{},hi[4]{};memcpy(lo,raw,16);memcpy(hi,raw+16,16);
 for(unsigned j=0;j<3;++j)if(!std::isfinite(lo[j]) || !std::isfinite(hi[j]) || lo[j]>hi[j])return false;
 for(unsigned p=0;p<6;++p){float n[4]{};memcpy(n,planes+p*16,16);for(float x:n)if(!std::isfinite(x))return false;float minimum=INFINITY;
  for(unsigned mask=0;mask<8;++mask){float x=(mask&1)?hi[0]:lo[0],y=(mask&2)?hi[1]:lo[1],z=(mask&4)?hi[2]:lo[2];float dot=(x*n[0]+z*n[2])+(y*n[1]+n[3]);minimum=(std::min)(minimum,dot);}
  if(minimum>0)return false;
 }return true;
}
void Snapshot(uint64_t sequence,uint64_t tracking,uint32_t frame,const char* boundary){
 uint64_t end{};unsigned count{};{Lock lock;end=total;count=unsigned((std::min)(end,uint64_t(RecordLimit)));uint64_t first=end-count;for(unsigned i=0;i<count;++i)snapshot[i]=records[(first+i)%RecordLimit];}
 std::error_code error;std::filesystem::create_directories(directory,error);if(error)return;
 auto path=directory/(L"notification-"+std::to_wstring(++captureId)+L"-frame-"+std::to_wstring(frame)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"schema\":2,\"captureId\":%llu,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"total\":%llu,\"retained\":%u,\"evicted\":%llu,\"targets\":%u,\"targetOverflow\":%u,\"boundary\":\"%s\",\"imageBase\":\"%llx\",\"gameplayWorld\":\"%llx\",\"note\":\"Up to256 actual head-visible NPCs armed only by F8 proxy snapshots. Full53-method private cloned entity tables, only virtual138 observer changed. Native code/shared vtables and NPC flags/decisions unchanged. Notification is called after bit7 setter writes the requested state; before/after below surround the unchanged notification, not the setter. Return1ce2b97 verifies bit7 setter; stack above it identifies actual caller. Bounded native unwind recovers exact fec740/fed7c0 manager and actor inputs; raw blocks are notification-boundary observations, not prior decisions. Opaque frameInput pointer is not dereferenced; positionInput is the proven distance-reference vec4. Input refusal never changes native forwarding. Completed calls only; rolling512 overlap intentional. Clones retained until process exit for in-flight/destruction safety.\"}\n",captureId,sequence,tracking,frame,end,count,end-count,targetCount.load(),overflow,boundary,reinterpret_cast<uint64_t>(image),world);
 for(unsigned i=0;i<targetCount.load();++i){auto& t=targets[i];uint64_t flags{};bool valid=t.installed && Identity(t,flags);fprintf(f,"{\"event\":\"target\",\"index\":%u,\"entity\":\"%llx\",\"handle\":\"%llx\",\"handleToken\":%u,\"proxy\":\"%llx\",\"originalVtable\":\"%llx\",\"ownedVtable\":\"%llx\",\"installed\":%s,\"identityValid\":%s,\"flags\":\"%llx\"}\n",i,t.entity,t.handle,t.handleToken,t.proxy,reinterpret_cast<uint64_t>(t.original),reinterpret_cast<uint64_t>(t.clone+1),t.installed?"true":"false",valid?"true":"false",flags);}
 for(unsigned i=0;i<count;++i){auto& x=snapshot[i];fprintf(f,"{\"event\":\"notification\",\"id\":%llu,\"entity\":\"%llx\",\"proxy\":\"%llx\",\"caller\":\"%llx\",\"beforeNotificationFlags\":\"%llx\",\"afterNotificationFlags\":\"%llx\",\"disabled\":%s,\"afterRead\":%s,\"frameRead\":%s,\"frameBefore\":%u,\"frameAfter\":%u,\"tick\":%llu,\"thread\":%u,\"stack\":[",x.id,x.entity,x.proxy,x.caller,x.before,x.after,(x.before&128)?"true":"false",x.afterRead?"true":"false",x.frameRead?"true":"false",x.frameBefore,x.frameAfter,x.tick,x.thread);
  for(unsigned j=0;j<x.stackCount;++j)fprintf(f,"%s\"%llx\"",j?",":"",x.stack[j]);fprintf(f,"],\"inputs\":{\"status\":%u,\"recovered\":%s,\"parentRecovered\":%s,\"valid\":%s,\"depth\":%u,\"parentDepth\":%u,\"slot\":%u,\"poolCount\":%u,\"nativeCaller\":\"%llx\",\"manager\":\"%llx\",\"state\":\"%llx\",\"descriptor\":\"%llx\",\"reference\":\"%llx\",\"statePool\":\"%llx\",\"frameInput\":\"%llx\",\"positionInput\":\"%llx\",\"positionRead\":%s",x.inputs.status,x.inputs.recovered?"true":"false",x.inputs.parentRecovered?"true":"false",x.inputs.valid?"true":"false",x.inputs.depth,x.inputs.parentDepth,x.inputs.slot,x.inputs.poolCount,x.inputs.caller,x.inputs.manager,x.inputs.state,x.inputs.descriptor,x.inputs.reference,x.inputs.statePool,x.inputs.frameInput,x.inputs.positionInput,x.inputs.positionRead?"true":"false");
  const struct{const char* name;const uint8_t* raw;size_t size;} blocks[]={{"managerRaw",x.inputs.managerRaw,sizeof(x.inputs.managerRaw)},{"poolRaw",x.inputs.poolRaw,sizeof(x.inputs.poolRaw)},{"stateRaw",x.inputs.stateRaw,sizeof(x.inputs.stateRaw)},{"descriptorRaw",x.inputs.descriptorRaw,sizeof(x.inputs.descriptorRaw)},{"referenceRaw",x.inputs.referenceRaw,sizeof(x.inputs.referenceRaw)},{"positionRaw",x.inputs.positionRaw,sizeof(x.inputs.positionRaw)}};
  for(auto& block:blocks){fprintf(f,",\"%s\":\"",block.name);if(x.inputs.valid)for(size_t j=0;j<block.size;++j)fprintf(f,"%02x",block.raw[j]);fprintf(f,"\"");}fprintf(f,"}}\n");}
 fprintf(f,"{\"event\":\"end\",\"count\":%u,\"inFlight\":%u}\n",count,inFlight.load());fclose(f);
 fprintf(report,"NPC notification63 snapshot=%llu sourceFrame=%u targets=%u completed=%llu retained=%u\n",captureId,frame,targetCount.load(),end,count);fflush(report);
}
}
bool Armed(uint64_t entity,uint64_t handle,uint64_t proxy,uint32_t* registeredState,uint32_t* currentState){uint64_t vt{},flags{};if(!enabled.load() || !Read(&vt,reinterpret_cast<void*>(entity),8))return false;auto t=Find(entity,vt);if(!t || t->handle!=handle || t->proxy!=proxy)return false;if(registeredState)*registeredState=t->handleToken;return Identity(*t,flags,currentState);}
uint64_t CanonicalVtable(uint64_t entity,uint64_t observed){auto t=Find(entity,observed);return t?reinterpret_cast<uint64_t>(t->original):observed;}
bool Install(uint8_t* base,void* primary,void* rendererWorld,const std::filesystem::path& output,FILE* log){
 image=base;source=primary;report=log;directory=output/L"npc-notifications63";
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x1cb634e,"\x48\x8b\x80\x30\x01\x00\x00\x48\x89\x44\x24\x40\xf0\xff\x40\x0c\x48\x8d\x54\x24\x40\xeb\x2d",23},{0x1c83aaf,"\x48\x8b\x93\x30\x01\x00\x00\x8b\xc7\xf0\x0f\xc1\x42\x0c\xff\xc8\xa9\xff\xff\xff\x00\x75\x0c\x48\x8d\x0d\xa3\x80\x5d\x03\xe8\x9e\xd7\xa7\x00",35},{0xfec740,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x4c\x89\x4c\x24\x20\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xec\xa0\x00\x00\x00\x48\x8b\xb4\x24\xf0\x00\x00\x00\x44\x8b\xf2\x44\x8b\xe2\x4d\x8b\xe8\x44\x8b\xfa\x4c\x8b\xce\x45\x69\xf6\xf0\x00\x00\x00\x45\x69\xe4\xb0\x01\x00\x00\x4c\x03\xb1\x84\x03\x00\x00\x48\x8b\xe9",83},{0xfec9be,"\x48\x89\x44\x24\x20\xe8\x08\x78\xf2\x00",10},{0xfeca1c,"\x48\x8b\xce\xe8\x4c\x5d\xf2\x00\xe9\x4f\x01\x00\x00",13},{0xfecac9,"\xe8\x02\x77\xf2\x00\x41\x83\xa6\xa0\x00\x00\x00\xf7",13},{0xfed7c0,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x57\x41\x56\x41\x57\x48\x81\xec\x50\x02\x00\x00\x48\x8b\x41\x40\x8b\xea\x8b\xfa\x4d\x8b\xf9\x4d\x8b\xf0\x8b\xf2\x69\xed\x60\x03\x00\x00\x69\xff\xb0\x01\x00\x00\x48\x03\x68\x60\x48\x03\xb9\x58\x02\x00\x00\x48\x8b\xd9",69},{0xfed827,"\x48\x8b\x84\x24\xa8\x02\x00\x00\x4c\x8b\x13\x4c\x8b\xcd\x48\x89\x44\x24\x28\x48\x8b\x84\x24\x98\x02\x00\x00\x4c\x8b\xc7\x48\x89\x44\x24\x20\x41\xff\x92\x60\x02\x00\x00",42},{0xfe6f30,"\x48\x89\x5c\x24\x10\x48\x89\x6c\x24\x18\x48\x89\x74\x24\x20\x57\x48\x83\xec\x30\x44\x8b\xda\x49\x8b\xf9\x49\x8b\xe8\x8b\xf2\x48\x8b\xd9\x45\x69\xdb\xf0\x00\x00\x00\x4c\x03\x99\x84\x03\x00\x00\x49\x8b\x03\x4c\x63\x50\x0c\x49\xc1\xe2\x20\x49\xc1\xfa\x3f\x4c\x85\x10\x74\x11\x41\xf6\x83\xa0\x00\x00\x00\x01\x77\x07\x32\xc0\xe9\xb8\x00\x00\x00\x41\xf7\x83\xa0\x00\x00\x00\x00\x00\x04\x00\x77\xec\x0f\x28\x0d\x57\xf5\xe8\x01\x0f\x57\xc0\x4c\x89\x74\x24\x40\x4c\x8b\x74\x24\x60\x0f\x28\xd1\x0f\x55\xc8\x41\x0f\x54\x91\xe0\x01\x00\x00\x0f\x56\xd1\x41\x0f\x5c\x16\x0f\x59\xd2\x0f\x28\xc2\x0f\xc6\xc2\x4e\x0f\x58\xd0\x0f\x28\xc2\x0f\xc6\xc2\x11\x0f\x58\xc2\x0f\x2f\x05\xab",166},{0x3818de0,"\x01\x24\x0d\x00\x24\x64\x1c\x00\x24\x54\x1b\x00\x24\x34\x1a\x00\x24\x01\x14\x00\x1d\xf0\x1b\xe0\x19\xd0\x17\xc0\x15\x70\x00\x00",32},{0x3817a3c,"\x01\x1b\x0b\x00\x1b\x64\x50\x00\x1b\x54\x4f\x00\x1b\x34\x4e\x00\x1b\x01\x4a\x00\x14\xf0\x12\xe0\x10\x70\x00\x00",28},{0x1ce2b50,"\x40\x53\x48\x83\xec\x20\x48\x8b\xd9\x48\x8b\x89\x88\x00\x00\x00\x44\x0f\xb6\xc2\x48\x8b\xc1\x48\xc1\xe8\x07\x83\xe0\x01\x49\x3b\xc0\x74\x65\x48\x8b\x03\x49\xc1\xe0\x07\x4c\x33\xc1\x41\x81\xe0\x80\x00\x00\x00\x4c\x33\xc1\x48\x8b\xcb\x4c\x89\x83\x88\x00\x00\x00\xff\x90\x38\x01\x00\x00\x48\x8b\x8b\x88\x00\x00\x00\x48\x8b\xc1\x48\xc1\xe8\x2b\x83\xe0\x1f\x83\xf8\x1c\x77\x2b\xba\x00\x00\xb0\x12\x0f\xa3\xc2\x73\x21\x48\x8b\xc1\x48\xc1\xe8\x13\xa8\x01\x74\x16\x48\xc1\xe9\x35\xf6\xc1\x01\x74\x0d\x48\x8b\xcb\x48\x83\xc4\x20\x5b\xe9\x58\xa6\xff\xff\x48\x83\xc4\x20\x5b\xc3",142},{0x1ce9250,"\x40\x56\x57\x41\x57\x48\x83\xec\x20\x48\x8b\xf1\x48\x8b\x89\x88\x00\x00\x00\x33\xff\x0f\xb6\xc1\x4c\x89\x6c\x24\x50\xc0\xe8\x07\xa8\x01\x75\x0c\x48\xc1\xe9\x10\x8d\x47\x01\xf6\xc1\x01\x74\x02\x8b\xc7\x84\xc0\x48\x8b\x46\x18\x48\x89\x5c\x24\x40\x41\x0f\x94\xc7\x48\x89\x6c\x24\x48\x48\x85\xc0\x74\x70\x40\x38\x78\x60\x74\x6a\x48\x8b\xce\xe8\x37\x8f\x40\x00\x48\x8b\xd8\x48\x85\xc0\x74\x5a\x41\x0f\xb6\xef\x66\x66\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x48\x8b\x93\x88\x00\x00\x00\x48\x8b\xca\x48\xc1\xe9\x10\x83\xe1\x01\x48\x3b\xcd\x74\x25\x48\x8b\xc5\x48\x8b\xcb\x48\xc1\xe0\x10\x48\x33\xc2\x25\x00\x00\x01\x00\x48\x33\xc2\x48\x89\x83\x88\x00\x00\x00\x48\x8b\x03\xff\x90\x38\x01\x00\x00\x48\x8b\xcb\xe8\x7d\x9a\x40\x00\x48\x8b\xd8\x48\x85\xc0\x75\xb5\x48\x8d\x8e\x51\x01\x00\x00\x48\x8b\xd6\xe8\x56\xc8\x61\xff\x4c\x8b\xe8\x48\x85\xc0\x0f\x84\x8c\x00\x00\x00\x0f\xb7\x88\xba\x02\x00\x00\x85\xc9\x0f\x84\x7d\x00\x00\x00\x4c\x89\x74\x24\x58\x8b\xe9\x4c\x8b\xf7\x90\x49\x8b\x8d\xb0\x02\x00\x00\x49\x8b\x1c\x0e\xe8\x30\xd3\xa0\x00\x48\x63\x4b\x0c\x4c\x8b\x03\x48\xc1\xe1\x20\x48\xc1\xf9\x3f\x48\x0b\xc8\x4c\x23\xc1\x74\x3d\x49\x8b\x88\x88\x00\x00\x00\x41\x0f\xb6\xd7\x48\x8b\xc1\x48\xc1\xe8\x10\x83\xe0\x01\x48\x3b\xc2\x74\x23\x49\x8b\x00\x48\xc1\xe2\x10\x48\x33\xd1\x81\xe2\x00\x00\x01\x00\x48\x33\xd1\x49\x8b\xc8\x49\x89\x90\x88\x00\x00\x00\xff\x90\x38\x01\x00\x00\x49\x83\xc6\x10\x48\xff\xcd\x75\x93\x4c\x8b\x74\x24\x58\x48\x8b\xce\xe8\x16\x03\xfe\xff\x4c\x8b\x6c\x24\x50\x48\x8b\x6c\x24\x48\x66\x3b\xbe\x82\x00\x00\x00\x73\x2f\x8b\xdf\x90\x48\x8b\x46\x78\x8b\xcb\x48\x8b\x0c\x01\x48\x85\xc9\x74\x0d\x48\x8b\x01\x41\x0f\xb6\xd7\xff\x90\xe8\x01\x00\x00\x0f\xb7\x86\x82\x00\x00\x00\xff\xc7\x83\xc3\x08\x3b\xf8\x72\xd4\x48\x8b\x5c\x24\x40\x48\x83\xc4\x20\x41\x5f\x5f\x5e\xc3\xcc\xcc\xcc\xcc\xcc\xcc\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x48\x8b\xf1\xe8\x2e\xfe\xff\xff\x0f\xb7\x96\xba\x01\x00\x00\x48\x8b\xbe\xb0\x01\x00\x00\xc1\xe2\x03\x48\x03\xd7\x48\x3b\xfa\x0f\x84\x9a\x00\x00\x00\x48\x89\x5c\x24\x30\x48\x8b\x1f\xe8\x34\xd2\xa0\x00\x48\x63\x4b\x0c\x4c\x8b\x03\x48\xc1\xe1\x20\x48\xc1\xf9\x3f\x48\x0b\xc8\x4c\x23\xc1\x74",530}};
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(report,"REFUSED NPC notification63 signature=%zx\n",g.rva);fflush(report);return false;}
 auto original=reinterpret_cast<void**>(base+0x3017d60);for(unsigned i=0;i<MethodCount;++i)if(original[i]!=base+Methods[i])return false;if(original[MethodCount]!=nullptr)return false;
 uint64_t graph{},graphOwner{},rendererType{},ownerType{},handle{},handleOwner{};uint32_t token{};
 if(!source || !rendererWorld || !Read(&graph,static_cast<uint8_t*>(source)+0x440,8) || !graph || !Read(&graphOwner,reinterpret_cast<void*>(graph+0x10),8) || graphOwner!=reinterpret_cast<uint64_t>(rendererWorld) ||
 !Read(&rendererType,rendererWorld,8) || rendererType!=reinterpret_cast<uint64_t>(base)+0x3094180 || !Read(&world,static_cast<uint8_t*>(rendererWorld)+0x28,8) || !world || !Read(&ownerType,reinterpret_cast<void*>(world),8) || ownerType!=reinterpret_cast<uint64_t>(base)+0x30225e0 ||
 !Read(&handle,static_cast<uint8_t*>(source)+0x250,8) || !handle || !Read(&handleOwner,reinterpret_cast<void*>(handle),8) || handleOwner!=world || !Read(&token,reinterpret_cast<void*>(handle+12),4) || int32_t(token)>=0)return false;
#ifdef ACUNITYVR_NPC63_CHECK
 extern Callback checkNative;native=checkNative;
#else
 native=reinterpret_cast<Callback>(base+0x1ce9250);
#endif
 enabled=true;fprintf(report,"NPC notification63 prepared exact full53-method generic entity table, unchanged native138/1ce9250 notification. F8 arms up to256 head-visible NPCs through writable entity vptr copies; original code/shared tables untouched. Native4GPR args/integer result forwarded once, completed bit7 caller stacks/strict native manager-state inputs bounded512. No NPC flags/camera/LOD decisions changed.\n");fflush(report);return true;
}
// Read-only validation for the real correction; independent of F8 observer arming.
bool LiveNpcIdentity(uint64_t entity,uint64_t handle,uint64_t proxy,float lo[3],float hi[3],uint32_t* currentState){
 if(!enabled.load() || !lo || !hi)return false;
 uint8_t e[0x138]{},h[16]{},p[0xb0]{},eh[0x138]{},hh[16]{};uint64_t owner{};
 if(!Read(e,reinterpret_cast<void*>(entity),sizeof(e)) || CanonicalVtable(entity,Q(e,0))!=reinterpret_cast<uint64_t>(image)+0x3017d60 || Q(e,0x60)!=world || Q(e,0x130)!=handle || Q(e,0xf8)!=proxy || (Q(e,0x88)&0x80) || !Read(h,reinterpret_cast<void*>(handle),16) || Q(h,0)!=entity || !NativeHandleState63::Live(D(h,12)) || !Read(p,reinterpret_cast<void*>(proxy),sizeof(p)) || Q(p,0x90)!=entity || (D(p,0x2c)&15))return false;
 if(auto observed=Find(entity,Q(e,0))){uint64_t flags{};if(!Identity(*observed,flags) || observed->handle!=handle || observed->proxy!=proxy)return false;}
 uint16_t count=W(e,0x82);uint64_t parts[128]{};bool biped=false,skeleton=false,player=false;
 if(!count || count>128 || !Read(parts,reinterpret_cast<void*>(Q(e,0x78)),size_t(count)*8))return false;
 for(unsigned i=0;i<count;++i){if(!parts[i])continue;uint8_t c[24]{};if(!Read(c,reinterpret_cast<void*>(parts[i]),sizeof(c)) || Q(c,0x10)!=entity)return false;auto vt=Q(c,0);biped|=vt==reinterpret_cast<uint64_t>(image)+0x3051e60;skeleton|=vt==reinterpret_cast<uint64_t>(image)+0x2e76630;player|=vt==reinterpret_cast<uint64_t>(image)+0x304d9e0 || vt==reinterpret_cast<uint64_t>(image)+0x3050b00;}
 if(!biped || !skeleton || player)return false;
 for(unsigned j=0;j<3;++j){memcpy(lo+j,p+j*4,4);memcpy(hi+j,p+16+j*4,4);if(!std::isfinite(lo[j]) || !std::isfinite(hi[j]) || lo[j]>hi[j] || hi[j]-lo[j]>100)return false;}
 if(!Read(eh,reinterpret_cast<void*>(entity),sizeof(eh)) || Q(eh,0)!=Q(e,0) || Q(eh,0x60)!=world || Q(eh,0xf8)!=proxy || Q(eh,0x130)!=handle || Q(eh,0x78)!=Q(e,0x78) || W(eh,0x82)!=count || (Q(eh,0x88)&0x80) || !Read(hh,reinterpret_cast<void*>(handle),16) || memcmp(h,hh,12) || !NativeHandleState63::SameLifetime(D(h,12),D(hh,12)) || !Read(&owner,reinterpret_cast<void*>(proxy+0x90),8) || owner!=entity)return false;
 if(currentState)*currentState=D(hh,12);return true;
}
void ConsiderProxy(uint64_t proxy,const uint8_t* planes){
 if(!enabled.load() || !planes || !proxy)return;
 uint8_t p[0xb0]{},e[0x138]{},h[16]{};
 if(!Read(p,reinterpret_cast<void*>(proxy),sizeof(p)) || (D(p,0x2c)&15)!=0 || !HeadVisible(p,planes))return;
 uint64_t entity=Q(p,0x90);if(!entity || !Read(e,reinterpret_cast<void*>(entity),sizeof(e)))return;
 if(Find(entity,Q(e,0)))return;
 if(Q(e,0)!=reinterpret_cast<uint64_t>(image)+0x3017d60 || Q(e,0x60)!=world || Q(e,0xf8)!=proxy)return;
 uint64_t handle=Q(e,0x130);if(!handle || !Read(h,reinterpret_cast<void*>(handle),16) || Q(h,0)!=entity || !NativeHandleState63::Live(D(h,12)))return;
 uint16_t count=W(e,0x82);uint64_t parts[128]{};bool biped=false,skeleton=false,player=false;
 if(!count || count>128 || !Read(parts,reinterpret_cast<void*>(Q(e,0x78)),size_t(count)*8))return;
 for(unsigned i=0;i<count;++i){if(!parts[i])continue;uint8_t component[24]{};if(!Read(component,reinterpret_cast<void*>(parts[i]),24) || Q(component,0x10)!=entity)return;auto v=Q(component,0);biped|=v==reinterpret_cast<uint64_t>(image)+0x3051e60;skeleton|=v==reinterpret_cast<uint64_t>(image)+0x2e76630;player|=v==reinterpret_cast<uint64_t>(image)+0x304d9e0 || v==reinterpret_cast<uint64_t>(image)+0x3050b00;}
 if(!biped || !skeleton || player || !Writable(reinterpret_cast<void*>(entity)))return;
 unsigned index=targetCount.load();if(index==TargetLimit){++overflow;return;}
 auto& t=targets[index];t.entity=entity;t.handle=handle;t.proxy=proxy;t.handleToken=D(h,12);t.original=image+0x3017d60;memcpy(t.clone,reinterpret_cast<void**>(t.original)-1,sizeof(t.clone));t.clone[1+ActivationSlot]=reinterpret_cast<void*>(Observer);
 t.installed=true;targetCount.store(index+1,std::memory_order_release);
 if(!Replace(t,true)){t.installed=false;return;}
 fprintf(report,"NPC notification63 armed target=%u entity=%llx proxy=%llx handle=%llx privateTable=%p\n",index,entity,proxy,handle,t.clone+1);fflush(report);
}
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame){
#ifdef ACUNITYVR_NPC63_CHECK
 extern bool checkF8;bool down=checkF8;
#else
 bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
#endif
 bool pressed=down&&!f8Down;f8Down=down;if(enabled.load() && pressed)Snapshot(sequence,tracking,frame,"F8 source boundary before native eye culling");
}
void Stop(){
 enabled=false;unsigned owned=0,restored=0,identityRefused=0,foreign=0;
 for(unsigned i=0;i<targetCount.load();++i){auto& t=targets[i];if(!t.installed)continue;uint64_t table{},flags{};
  if(!Read(&table,reinterpret_cast<void*>(t.entity),8) || table!=reinterpret_cast<uint64_t>(t.clone+1)){++foreign;continue;}++owned;
  if(!Identity(t,flags)){++identityRefused;continue;}if(Replace(t,false)){++restored;t.installed=false;}}
 uint32_t frame{};Read(&frame,static_cast<uint8_t*>(source)+0x294,4);if(report){Snapshot(0,0,frame,"manual stop");fprintf(report,"NPC notification63 retired owned=%u restored=%u identityRefused=%u foreignOrGone=%u inFlight=%u; all private tables retained until process exit\n",owned,restored,identityRefused,foreign,inFlight.load());fflush(report);}
}
}
