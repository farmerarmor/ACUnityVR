#include "FirstPerson70.h"
#include "FirstPersonInputs70.h"
#include <windows.h>
#include <atomic>
#include <share.h>
namespace FirstPerson70 {namespace {
using Task=void(__fastcall*)(void*,void*);
uint8_t* image{};uint64_t world{};FILE* log{};std::filesystem::path directory;
Task native{};std::atomic<bool> installed{},active{};std::atomic<uint64_t> player{},proxy{},states[2]{},hidden{};
bool requested{},hideBody{},pauseDown{},f8Down{};float height{},forward{};unsigned reports{},captures{};bool lastActive{};
bool ReadBytes(void* out,uint64_t p,size_t n) noexcept {if(!out||p<0x10000||p>0x00007ffffffeffffULL||!n||n>0x10000)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void __fastcall Selected(void* state,void* object){
 if(installed.load(std::memory_order_relaxed) && active.load(std::memory_order_acquire) && hideBody &&
 FirstPersonInputs70::BodyMatches(ReadBytes,reinterpret_cast<uint64_t>(state),reinterpret_cast<uint64_t>(object),states[0].load(std::memory_order_relaxed),states[1].load(std::memory_order_relaxed),player.load(std::memory_order_relaxed),proxy.load(std::memory_order_relaxed))){hidden.fetch_add(1,std::memory_order_relaxed);return;}
 native(state,object);
}
float Setting(const std::filesystem::path& path,const wchar_t* key,float fallback){wchar_t value[64]{};GetPrivateProfileStringW(L"FirstPerson",key,L"",value,64,path.c_str());if(!*value)return fallback;wchar_t* end{};float v=float(wcstod(value,&end));return end!=value && !*end && std::isfinite(v) && std::abs(v)<=1?v:fallback;}
}
bool Initialize(unsigned char* base,void* rendererWorld,const std::filesystem::path& output,FILE* report){
 if(installed || !base || !rendererWorld || !report)return false;
 // Exact native accessors/producer reviewed against this executable. No code patch.
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x1ef3d0,"\x48\x83\xec\x28\x48\x8b\x0d\xf5\xb6\x02\x05\xe8\x90\xda\xe5\x01\x48\x85\xc0\x74\x10\x48\x8b\x0d\xe4\xb6\x02\x05\x48\x83\xc4\x28\xe9\x7b\xda\xe5\x01\x48\x83\xc4\x28\xc3\xcc\xcc\xcc\xcc\xcc\xcc\x48\x83\xec\x48\x48\x8b\xd1\x48\x8d\x4c\x24\x20\xe8\x1f\x5f\x13",64},{0x93310,"\x48\x83\xec\x28\x0f\xb7\xc2\x0f\x29\x3c\x24\x48\xc1\xe0\x05\x48\x03\x81\x78\x01\x00\x00\x0f\x28\x28\x0f\x28\x78\x10\x8b\x81\x8c\x00\x00\x00\xc1\xe8\x1a\xa8\x01\x0f\x85\xf8\x00\x00\x00\x0f\x28\x59\x10\x0f\x29\x74\x24\x10\x0f\x28\xe3\x0f\x28\xf3\x0f\x28\xd3\x0f\x59\xe5\x0f\xc6\xf3\xc9\x0f\xc6\xd3\xff\x0f\x28\xcc\x0f\x28",80},{0x2bac0,"\x0f\xb7\x41\x42\x4c\x8b\x51\x38\x44\x8b\xca\xc1\xe0\x03\x4d\x8b\xc2\x8b\xc8\x44\x8b\xd8\x49\x2b\xca\x49\x03\xca\x48\xc1\xf9\x03\x48\x85\xc9\x7e\x38\x66\x66\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x48\x8b\xc1\x48\x99\x48\x2b\xc2\x48\xd1\xf8\x45\x39\x0c\xc0\x49\x8d\x14\xc0\x73\x10\x4c\x8d\x42\x08\x48\x83\xca\xff\x48\x2b\xd0\x48\x03\xca\xeb\x03\x48\x8b\xc8\x48\x85\xc9\x7f\xd3\x4b\x8d\x04\x13\x4c\x3b\xc0\x74\x0b\x45\x39\x08\x75\x06\x41\x0f\xb7\x40\x04\xc3\xb8\xff\xff\x00\x00\xc3",119},{0x21dd830,"\x44\x8b\x81\x4c\x86\x00\x00\x8b\x42\x30\x4c\x8b\xc9\xf0\x44\x0f\xb1\x42\x30\x41\x3b\xc0\x74\x19\xb9\x01\x00\x00\x00\xf0\x41\x0f\xc1\x89\x70\x22\x00\x00\x49\x8b\x81\x30\x22\x00\x00\x48\x89\x14\xc8\xf3\xc3",51}};
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(report,"REFUSED first-person70 signature=%zx\n",g.rva);fflush(report);return false;}
 image=base;world=reinterpret_cast<uint64_t>(rendererWorld);directory=output;log=report;native=reinterpret_cast<Task>(base+0x21dd830);
 auto ini=output/L"ACUnityVR.ini";requested=GetPrivateProfileIntW(L"FirstPerson",L"Enabled",1,ini.c_str())!=0;hideBody=GetPrivateProfileIntW(L"FirstPerson",L"HideBody",1,ini.c_str())!=0;
 height=Setting(ini,L"HeightOffset",0);forward=Setting(ini,L"ForwardOffset",0);installed=true;
 fprintf(log,"First-person70 prepared enabled=%d hideBody=%d heightOffset=%.3f forwardOffset=%.3f; Pause toggles, animated position only, native camera heading retained. Player/skeleton reads; body selection filtered only in owned eye states.\n",requested,hideBody,height,forward);fflush(log);return true;
}
// Reuse the accepted eye anchor without polling keys or changing body state.
// The caller holds the same camera lock as BeginFrame.
void AnchorCamera(CameraMath::Matrix& base){
 FirstPersonInputs70::Snapshot s{};
 if(installed && requested && FirstPersonInputs70::Capture(ReadBytes,reinterpret_cast<uint64_t>(image),world,base,s)){
  auto next=FirstPersonInputs70::Attach(base,s,height,forward);if(CameraMath::Rigid(next))base=next;
 }
}
void BeginFrame(bool tracked,uint64_t sequence,CameraMath::Matrix& base){
 bool pause=(GetAsyncKeyState(VK_PAUSE)&0x8000)!=0;if(pause&&!pauseDown){requested=!requested;fprintf(log,"First-person70 requested=%d key=Pause\n",requested);fflush(log);}pauseDown=pause;
 bool f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0,take=f8&&!f8Down;f8Down=f8;
 FirstPersonInputs70::Snapshot s{};bool use=installed && requested && tracked && FirstPersonInputs70::Capture(ReadBytes,reinterpret_cast<uint64_t>(image),world,base,s);
 active.store(false,std::memory_order_release);auto original=base;if(use){auto next=FirstPersonInputs70::Attach(base,s,height,forward);use=CameraMath::Rigid(next);if(use)base=next;}
 player.store(use?s.player:0,std::memory_order_relaxed);proxy.store(use?s.proxy:0,std::memory_order_relaxed);active.store(use,std::memory_order_release);
 if((use!=lastActive || sequence<=3) && reports<32){++reports;fprintf(log,"First-person70 effective=%d requested=%d sequence=%llu player=%llx bones=%u/%u/%u anchor=(%.4f,%.4f,%.4f) source=(%.4f,%.4f,%.4f) hidden=%llu\n",use,requested,sequence,s.player,s.headIndex,s.leftIndex,s.rightIndex,base.m[12],base.m[13],base.m[14],original.m[12],original.m[13],original.m[14],hidden.load());fflush(log);}lastActive=use;
 if(take){auto name=directory/(L"first-person70-"+std::to_wstring(++captures)+L".json");auto f=_wfsopen(name.c_str(),L"w",_SH_DENYNO);if(f){fprintf(f,"{\"sequence\":%llu,\"requested\":%s,\"effective\":%s,\"hideBody\":%s,\"player\":\"%llx\",\"proxy\":\"%llx\",\"skeleton\":\"%llx\",\"eyeBones\":%s,\"worldBones\":%s,\"anchor\":[%.9g,%.9g,%.9g],\"source\":[%.9g,%.9g,%.9g],\"hiddenSelections\":%llu}\n",sequence,requested?"true":"false",use?"true":"false",hideBody?"true":"false",s.player,s.proxy,s.skeleton,s.eyes?"true":"false",s.worldBones?"true":"false",base.m[12],base.m[13],base.m[14],original.m[12],original.m[13],original.m[14],hidden.load());fclose(f);}}
}
bool Selection(void* view){__try{
 using FirstPersonInputs70::Read;auto v=reinterpret_cast<uint64_t>(view);uint64_t state{},back{},fn{},data{};
 if(!Read(ReadBytes,v+0x430,state) || !Read(ReadBytes,state+0x8208,back) || back!=v || !Read(ReadBytes,state+0x17a8,fn) || (fn!=reinterpret_cast<uint64_t>(native)&&fn!=reinterpret_cast<uint64_t>(Selected)) || !Read(ReadBytes,state+0x17f0,data) || data!=state)return false;
 if(fn==reinterpret_cast<uint64_t>(Selected))return states[0].load()==state||states[1].load()==state;
 unsigned index=states[0].load()==state?0:states[1].load()==state?1:states[0].load()==0?0:states[1].load()==0?1:2;if(index==2)return false;
 states[index]=state;*reinterpret_cast<void**>(state+0x17a8)=reinterpret_cast<void*>(Selected);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Stop(){active=false;installed=false;if(log){fprintf(log,"First-person70 stopped hiddenSelections=%llu; only owned-eye callback storage retained\n",hidden.load());fflush(log);}}
}
