// Tracked head position/direction for the native crowd cone only. No global
// camera, native code/shared table, actor flags, distance/FOV/budget bypass.
#include "NpcCone63.h"
#include "NpcDecision63.h"
#include "BulkDraw83.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <share.h>
namespace NpcCone63 {namespace {
constexpr unsigned MethodCount=15,Slot=0x48/8,RecordLimit=512;
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
uint8_t* image{};uint64_t source{},world{},bulk{},collection{},manager{},selector{};FILE* report{};std::filesystem::path directory;void* original{};void* clone[MethodCount+1]{};Callback native{};
std::atomic<bool> enabled{},installed{};std::atomic<unsigned> inFlight{};SRWLOCK lock=SRWLOCK_INIT;
uint64_t total{},captureId{},applied{},refused{},priorityApplied{},lastStatus{};bool f8Down{},f2Down{},correction=true,priorityCorrection=false;
#ifdef ACUNITYVR_CONE63_CHECK
bool checkF8{},checkF2{},checkF11{};uint64_t checkNow{};
#endif
uint64_t Now(){
#ifdef ACUNITYVR_CONE63_CHECK
 return checkNow;
#else
 return GetTickCount64();
#endif
}
bool Key(int code){
#ifdef ACUNITYVR_CONE63_CHECK
 return code==VK_F8?checkF8:code==VK_F11?checkF11:checkF2;
#else
 return (GetAsyncKeyState(code)&0x8000)!=0;
#endif
}
struct Head {uint64_t tick{},sequence{},tracking{};uint32_t frame{};bool valid{},correction{},opticsValid{},priorityCorrection{},frustumsValid{};BinocularVisibility63::Frustums views{};float opticsCos{};float position[4]{},direction[4]{};} head;
struct Record {uint64_t id{},caller{},tick{},rbp{},selector{},sequence{},tracking{},headAge{};uint32_t depth{},status{},sourceFrame{},headFrame{};bool recovered{},applied{},correction{},thresholdsValid{},priorityEnabled{},priorityApplied{};float priorityAfter{},opticsCos{};float activationDot{},priorityDot{},removalDot{};uint8_t positionBefore[16]{},directionBefore[16]{},positionAfter[16]{},directionAfter[16]{};};std::array<Record,RecordLimit> records{},snapshot{};
constexpr std::array<size_t,MethodCount> Methods{0xfa8950,0xfc9370,0xfeb4d0,0xff0500,0xfce760,0xfd19a0,0xfcf840,0xfcf910,0xfcffa0,0x21d2f30,0xfd2ef0,0xfb2b40,0x204d950,0xfd5b30,0xfd59c0};
struct Lock {Lock(){AcquireSRWLockExclusive(&lock);}~Lock(){ReleaseSRWLockExclusive(&lock);}};struct Flight {Flight(){++inFlight;}~Flight(){--inFlight;}};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(!out || p<0x10000 || p>0x00007ffffffeffffULL || !n || n>0x10000 || p+n<p)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Q(uint64_t p,size_t o,uint64_t expected){uint64_t v{};return Read(&v,p+o,8) && v==expected;}
bool Identity(bool own){uint64_t vt{},bulkTable{};return Q(world,0,reinterpret_cast<uint64_t>(image)+0x30225e0) && Q(world,0x998,bulk) && Read(&bulkTable,bulk,8) && BulkDraw83::CanonicalBulk(bulk,bulkTable)==reinterpret_cast<uint64_t>(image)+0x304ab90 && Q(bulk,0x500,collection) && Q(bulk,0x4f8,selector) && Q(collection,0,own?reinterpret_cast<uint64_t>(clone+1):reinterpret_cast<uint64_t>(image)+0x2f6b880) && Q(collection,0xc0,manager) && Read(&vt,manager,8) && NpcDecision63::CanonicalManager(manager,vt)==reinterpret_cast<uint64_t>(image)+0x2f6bcb0 && Q(manager,0x278,world) && Q(selector,0,reinterpret_cast<uint64_t>(image)+0x2f6b980) && Q(selector,0x80,bulk);}
bool Vector(const uint8_t* raw,bool direction){float v[4]{};memcpy(v,raw,16);for(float f:v)if(!std::isfinite(f))return false;if(std::abs(v[3])>1e-4f)return false;if(direction && std::abs(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]-1)>1e-3f)return false;return true;}
struct alignas(16) Lanes {float activation[4],priority[4],removal[4];};
extern "C" uint64_t __fastcall NpcCone63Thunk(void*,void*,void*,void*);
__declspec(noinline) void Apply(Record& out,const Head& h,Lanes* lanes) noexcept {
 __try {
  CONTEXT c{};RtlCaptureContext(&c);ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);out.status=1;
  for(unsigned depth=0;depth<24;++depth){
   if(c.Rsp<low || c.Rsp>high-8 || (c.Rsp&7) || !c.Rip){out.status=2;return;}
   if(c.Rip==reinterpret_cast<uint64_t>(image)+0xfe9780){
    DWORD64 owner{};auto fn=RtlLookupFunctionEntry(c.Rip,&owner,nullptr);if(!fn || owner!=reinterpret_cast<uint64_t>(image) || fn->BeginAddress!=0xfe95a8 || fn->EndAddress!=0xfea776 || fn->UnwindData!=0x381710c){out.status=3;return;}
    out.recovered=true;out.depth=depth;out.rbp=c.Rbp;out.selector=c.R13;
    // Read recovered caller nonvolatile registers only; never write a CONTEXT or threshold.
    memcpy(&out.activationDot,&c.Xmm8,4);memcpy(&out.priorityDot,&c.Xmm12,4);memcpy(&out.removalDot,&c.Xmm13,4);
    out.thresholdsValid=std::isfinite(out.activationDot) && std::isfinite(out.priorityDot) && std::isfinite(out.removalDot) && out.activationDot>=-1 && out.activationDot<=1 && out.priorityDot>=-1 && out.priorityDot<=1 && out.removalDot>=-1 && out.removalDot<=1 && std::abs(out.activationDot-out.removalDot-0.02f)<1e-4f;out.priorityAfter=out.priorityDot;out.opticsCos=h.opticsCos;out.priorityEnabled=h.priorityCorrection;

    if(c.R13!=selector || c.Rbx!=bulk || out.rbp<low || out.rbp>high-0xe0 || !Identity(true)){out.status=4;return;}
    if(!Read(out.positionBefore,out.rbp+0x30,16) || !Read(out.directionBefore,out.rbp+0x90,16) || !Vector(out.positionBefore,false) || !Vector(out.directionBefore,true)){out.status=5;return;}
    out.sequence=h.sequence;out.tracking=h.tracking;out.headFrame=h.frame;out.correction=h.correction;out.headAge=out.tick>=h.tick?out.tick-h.tick:~uint64_t(0);
    if(!Read(&out.sourceFrame,source+0x294,4)){out.status=6;return;}
    if(!h.correction){memcpy(out.positionAfter,out.positionBefore,16);memcpy(out.directionAfter,out.directionBefore,16);out.status=0;return;}
    if(!h.valid || out.headAge>100 || out.sourceFrame<h.frame || out.sourceFrame-h.frame>4 || !Vector(reinterpret_cast<const uint8_t*>(h.position),false) || !Vector(reinterpret_cast<const uint8_t*>(h.direction),true)){out.status=7;return;}
    uint8_t p[16]{},d[16]{};if(!Identity(true) || !Read(p,out.rbp+0x30,16) || !Read(d,out.rbp+0x90,16) || memcmp(p,out.positionBefore,16) || memcmp(d,out.directionBefore,16)){out.status=8;return;}
    // Only these two proven native stack locals are written; the frame/thread is
    // suspended in its own callback and owns the ranges until native fe9500 returns.
    memcpy(reinterpret_cast<void*>(out.rbp+0x30),h.position,16);memcpy(reinterpret_cast<void*>(out.rbp+0x90),h.direction,16);
    memcpy(out.positionAfter,h.position,16);memcpy(out.directionAfter,h.direction,16);out.applied=true;
    // Failed priority scalar experiment permanently disabled. Native lanes remain exact.
    (void)lanes;
    out.status=0;return;
   }
   DWORD64 module{};auto fn=RtlLookupFunctionEntry(c.Rip,&module,nullptr);auto previous=c.Rsp,previousRip=c.Rip;
   if(fn){PVOID handler{};DWORD64 frame{};RtlVirtualUnwind(0,module,c.Rip,fn,&c,&handler,&frame,nullptr);}else{if(!Read(&c.Rip,c.Rsp,8)){out.status=9;return;}c.Rsp+=8;}
   if(c.Rsp<=previous || c.Rsp>high || c.Rip==previousRip){out.status=10;return;}
  }out.status=11;
 }__except(EXCEPTION_EXECUTE_HANDLER){out.status=12;}
}
extern "C" __declspec(noinline) uint64_t __fastcall NpcCone63Observer(void* a,void* b,void* c,void* d,Lanes* lanes,uint64_t caller){
 if(!enabled.load(std::memory_order_acquire))return native(a,b,c,d);Flight flight;Record r{};r.caller=caller;bool take=reinterpret_cast<uint64_t>(a)==collection && r.caller==reinterpret_cast<uint64_t>(image)+0xfe9780;
 if(take){Head h{};{Lock guard;h=head;}r.tick=Now();Apply(r,h,lanes);}
 uint64_t value=native(a,b,c,d); // no lock, reader SEH or I/O around native code
 if(take){Lock guard;if(enabled.load()){r.id=++total;records[(r.id-1)%RecordLimit]=r;if(r.applied)++applied;else if(r.status)++refused;if(r.priorityApplied)++priorityApplied;}}
 return value;
}
bool Replace(bool attach) noexcept {__try{return InterlockedCompareExchangePointer(reinterpret_cast<void**>(collection),attach?static_cast<void*>(clone+1):original,attach?original:static_cast<void*>(clone+1))==(attach?original:static_cast<void*>(clone+1));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Writable(uint64_t p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
void Snapshot(uint64_t sequence,uint64_t tracking,uint32_t frame,const char* boundary){uint64_t end{},a{},r{};unsigned count{};{Lock guard;end=total;a=applied;r=refused;count=unsigned((std::min)(end,uint64_t(RecordLimit)));for(unsigned i=0;i<count;++i)snapshot[i]=records[(end-count+i)%RecordLimit];}
 std::error_code error;std::filesystem::create_directories(directory,error);if(error)return;auto path=directory/(L"cone-"+std::to_wstring(++captureId)+L"-frame-"+std::to_wstring(frame)+L".jsonl");FILE* f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;fprintf(f,"{\"event\":\"begin\",\"schema\":1,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"total\":%llu,\"retained\":%u,\"applied\":%llu,\"refused\":%llu,\"boundary\":\"%s\",\"note\":\"Scoped native camera position/direction locals at exact selector fe9780. Complete15-method private collection table/virtual48, no shared vtable or executable patch. Original forward once. Recovered native cone threshold registers recorded read-only; rejected threshold data is not evidence. Native priority scalar experiment disabled; the separate F11 eligibility guard uses the exact renderer binocular frustums. Latest accepted renderer head-centre input, max100ms/four source frames. F2 comparison; invalid/stale/foreign identity refuses both writes. Cached source quaternion is not used after this boundary and remains native.\"}\n",sequence,tracking,frame,end,count,a,r,boundary);
 for(unsigned i=0;i<count;++i){auto& x=snapshot[i];fprintf(f,"{\"event\":\"cone\",\"id\":%llu,\"caller\":\"%llx\",\"tick\":%llu,\"rbp\":\"%llx\",\"selector\":\"%llx\",\"sequence\":%llu,\"tracking\":%llu,\"headAgeMs\":%llu,\"sourceFrame\":%u,\"headFrame\":%u,\"status\":%u,\"depth\":%u,\"recovered\":%s,\"correction\":%s,\"applied\":%s",x.id,x.caller,x.tick,x.rbp,x.selector,x.sequence,x.tracking,x.headAge,x.sourceFrame,x.headFrame,x.status,x.depth,x.recovered?"true":"false",x.correction?"true":"false",x.applied?"true":"false");fprintf(f,",\"thresholdsValid\":%s,\"activationDot\":%.9g,\"priorityDot\":%.9g,\"removalDot\":%.9g",x.thresholdsValid?"true":"false",x.thresholdsValid?x.activationDot:0.0f,x.thresholdsValid?x.priorityDot:0.0f,x.thresholdsValid?x.removalDot:0.0f);fprintf(f,",\"priorityEnabled\":%s,\"priorityApplied\":%s,\"priorityAfter\":%.9g,\"opticsCos\":%.9g",x.priorityEnabled?"true":"false",x.priorityApplied?"true":"false",std::isfinite(x.priorityAfter)?x.priorityAfter:0.0f,x.opticsCos);const struct{const char* name;const uint8_t* data;} blocks[]={{"positionBefore",x.positionBefore},{"directionBefore",x.directionBefore},{"positionAfter",x.positionAfter},{"directionAfter",x.directionAfter}};for(auto& block:blocks){fprintf(f,",\"%s\":\"",block.name);if(!x.status)for(unsigned k=0;k<16;++k)fprintf(f,"%02x",block.data[k]);fprintf(f,"\"");}fprintf(f,"}\n");}fprintf(f,"{\"event\":\"end\",\"count\":%u,\"inFlight\":%u}\n",count,inFlight.load());fclose(f);fprintf(report,"NPC cone63 snapshot=%llu frame=%u completed=%llu applied=%llu refused=%llu\n",captureId,frame,end,a,r);fflush(report);
}
}
uint64_t CanonicalCollection(uint64_t object,uint64_t table){return object==collection && table==reinterpret_cast<uint64_t>(clone+1)?reinterpret_cast<uint64_t>(original):table;}
bool Install(uint8_t* base,void* primary,void* rendererWorld,const std::filesystem::path& output,FILE* log){image=base;source=reinterpret_cast<uint64_t>(primary);report=log;directory=output/L"npc-cone63";const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0xfe9500,"\x48\x8b\xc4\x48\x89\x48\x08\x55\x56\x41\x54\x48\x8d\xa8\x08\xfa\xff\xff\x48\x81\xec\xe0\x06\x00\x00\x48\x8b\xb1\x80\x00\x00\x00\x48\x89\x58\xe0\x4c\x89\x68\xd0\x48\x81\xc6\x14\x05\x00\x00\x4c\x8b\xe9",50},{0xfe957d,"\x48\x89\xbc\x24\xd0\x06\x00\x00\x49\x8b\xbd\x80\x00\x00\x00\x4c\x89\xb4\x24\xc0\x06\x00\x00\x4c\x89\xbc\x24\xb8\x06\x00\x00\x48\x8d\x55\x30\x0f\x29\xb4\x24\xa0\x06\x00\x00\x0f\x29\xbc\x24\x90\x06\x00\x00\x44\x0f\x29\x84\x24\x80\x06\x00\x00\x44\x0f\x29\x8c\x24\x70\x06\x00\x00\x44\x0f\x29\x94\x24\x60\x06\x00\x00\x44\x0f\x29\x9c\x24\x50\x06\x00\x00\x44\x0f\x29\xa4\x24\x40\x06\x00\x00\x44\x0f\x29\xac\x24\x30\x06\x00\x00",105},{0xfe95e6,"\xe8\x65\x7e\xea\x00\x48\x8b\x0d\x76\x11\x23\x04\x48\x8d\x95\xd0\x00\x00\x00\xe8\x32\x7d\xea\x00\x48\x8b\x0d\x63\x11\x23\x04\xe8\xc6\x7b\xea\x00\x44\x0f\x28\xd0\xe8\x3d\xf4\x90\x01\x0f\x28\x9d\xd0\x00\x00\x00\x0f\x57\xff\x0f\x28\x35\xbc\x8c\x60\x02\x44\x0f\x28\x0d\x34\x54\x63\x02\x0f\x28\x2d\x9d\x42\x60\x02\x44\x0f\x28\xe0\x0f\x28\xe3\x0f\x28\xc3\x0f\x59\xe6\x0f\xc6\xc3\xd2\x0f\x28\xd4\x0f\x28\xcc\x44\x0f\x28\xc6\x0f\xc6\xcc\x55\x0f\xc6\xd4\xaa\x44\x0f\xc6\xc6\xd2\xf3\x0f\x58\xca\x0f\x28\xd3\xf3\x0f\x58\xe1\x0f\x28\xce\x0f\xc6\xd3\xff\x0f\xc6\xce\xc9\x0f\xc6\xe4\x00\x0f\x59\xc8\x0f\x59\xe3\x0f\x28\xc3\x0f\xc6\xc3\xc9\x44\x0f\x59\xc0\x44\x0f\x5c\xc1\x44\x0f\x59\xc2\x0f\x59\xd2\x44\x0f\x58\xc4\x41\x0f\x5c\xd1\x0f\x59\xd6\x44\x0f\x58\xc2\x44\x0f\x59\x05\x68\xe9\xe8\x01\x44\x0f\x54\xc5\x0f\x55\xef\x44\x0f\x56\xc5\x41\x0f\x28\xc8\x41\x0f\x59\xc8\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x0f\x28\x0d\xda\xe9\xe8\x01\x0f\x5c\xfa\x0f\x52\xda\x0f\x28\xe2\x44\x0f\x59\xcb\x0f\x5f\xe7\x0f\x28\xc3\x0f\xc2\x25\xdf\x47\x60\x02\x02\x0f\x59\xc2\x0f\x59\xc3\x0f\x5c\xc8\x41\x0f\x59\xc9\x49\x8b\x9d\x80\x00\x00\x00\xf3\x44\x0f\x59\x15\x0a\x3a\xf8\x01\x41\x0f\x59\xc8\x0f\x28\xc4\xf3\x0f\x10\xbb\xdc\x02\x00\x00\x0f\x55\xc1\x44\x0f\x54\xc4\x44\x0f\x28\xdf\x0f\x28\xf7\xf3\x44\x0f\x58\x1d\xdc\xcc\x6e\x02\xf3\x0f\x58\x35\x44\xe8\xe8\x01\x41\x0f\x56\xc0\x0f\x29\x85\x90\x00\x00\x00\x41\x0f\x28\xc2\xe8\x00\xf3\x90\x01\x44\x38\x25\xc6\xf5\x22\x04\x44\x0f\x28\xc0\x74\x09\xf3\x44\x0f\x58\x05\xda\x39\xf8\x01\x48\x8b\x8b\x00\x05\x00\x00\x45\x0f\x28\xe8\xf3\x44\x0f\x5c\x2d\xde\x98\xef\x02\x48\x8b\x01\xff\x50\x48",410},{0xfe98dc,"\x48\x8d\x55\x30\x0f\x28\xd6\xe8\xb8\x7c\xee\x00\x49\x8b\x07\x49\x8b\xcf\xff\x50\x30\x8b\x45",23},{0xfe9c14,"\x4c\x8d\x8d\x90\x00\x00\x00\x4c\x8d\x45\x30\x48\x8d\x95\xe0\x00\x00\x00\x49\x8b\xcd\x48\x89\x44\x24\x20\xe8\xdd\x61\xfc\xff",31},{0xfeab99,"\x4c\x8d\x8d\x90\x00\x00\x00\x4c\x89\x34\x30\x49\x8b\x46\x10\x4c\x8d\x45\x30\x41\x0f\x28\x86\xe0\x01\x00\x00\x66\x0f\x7f\x85\x00\x01\x00\x00\x48\x63\x48\x0c\x48\x8d\x95\x00\x01\x00\x00\x48\xc1\xe1\x20\x48\xc1\xf9\x3f\x48\x23\x08\x41\x0f\xb7\x46\x72\x25\xff\x0f\x00\x00\x48\x89\x8d\x18\x06\x00\x00\x49\x8b\xcd\x89\x85\x00\x06\x00\x00\x48\x8d\x45\xf8\x48\x89\x44\x24\x28\x48\x8d\x45\x00\x48\x89\x44\x24\x20\xe8\x0d\x52\xfc\xff",106},{0x21d2f30,"\x48\x85\xc9\x74\x05\x48\x8d\x41\x08\xc3\x33\xc0\xc3",13},{0x38170a4,"\x01\x19\x05\x00\x19\x01\xdc\x00\x0b\xc0\x09\x60\x08\x50\x00\x00\x21\x08\x04\x00\x08\xd4\xd9\x00\x04\x34\xdb\x00\x00\x95\xfe\x00\x20\x95\xfe\x00\xa4\x70\x81\x03\x21\x17\x04\x00\x17\xe4\xd8\x00\x08\x74\xda\x00\x20\x95\xfe\x00\x7d\x95\xfe\x00\xb4\x70\x81\x03\x21\x08\x02\x00\x08\xf4\xd7\x00\x7d\x95\xfe\x00\x94\x95\xfe\x00\xcc\x70\x81\x03\x21\x08\x02\x00\x08\x68\x6a\x00\x94\x95\xfe\x00\xa0\x95\xfe\x00\xe4\x70\x81\x03\x21\x3e\x0e\x00\x3e\xd8\x63\x00\x35\xc8\x64\x00\x2c\xb8\x65\x00\x23\xa8\x66\x00\x1a\x98\x67\x00\x11\x88\x68\x00\x08\x78\x69\x00\xa0\x95\xfe\x00\xa8\x95\xfe\x00\xf8\x70\x81\x03",148},{0xfafe10,"\x48\x8b\xc4\x48\x81\xec\xb8\x00\x00\x00\x0f\x28\x15\xaf\xda\x63\x02\x0f\x28\x2d\x88\x82\xec\x01\x48\x8b\x8c\x24\xe0\x00\x00\x00\x0f\x29\x70\xe8\x0f\x29\x78\xd8\x44\x0f\x29\x40\xc8\x0f\x57\xf6\x44\x0f\x29\x48\xb8\x44\x0f\x29\x50\xa8\x44\x0f\x29\x58\x98\x44\x0f\x28\x1d\x09\xec\x66\x02\x44\x0f\x29\x60\x88\x44\x0f\x29\x6c\x24\x30\x44\x0f\x28\x2a\x44\x0f\x29\x74\x24\x20\x44\x0f\x29\x7c\x24\x10\x41\x0f\x28\xc5\x0f\x29\x04\x24\x45\x0f\x5c\x28\xf3\x0f\x10\x42\x08\xf3\x0f\x58\x05\x9d\x8b\xed\x01\x44\x0f\x54\xea\xf3\x0f\x11\x44\x24\x08\x44\x0f\x28\x04\x24\x0f\x28\xc2\x0f\x55\xc6\x44\x0f\x56\xe8\x45\x0f\x5c\x00\x45\x0f\x28\xcd\x44\x0f\x54\xc2\x0f\x55\xd6\x45\x0f\x59\xcd\x44\x0f\x56\xc2\x41\x0f\x28\xf8\x41\x0f\x28\xc1\x41\x0f\x28\xc9\x41\x0f\xc6\xc1\x4e\x41\x0f\x59\xf8\x0f\x58\xc8\x0f\x28\xc6\x44\x0f\x28\xf9\x44\x0f\xc6\xf9\x11\x44\x0f\x58\xf9\x0f\x28\xcf\x41\x0f\x5c\xc7\x45\x0f\x28\xd7\x45\x0f\x52\xe7\x44\x0f\x5f\xd0\x44\x0f\xc2\x15\xd1\xdf\x63\x02\x02\x0f\x28\xc7\x0f\xc6\xc7\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x0f\x28\xcd\x0f\x5c\xf2\x0f\x52\xda\x0f\x28\xe2\x0f\x5f\xe6\x0f\x28\xc3\x0f\xc2\x25\xa3\xdf\x63\x02\x02\x0f\x59\xc2\x0f\x59\xc3\x44\x0f\x28\xf4\x41\x0f\x54\xe0\x0f\x5c\xc8\x41\x0f\x28\xc3\x0f\x59\xc3\x0f\x59\xc8\x41\x0f\x28\xc1\x41\x0f\x59\xc8\x41\x0f\xc6\xc1\x4e\x44\x0f\x58\xc8\x0f\x28\xc7\x44\x0f\x55\xf1\x41\x0f\x28\xc9\x41\x0f\xc6\xc9\x11\x44\x0f\x56\xf4\x41\x0f\x58\xc9\xf3\x0f\x11\x09\xf3\x0f\x10\x1d\xab\xd1\xfb\x01\xf3\x0f\x10\x35\x8b\x64\x72\x02\x0f\xc6\xc7\x4e\x0f\x2f\xcb\x0f\x58\xf8\x0f\x28\xc7\x0f\xc6\xc7\x11\x0f\x58\xc7\x0f\x29\x04\x24\x73\x05\x0f\x28\xd6\xeb\x42\x41\x0f\x28\xc4\x41\x0f\x28\xca\x45\x0f\x59\xdc\x41\x0f\x59\xc7\x45\x0f\x54\xd5\x41\x0f\x59\xc4\x0f\x5c\xe8\x41\x0f\x59\xeb\x41\x0f\x59\xed\x0f\x55\xcd\x41\x0f\x56\xca\x41\x0f\x59\x09\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd1\x0f\xc6\xd1\x11\x0f\x58\xd1\x48\x8b\x84\x24\xe8\x00\x00\x00\xf3\x0f\x10\x2c\x24\x0f\x2f\xeb\xf3\x0f\x11\x10\x73\x05\x0f\x28\xde\xeb\x1c\x41\x0f\x28\x09\x41\x0f\x59\xce\x0f\x28\xc1\x0f\xc6\xc1\x4e\x0f\x58\xc8\x0f\x28\xd9\x0f\xc6\xd9\x11\x0f\x58\xd9\x0f\x28\xce\x0f\x28\xc3\xf3\x0f\x58\xd6\xf3\x0f\x5c\x8c\x24\xf0\x00\x00\x00\xf3\x0f\x58\xc6\xf3\x0f\x59\x0d\x18\x30\xf3\x02\xf3\x0f\x58\x0d\x0c\x30\xf3\x02\x0f\x28\xe1\xf3\x0f\x5c\xc8\xf3\x0f\x5c\xe2\xf3\x0f\x59\xcd\xf3\x0f\x59\x21\x0f\x2f\xcc\x73\x0d\xf3\x0f\x11\x29\x0f\x28\xc1\xf3\x0f\x11\x18\xeb\x03\x0f\x28\xc4\x44\x0f\x28\x6c\x24\x30\x44\x0f\x28\x74\x24\x20\x44\x0f\x28\x7c\x24\x10\x4c\x8d\x9c\x24\xb8\x00\x00\x00\x41\x0f\x28\x73\xe8\x41\x0f\x28\x7b\xd8\x45\x0f\x28\x43\xc8\x45\x0f\x28\x4b\xb8\x45\x0f\x28\x53\xa8\x45\x0f\x28\x5b\x98\x45\x0f\x28\x63\x88\x49\x8b\xe3\xc3",669},{0x3ee3050,"\xa3\x01\x00\x40\x5c\x8f\xc2\x3e\x0a\xd7\xa3\x3c",12}};for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(log,"REFUSED NPC cone63 signature=%zx\n",g.rva);return false;}
 uint64_t renderer=reinterpret_cast<uint64_t>(rendererWorld),handle{};uint8_t h[16]{};
 if(!Q(renderer,0,reinterpret_cast<uint64_t>(base)+0x3094180) || !Read(&world,renderer+0x28,8) || !Read(&handle,source+0x250,8) || !Read(h,handle,16))return false;uint64_t handleWorld{};uint32_t token{};memcpy(&handleWorld,h,8);memcpy(&token,h+12,4);
 if(handleWorld!=world || int32_t(token)>=0 || !Read(&bulk,world+0x998,8) || !Read(&collection,bulk+0x500,8) || !Read(&manager,collection+0xc0,8) || !Read(&selector,bulk+0x4f8,8) || !Identity(false) || !Writable(collection))return false;
 original=base+0x2f6b880;auto table=reinterpret_cast<void**>(original);if(table[-1] || table[MethodCount])return false;for(unsigned i=0;i<MethodCount;++i)if(table[i]!=base+Methods[i])return false;memcpy(clone,table-1,sizeof(clone));clone[1+Slot]=reinterpret_cast<void*>(NpcCone63Thunk);
