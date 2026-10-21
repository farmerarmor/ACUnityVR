// F8-only read-only inputs/output flags at exact native entity-visibility callbacks.
// Native callbacks always forward once. No view/frustum/entity data is changed.
#include "EntityVisibility41.h"
#include "GpuCapture.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <share.h>
namespace EntityVisibility41 {
using acu::Field;
using Prepare=void(__fastcall*)(void*);using Update=void(__fastcall*)(void*,void*);
constexpr unsigned CameraLimit=16,EntityLimit=1024;
struct Camera {uint64_t frustum{};int view=-1;uint8_t raw[64]{},planes[0xc0]{};bool read{};};
struct View {uint64_t address{},frustum{},state{};uint32_t frame{};uint8_t raw[0x440]{},planes[0xc0]{},statePrefix[0x180]{};bool read{};};
struct Entity {uint64_t address{},vtable{},flagsBefore{},flagsAfter{},boundsTransform{};uint8_t bounds[0x28]{},transform[64]{};uint64_t frustumHashes[3]{};bool before{},after{},transformRead{},frustaRead{};};
uint8_t* image{};void* source{};void* owner{};void* task{};std::atomic<void*> views[3]{};void** prepareSlot{},**updateSlot{};
Prepare nativePrepare{};Update nativeUpdate{};std::filesystem::path directory;FILE* report{};
std::atomic<bool> enabled{},capturing{};std::atomic<unsigned> inFlight{},attempts{},finished{};
std::array<Camera,CameraLimit> cameras{},updateCameras{};std::atomic<unsigned> updateCameraStatus{};std::array<View,3> viewInputs{};std::array<Entity,EntityLimit> entities{};
unsigned cameraCount{},viewCount{},invalidInputs{};uint64_t captureId{},tick{},recordPointer{};uint32_t sourceFrame{};std::atomic<bool> pendingFile{};bool f8Down{},preparedRead{};
SRWLOCK lifecycle=SRWLOCK_INIT;
struct Lock{Lock(){AcquireSRWLockExclusive(&lifecycle);}~Lock(){ReleaseSRWLockExclusive(&lifecycle);}};
// Only metadata reads use SEH; native game calls execute outside it.
bool Read(void* target,const void* address,size_t size) noexcept {
 if(!address || !target || !size || size>0x10000)return false;
 __try{memcpy(target,address,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
__declspec(noinline) bool Identity(uint8_t* base,void* s,void** outOwner,void** outTask) noexcept {__try{
 if(Field<void*>(s,0x420)!=Field<void*>(base,0x523c6d8))return false;
 auto handle=Field<void*>(s,0x250);if(!handle || int32_t(Field<uint32_t>(handle,0xc))>=0)return false;
 auto o=Field<void*>(handle,0);auto t=Field<uint8_t*>(o,0x7d0);if(!o || !t || Field<void*>(t,0)!=o)return false;
 if(Field<void*>(t,0x850)!=base+0x1d6c8b0 || Field<void*>(t,0xa20)!=base+0x1ce8a70 ||
  (Field<uintptr_t>(t,0x860)&~uintptr_t(3))!=reinterpret_cast<uintptr_t>(o) || Field<uint8_t*>(t,0xa30)!=static_cast<uint8_t*>(o)+0xa28)return false;
 *outOwner=o;*outTask=t;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
void Hex(FILE* f,const uint8_t* p,size_t size){fputc('"',f);for(size_t i=0;i<size;++i)fprintf(f,"%02x",p[i]);fputc('"',f);}
void Flush(){
 if(!pendingFile || inFlight.load()!=0)return;pendingFile=false;std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return;
 auto path=directory/(L"entity-"+std::to_wstring(captureId)+L"-frame-"+std::to_wstring(sourceFrame)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"captureId\":%llu,\"sourceFrame\":%u,\"tick\":%llu,\"owner\":\"%llx\",\"task\":\"%llx\",\"records\":\"%llx\",\"cameraCount\":%u,\"preparedRead\":%s,\"boundary\":\"after native PrepareUpdateEntitiesCullingFlags, before dependent native UpdateEntitiesCullingFlags jobs\",\"note\":\"F8 key sampled in earlier task; match frame stamps with later render captures. Read-only snapshot, no visibility changes. Entity subset bounded1024.\"}\n",captureId,sourceFrame,tick,reinterpret_cast<uintptr_t>(owner),reinterpret_cast<uintptr_t>(task),recordPointer,cameraCount,preparedRead?"true":"false");
 for(unsigned i=0;i<viewCount;++i){auto& v=viewInputs[i];fprintf(f,"{\"event\":\"view\",\"index\":%u,\"address\":\"%llx\",\"frustum\":\"%llx\",\"state\":\"%llx\",\"viewFrame\":%u,\"readOK\":%s,\"raw\":",i,v.address,v.frustum,v.state,v.frame,v.read?"true":"false");Hex(f,v.raw,sizeof(v.raw));fprintf(f,",\"planes\":");Hex(f,v.planes,sizeof(v.planes));fprintf(f,",\"statePrefix\":");Hex(f,v.statePrefix,sizeof(v.statePrefix));fprintf(f,"}\n");}
 for(unsigned i=0;i<cameraCount;++i){auto& c=cameras[i];fprintf(f,"{\"event\":\"camera\",\"index\":%u,\"view\":%d,\"frustum\":\"%llx\",\"readOK\":%s,\"raw\":",i,c.view,c.frustum,c.read?"true":"false");Hex(f,c.raw,sizeof(c.raw));fprintf(f,",\"planes\":");Hex(f,c.planes,sizeof(c.planes));fprintf(f,"}\n");}
 fprintf(f,"{\"event\":\"workerInputStatus\",\"status\":%u,\"note\":\"First native UpdateEntitiesCullingFlags entry before forwarding; per-entity frustum hashes detect later changes. No native frustum locks are acquired.\"}\n",updateCameraStatus.load());
 if(updateCameraStatus==2)for(unsigned i=0;i<cameraCount;++i){auto& c=updateCameras[i];fprintf(f,"{\"event\":\"cameraAtUpdate\",\"index\":%u,\"view\":%d,\"frustum\":\"%llx\",\"readOK\":%s,\"raw\":",i,c.view,c.frustum,c.read?"true":"false");Hex(f,c.raw,sizeof(c.raw));fprintf(f,",\"planes\":");Hex(f,c.planes,sizeof(c.planes));fprintf(f,"}\n");}
 unsigned saved=(std::min)(attempts.load(),EntityLimit),complete=0;
 for(unsigned i=0;i<saved;++i){auto& e=entities[i];complete+=e.before&&e.after;fprintf(f,"{\"event\":\"entity\",\"index\":%u,\"address\":\"%llx\",\"vtable\":\"%llx\",\"flagsBefore\":\"%llx\",\"flagsAfter\":\"%llx\",\"beforeOK\":%s,\"afterOK\":%s,\"bounds\":",i,e.address,e.vtable,e.flagsBefore,e.flagsAfter,e.before?"true":"false",e.after?"true":"false");Hex(f,e.bounds,sizeof(e.bounds));fprintf(f,",\"transform\":\"%llx\",\"transformRead\":%s,\"transformBytes\":",e.boundsTransform,e.transformRead?"true":"false");Hex(f,e.transform,sizeof(e.transform));fprintf(f,",\"frustaRead\":%s,\"frustumHashes\":[\"%llx\",\"%llx\",\"%llx\"]}\n",e.frustaRead?"true":"false",e.frustumHashes[0],e.frustumHashes[1],e.frustumHashes[2]);}
 fprintf(f,"{\"event\":\"end\",\"attempts\":%u,\"completedJobs\":%u,\"saved\":%u,\"validBeforeAfter\":%u,\"invalidInputs\":%u}\n",attempts.load(),finished.load(),saved,complete,invalidInputs);fclose(f);
 fprintf(report,"F8 entity visibility41 capture=%llu sourceFrame=%u cameras=%u jobs=%u saved=%u valid=%u invalidInputs=%u; earlier-task input evidence\n",captureId,sourceFrame,cameraCount,finished.load(),saved,complete,invalidInputs);fflush(report);
}
uint64_t Hash(const uint8_t* data,size_t size){uint64_t value=14695981039346656037ULL;for(size_t i=0;i<size;++i){value^=data[i];value*=1099511628211ULL;}return value;}
bool WorkerFrusta(void* records,Entity* e){
 uint8_t header[16]{};if(!Read(header,records,sizeof(header)))return false;uint64_t pointer{};uint16_t count{};memcpy(&pointer,header,8);memcpy(&count,header+10,2);
 if(pointer!=recordPointer || count!=cameraCount)return false;
 for(unsigned i=0;i<(std::min)(count,uint16_t(3));++i){uint64_t ptr{};uint8_t planes[96]{};
  if(!Read(&ptr,reinterpret_cast<void*>(pointer+i*64+32),8) || ptr!=cameras[i].frustum || !Read(planes,reinterpret_cast<void*>(ptr),sizeof(planes)))return false;
  e->frustumHashes[i]=Hash(planes,sizeof(planes));}
 return true;
}
void SnapshotFirstWorker(void* records){
 unsigned expected=0;if(!updateCameraStatus.compare_exchange_strong(expected,1))return;
 uint8_t header[16]{};uint64_t pointer{};uint16_t count{};bool ok=Read(header,records,sizeof(header));memcpy(&pointer,header,8);memcpy(&count,header+10,2);
 ok=ok && pointer==recordPointer && count==cameraCount;
 if(ok)for(unsigned i=0;i<cameraCount;++i){auto& c=updateCameras[i];c={};c.view=-1;c.read=Read(c.raw,reinterpret_cast<void*>(pointer+i*64),sizeof(c.raw));
  if(c.read){memcpy(&c.frustum,c.raw+32,8);for(unsigned v=0;v<viewCount;++v)if(viewInputs[v].frustum==c.frustum)c.view=int(v);c.read=Read(c.planes,reinterpret_cast<void*>(c.frustum),sizeof(c.planes));}ok=ok && c.read;}
 updateCameraStatus=ok?2:3;
}
bool SnapshotPrepared(void* o){
 viewCount=0;for(unsigned i=0;i<3;++i){auto current=views[i].load();if(!current)break;auto& v=viewInputs[i];v={};v.address=reinterpret_cast<uintptr_t>(current);
  v.read=Read(v.raw,current,sizeof(v.raw));if(v.read){memcpy(&v.frustum,v.raw+0x428,8);memcpy(&v.state,v.raw+0x430,8);memcpy(&v.frame,v.raw+0x294,4);
   v.read=Read(v.planes,reinterpret_cast<void*>(v.frustum),sizeof(v.planes)) && Read(v.statePrefix,reinterpret_cast<void*>(v.state),sizeof(v.statePrefix));}++viewCount;}
 uint8_t header[16]{};if(!Read(header,static_cast<uint8_t*>(o)+0xa28,sizeof(header)))return false;
 unsigned n{};memcpy(&recordPointer,header,8);uint16_t shortCount{};memcpy(&shortCount,header+10,2);n=shortCount;if(!n || n>CameraLimit)return false;cameraCount=n;
 for(unsigned i=0;i<n;++i){auto& c=cameras[i];c={};c.view=-1;c.read=Read(c.raw,reinterpret_cast<void*>(recordPointer+i*64),sizeof(c.raw));if(!c.read)continue;
  memcpy(&c.frustum,c.raw+32,8);for(unsigned v=0;v<viewCount;++v)if(viewInputs[v].frustum==c.frustum)c.view=int(v);
  c.read=Read(c.planes,reinterpret_cast<void*>(c.frustum),sizeof(c.planes));}
 for(unsigned i=0;i<cameraCount;++i)if(!cameras[i].read){++invalidInputs;return false;}
 return true;
}
bool F8Down(){
#ifdef ACUNITYVR_ENTITY41_CHECK
 extern bool checkF8;return checkF8;
#else
 return (GetAsyncKeyState(VK_F8)&0x8000)!=0;
#endif
}
void __fastcall HookPrepare(void* o){
 // Close the previous capture only after its dependent jobs have returned.
 if(pendingFile.load()){Lock lock;capturing=false;Flush();}
 nativePrepare(o);
 if(!enabled || o!=owner)return;
 bool down=F8Down(),pressed=down&&!f8Down;f8Down=down;
 if(!pressed)return;Lock lock;if(pendingFile || inFlight.load())return;
 ++captureId;tick=GetTickCount64();Read(&sourceFrame,static_cast<uint8_t*>(source)+0x294,sizeof(sourceFrame));attempts=finished=0;updateCameraStatus=0;cameraCount=viewCount=invalidInputs=0;
 preparedRead=SnapshotPrepared(o);pendingFile=true;capturing=preparedRead;
}
void __fastcall HookUpdate(void* entity,void* records){
 bool take=false;unsigned id=EntityLimit;
 if(capturing && records==static_cast<uint8_t*>(owner)+0xa28){
  ++inFlight;if(capturing){id=attempts.fetch_add(1);take=id<EntityLimit;}if(!take)--inFlight;
 }
 if(take){SnapshotFirstWorker(records);auto& e=entities[id];e={};e.frustaRead=WorkerFrusta(records,&e);e.address=reinterpret_cast<uintptr_t>(entity);
  e.before=Read(&e.vtable,entity,8) && Read(&e.flagsBefore,static_cast<uint8_t*>(entity)+0x88,8) && Read(e.bounds,static_cast<uint8_t*>(entity)+0x98,sizeof(e.bounds));
  if(e.before){memcpy(&e.boundsTransform,e.bounds+0x20,8);e.transformRead=e.boundsTransform && Read(e.transform,reinterpret_cast<void*>(e.boundsTransform),sizeof(e.transform));}
 }
 nativeUpdate(entity,records);
 if(take){auto& e=entities[id];e.after=Read(&e.flagsAfter,static_cast<uint8_t*>(entity)+0x88,8);++finished;--inFlight;}
}
bool Install(uint8_t* base,void* s,const std::filesystem::path& output,FILE* log){
 image=base;source=s;report=log;views[0]=s;directory=output/L"entity-visibility";
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x1d53511,"\x48\x8b\x8b\xd0\x07\x00\x00\x48\x8b\xd3\x48\x83\xc4\x40\x41\x5d\x5f\x5e\x5d\x5b\xe9\x96\xd9\x00\x00",25},
{0x1d66480,"\x48\x89\x4c\x24\x08\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8d\xac\x24\x98\xfe\xff\xff\x48\x81\xec\x68\x02\x00\x00\x48\x8d\xb9\x38\x0a\x00\x00",39},
{0x1d66640,"\x48\x8b\xbd\xb0\x01\x00\x00",7},
{0x1d66bf4,"\x4c\x8d\x05\xb5\x5c\x00\x00\x48\x0b\x17\x4c\x89\x87\x50\x08\x00\x00",17},
{0x1d66d6e,"\x48\x8d\x0d\xfb\x1c\xf8\xff\x48\x89\x8f\x20\x0a\x00\x00\x48\x89\x87\x28\x0a\x00\x00\x48\x8b\x07\x48\x05",26},
{0x1d6c8b0,"\x40\x53\x55\x56\x57\x41\x55\x41\x56\x41\x57\x48\x81\xec\x20\x01\x00\x00\x4c\x8b\x2d\xa7\xe1\x4c\x03",25},
{0x1d6cb27,"\x41\xbe\x01\x00\x00\x00\x48\x8b\x81\x28\x04\x00\x00\x48\x89\x43\x20",17},
{0x1ce8a70,"\x4c\x8b\xdc\x56\x57\x48\x81\xec\x98\x00\x00\x00\x4c\x8b\x89\x88\x00\x00\x00\x0f\x29\x7c\x24\x60\x49\x89\x5b\x08\x49\x89\x6b\x10",32},
{0x1ce8bc2,"\x8b\x8f\xd4\x00\x00\x00\xba\x01\x00\x00\x00\x83\xe1\x07\xd3\xe2\x84\x53\x3c\x74\x0c\x0f\x2f\x73\x34\x72\x09\x0f\x2f\x73\x38\x77\x03\x45\x32\xff\x48\x8b\x4b\x20\x4c\x8d\x43\x10\x48\x8d\x97\x98\x00\x00\x00\xe8\x56\x61\x54\x00\x84\xc0\x75\x05\x45\x32\xe4\xeb\x05\x45\x84\xe4\x75\x10\x80\x7b\x31\x00\x40\x0f\xb6\xf6\xb8\x01\x00\x00\x00\x0f\x44\xf0\x41\x0f\xb7\x45\x0a\xff\xc5\x41\x83\xc6\x40\x48\x8d\x8f\x98\x00\x00\x00\x3b\xe8",106},
{0x1ce8c79,"\x4c\x8b\xbc\x24\x80\x00\x00\x00\xbb\x00\x00\x00\x00\x48\xb8\xef\xff\xff\xff\xbf\xff\xff\xff\x48\x21\x87\x88\x00\x00\x00\x40\x0f\xb6\xc6\x83\xe0\x01\x48\xc1\xe0\x22\x45\x84\xe4\x4c\x8b\xa4\x24\xc0\x00\x00\x00\x0f\x94\xc3\x83\xe3\x01\x48\x0b\xc3\x48\x8b\x9c\x24\xb0\x00\x00\x00\x48\x0f\xba\xe8\x13\x48\xc1\xe0\x04\x48\x09\x87\x88\x00\x00\x00",85}};
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(report,"REFUSED entity visibility signature=%zx\n",g.rva);fflush(report);return false;}
 if(!Identity(base,s,&owner,&task))return false;
 prepareSlot=reinterpret_cast<void**>(static_cast<uint8_t*>(task)+0x850);updateSlot=reinterpret_cast<void**>(static_cast<uint8_t*>(task)+0xa20);
 if(!Writable(prepareSlot) || !Writable(updateSlot))return false;nativePrepare=reinterpret_cast<Prepare>(base+0x1d6c8b0);nativeUpdate=reinterpret_cast<Update>(base+0x1ce8a70);
 if(InterlockedCompareExchangePointer(updateSlot,reinterpret_cast<void*>(HookUpdate),reinterpret_cast<void*>(nativeUpdate))!=reinterpret_cast<void*>(nativeUpdate))return false;
 if(InterlockedCompareExchangePointer(prepareSlot,reinterpret_cast<void*>(HookPrepare),reinterpret_cast<void*>(nativePrepare))!=reinterpret_cast<void*>(nativePrepare)){InterlockedCompareExchangePointer(updateSlot,reinterpret_cast<void*>(nativeUpdate),reinterpret_cast<void*>(HookUpdate));return false;}
 enabled=true;fprintf(report,"Entity visibility41 installed exact owner=%p graph=%p prepareSlot=%p updateSlot=%p; callback replacements only, native forwarding once; F8-only data reads, no view/entity/frustum writes\n",owner,task,prepareSlot,updateSlot);fflush(report);return true;
}
void SetEyes(void* left,void* right){views[1]=left;views[2]=right;}
void Stop(){
 enabled=false;capturing=false;Lock lock;
 if(prepareSlot)InterlockedCompareExchangePointer(prepareSlot,reinterpret_cast<void*>(nativePrepare),reinterpret_cast<void*>(HookPrepare));
 if(updateSlot)InterlockedCompareExchangePointer(updateSlot,reinterpret_cast<void*>(nativeUpdate),reinterpret_cast<void*>(HookUpdate));
 Flush();if(report && (prepareSlot || updateSlot)){fprintf(report,"Entity visibility41 retired prepareRestored=%d updateRestored=%d retained inFlight=%u\n",prepareSlot && *prepareSlot==reinterpret_cast<void*>(nativePrepare),updateSlot && *updateSlot==reinterpret_cast<void*>(nativeUpdate),inFlight.load());fflush(report);}
 prepareSlot=updateSlot=nullptr;
}
}
