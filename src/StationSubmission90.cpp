// Scoped fallback-model submission, after the native mouse cone and immediately
// before its existing producer callback. No activation, LOD or camera-cache edits.
#include "StationSubmission90.h"
#include "BulkDraw83.h"
#include "DenseVisibility87.h"
#include <array>
#include <atomic>
#include <cstring>
#include <share.h>
namespace StationSubmission90 { namespace {
#include "StationSubmission90Signatures.h"
constexpr uint16_t Visible=0x1000;
constexpr unsigned Limit=4096,RecordLimit=128;
uint8_t* image{};uint64_t source{},world{},manager{};Reader read{};Owner owned{};FILE* report{};std::filesystem::path directory;
std::atomic<bool> active{};bool configured{};uint64_t lastStatus{},capture{};
std::array<std::atomic_flag,Limit> slots{};
std::atomic<uint64_t> seen{},candidates{},applied{},restored{},conflicts{},stale{},refused{},off{},nativeVisible{},outside{};
std::atomic<uint64_t> firstCaller{};
struct Head {CameraMath::Matrix pose{};uint64_t tick{},sequence{},tracking{};uint32_t frame{};float cosine{};bool valid{};} head;
struct Record {uint64_t id{},sequence{},tracking{},age{},state{},handle{},resource{};uint32_t slot{},frame{},status{};uint16_t before{},after{};float position[4]{};bool restored{};};
std::array<Record,RecordLimit> records{};uint64_t completed{};
SRWLOCK headLock=SRWLOCK_INIT,recordLock=SRWLOCK_INIT;
template<class T>T Get(const uint8_t* raw,size_t at){T v{};memcpy(&v,raw+at,sizeof(v));return v;}
uint64_t Now(){return GetTickCount64();}
bool Q(uint64_t p,size_t at,uint64_t value){uint64_t got{};return read(&got,p+at,8)&&got==value;}
bool Pool(uint64_t& pool,uint64_t& states,unsigned& count){uint8_t raw[0x78]{};
 if(!owned() || !read(&pool,manager+0x40,8) || !read(raw,pool,sizeof(raw)))return false;
 count=Get<unsigned>(raw,0x68);auto cap=Get<unsigned>(raw,0x6c)&0x1fffffff;states=Get<uint64_t>(raw,0x60);
 return Get<uint64_t>(raw,0)==uint64_t(image)+0x304c640 && Get<uint64_t>(raw,0x50)==pool && Get<uint64_t>(raw,0x58)==manager && Get<uint64_t>(raw,0x70)==world && states && count && count<=Limit && cap>=count && cap<=Limit;
}
bool Exchange(uint64_t state,uint16_t expected,uint16_t value) noexcept {
 __try{return uint16_t(InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(state+0x28),SHORT(value),SHORT(expected)))==expected;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct Scope {
 Record record{};uint64_t pool{},states{};bool locked{},changed{};
 ~Scope(){
  if(changed){uint64_t p{},s{};unsigned n{};uint16_t bits{};uint32_t mode{};
   // Preserve every native bit except our temporary submission bit. Refuse
   // restoration if the slot changed ownership/resource/mode during native work.
   bool same=Pool(p,s,n)&&p==pool&&s==states&&record.slot<n&&record.state==s+uint64_t(record.slot)*0x360&&Q(record.state,0x10,record.handle)&&Q(record.state,0x200,record.resource)&&read(&mode,record.state+0x80,4)&&mode==0;
   if(same&&read(&bits,record.state+0x28,2)&&(bits&Visible)&&Exchange(record.state,bits,uint16_t(bits&~Visible))){record.restored=true;++restored;}else{++conflicts;record.status=9;}
  }
  if(locked)slots[record.slot].clear(std::memory_order_release);
  if(record.state){AcquireSRWLockExclusive(&recordLock);record.id=++completed;records[(record.id-1)%RecordLimit]=record;ReleaseSRWLockExclusive(&recordLock);}
 }
};
void Prepare(Scope& scope,void* a,void* b,void* e,uint64_t caller){
 if(!active.load(std::memory_order_acquire))return;++seen;
 if(uint64_t(a)==manager && !firstCaller.load(std::memory_order_relaxed)){uint64_t empty{};firstCaller.compare_exchange_strong(empty,caller);}
 if(!configured || !DenseVisibility87::CorrectionEnabled()){++off;return;}
 auto slot=uint64_t(b),state=uint64_t(e);
 if(uint64_t(a)!=manager || caller!=uint64_t(image)+NativeCaller || slot>=Limit || state<0x10000){++refused;return;}
 uint8_t flags[0x60]{};
 if(!read(flags,state+0x28,sizeof(flags))){++refused;return;}
 auto bits=Get<uint16_t>(flags,0);if(bits&Visible){++nativeVisible;return;}
 if(Get<uint32_t>(flags,0x58)!=0 || (flags[0x50]&2) || (bits&0xc)!=0xc)return;
 ++candidates;Head h{};AcquireSRWLockShared(&headLock);h=head;ReleaseSRWLockShared(&headLock);
 auto now=Now();uint32_t frame{};
 if(!h.valid || !h.tracking || !read(&frame,source+0x294,4) || now<h.tick || now-h.tick>100 || uint32_t(frame-h.frame)>4){++stale;return;}
 auto& r=scope.record;r.state=state;r.slot=unsigned(slot);r.frame=frame;r.sequence=h.sequence;r.tracking=h.tracking;r.age=now-h.tick;r.before=bits;r.after=bits;r.status=1;
 if(!read(r.position,state+0x1e0,16) || !HeadVisible(h.pose,r.position,h.cosine)){++outside;r.status=2;r.state=0;return;}
 unsigned count{};
 if(!Pool(scope.pool,scope.states,count) || slot>=count || state!=scope.states+slot*0x360 || !read(&r.handle,state+0x10,8) || r.handle!=uint64_t(image)+0x525bb58 || !read(&r.resource,state+0x200,8) || !r.resource){++refused;r.status=3;return;}
 if(slots[slot].test_and_set(std::memory_order_acquire)){++refused;r.status=4;return;}scope.locked=true;
 uint8_t check[0x60]{};float position[4]{};
 if(!active.load() || !DenseVisibility87::CorrectionEnabled() || !owned() || !read(check,state+0x28,sizeof(check)) || memcmp(flags,check,sizeof(flags)) || !read(position,state+0x1e0,16) || memcmp(position,r.position,16) || !Q(state,0x10,r.handle) || !Q(state,0x200,r.resource) || !Exchange(state,bits,uint16_t(bits|Visible))){++refused;r.status=5;return;}
 scope.changed=true;r.after=uint16_t(bits|Visible);r.status=0;++applied;
}
}
bool Configure(uint8_t* base,uint64_t primary,uint64_t scene,uint64_t owner,Reader reader,Owner identity,FILE* log,const std::filesystem::path& output){
 if(!base || !reader || !identity)return false;
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(log,"REFUSED StationSubmission90 signature=%x\n",g.rva);return false;}
 image=base;source=primary;world=scene;manager=owner;read=reader;owned=identity;report=log;directory=output/L"stationary-submission90";
 configured=GetPrivateProfileIntW(L"Culling",L"StationaryCrowdSubmission",1,(output/L"ACUnityVR.ini").c_str())!=0;
 active=true;fprintf(log,"StationSubmission90 prepared existing manager virtual1c8/fec740, five native args once; mode0/empty-handle fallback only, scoped bit12 admission using fresh head/native cone; NumPad9 follows DenseVisibility87, ini=%d; shared camera/activation/LOD/budgets unchanged\n",configured);fflush(log);return true;
}
void Publish(uint32_t frame){if(!active.load())return;Head h{};uint64_t age{};h.frame=frame;h.tick=Now();uint64_t camera{};
 h.valid=BulkDraw83::CurrentCamera(frame,h.pose,age,h.sequence,h.tracking)&&read(&camera,uint64_t(image)+0x521a768,8)&&read(&h.cosine,camera+0xf4,4)&&std::isfinite(h.cosine)&&h.cosine>=-1&&h.cosine<=1;
 if(h.valid && h.tick>=age)h.tick-=age;else h.valid=false;
 AcquireSRWLockExclusive(&headLock);head=h;ReleaseSRWLockExclusive(&headLock);
}
uint64_t Invoke(Callback native,void* a,void* b,void* c,void* d,void* e,uint64_t caller){
 Scope scope;Prepare(scope,a,b,e,caller);
 // No metadata lock or file operation surrounds the native call. Scope also
 // restores on a C++ exception; native exceptions retain the normal game path.
 return native(a,b,c,d,e);
}
void Poll(uint64_t sequence){if(!active.load())return;auto now=Now();if(now-lastStatus>=5000){fprintf(report,"StationSubmission90 on=%d seen=%llu nativeVisible=%llu candidates=%llu headOutside=%llu applied=%llu restored=%llu conflicts=%llu stale=%llu refused=%llu off=%llu sequence=%llu caller=%llx expected=%llx\n",configured&&DenseVisibility87::CorrectionEnabled(),seen.load(),nativeVisible.load(),candidates.load(),outside.load(),applied.load(),restored.load(),conflicts.load(),stale.load(),refused.load(),off.load(),sequence,firstCaller.load(),uint64_t(image)+NativeCaller);fflush(report);lastStatus=now;}}
void Save(uint64_t sequence,uint64_t tracking,uint32_t frame){if(!active.load())return;std::array<Record,RecordLimit> snapshot{};uint64_t end{};
 AcquireSRWLockShared(&recordLock);snapshot=records;end=completed;ReleaseSRWLockShared(&recordLock);
 std::error_code error;std::filesystem::create_directories(directory,error);auto path=directory/(L"capture-"+std::to_wstring(++capture)+L".jsonl");auto f=_wfsopen(path.c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"event\":\"capture\",\"sequence\":%llu,\"tracking\":%llu,\"frame\":%u,\"completed\":%llu,\"applied\":%llu,\"restored\":%llu,\"conflicts\":%llu}\n",sequence,tracking,frame,end,applied.load(),restored.load(),conflicts.load());
 auto count=unsigned((std::min)(end,uint64_t(RecordLimit)));for(unsigned n=0;n<count;++n){auto& r=snapshot[(end-count+n)%RecordLimit];fprintf(f,"{\"event\":\"submission\",\"id\":%llu,\"sequence\":%llu,\"tracking\":%llu,\"frame\":%u,\"headAgeMs\":%llu,\"slot\":%u,\"state\":\"%llx\",\"resource\":\"%llx\",\"status\":%u,\"before\":%u,\"after\":%u,\"restored\":%s,\"position\":[%.9g,%.9g,%.9g,%.9g]}\n",r.id,r.sequence,r.tracking,r.frame,r.age,r.slot,r.state,r.resource,r.status,r.before,r.after,r.restored?"true":"false",r.position[0],r.position[1],r.position[2],r.position[3]);}fclose(f);
}
void Stop(){active=false;AcquireSRWLockExclusive(&headLock);head={};ReleaseSRWLockExclusive(&headLock);}
}
