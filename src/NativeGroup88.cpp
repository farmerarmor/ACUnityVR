// Private copies retain the native group-selection code and unwind metadata.
// Only camera calls inside the copy can receive a scoped headset input.
#include "NativeGroup88.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <limits>

namespace NativeGroup88 { namespace {
struct Body {uint32_t original,size,destination;const uint8_t* bytes;};
struct Relocation {uint32_t field,next,target,replacement;};
struct Unwind {uint32_t original,size,destination;const uint8_t* bytes;uint32_t chainAt;RUNTIME_FUNCTION chain;};
struct Guard {uint32_t original,size;const uint8_t* bytes;};
#include "NativeGroup88Data.h"
constexpr size_t AllocationSize=0x10000;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
using Getter=void*(__fastcall*)(void*,void*,int);
uint8_t* image{};uint8_t* block{};RUNTIME_FUNCTION registered[_countof(functions)]{};
Getter nativePosition{},nativeDirection{};Callback group{};
std::atomic<uint64_t> reads{};
struct Camera {alignas(16) uint8_t bytes[32];Camera* previous;};
thread_local Camera* current{};

void* __fastcall Position(void* camera,void* out,int flags){
 auto value=nativePosition(camera,out,flags);
 if(current && camera==*reinterpret_cast<void**>(image+0x521a768)){++reads;return current->bytes;}return value;
}
void* __fastcall Direction(void* camera,void* out,int flags){
 auto value=nativeDirection(camera,out,flags);
 if(current && camera==*reinterpret_cast<void**>(image+0x521a768)){++reads;return current->bytes+16;}return value;
}
uint8_t* AllocateNear(uint8_t* base,FILE* log){
 SYSTEM_INFO info{};GetSystemInfo(&info);auto gran=uint64_t(info.dwAllocationGranularity);
 // Intersect the signed rel32 intervals of every external copied operand.
 // Camera helper thunks and the private group call are internal to the copy.
 int64_t low=int64_t(info.lpMinimumApplicationAddress),high=int64_t(info.lpMaximumApplicationAddress)-AllocationSize;
 for(auto& r:relocations)if(!r.replacement){auto target=int64_t(base)+r.target-r.next;low=(std::max)(low,target-std::numeric_limits<int32_t>::max());high=(std::min)(high,target-int64_t(std::numeric_limits<int32_t>::min()));}
 if(low>high){fprintf(log,"REFUSED NativeGroup88 empty relocation interval\n");return nullptr;}
 auto first=(uint64_t(low)+gran-1)&~(gran-1),last=uint64_t(high)&~(gran-1);unsigned attempts{},regions{};DWORD lastError{};
 auto search=[&](uint64_t from,uint64_t through)->uint8_t*{
  for(auto cursor=from;cursor<=through;){MEMORY_BASIC_INFORMATION region{};
   if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))){lastError=GetLastError();break;}++regions;
   auto end=uint64_t(region.BaseAddress)+region.RegionSize;if(end<=cursor)break;
   if(region.State==MEM_FREE){auto candidate=((std::max)(cursor,uint64_t(region.BaseAddress))+gran-1)&~(gran-1);
    for(unsigned retry=0;retry<4 && candidate<=through && candidate+AllocationSize<=end;++retry,candidate+=gran){++attempts;
     if(auto p=VirtualAlloc(reinterpret_cast<void*>(candidate),AllocationSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))return static_cast<uint8_t*>(p);
     lastError=GetLastError();
    }
   }cursor=end;
  }
  return nullptr;
 };
 auto preferred=(std::max)(first,(std::min)(last,(uint64_t(base)+0x08000000+gran-1)&~(gran-1)));
 auto result=search(preferred,last);if(!result && preferred>first)result=search(first,preferred-gran);
 fprintf(log,"NativeGroup88 private allocation interval=%llx..%llx regions=%u attempts=%u lastError=%lu result=%p\n",first,last,regions,attempts,lastError,result);return result;
}
bool Relocate(uint8_t* copy,uint8_t* base){
 for(auto& body:bodies)memcpy(copy+body.destination,base+body.original,body.size);
 for(auto& u:unwinds){memcpy(copy+u.destination,base+u.original,u.size);if(u.chainAt)memcpy(copy+u.destination+u.chainAt,&u.chain,sizeof(u.chain));}
 // Leaf tail-jump thunks preserve all argument registers and native return ABI.
 const void* targets[]={reinterpret_cast<void*>(Position),reinterpret_cast<void*>(Direction)};
 for(unsigned i=0;i<2;++i){const uint8_t jump[]={0xff,0x25,0,0,0,0};memcpy(copy+0x2000+i*16,jump,sizeof(jump));memcpy(copy+0x2006+i*16,&targets[i],8);}
 for(auto& r:relocations){uint8_t* destination=base+r.target;if(r.replacement==1)destination=copy+0x1000;else if(r.replacement==2)destination=copy+0x2000;else if(r.replacement==3)destination=copy+0x2010;
  auto displacement=int64_t(destination)-int64_t(copy+r.next);if(displacement<std::numeric_limits<int32_t>::min() || displacement>std::numeric_limits<int32_t>::max())return false;
  auto value=int32_t(displacement);memcpy(copy+r.field,&value,4);
 }return true;
}
}
bool Initialize(uint8_t* base,FILE* log){
 if(block)return image==base;
 for(auto& b:bodies)if(memcmp(base+b.original,b.bytes,b.size)){fprintf(log,"REFUSED NativeGroup88 body=%x\n",b.original);return false;}
 for(auto& u:unwinds)if(memcmp(base+u.original,u.bytes,u.size)){fprintf(log,"REFUSED NativeGroup88 unwind=%x\n",u.original);return false;}
 for(auto& g:guards)if(memcmp(base+g.original,g.bytes,g.size)){fprintf(log,"REFUSED NativeGroup88 getter=%x\n",g.original);return false;}
 auto copy=AllocateNear(base,log);if(!copy){fprintf(log,"REFUSED NativeGroup88 private allocation unavailable\n");return false;}
 if(!Relocate(copy,base)){VirtualFree(copy,0,MEM_RELEASE);fprintf(log,"REFUSED NativeGroup88 displacement\n");return false;}
 DWORD old{};if(!VirtualProtect(copy,AllocationSize,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),copy,AllocationSize)){VirtualFree(copy,0,MEM_RELEASE);fprintf(log,"REFUSED NativeGroup88 executable protection\n");return false;}
 memcpy(registered,functions,sizeof(functions));if(!RtlAddFunctionTable(registered,_countof(registered),DWORD64(copy))){VirtualFree(copy,0,MEM_RELEASE);fprintf(log,"REFUSED NativeGroup88 unwind registration\n");return false;}
 image=base;block=copy;nativePosition=reinterpret_cast<Getter>(base+0x1e91b70);nativeDirection=reinterpret_cast<Getter>(base+0x1e91f10);group=reinterpret_cast<Callback>(copy);
 fprintf(log,"NativeGroup88 private native B8/group bodies=%p/%p unwindRanges=%zu; scoped position/direction calls only inside copy; original code/getters unchanged, copies retained until exit\n",copy,copy+0x1000,_countof(functions));fflush(log);return true;
}
uint64_t Invoke(void* a,void* b,void* c,void* d,const uint8_t head[32]){
 Camera camera{};memcpy(camera.bytes,head,32);camera.previous=current;
 struct Scope {Camera& camera;Scope(Camera& c):camera(c){current=&camera;}~Scope(){current=camera.previous;}} scope(camera);
 return group(a,b,c,d);
}
uint64_t CameraReads(){return reads.load();}
}