#ifdef ACUNITYVR_CONE63_CHECK
 extern Callback checkNative;native=checkNative;
#else
 native=reinterpret_cast<Callback>(base+0x21d2f30);
#endif
 enabled=true;if(!Replace(true)){enabled=false;return false;}installed=true;fprintf(log,"NPC cone63 prepared private full15-method collection table/virtual48; exact native selector camera locals only, F2 ON, no camera entity/global/flags/cone/distance/budget writes; native priority scalar unchanged; exact eye frustums published for separate F11 eligibility guard\n");fflush(log);return true;
}
void BeginFrame(uint32_t frame,uint64_t sequence,uint64_t tracking,bool tracked,const CameraMath::Matrix poses[2],const Transport::Eye* optics){if(!enabled.load())return;bool down=Key(VK_F2);if(down && !f2Down){correction=!correction;fprintf(report,"NPC cone63 F2 correction=%d sequence=%llu\n",correction,sequence);fflush(report);}f2Down=down;Head h{};h.tick=Now();h.frame=frame;h.sequence=sequence;h.tracking=tracking;h.correction=correction;h.priorityCorrection=false;
 if(tracked && tracking && CameraMath::Rigid(poses[0]) && CameraMath::Rigid(poses[1])){bool same=true;for(unsigned i=0;i<12;++i)if(std::abs(poses[0].m[i]-poses[1].m[i])>1e-4f)same=false;if(same){for(unsigned i=0;i<3;++i){h.position[i]=(poses[0].m[12+i]+poses[1].m[12+i])*.5f;h.direction[i]=poses[0].m[4+i];}h.valid=true;}}
 if(h.valid && optics){bool ok=true;float smallest=1;for(unsigned i=0;i<2;++i){float angles[]={optics[i].left,optics[i].right,optics[i].up,optics[i].down};for(float a:angles)if(!std::isfinite(a)||std::abs(a)>=1.5f)ok=false;if(!(angles[0]<0 && angles[1]>0 && angles[2]>0 && angles[3]<0))ok=false;if(ok)for(unsigned x=0;x<2;++x)for(unsigned y=2;y<4;++y){float tx=std::tan(angles[x]),ty=std::tan(angles[y]);float v=1/std::sqrt(1+tx*tx+ty*ty);if(!std::isfinite(v)||v<=0)ok=false;smallest=(std::min)(smallest,v);}}if(ok){h.opticsValid=true;h.opticsCos=smallest;}}
  CameraMath::Matrix depth{};if(h.valid && h.opticsValid && Read(&depth,source+0x320,sizeof(depth)))h.frustumsValid=BinocularVisibility63::Build(depth,poses,optics,h.views);
 {Lock guard;head=h;}auto now=Now();if(now-lastStatus>=5000){uint64_t a{},r{},p{};{Lock guard;a=applied;r=refused;p=priorityApplied;}fprintf(report,"NPC cone63 head frame=%u sequence=%llu tracking=%llu valid=%d correction=%d applied=%llu refused=%llu; native cone position/direction, distance/cone/crowd budget untouched; priorityOptics=%d valid=%d targetCos=%.9g priorityApplied=%llu frustums=%d\n",frame,sequence,tracking,h.valid,h.correction,a,r,h.priorityCorrection,h.opticsValid,h.opticsCos,p,h.frustumsValid);fflush(report);lastStatus=now;}
}
bool CurrentFrustums(uint32_t frame,BinocularVisibility63::Frustums& views,uint64_t& age){
 if(!enabled.load())return false;Head h{};{Lock guard;h=head;}auto now=Now();age=now>=h.tick?now-h.tick:~uint64_t(0);
 if(!h.valid || !h.correction || !h.tracking || !h.frustumsValid || age>100 || frame<h.frame || frame-h.frame>4 || !Identity(true))return false;
 views=h.views;return true;
}
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame){if(!enabled.load())return;bool down=Key(VK_F8);if(down && !f8Down)Snapshot(sequence,tracking,frame,"F8");f8Down=down;}
void Stop(){if(!enabled.exchange(false))return;bool owned=Q(collection,0,reinterpret_cast<uint64_t>(clone+1));bool identity=owned && Identity(true);bool restored=identity && Replace(false);installed=false;Snapshot(0,0,0,"Stop");fprintf(report,"NPC cone63 retired owned=%d identity=%d restored=%d inFlight=%u; private table retained until process exit\n",owned,identity,restored,inFlight.load());fflush(report);}
}
