// Exact native NetFakeCrowdHerder visibility stage, before native culling.
// Temporary per-herder camera input only; no getter patches, flags or budgets.
#include "DenseVisibility87.h"
#include "BulkDraw83.h"
#include "NativeHandleState63.h"
#include "NativeGroup88.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <share.h>
namespace DenseVisibility87 { namespace {
#include "DenseVisibility87Signatures.h"
constexpr unsigned TargetLimit=64,MethodCount=56,GroupSlot=0xb8/8,VisibilitySlot=0xc0/8,RecordLimit=256;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
uint8_t* image{};uint64_t source{},world{},graph{},fake{},bulk{};FILE* report{};std::filesystem::path directory;
std::atomic<bool> enabled{},correction{true};std::atomic<unsigned> count{},inFlight{};Callback nativePhase[2]{};
SRWLOCK lock=SRWLOCK_INIT;
struct Target {uint64_t object{},handle{};uint32_t token{};void* table[MethodCount+1]{};std::atomic<bool> installed{};std::atomic_flag busy=ATOMIC_FLAG_INIT;uint64_t sequence{},tracking{};uint32_t frame{};uint8_t camera[32]{};};
std::array<Target,TargetLimit> targets{};
struct Record {uint64_t id{},object{},tick{},age{},sequence{},tracking{},caller{};uint32_t phase{},frame{},status{},members{},selected{},groups{};bool correction{},applied{},restored{};uint8_t before[32]{},after[32]{};};
std::array<Record,RecordLimit> records{},snapshot{};uint64_t total{},capture{},lastStatus{},lastDiscover{};bool keyDown{},f8Down{};std::array<uint64_t,12> statuses{};
std::atomic<uint64_t> phaseSeen[2]{},phaseApplied[2]{};std::atomic<uint64_t> visibilitySeen{},cacheRestored{},cacheConflicts{},cacheApplied{};
constexpr size_t Methods[MethodCount]={0x26daf0,0x2708870,0x29c860,0x2970d0,0x1e6d4e0,0x3489a0,0x361eb0,0x3fa110,0x1e6d4e0,0x1e6d4e0,0xb25e90,0x1e6d4e0,0xb25e90,0x1e6d4e0,0x204d950,0x8085e0,0x348800,0x3487d0,0x3517f0,0x8085e0,0x1e6d4e0,0x1e6d4e0,0x297a80,0x36eb60,0x3c1210,0x1c50c20,0x28fa80,0x204d950,0x365cd0,0x365e20,0x3f9920,0x3c0a00,0x3c0da0,0x3c0b00,0x3c0b20,0x412690,0x3c1240,0x3c11f0,0x4127c0,0x411650,0x36ead0,0x3fb060,0x3fb140,0x3fb100,0x3fb080,0x4124c0,0x410af0,0x291220,0x27b6a0,0x27b610,0x272a30,0x272a50,0x27b750,0x346260,0x3487a0,0xb25e90};
struct Lock {Lock(){AcquireSRWLockExclusive(&lock);}~Lock(){ReleaseSRWLockExclusive(&lock);}};
struct Flight {Flight(){++inFlight;}~Flight(){--inFlight;}};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(!out || p<0x10000 || p>0x00007ffffffeffffULL || !n || n>0x10000 || p+n<p)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>T Get(const uint8_t* p,size_t o){T v{};memcpy(&v,p+o,sizeof(v));return v;}
bool Q(uint64_t p,size_t o,uint64_t expected){uint64_t x{};return Read(&x,p+o,8) && x==expected;}
bool Root(){return Q(world,0x998,bulk) && Q(world,0,uint64_t(image)+0x30225e0) && Q(graph,0,world) && Q(world,0x7d0,graph) && Q(world,0x918,fake) && Q(fake,0,uint64_t(image)+0x2f6a8b0) && Q(fake,0x268,world);}
bool Identity(const Target& t,bool own){uint8_t h[16]{},e[0x30]{};return Root() && Read(h,t.handle,16) && Get<uint64_t>(h,0)==t.object && NativeHandleState63::SameLifetime(Get<uint32_t>(h,12),t.token) && Read(e,t.object,sizeof(e)) && Get<uint64_t>(e,0)==(own?uint64_t(t.table+1):uint64_t(image)+0x2e8a540) && Get<uint64_t>(e,0x20)==world && Q(t.object,0x90,uint64_t(image)+0x3a7e320) && Q(uint64_t(image)+0x3a7e320,0x1d0,uint64_t(image)+0x1dc200) && Q(uint64_t(image)+0x3a7e320,0x1f0,uint64_t(image)+0x1dd2a0) && Q(t.object,0x590,bulk);}
bool Vector(const uint8_t* bytes,bool direction){float v[4]{};memcpy(v,bytes,16);for(float x:v)if(!std::isfinite(x))return false;if(std::abs(v[3])>1e-4f)return false;return !direction || std::abs(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]-1)<1e-3f;}
Target* Find(void* a){unsigned n=count.load(std::memory_order_acquire);for(unsigned i=0;i<n;++i)if(targets[i].object==uint64_t(a) && targets[i].installed.load())return &targets[i];return nullptr;}
bool SetTable(Target& t,bool attach) noexcept {__try{return InterlockedCompareExchangePointer(reinterpret_cast<void**>(t.object),attach?static_cast<void*>(t.table+1):static_cast<void*>(image+0x2e8a540),attach?static_cast<void*>(image+0x2e8a540):static_cast<void*>(t.table+1))==(attach?static_cast<void*>(image+0x2e8a540):static_cast<void*>(t.table+1));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool WriteCache(uint64_t destination,const uint8_t value[32]) noexcept {__try{memcpy(reinterpret_cast<void*>(destination),value,32);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Apply(Target& t,Record& r){
 r.tick=GetTickCount64();r.object=t.object;r.status=1;r.correction=correction.load();
 if(!enabled.load() || !Identity(t,true) || !Read(&r.frame,source+0x294,4))return;
 r.status=2;if(!Read(r.before,t.object+0x570,32) || !Vector(r.before,false) || !Vector(r.before+16,true))return;
 memcpy(r.after,r.before,32);Read(&r.members,t.object+0x120,4);Read(&r.selected,t.object+0x358,4);Read(&r.groups,t.object+0x348,4);
 r.status=3;if(!r.correction)return;
 float position[4]{},direction[4]{};CameraMath::Matrix pose{};if(!BulkDraw83::CurrentCamera(r.frame,pose,r.age,r.sequence,r.tracking)){r.status=4;return;}for(unsigned i=0;i<3;++i){position[i]=pose.m[12+i];direction[i]=pose.m[4+i];}
 memcpy(r.after,position,16);memcpy(r.after+16,direction,16);if(!Vector(r.after,false) || !Vector(r.after+16,true)){r.status=5;return;}
 uint8_t unchanged[32]{};r.status=6;
 // Exact native visibility callback owns this temporary per-herder input.
 // Native camera getters, actor flags, FOV/range/LOD and budgets are unchanged.
 // The lock protects mod metadata/Stop, never surrounds the native callback.
 Lock guard;if(!enabled.load() || !correction.load() || !Identity(t,true) || !Read(unchanged,t.object+0x570,32) || memcmp(unchanged,r.before,32))return;
 r.applied=WriteCache(t.object+0x570,r.after);
 if(!r.applied){r.status=7;return;}memcpy(t.camera,r.after,32);t.frame=r.frame;t.sequence=r.sequence;t.tracking=r.tracking;++cacheApplied;r.status=0;
}
// The native call is outside metadata locks and SEH. A C++ exception unwinds
// the temporary cache scope normally; no exception is swallowed or translated.
struct CacheScope {
 Target* target;Record& record;
 ~CacheScope(){if(!target)return;
  if(record.applied){Lock guard;uint8_t current[32]{};
   if((Identity(*target,true) || Identity(*target,false)) && Read(current,target->object+0x570,32) && !memcmp(current,record.after,32)){
    record.restored=WriteCache(target->object+0x570,record.before);if(record.restored)++cacheRestored;
   }else ++cacheConflicts;
  }
  target->busy.clear(std::memory_order_release);
 }
};
__declspec(noinline) uint64_t Forward(unsigned phase,uint64_t caller,void* a,void* b,void* c,void* d){
 if(!enabled.load(std::memory_order_acquire))return nativePhase[phase](a,b,c,d);Flight flight;
 auto target=Find(a);if(target && target->busy.test_and_set(std::memory_order_acquire))target=nullptr;Record r{};r.phase=phase;r.caller=caller;uint64_t value{};
 {CacheScope scope{target,r};if(target){++visibilitySeen;++phaseSeen[phase];Apply(*target,r);if(r.applied)++phaseApplied[phase];}value=phase==0 && r.applied?NativeGroup88::Invoke(a,b,c,d,r.after):nativePhase[phase](a,b,c,d);}
 if(target){Lock guard;if(enabled.load()){r.id=++total;records[(r.id-1)%RecordLimit]=r;if(r.status<statuses.size())++statuses[r.status];}}
 return value;
}
uint64_t __fastcall Group(void* a,void* b,void* c,void* d){return Forward(0,uint64_t(_ReturnAddress()),a,b,c,d);}
uint64_t __fastcall Visibility(void* a,void* b,void* c,void* d){return Forward(1,uint64_t(_ReturnAddress()),a,b,c,d);}
void Discover(){Lock guard;if(!enabled.load() || !Root())return;uint64_t pointer{};uint16_t size{};if(!Read(&pointer,fake+0x230,8) || !Read(&size,fake+0x23a,2) || size>TargetLimit)return;uint64_t handles[TargetLimit]{};if(size&&!Read(handles,pointer,8*size))return;
 for(unsigned k=0;k<size;++k){uint8_t h[16]{};if(!Read(h,handles[k],16) || !NativeHandleState63::Live(Get<uint32_t>(h,12)))continue;uint64_t object=Get<uint64_t>(h,0);bool known=false;auto n=count.load();for(unsigned i=0;i<n;++i)if(targets[i].object==object && targets[i].handle==handles[k] && NativeHandleState63::SameLifetime(targets[i].token,Get<uint32_t>(h,12)))known=true;if(known || n>=TargetLimit)continue;
 auto& t=targets[n];t.object=object;t.handle=handles[k];t.token=Get<uint32_t>(h,12);if(!Identity(t,false))continue;memcpy(t.table,image+0x2e8a538,sizeof(t.table));t.table[1+GroupSlot]=reinterpret_cast<void*>(Group);t.table[1+VisibilitySlot]=reinterpret_cast<void*>(Visibility);
 // Publish all immutable metadata before the native object can call its table.
 count.store(n+1,std::memory_order_release);if(SetTable(t,true)){t.installed=true;fprintf(report,"DenseVisibility87 attached object=%llx handle=%llx full56 private table; visibilityB8/C0 only\n",object,t.handle);fflush(report);}
 }
}
void Save(uint64_t sequence){uint64_t end{};unsigned n{};{Lock guard;end=total;n=unsigned((std::min)(end,uint64_t(RecordLimit)));for(unsigned i=0;i<n;++i)snapshot[i]=records[(end-n+i)%RecordLimit];}
 std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return;auto path=directory/(L"visibility-"+std::to_wstring(++capture)+L"-sequence-"+std::to_wstring(sequence)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"sequence\":%llu,\"enabled\":%s,\"total\":%llu,\"cacheApplied\":%llu,\"visibilitySeen\":%llu,\"cacheRestored\":%llu}\n",sequence,correction?"true":"false",end,cacheApplied.load(),visibilitySeen.load(),cacheRestored.load());
 for(unsigned i=0;i<n;++i){auto& r=snapshot[i];fprintf(f,"{\"event\":\"visibility\",\"phase\":%u,\"object\":\"%llx\",\"frame\":%u,\"status\":%u,\"sequence\":%llu,\"tracking\":%llu,\"ageMs\":%llu,\"applied\":%s,\"restored\":%s,\"caller\":\"%llx\",\"members\":%u,\"selected\":%u,\"groups\":%u,\"before\":\"",r.phase,r.object,r.frame,r.status,r.sequence,r.tracking,r.age,r.applied?"true":"false",r.restored?"true":"false",r.caller,r.members,r.selected,r.groups);for(auto x:r.before)fprintf(f,"%02x",x);fprintf(f,"\",\"after\":\"");for(auto x:r.after)fprintf(f,"%02x",x);fprintf(f,"\"}\n");}fclose(f);
}
}
bool Initialize(uint8_t* base,void* primary,const std::filesystem::path& output,FILE* log){image=base;source=uint64_t(primary);report=log;directory=output/L"dense-visibility87";
 if(!Signatures87(base,log) || !NativeGroup88::Initialize(base,log))return false;uint64_t renderer{},rendererGraph{};if(!Read(&rendererGraph,source+0x440,8) || !Q(rendererGraph,0,source) || !Read(&renderer,rendererGraph+0x10,8) || !Q(renderer,0,uint64_t(image)+0x3094180) || !Read(&world,renderer+0x28,8) || !Read(&graph,world+0x7d0,8) || !Read(&fake,world+0x918,8) || !Read(&bulk,world+0x998,8) || !Root())return false;
 for(unsigned i=0;i<MethodCount;++i)if(reinterpret_cast<void**>(base+0x2e8a540)[i]!=base+Methods[i])return false;
 nativePhase[0]=reinterpret_cast<Callback>(base+0x36eb60);nativePhase[1]=reinterpret_cast<Callback>(base+0x3c1210);correction=GetPrivateProfileIntW(L"Culling",L"DenseVisibilityCamera",1,(output/L"ACUnityVR.ini").c_str())!=0;enabled=true;Discover();fprintf(log,"DenseVisibility87 prepared exact NetFakeCrowdHerder native virtualB8 ->1dc200 ->37e710/29a0d0 and C0 ->1dd2a0 ->379600, r24 group uses private native1dc200/37e250 copy so its camera reload receives scoped head vectors; individual uses original callback. Conditional per-herder cache restore; full56 table, native logic once, NumPad9=%d. Original game code/getters, global camera, flags/FOV/LOD/budgets unchanged\n",int(correction.load()));fflush(log);return true;
}
void Poll(uint64_t sequence){if(!enabled.load())return;auto now=GetTickCount64();if(now-lastDiscover>=1000){Discover();lastDiscover=now;}bool down=(GetAsyncKeyState(VK_NUMPAD9)&0x8000)!=0;if(down&&!keyDown){correction=!correction.load();fprintf(report,"DenseVisibility87 NumPad9 correction=%d sequence=%llu\n",int(correction.load()),sequence);fflush(report);}keyDown=down;bool f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if((f8&&!f8Down) || (!capture && cacheApplied.load()))Save(sequence);f8Down=f8;
 if(now-lastStatus>=5000){uint64_t n{};std::array<uint64_t,12> s{};{Lock guard;n=total;s=statuses;}fprintf(report,"DenseVisibility87 targets=%u seen=%llu applied=%llu correction=%d visibilitySeen=%llu cacheRestored=%llu conflicts=%llu refusedIdentity=%llu staleHead=%llu invalidVector=%llu disabled=%llu\n",count.load(),n,cacheApplied.load(),int(correction.load()),visibilitySeen.load(),cacheRestored.load(),cacheConflicts.load(),s[1]+s[6],s[4],s[2]+s[5],s[3]);fprintf(report,"DenseVisibility87 phases group/individual seen=%llu/%llu applied=%llu/%llu\n",phaseSeen[0].load(),phaseSeen[1].load(),phaseApplied[0].load(),phaseApplied[1].load());fprintf(report,"NativeGroup88 scopedCameraReads=%llu (two per corrected group call)\n",NativeGroup88::CameraReads());fflush(report);lastStatus=now;}}
bool CorrectionEnabled(){return enabled.load(std::memory_order_acquire)&&correction.load(std::memory_order_acquire);}
void Stop(){if(!enabled.exchange(false))return;Lock guard;correction=false;unsigned restored{},refused{};for(unsigned i=0;i<count.load();++i){auto& t=targets[i];if(t.installed && Identity(t,true) && SetTable(t,false)){++restored;t.installed=false;}else if(t.installed)++refused;}if(report){fprintf(report,"DenseVisibility87 stopped private tables restored=%u identityRefused=%u inFlight=%u; temporary cache ownership restored at callback boundary, clones retained until exit\n",restored,refused,inFlight.load());fflush(report);}}
}
