// Two owned-eye callback slots, forwarded once; observe after native selection.
#include "SelectionBoundary46.h"
#include <atomic>
#include <cstring>
namespace SelectionBoundary46 {namespace {
using Task=void(__fastcall*)(void*);Task original{};Observer observe{};FILE* log{};
void *views[2]{},*graphs[2]{};void** slots[2]{};std::atomic<bool> enabled{};std::atomic<unsigned> inFlight{};
struct Flight{Flight(){++inFlight;}~Flight(){--inFlight;}};
bool Read(void* dst,const void* src,size_t n) noexcept {__try{memcpy(dst,src,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void __fastcall Extra(void* graph){Flight flight;original(graph);if(!enabled)return;for(unsigned i=0;i<2;++i)if(graph==graphs[i]){observe(i,views[i]);return;}}
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
}
bool Install(unsigned char* image,void* left,void* right,Observer callback,FILE* report){
 if(enabled || inFlight || !image || !callback)return false;
 const unsigned char bytes[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf1,0x48,0x8b,0x49,0x10,0x48,0x8b,0x49,0x18,0x48,0x8b,0x56,0x08};
 if(memcmp(image+0x219f780,bytes,sizeof(bytes)))return false;
 original=reinterpret_cast<Task>(image+0x219f780);views[0]=left;views[1]=right;observe=callback;log=report;
 for(unsigned i=0;i<2;++i){void *owner{},*state{},*back{};
  if(!views[i] || !Read(&graphs[i],static_cast<unsigned char*>(views[i])+0x440,8) || !graphs[i] ||
   !Read(&owner,graphs[i],8) || owner!=views[i] || !Read(&state,static_cast<unsigned char*>(graphs[i])+8,8) || !state ||
   !Read(&back,static_cast<unsigned char*>(state)+0x8208,8) || back!=views[i])return false;
  slots[i]=reinterpret_cast<void**>(static_cast<unsigned char*>(graphs[i])+0x1068);
  if(!Writable(slots[i]) || *slots[i]!=reinterpret_cast<void*>(original))return false;
 }
 for(unsigned i=0;i<2;++i)if(InterlockedCompareExchangePointer(slots[i],reinterpret_cast<void*>(Extra),reinterpret_cast<void*>(original))!=reinterpret_cast<void*>(original)){
  for(auto slot:slots)if(slot)InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(original),reinterpret_cast<void*>(Extra));return false;}
 enabled=true;if(log){fprintf(log,"Selection boundary46 installed two exact owned-eye extra callbacks; original forwarded once, snapshot after native selection, no native data writes\n");fflush(log);}return true;
}
void Stop(){enabled=false;unsigned restored{};for(auto slot:slots)if(slot){InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(original),reinterpret_cast<void*>(Extra));restored+=*slot==reinterpret_cast<void*>(original);}
 if(log){fprintf(log,"Selection boundary46 retired slotsRestored=%u/2 inFlight=%u; callback storage retained until game exit\n",restored,inFlight.load());fflush(log);}}
}
