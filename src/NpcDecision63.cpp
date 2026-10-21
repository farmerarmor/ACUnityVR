#include "NpcCone63.h"
// Native selector observers share one complete private manager table. Tracked74
// optionally scopes the r26 fallback submission experiment (OFF in r27).
#include "NpcDecision63.h"
#include "BulkDraw83.h"
#include "NpcCallback63.h"
#include "NativeHandleState63.h"
#ifdef ACUNITYVR_STATION90
#include "StationSubmission90.h"
#endif
#ifdef ACUNITYVR_EARLY91
#include "EarlyCamera91.h"
#endif
#include <array>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <intrin.h>
#include <share.h>
namespace NpcDecision63 { namespace {
constexpr unsigned MethodCount=81,Slot=0xa0/8,GateSlot=0x98/8,RecordLimit=512;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
uint8_t* image{};uint64_t source{},world{},bulk{},collection{},manager{},selector{};FILE* report{};std::filesystem::path directory;
void* originalTable{};void* clone[MethodCount+1]{};Callback native{},nativeGate{};std::atomic<bool> enabled{},installed{};std::atomic<unsigned> inFlight{};
uint64_t total{},captureId{},guardTotal{},guardCaptureId{},protectedCount{},guardRefused{},lastGuardStatus{};bool f8Down{},f11Down{};std::atomic<bool> guardEnabled{true};
#ifdef ACUNITYVR_DECISION63_CHECK
bool checkF8{},checkF11{};
#endif
SRWLOCK lock=SRWLOCK_INIT;
struct Record {uint64_t id{},caller{},tick{},entity{},handle{},proxy{},state{},slot{},rbp{},selector{},manager{};uint32_t status{},depth{},thread{},frame{},mode{};uint16_t activation{};bool recovered{},valid{},budgetRead{};uint32_t groupIndex{},registeredHandleState{},currentHandleState{};float budgetScale{},removalRadius{},removalDot{},priorityCos{},groupLimit{};uint8_t position[16]{},direction[16]{},orientation[16]{},actorPosition[16]{},locals[8]{},stateRaw[0x360]{},referenceRaw[0xf0]{};};
std::array<Record,RecordLimit> records{},snapshot{};
struct Eligibility {uint64_t id{},entity{},handle{},proxy{},caller{},age{},nativeResult{},result{};uint32_t frame{},status{},eyeMask{};bool correction{},protectedNpc{},inputsValid{};float dot{},squaredDistance{},radius{},removalDot{},groupLimit{},scale{},lo[3]{},hi[3]{};};
std::array<Eligibility,RecordLimit> guardRecords{},guardSnapshot{};

constexpr std::array<size_t,MethodCount> Methods{0xfa8c40,0x2708870,0xfbe590,0xfb9880,0x1e6d4e0,0xfc4530,0x204d950,0xfd8860,0x1e6d4e0,0x1e6d4e0,0xb25e90,0x1e6d4e0,0xb25e90,0x1e6d4e0,0x204d950,0x8085e0,0xfc4160,0x11c33e0,0xc98be0,0x8085e0,0xfd2480,0xfd1400,0xfbb300,0x1e6d4e0,0x1e6d4e0,0xc98bf0,0xfb1c60,0x1ef7640,0x1e6d4e0,0xfe4ba0,0xfeb540,0xfd3280,0x1e6d4e0,0xfde600,0xfcfec0,0xfd0610,0xfce7f0,0xfd1ab0,0xfd2520,0x1e6d4e0,0xb25e90,0xfcee80,0xfe51e0,0x8085e0,0xfefa10,0xfcdea0,0xfdd500,0xfdd6d0,0xfc8920,0xfc1d10,0xfcecf0,0xfc7490,0xfdd220,0xfae640,0xfd37b0,0xfb64c0,0xfee1f0,0xfec740,0xfc41b0,0xfc7ec0,0xfc7d60,0xfae6c0,0xfb8270,0xfcd2c0,0xfcd860,0xfcd660,0xfdb6c0,0xfdbe80,0xfccf90,0xfc40b0,0xfd3ae0,0xff1330,0xfd6090,0xfb04c0,0xfb0e50,0xfc9ba0,0xfe6f30,0xfc6ea0,0xfe4ed0,0xfcf860,0x1e6d4e0};
struct Lock {Lock(){AcquireSRWLockExclusive(&lock);}~Lock(){ReleaseSRWLockExclusive(&lock);}};
struct Flight {Flight(){++inFlight;}~Flight(){--inFlight;}};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(!out || p<0x10000 || p>0x00007ffffffeffffULL || !n || n>0x10000 || p+n<p)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>T Get(const uint8_t* p,size_t o){T v{};memcpy(&v,p+o,sizeof(v));return v;}
bool Q(uint64_t p,size_t o,uint64_t expected){uint64_t v{};return Read(&v,p+o,8) && v==expected;}
bool CollectionIdentity(){uint64_t vt{};return Read(&vt,collection,8) && NpcCone63::CanonicalCollection(collection,vt)==reinterpret_cast<uint64_t>(image)+0x2f6b880;}
bool Identity(bool owned){uint64_t bulkTable{};return Q(world,0,reinterpret_cast<uint64_t>(image)+0x30225e0) && Q(world,0x998,bulk) && Read(&bulkTable,bulk,8) && BulkDraw83::CanonicalBulk(bulk,bulkTable)==reinterpret_cast<uint64_t>(image)+0x304ab90 && Q(bulk,0x500,collection) && Q(bulk,0x4f8,selector) && CollectionIdentity() && Q(collection,0xc0,manager) && Q(manager,0,owned?reinterpret_cast<uint64_t>(clone+1):reinterpret_cast<uint64_t>(image)+0x2f6bcb0) && Q(manager,0x278,world) && Q(selector,0,reinterpret_cast<uint64_t>(image)+0x2f6b980) && Q(selector,0x80,bulk);}
#ifdef ACUNITYVR_STATION90
bool SubmissionOwner(){return Identity(true)||Identity(false);}
__declspec(noinline) uint64_t __fastcall SubmissionObserver(void* a,void* b,void* c,void* d,void* e){
#ifdef ACUNITYVR_EARLY91
 EarlyCamera91::VisibilityBoundary();
#endif
 Flight flight;auto callback=reinterpret_cast<StationSubmission90::Callback>(image+0xfec740);
 return StationSubmission90::Invoke(callback,a,b,c,d,e,uint64_t(_ReturnAddress()));
}
#endif
__declspec(noinline) void Recover(Record& out,bool eligibility=false) noexcept {
 __try {
  CONTEXT c{};RtlCaptureContext(&c);ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);out.status=1;
  for(unsigned depth=0;depth<24;++depth){
   if(c.Rsp<low || c.Rsp>high-8 || (c.Rsp&7) || !c.Rip){out.status=2;return;}
   if(c.Rip==reinterpret_cast<uint64_t>(image)+(eligibility?0xfeacaf:0xfeacdf)){
    DWORD64 owner{};auto fn=RtlLookupFunctionEntry(c.Rip,&owner,nullptr);
    if(!fn || owner!=reinterpret_cast<uint64_t>(image) || fn->BeginAddress!=0xfea9dc || fn->EndAddress!=0xfeaef4 || fn->UnwindData!=0x3817184){out.status=3;return;}
    out.recovered=true;out.depth=depth;out.rbp=c.Rbp;out.selector=c.R13;out.manager=c.R12;out.state=c.R14;
    if(out.selector!=selector || out.manager!=manager || out.rbp<low+8 || out.rbp>high-0x110 || !Identity(true)){out.status=4;return;}
    uint64_t pool{},states{},countCap{};uint8_t poolRaw[0x78]{};
    if(!Read(&pool,manager+0x40,8) || !Read(poolRaw,pool,sizeof(poolRaw)) || Get<uint64_t>(poolRaw,0)!=reinterpret_cast<uint64_t>(image)+0x304c640 || Get<uint64_t>(poolRaw,0x50)!=pool || Get<uint64_t>(poolRaw,0x58)!=manager || Get<uint64_t>(poolRaw,0x70)!=world){out.status=5;return;}
    states=Get<uint64_t>(poolRaw,0x60);countCap=Get<uint64_t>(poolRaw,0x68);uint32_t count=uint32_t(countCap),capacity=uint32_t(countCap>>32)&0x1fffffff;
    if(!states || !count || count>4096 || capacity<count || capacity>4096 || out.state<states || (out.state-states)%0x360 || (out.state-states)/0x360>=count){out.status=6;return;}
    out.slot=(out.state-states)/0x360;
    if(!Read(out.stateRaw,out.state,sizeof(out.stateRaw))){out.status=7;return;}out.handle=Get<uint64_t>(out.stateRaw,0x10);
    uint8_t h[16]{},e[0x138]{};if(!Read(h,out.handle,16) || int32_t(Get<uint32_t>(h,12))>=0){out.status=8;return;}out.entity=Get<uint64_t>(h,0);
    if(!Read(e,out.entity,sizeof(e)) || Get<uint64_t>(e,0x60)!=world || Get<uint64_t>(e,0x130)!=out.handle){out.status=9;return;}out.proxy=Get<uint64_t>(e,0xf8);
    float lo[3]{},hi[3]{};if(eligibility?!NpcCallback63::LiveNpcIdentity(out.entity,out.handle,out.proxy,lo,hi,&out.currentHandleState):!NpcCallback63::Armed(out.entity,out.handle,out.proxy,&out.registeredHandleState,&out.currentHandleState)){out.status=10;return;}
    uint64_t references{};if(!Read(&references,manager+0x384,8) || !Read(out.referenceRaw,references+out.slot*0xf0,sizeof(out.referenceRaw))){out.status=11;return;}
    // Proven local inputs to native fafe10 and its immediate removal branch.
    if(!Read(out.position,out.rbp+0x30,16) || !Read(out.direction,out.rbp+0x90,16) || !Read(out.orientation,out.rbp+0xd0,16) || !Read(out.actorPosition,out.rbp+0x100,16) || !Read(out.locals,out.rbp-8,4) || !Read(out.locals+4,out.rbp,4)){out.status=12;return;}
    uint8_t checkPool[0x78]{},checkHandle[16]{};
    if(!Identity(true) || !Q(manager,0x40,pool) || !Q(manager,0x384,references) || !Q(out.state,0x10,out.handle) || !Read(checkPool,pool,sizeof(checkPool)) || memcmp(poolRaw+0x50,checkPool+0x50,0x28) || !Read(checkHandle,out.handle,16) || (memcmp(h,checkHandle,12) || !NativeHandleState63::SameLifetime(Get<uint32_t>(h,12),Get<uint32_t>(checkHandle,12))) || (eligibility?!NpcCallback63::LiveNpcIdentity(out.entity,out.handle,out.proxy,lo,hi):!NpcCallback63::Armed(out.entity,out.handle,out.proxy))){out.status=13;return;}
    // Capture the actual remaining priority gate at this request boundary.
    // All four registers are native nonvolatile inputs; slot600 is proven by
    // exact feac5a. No change to the original decision or callback result.
    memcpy(&out.budgetScale,&c.Xmm6,4);memcpy(&out.removalRadius,&c.Xmm11,4);memcpy(&out.priorityCos,&c.Xmm12,4);memcpy(&out.removalDot,&c.Xmm13,4);
    uint64_t groupPointer{};uint16_t groupCount{};
    if(out.rbp<=high-0x604 && Read(&out.groupIndex,out.rbp+0x600,4) && Read(&groupPointer,selector+8,8) && Read(&groupCount,selector+0x12,2) && groupCount<=4096 && out.groupIndex<groupCount && Read(&out.groupLimit,groupPointer+uint64_t(out.groupIndex)*24+0xc,4)){
      out.budgetRead=std::isfinite(out.budgetScale) && out.budgetScale>0 && std::isfinite(out.removalRadius) && out.removalRadius>0 && std::isfinite(out.priorityCos) && out.priorityCos>=-1 && out.priorityCos<=1 && std::isfinite(out.removalDot) && out.removalDot>=-1 && out.removalDot<=1 && std::isfinite(out.groupLimit) && out.groupLimit>=0;
    }
    out.mode=Get<uint32_t>(out.stateRaw,0x80);out.activation=Get<uint16_t>(out.stateRaw,0x28);out.valid=true;out.status=0;return;
   }
   DWORD64 module{};auto fn=RtlLookupFunctionEntry(c.Rip,&module,nullptr);auto previous=c.Rsp,previousRip=c.Rip;
   if(fn){PVOID handler{};DWORD64 frame{};RtlVirtualUnwind(0,module,c.Rip,fn,&c,&handler,&frame,nullptr);}else{if(!Read(&c.Rip,c.Rsp,8)){out.status=14;return;}c.Rsp+=8;}
   if(c.Rsp<=previous || c.Rsp>high || c.Rip==previousRip){out.status=15;return;}
  }out.status=16;
 }__except(EXCEPTION_EXECUTE_HANDLER){out.status=17;}
}
__declspec(noinline) uint64_t __fastcall Observer(void* a,void* b,void* c,void* d){
 if(!enabled.load(std::memory_order_acquire))return native(a,b,c,d);
 Flight flight;Record r{};r.caller=reinterpret_cast<uint64_t>(_ReturnAddress());bool take=reinterpret_cast<uint64_t>(a)==manager && r.caller==reinterpret_cast<uint64_t>(image)+0xfeacdf;
 if(take){r.tick=GetTickCount64();r.thread=GetCurrentThreadId();Read(&r.frame,source+0x294,4);Recover(r);if(r.valid && r.slot!=reinterpret_cast<uint64_t>(b)){r.valid=false;r.status=18;}}
 // Original call is outside metadata SEH, locks and all file I/O. The native
 // caller writes activation bits only after this method returns.
 uint64_t result=native(a,b,c,d);
 if(take){Lock guard;if(enabled.load()){r.id=++total;records[(r.id-1)%RecordLimit]=r;}}
 return result;
}
// Exact native AL eligibility gate: returning false follows the native keep /
// delay-counter reset branch. Never patch code, increment references or write flags.
__declspec(noinline) uint64_t __fastcall EligibilityObserver(void* a,void* b,void* c,void* d){
 if(!enabled.load(std::memory_order_acquire))return nativeGate(a,b,c,d);
 Flight flight;Eligibility g{};g.caller=reinterpret_cast<uint64_t>(_ReturnAddress());bool take=reinterpret_cast<uint64_t>(a)==manager && g.caller==reinterpret_cast<uint64_t>(image)+0xfeacaf;
 if(take){Record r{};r.caller=g.caller;Read(&r.frame,source+0x294,4);Recover(r,true);g.entity=r.entity;g.handle=r.handle;g.proxy=r.proxy;g.frame=r.frame;g.status=r.status;g.correction=guardEnabled.load();
  if(r.valid){g.dot=Get<float>(r.locals,0);g.squaredDistance=Get<float>(r.locals,4);g.radius=r.removalRadius;g.removalDot=r.removalDot;g.groupLimit=r.groupLimit;g.scale=r.budgetScale;g.inputsValid=r.budgetRead && std::isfinite(g.dot) && std::isfinite(g.squaredDistance);
   // Reaching virtual98 while angle/range passed proves priority overflow.
   if(r.slot!=reinterpret_cast<uint64_t>(b)){g.status=18;}
   else if(r.mode!=10 || !(r.activation&1) || (r.activation&0x10)){g.status=19;}
   else if(!g.correction){g.status=20;}
   else if(!r.budgetRead || !std::isfinite(g.dot) || !std::isfinite(g.squaredDistance) || g.dot<g.removalDot || g.squaredDistance<0 || g.squaredDistance>g.radius*g.radius){g.status=21;}
   else {BinocularVisibility63::Frustums views{};
    if(!NpcCone63::CurrentFrustums(g.frame,views,g.age)){g.status=22;}
    else if(!NpcCallback63::LiveNpcIdentity(g.entity,g.handle,g.proxy,g.lo,g.hi)){g.status=23;}
    else {g.eyeMask=BinocularVisibility63::EyeMask(views,g.lo,g.hi);g.status=g.eyeMask?0:24;}
   }
  }
 }
 // Native ABI call once, outside metadata SEH/locks and I/O. Upper RAX is
 // preserved; exact native caller consumes AL only (test al,al at feacaf).
 uint64_t value=nativeGate(a,b,c,d);g.nativeResult=value;
 if(take && !g.status && g.eyeMask && uint8_t(value) && enabled.load() && guardEnabled.load() && Identity(true)){value&=~uint64_t(0xff);g.protectedNpc=true;}
 if(take){g.result=value;Lock guard;if(enabled.load()){g.id=++guardTotal;guardRecords[(g.id-1)%RecordLimit]=g;if(g.protectedNpc)++protectedCount;else if(g.status!=20)++guardRefused;}}
 return value;
}
bool Replace(bool attach) noexcept {__try{return InterlockedCompareExchangePointer(reinterpret_cast<void**>(manager),attach?static_cast<void*>(clone+1):originalTable,attach?originalTable:static_cast<void*>(clone+1))==(attach?originalTable:static_cast<void*>(clone+1));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Writable(uint64_t p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool F8(){
#ifdef ACUNITYVR_DECISION63_CHECK
 return checkF8;
#else
 return (GetAsyncKeyState(VK_F8)&0x8000)!=0;
#endif
}
void Snapshot(uint64_t sequence,uint64_t tracking,uint32_t frame,const char* boundary){uint64_t end{};unsigned count{};{Lock guard;end=total;count=unsigned((std::min)(end,uint64_t(RecordLimit)));for(unsigned i=0;i<count;++i)snapshot[i]=records[(end-count+i)%RecordLimit];}
 std::error_code error;std::filesystem::create_directories(directory,error);if(error)return;auto path=directory/(L"decision-"+std::to_wstring(++captureId)+L"-frame-"+std::to_wstring(frame)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"schema\":1,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"total\":%llu,\"retained\":%u,\"evicted\":%llu,\"boundary\":\"%s\",\"manager\":\"%llx\",\"selector\":\"%llx\",\"note\":\"Exact native feacdf deactivation requests only. Complete81-method private manager table, only A0 reader changed. Native four GPR arguments/result forwarded once; decisions unchanged. Bounded native unwind recovers fe9500 camera position/orientation/direction and actor inputs before native caller clears activation bits. Refusals are not input evidence. Completed calls only.\"}\n",sequence,tracking,frame,end,count,end-count,boundary,manager,selector);
 for(unsigned i=0;i<count;++i){auto& x=snapshot[i];fprintf(f,"{\"event\":\"handleState\",\"id\":%llu,\"registered\":%u,\"current\":%u}\n",x.id,x.registeredHandleState,x.currentHandleState);fprintf(f,"{\"event\":\"decision\",\"id\":%llu,\"caller\":\"%llx\",\"entity\":\"%llx\",\"handle\":\"%llx\",\"proxy\":\"%llx\",\"state\":\"%llx\",\"slot\":%llu,\"rbp\":\"%llx\",\"selector\":\"%llx\",\"manager\":\"%llx\",\"frame\":%u,\"thread\":%u,\"tick\":%llu,\"status\":%u,\"depth\":%u,\"recovered\":%s,\"valid\":%s,\"mode\":%u,\"activation\":%u",x.id,x.caller,x.entity,x.handle,x.proxy,x.state,x.slot,x.rbp,x.selector,x.manager,x.frame,x.thread,x.tick,x.status,x.depth,x.recovered?"true":"false",x.valid?"true":"false",x.mode,x.activation);
 fprintf(f,",\"budgetRead\":%s,\"groupIndex\":%u,\"budgetScale\":%.9g,\"removalRadius\":%.9g,\"removalDot\":%.9g,\"priorityCos\":%.9g,\"groupLimit\":%.9g",x.budgetRead?"true":"false",x.groupIndex,x.budgetRead?x.budgetScale:0.0f,x.budgetRead?x.removalRadius:0.0f,x.budgetRead?x.removalDot:0.0f,x.budgetRead?x.priorityCos:0.0f,x.budgetRead?x.groupLimit:0.0f);const struct{const char* name;const uint8_t* raw;size_t size;} blocks[]={{"cameraPosition",x.position,16},{"cameraDirection",x.direction,16},{"cameraOrientation",x.orientation,16},{"actorPosition",x.actorPosition,16},{"dotAndSquaredDistance",x.locals,8},{"stateRaw",x.stateRaw,sizeof(x.stateRaw)},{"referenceRaw",x.referenceRaw,sizeof(x.referenceRaw)}};
 for(auto& block:blocks){fprintf(f,",\"%s\":\"",block.name);if(x.valid)for(size_t k=0;k<block.size;++k)fprintf(f,"%02x",block.raw[k]);fprintf(f,"\"");}fprintf(f,"}\n");}fprintf(f,"{\"event\":\"end\",\"count\":%u,\"inFlight\":%u}\n",count,inFlight.load());fclose(f);fprintf(report,"NPC decision63 snapshot=%llu frame=%u completed=%llu retained=%u\n",captureId,frame,end,count);fflush(report);
}
void GuardSnapshot(uint64_t sequence,uint64_t tracking,uint32_t frame,const char* boundary){
 uint64_t end{},protectedN{};unsigned count{};{Lock guard;end=guardTotal;protectedN=protectedCount;count=unsigned((std::min)(end,uint64_t(RecordLimit)));for(unsigned i=0;i<count;++i)guardSnapshot[i]=guardRecords[(end-count+i)%RecordLimit];}
 auto output=directory.parent_path()/L"npc-eligibility63";std::error_code error;std::filesystem::create_directories(output,error);if(error)return;
 auto path=output/(L"eligibility-"+std::to_wstring(++guardCaptureId)+L"-frame-"+std::to_wstring(frame)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"begin\",\"schema\":1,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"total\":%llu,\"protected\":%llu,\"retained\":%u,\"boundary\":\"%s\",\"note\":\"Exact feacaf virtual98 native eligibility. F11 keeps only live non-player active mode10 NPCs intersecting either current headset eye when native angle and range passed. Native priority budget calculations stay native; some visible priority-based removals are vetoed. Native 4GPR/RAX upper bits unchanged, AL only may be cleared; no reference or actor flag writes. No F8 arming requirement. Native LOD selection unchanged.\"}\n",sequence,tracking,frame,end,protectedN,count,boundary);
 for(unsigned i=0;i<count;++i){auto& x=guardSnapshot[i];fprintf(f,"{\"event\":\"eligibility\",\"id\":%llu,\"entity\":\"%llx\",\"handle\":\"%llx\",\"proxy\":\"%llx\",\"caller\":\"%llx\",\"frame\":%u,\"status\":%u,\"eyeMask\":%u,\"headAgeMs\":%llu,\"correction\":%s,\"protected\":%s,\"nativeResult\":\"%llx\",\"result\":\"%llx\"",x.id,x.entity,x.handle,x.proxy,x.caller,x.frame,x.status,x.eyeMask,x.age,x.correction?"true":"false",x.protectedNpc?"true":"false",x.nativeResult,x.result);fprintf(f,",\"inputsValid\":%s,\"dot\":%.9g,\"squaredDistance\":%.9g,\"radius\":%.9g,\"removalDot\":%.9g,\"groupLimit\":%.9g,\"scale\":%.9g}",x.inputsValid?"true":"false",x.inputsValid?x.dot:0.0f,x.inputsValid?x.squaredDistance:0.0f,x.inputsValid?x.radius:0.0f,x.inputsValid?x.removalDot:0.0f,x.inputsValid?x.groupLimit:0.0f,x.inputsValid?x.scale:0.0f);fputc('\n',f);}
 fprintf(f,"{\"event\":\"end\",\"count\":%u,\"inFlight\":%u}\n",count,inFlight.load());fclose(f);
 fprintf(report,"NPC eligibility63 snapshot=%llu frame=%u completed=%llu protected=%llu retained=%u\n",guardCaptureId,frame,end,protectedN,count);fflush(report);
}
}
uint64_t CanonicalManager(uint64_t object,uint64_t observed){return object==manager && observed==reinterpret_cast<uint64_t>(clone+1)?reinterpret_cast<uint64_t>(originalTable):observed;}
bool Install(uint8_t* base,void* primary,void* rendererWorld,const std::filesystem::path& output,FILE* log){image=base;source=reinterpret_cast<uint64_t>(primary);report=log;directory=output/L"npc-decisions63";
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x8085e0,"\xb0\x01\xc3",3},{0xfeac98,"\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\x98\x00\x00\x00\x84\xc0\x74\x40\x41\xff\x86\x34\x03\x00\x00\x41\x83\xbe\x34\x03\x00\x00\x02\x0f\x8e\x90\xfe\xff\xff\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\xa0\x00\x00\x00\x33\xd2\x49\x8b\xce\xe8\x57\xcf\xf1\x00\xb8\xfe\xff\x00\x00\x66\x41\x21\x46\x28\x33\xc0\x41\x89\x86\x34\x03\x00\x00\xe9\x57\xfe\xff",104},{0xfe9500,"\x48\x8b\xc4\x48\x89\x48\x08\x55\x56\x41\x54\x48\x8d\xa8\x08\xfa\xff\xff\x48\x81\xec\xe0\x06\x00\x00\x48\x8b\xb1\x80\x00\x00\x00\x48\x89\x58\xe0\x4c\x89\x68\xd0\x48\x81\xc6\x14\x05\x00\x00\x4c\x8b\xe9",50},{0xfe957d,"\x48\x89\xbc\x24\xd0\x06\x00\x00\x49\x8b\xbd\x80\x00\x00\x00\x4c\x89\xb4\x24\xc0\x06\x00\x00\x4c\x89\xbc\x24\xb8\x06\x00\x00\x48\x8d\x55\x30\x0f\x29\xb4\x24\xa0\x06\x00\x00\x0f\x29\xbc\x24\x90\x06\x00\x00\x44\x0f\x29\x84\x24\x80\x06\x00\x00\x44\x0f\x29\x8c\x24\x70\x06\x00\x00\x44\x0f\x29\x94\x24\x60\x06\x00\x00\x44\x0f\x29\x9c\x24\x50\x06\x00\x00\x44\x0f\x29\xa4\x24\x40\x06\x00\x00\x44\x0f\x29\xac\x24\x30\x06\x00\x00",105},{0xfe95e6,"\xe8\x65\x7e\xea\x00\x48\x8b\x0d\x76\x11\x23\x04\x48\x8d\x95\xd0\x00\x00\x00\xe8\x32\x7d\xea\x00\x48\x8b\x0d\x63\x11\x23\x04\xe8\xc6\x7b\xea\x00\x44\x0f\x28\xd0\xe8\x3d\xf4\x90\x01\x0f\x28\x9d\xd0\x00\x00\x00\x0f\x57\xff\x0f\x28\x35\xbc\x8c\x60\x02\x44\x0f\x28\x0d\x34\x54\x63\x02\x0f\x28\x2d\x9d\x42\x60\x02\x44\x0f\x28\xe0\x0f\x28\xe3\x0f\x28\xc3\x0f\x59\xe6\x0f\xc6\xc3\xd2\x0f\x28\xd4\x0f\x28\xcc\x44\x0f\x28\xc6\x0f\xc6\xcc\x55\x0f\xc6\xd4\xaa\x44\x0f\xc6\xc6\xd2\xf3\x0f\x58\xca\x0f\x28\xd3\xf3\x0f\x58\xe1\x0f\x28\xce\x0f\xc6\xd3\xff\x0f\xc6\xce\xc9\x0f\xc6\xe4\x00\x0f\x59\xc8\x0f\x59\xe3\x0f\x28\xc3\x0f\xc6\xc3\xc9\x44\x0f\x59\xc0\x44\x0f\x5c\xc1\x44\x0f\x59\xc2\x0f\x59\xd2\x44\x0f\x58\xc4\x41\x0f\x5c\xd1\x0f\x59\xd6\x44\x0f\x58\xc2\x44\x0f\x59\x05\x68\xe9\xe8\x01\x44\x0f\x54\xc5\x0f\x55\xef\x44\x0f\x56\xc5\x41\x0f\x28\xc8\x41\x0f\x59\xc8\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x0f\x28\x0d\xda\xe9\xe8\x01\x0f\x5c\xfa\x0f\x52\xda\x0f\x28\xe2\x44\x0f\x59\xcb\x0f\x5f\xe7\x0f\x28\xc3\x0f\xc2\x25\xdf\x47\x60\x02\x02\x0f\x59\xc2\x0f\x59\xc3\x0f\x5c\xc8\x41\x0f\x59\xc9\x49\x8b\x9d\x80\x00\x00\x00\xf3\x44\x0f\x59\x15\x0a\x3a\xf8\x01\x41\x0f\x59\xc8\x0f\x28\xc4\xf3\x0f\x10\xbb\xdc\x02\x00\x00\x0f\x55\xc1\x44\x0f\x54\xc4\x44\x0f\x28\xdf\x0f\x28\xf7\xf3\x44\x0f\x58\x1d\xdc\xcc\x6e\x02\xf3\x0f\x58\x35\x44\xe8\xe8\x01\x41\x0f\x56\xc0\x0f\x29\x85\x90\x00\x00\x00\x41\x0f\x28\xc2\xe8\x00\xf3\x90\x01\x44\x38\x25\xc6\xf5\x22\x04\x44\x0f\x28\xc0\x74\x09\xf3\x44\x0f\x58\x05\xda\x39\xf8\x01\x48\x8b\x8b\x00\x05\x00\x00\x45\x0f\x28\xe8\xf3\x44\x0f\x5c\x2d\xde\x98\xef\x02\x48\x8b\x01\xff",408},{0xfeaa80,"\x48\x8b\x44\x24\x50\x49\x8b\x8d\x80\x00\x00\x00\x44\x8b\xfe\x0f\xb7\x54\x30\x18\x48\x81\xc1\xa0\x04\x00\x00\xe8\xa0\x96\xf0\x00\x4c\x8b\xe0\x48\x85\xc0\x0f\x84\xbe\x02\x00\x00\x48\x8b\x10\x48\x8b\xc8\xff\x92\xc8\x00\x00\x00\x48\x8b\xf8\x48\x85\xc0\x74\x08\x48\x8b\xc8\xe8\x58\x4b\x13\x01\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\x80\x00\x00\x00\x4c\x8b\xf0\x48\x85\xc0\x0f\x85\x83\x00\x00\x00\x48\x8b\x54\x24\x50\x45\x33\xc0\x49\x8b\xcd\x48\x83\xc2\x10\x49\x03\xd7\xe8\xfe\x75\xfc\xff\x44\x8b\x74\x24\x58\x44\x39\xb5\x08\x06\x00\x00\x74\x18\x48\x8b\x44\x24\x50\x41\x8d\x56\xff\xc1\xe2\x05\x48\x8d\x0c\x30\x48\x03\xd0\xe8\xd8\xda\xfb\xff\x48\x8b\x44\x24\x50\x49\x8b\xce\x48\xc1\xe1\x05\x48\x8b\x54\x01\xf0\xf0\xff\x4a\x08\x75\x15\xf7\x42\x0c\xff\xff\xff\x00\x75\x0c\x48\x8d\x0d\x21\x10\x27\x04\xe8\x8c\x1d\x70\x01\xff\x4c\x24\x58\x48\x85\xff\x0f\x84\x70\x02\x00\x00\x48\x8b\xcf\xe8\x57\x20\x14\x01\xe9\x63\x02\x00\x00\x83\xb8\x80\x00\x00\x00\x00\x0f\x84\x86\x01\x00\x00\xf6\x40\x78\x02\x0f\x85\x7c\x01\x00\x00\x84\xdb\x0f\x85\x78\x01\x00\x00\x48\x8b\x44\x24\x50\xf3\x44\x0f\x11\x64\x24\x30\x4c\x8d\x8d\x90\x00\x00\x00\x4c\x89\x34\x30\x49\x8b\x46\x10\x4c\x8d\x45\x30\x41\x0f\x28\x86\xe0\x01\x00\x00\x66\x0f\x7f\x85\x00\x01\x00\x00\x48\x63\x48\x0c\x48\x8d\x95\x00\x01\x00\x00\x48\xc1\xe1\x20\x48\xc1\xf9\x3f\x48\x23\x08\x41\x0f\xb7\x46\x72\x25\xff\x0f\x00\x00\x48\x89\x8d\x18\x06\x00\x00\x49\x8b\xcd\x89\x85\x00\x06\x00\x00\x48\x8d\x45\xf8\x48\x89\x44\x24\x28\x48\x8d\x45\x00\x48\x89\x44\x24\x20\xe8\x0d\x52\xfc\xff\x48\x8b\x44\x24\x50\x0f\x28\xd0\xf3\x0f\x11\x54\x30\x0c\x41\x8b\x86\x80\x00\x00\x00\xff\xc8\x83\xf8\x03\x77\x04\x41\xff\x45\x78\x48\x83\xbd\x18\x06\x00\x00\x00\x0f\x84\x97\x00\x00\x00\x41\xf6\x46\x78\x04\x0f\x85\x8c\x00\x00\x00\xf3\x0f\x10\x45\xf8\x41\x0f\x2f\xc5\x72\x32\xf3\x0f\x10\x45\x00\x41\x0f\x28\xcb\xf3\x41\x0f\x59\xcb\x0f\x2f\xc1\x77\x1f\x8b\x85\x00\x06\x00\x00\x8d\x0c\x40\x49\x8b\x45\x08\xc1\xe1\x03\xf3\x0f\x10\x44\x01\x0c\xf3\x0f\x59\xc6\x0f\x2f\xd0\x76\x7a\x41\x8b\x86\x80\x00\x00\x00\x83\xf8\x0a\x74\x07\xff\xc8\x83\xf8\x03\x77\x67\x41\x0f\xb6\x46\x28\xc0\xe8\x04\xa8\x01\x75\x5b\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\x98\x00\x00\x00\x84\xc0\x74\x40\x41\xff\x86\x34\x03\x00\x00\x41\x83\xbe\x34\x03\x00\x00\x02\x0f\x8e\x90\xfe\xff\xff\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\xa0\x00\x00\x00\x33\xd2\x49\x8b\xce\xe8\x57\xcf\xf1\x00\xb8\xfe\xff\x00\x00\x66\x41\x21\x46\x28\x33\xc0",629},{0xfafe10,"\x48\x8b\xc4\x48\x81\xec\xb8\x00\x00\x00\x0f\x28\x15\xaf\xda\x63\x02\x0f\x28\x2d\x88\x82\xec\x01\x48\x8b\x8c\x24\xe0\x00\x00\x00\x0f\x29\x70\xe8\x0f\x29\x78\xd8\x44\x0f\x29\x40\xc8\x0f\x57\xf6\x44\x0f\x29\x48\xb8\x44\x0f\x29\x50\xa8\x44\x0f\x29\x58\x98\x44\x0f\x28\x1d\x09\xec\x66\x02\x44\x0f\x29\x60\x88\x44\x0f\x29\x6c\x24\x30\x44\x0f\x28\x2a\x44\x0f\x29\x74\x24\x20\x44\x0f\x29\x7c\x24\x10\x41\x0f\x28\xc5\x0f\x29\x04\x24\x45\x0f\x5c\x28\xf3\x0f\x10\x42\x08\xf3\x0f\x58\x05\x9d\x8b\xed\x01\x44\x0f\x54\xea\xf3\x0f\x11\x44\x24\x08\x44\x0f\x28\x04\x24\x0f\x28\xc2\x0f\x55\xc6\x44\x0f\x56\xe8\x45\x0f\x5c\x00\x45\x0f\x28\xcd\x44\x0f\x54\xc2\x0f\x55\xd6\x45\x0f\x59\xcd\x44\x0f\x56\xc2\x41\x0f\x28\xf8\x41\x0f\x28\xc1\x41\x0f\x28\xc9\x41\x0f\xc6\xc1\x4e\x41\x0f\x59\xf8\x0f\x58\xc8\x0f\x28\xc6\x44\x0f\x28\xf9\x44\x0f\xc6\xf9\x11\x44\x0f\x58\xf9\x0f\x28\xcf\x41\x0f\x5c\xc7\x45\x0f\x28\xd7\x45\x0f\x52\xe7\x44\x0f\x5f\xd0\x44\x0f\xc2\x15\xd1\xdf\x63\x02\x02\x0f\x28\xc7\x0f\xc6\xc7\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x0f\x28\xcd\x0f\x5c\xf2\x0f\x52\xda\x0f\x28\xe2\x0f\x5f\xe6\x0f\x28\xc3\x0f\xc2\x25\xa3\xdf\x63\x02\x02\x0f\x59\xc2\x0f\x59\xc3\x44\x0f\x28\xf4\x41\x0f\x54\xe0\x0f\x5c\xc8\x41\x0f\x28\xc3\x0f\x59\xc3\x0f\x59\xc8\x41\x0f\x28\xc1\x41\x0f\x59\xc8\x41\x0f\xc6\xc1\x4e\x44\x0f\x58\xc8\x0f\x28\xc7\x44\x0f\x55\xf1\x41\x0f\x28\xc9\x41\x0f\xc6\xc9\x11\x44\x0f\x56\xf4\x41\x0f\x58\xc9\xf3\x0f\x11\x09\xf3\x0f\x10\x1d\xab\xd1\xfb\x01\xf3\x0f\x10\x35\x8b\x64\x72\x02\x0f\xc6\xc7\x4e\x0f\x2f\xcb\x0f\x58\xf8\x0f\x28\xc7\x0f\xc6\xc7\x11\x0f\x58\xc7\x0f\x29\x04\x24\x73\x05\x0f\x28\xd6\xeb\x42\x41\x0f\x28\xc4\x41\x0f\x28\xca\x45\x0f\x59\xdc\x41\x0f\x59\xc7\x45\x0f\x54\xd5\x41\x0f\x59\xc4\x0f\x5c\xe8\x41\x0f\x59\xeb\x41\x0f\x59\xed\x0f\x55\xcd\x41\x0f\x56\xca\x41\x0f\x59\x09\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x48\x8b\x84\x24\xe8\x00\x00\x00\xf3\x0f\x10\x2c\x24\x0f\x2f\xeb\xf3\x0f\x11\x10\x73\x05\x0f\x28\xde\xeb\x1c\x41\x0f\x28\x09\x41\x0f\x59\xce\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd9\x0f\xc6\xd9\x11\x0f\x58\xd9\x0f\x28\xce\x0f\x28\xc3\xf3\x0f\x58\xd6\xf3\x0f\x5c\x8c\x24\xf0\x00\x00\x00\xf3\x0f\x58\xc6\xf3\x0f\x59\x0d\x18\x30\xf3\x02\xf3\x0f\x58\x0d\x0c\x30\xf3\x02\x0f\x28\xe1\xf3\x0f\x5c\xc8\xf3\x0f\x5c\xe2\xf3\x0f\x59\xcd\xf3\x0f\x59\x21\x0f\x2f\xcc\x73\x0d\xf3\x0f\x11\x29\x0f\x28\xc1\xf3\x0f\x11\x18\xeb\x03\x0f\x28\xc4\x44\x0f\x28\x6c\x24\x30\x44\x0f\x28\x74\x24\x20\x44\x0f\x28\x7c\x24\x10\x4c\x8d\x9c\x24\xb8\x00\x00\x00\x41\x0f\x28\x73\xe8\x41\x0f\x28\x7b\xd8\x45\x0f\x28\x43\xc8\x45\x0f\x28\x4b\xb8\x45\x0f\x28\x53\xa8\x45\x0f\x28\x5b\x98\x45\x0f\x28\x63\x88\x49\x8b\xe3\xc3",669},{0xfd2480,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x48\x8d\x99\x48\x01\x00\x00\x8b\xf2\x48\x8b\xf9\x48\x85\xdb\x74\x08\x48\x8b\xcb\xe8\x78\xd1\x14\x01\x0f\xb7\x87\x8e\x03\x00\x00\x3b\xf0\x73\x4f\x48\x8b\xbf\x84\x03\x00\x00\x69\xf6\xf0\x00\x00\x00\x8b\xce\x48\x03\xf9\x48\x8b\x0f\x48\x63\x41\x0c\x48\xc1\xe0\x20\x48\xc1\xf8\x3f\x48\x85\x01\x74\x29\x48\x63\x41\x0c\x48\x8b\x09\x48\xc1\xe0\x20\x48\xc1\xf8\x3f\x48\x23\xc8\xe8\xb0\x09\xd0\x00\x48\x8b\xc8\xe8\xe8\x55\x04\xff\x81\xa7\xa0\x00\x00\x00\xff\xdf\xff\xff\x48\x85\xdb\x74\x08\x48\x8b\xcb\xe8\xb1\xa6\x15\x01\x48\x8b\x5c\x24\x30\x48\x8b\x74\x24\x38\x48\x83\xc4\x20\x5f\xc3",159},{0x38170a4,"\x01\x19\x05\x00\x19\x01\xdc\x00\x0b\xc0\x09\x60\x08\x50\x00\x00\x21\x08\x04\x00\x08\xd4\xd9\x00\x04\x34\xdb\x00\x00\x95\xfe\x00\x20\x95\xfe\x00\xa4\x70\x81\x03\x21\x17\x04\x00\x17\xe4\xd8\x00\x08\x74\xda\x00\x20\x95\xfe\x00\x7d\x95\xfe\x00\xb4\x70\x81\x03\x21\x08\x02\x00\x08\xf4\xd7\x00\x7d\x95\xfe\x00\x94\x95\xfe\x00\xcc\x70\x81\x03\x21\x08\x02\x00\x08\x68\x6a\x00\x94\x95\xfe\x00\xa0\x95\xfe\x00\xe4\x70\x81\x03\x21\x3e\x0e\x00\x3e\xd8\x63\x00\x35\xc8\x64\x00\x2c\xb8\x65\x00\x23\xa8\x66\x00\x1a\x98\x67\x00\x11\x88\x68\x00\x08\x78\x69\x00\xa0\x95\xfe\x00\xa8\x95\xfe\x00\xf8\x70\x81\x03\x21\x00\x0c\x00\x00\xd8\x63\x00\x00\xc8\x64\x00\x00\xb8\x65\x00\x00\xa8\x66\x00\x00\x98\x67\x00\x00\x88\x68\x00\xa0\x95\xfe\x00\xa8\x95\xfe\x00\xf8\x70\x81\x03\x21\x00\x0a\x00\x00\xd8\x63\x00\x00\xc8\x64\x00\x00\xb8\x65\x00\x00\xa8\x66\x00\x00\x88\x68\x00\xa0\x95\xfe\x00\xa8\x95\xfe\x00\xf8\x70\x81\x03\x21\x00\x08\x00\x00\xd8\x63\x00\x00\xc8\x64\x00\x00\xb8\x65\x00\x00\xa8\x66\x00\xa0\x95\xfe\x00\xa8\x95\xfe\x00\xf8\x70\x81\x03\x21\x00\x02\x00\x00\xa8\x66\x00\x94\x95\xfe\x00\xa0\x95\xfe\x00\xe4\x70\x81\x03\x21\x00\x00\x00\x7d\x95\xfe\x00\x94\x95\xfe\x00\xcc\x70\x81\x03\x21\x00\x00\x00\x20\x95\xfe\x00\x7d\x95\xfe\x00\xb4\x70\x81\x03\x21\x00\x00\x00\x00\x95\xfe\x00\x20\x95\xfe\x00\xa4\x70\x81\x03\x01\x15\x02\x00",328},{0xfeac3c,"\xf3\x0f\x10\x45\xf8\x41\x0f\x2f\xc5\x72\x32\xf3\x0f\x10\x45\x00\x41\x0f\x28\xcb\xf3\x41\x0f\x59\xcb\x0f\x2f\xc1\x77\x1f\x8b\x85\x00\x06\x00\x00\x8d\x0c\x40\x49\x8b\x45\x08\xc1\xe1\x03\xf3\x0f\x10\x44\x01\x0c\xf3\x0f\x59\xc6\x0f\x2f\xd0\x76\x7a\x41\x8b\x86\x80\x00\x00\x00\x83\xf8\x0a\x74\x07\xff\xc8\x83\xf8\x03\x77\x67\x41\x0f\xb6\x46\x28\xc0\xe8\x04\xa8\x01\x75\x5b\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41\xff\x90\x98\x00\x00\x00\x84\xc0\x74\x40\x41\xff\x86\x34\x03\x00\x00\x41\x83\xbe\x34\x03\x00\x00\x02\x0f\x8e\x90\xfe\xff\xff\x48\x8b\x44\x24\x50\x4d\x8b\x04\x24\x49\x8b\xcc\x8b\x54\x30\x08\x41",157}};for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(log,"REFUSED NPC decision63 signature=%zx\n",g.rva);return false;}
 uint64_t renderer=reinterpret_cast<uint64_t>(rendererWorld),handle{};uint8_t h[16]{};
 if(!Q(renderer,0,reinterpret_cast<uint64_t>(base)+0x3094180) || !Read(&world,renderer+0x28,8) || !Read(&handle,source+0x250,8) || !Read(h,handle,16) || Get<uint64_t>(h,0)!=world || int32_t(Get<uint32_t>(h,12))>=0 || !Read(&bulk,world+0x998,8) || !Read(&collection,bulk+0x500,8) || !Read(&manager,collection+0xc0,8) || !Read(&selector,bulk+0x4f8,8) || !Identity(false) || !Writable(manager))return false;
 originalTable=base+0x2f6bcb0;auto table=reinterpret_cast<void**>(originalTable);if(table[-1]!=nullptr || table[MethodCount]!=nullptr)return false;for(unsigned i=0;i<MethodCount;++i)if(table[i]!=base+Methods[i])return false;
 memcpy(clone,table-1,sizeof(clone));clone[1+Slot]=reinterpret_cast<void*>(Observer);clone[1+GateSlot]=reinterpret_cast<void*>(EligibilityObserver);
#ifdef ACUNITYVR_STATION90
 if(!StationSubmission90::Configure(base,source,world,manager,Read,SubmissionOwner,log,output))return false;
 clone[1+0x1c8/8]=reinterpret_cast<void*>(SubmissionObserver);
#endif
#ifdef ACUNITYVR_DECISION63_CHECK
 extern Callback checkNative,checkNativeGate;native=checkNative;nativeGate=checkNativeGate?checkNativeGate:reinterpret_cast<Callback>(base+0x8085e0);
#else
 native=reinterpret_cast<Callback>(base+0xfd2480);nativeGate=reinterpret_cast<Callback>(base+0x8085e0);
#endif
 enabled=true;if(!Replace(true)){enabled=false;return false;}installed=true;fprintf(log,"NPC decision63 prepared exact81-method private manager table/A0 observer plus98 eligibility guard at feacaf; F11 guard ON; current binocular bounds, priority-only live NPC removals vetoed; live-NPC activation and LOD unchanged\n");fflush(log);return true;
}
void BeginFrame(uint64_t sequence){if(!enabled.load())return;
#ifdef ACUNITYVR_STATION90
 StationSubmission90::Poll(sequence);
#endif
#ifdef ACUNITYVR_DECISION63_CHECK
 bool down=checkF11;
#else
 bool down=(GetAsyncKeyState(VK_F11)&0x8000)!=0;
#endif
 if(down&&!f11Down){guardEnabled=!guardEnabled.load();fprintf(report,"NPC eligibility63 F11 guard=%d sequence=%llu\n",int(guardEnabled.load()),sequence);fflush(report);}f11Down=down;
 auto now=GetTickCount64();if(now-lastGuardStatus>=5000){uint64_t n{},p{},r{};{Lock guard;n=guardTotal;p=protectedCount;r=guardRefused;}fprintf(report,"NPC eligibility63 guard=%d seen=%llu protected=%llu refused=%llu sequence=%llu\n",int(guardEnabled.load()),n,p,r,sequence);fflush(report);lastGuardStatus=now;}
}
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame){if(!enabled.load())return;bool down=F8();if(down && !f8Down){Snapshot(sequence,tracking,frame,"F8");GuardSnapshot(sequence,tracking,frame,"F8");
#ifdef ACUNITYVR_STATION90
 StationSubmission90::Save(sequence,tracking,frame);
#endif
 }f8Down=down;}
void Stop(){if(!enabled.exchange(false))return;bool owned=Q(manager,0,reinterpret_cast<uint64_t>(clone+1));bool identity=owned && Identity(true);bool restored=identity && Replace(false);installed=false;Snapshot(0,0,0,"Stop");GuardSnapshot(0,0,0,"Stop");fprintf(report,"NPC decision63 retired owned=%d identity=%d restored=%d inFlight=%u; private table retained until process exit\n",owned,identity,restored,inFlight.load());fflush(report);}
}
