// Change the actual gameplay camera output after its native update, before its
// visibility consumers run. No shared getter detours or native code patches.
#include "EarlyCamera91.h"
#include "CrowdCameraMath85.h"
#include "NativeHandleState63.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstring>
namespace EarlyCamera91 {namespace {
#include "CrowdCamera85Signatures.h"
#include "EarlyCamera91Signatures.h"
using Update=uint64_t(__fastcall*)(void*,float);
using Getter=void*(__fastcall*)(void*,void*,unsigned);
constexpr size_t TableRva=0x4eec760,TableBytes=0x5e0,UpdateOffset=0x410;
uint8_t* image{};uint64_t source{},collection{},component{},entity{},world{},handle{},director{};uint32_t token{};
alignas(16) std::array<uint8_t,TableBytes> table{};
Update native{};Provider provider{};FILE* report{};
std::atomic<bool> enabled{};std::atomic_flag updating=ATOMIC_FLAG_INIT;
std::atomic<bool> exposed{};
std::atomic<uint64_t> lastVisibilityTicket{UINT64_MAX};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock{Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
alignas(16) float original[2][4]{},written[2][4]{};
CameraMath::Matrix mouse{};bool owned{};uint64_t epoch{},tracking{},writeTick{},lastStatus{};
std::atomic<uint64_t> updates{},applied{},restored{},conflicts{},refused{},getterPass{},getterFail{},sceneWithHead{},visibilityWithHead{},visibilityWithoutHead{};
std::atomic<uint64_t> visibilityCacheHead{},visibilityCacheOther{};
std::atomic<uint64_t> sceneCacheHead{},sceneCacheOther{};
std::atomic<bool> configured{};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(!out||p<0x10000||p>0x00007ffffffeffffULL||!n||n>0x10000)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>bool At(uint64_t p,size_t off,T& out){return Read(&out,p+off,sizeof(out));}
bool Q(uint64_t p,size_t off,uint64_t expected){uint64_t v{};return At(p,off,v)&&v==expected;}
bool Identity(bool cloned){uint64_t list{},current{};uint16_t count{};uint32_t state{};
 return Q(uint64_t(image)+0x521aad0,0,collection)&&Q(collection,0,uint64_t(image)+0x304d800)&&At(collection,0x40,list)&&At(collection,0x4a,count)&&count==1&&Q(list,0,component)&&
 Q(component,0,uint64_t(image)+0x3050b00)&&Q(component,0x10,entity)&&Q(component,0x120,cloned?uint64_t(table.data()):uint64_t(image)+TableRva)&&
 Q(entity,0,uint64_t(image)+0x3017d60)&&Q(entity,0x60,world)&&Q(entity,0x130,handle)&&Q(handle,0,entity)&&At(handle,12,state)&&NativeHandleState63::SameLifetime(state,token)&&
 Q(world,0,uint64_t(image)+0x30225e0)&&Q(world,0xd8,entity)&&At(source,0x250,current)&&Q(current,0,world);
}
bool Gameplay(){uint32_t mode{};uint8_t blend{};return Q(uint64_t(image)+0x521a768,0,director)&&Q(director,0,uint64_t(image)+0x3040d10)&&At(director,0x64,mode)&&mode==1&&At(director,0x58,blend)&&!blend;}
bool Store(const float value[2][4]) noexcept {__try{memcpy(reinterpret_cast<void*>(component+0x50),value,32);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool StorePart(unsigned i,const float value[4]) noexcept {__try{memcpy(reinterpret_cast<void*>(component+0x50+i*16),value,16);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Restore(){if(!owned)return false;owned=false;exposed=false;alignas(16) float current[2][4]{};
 bool full=Identity(true)&&Read(current,component+0x50,32),any=false;
 if(full)for(unsigned i=0;i<2;++i){bool same=!memcmp(current[i],written[i],16)&&StorePart(i,original[i]);any|=same;full&=same;}
 if(any)++restored;if(full)return true;
 ++conflicts;configured=false;return false; // Preserve native replacements; stop on unexpected ordering.
}
bool NativeReaders(){
 alignas(16) float values[3][4]{};const size_t getters[]={0x1e91b70,0x1e91980,0x1e91f10};
 for(unsigned i=0;i<3;++i)if(reinterpret_cast<Getter>(image+getters[i])(reinterpret_cast<void*>(director),values[i],0)!=values[i])return false;
 auto q=Transport::Quaternion{written[1][0],written[1][1],written[1][2],written[1][3]};auto forward=CameraMath::Rotate(q,{0,1,0});const float expected[3]={forward.x,forward.y,forward.z};
 for(unsigned i=0;i<3;++i){if(std::abs(values[0][i]-written[0][i])>.002f||std::abs(values[2][i]-expected[i])>.002f)return false;}
 float dot{};for(unsigned i=0;i<4;++i)dot+=values[1][i]*written[1][i];return std::abs(std::abs(dot)-1)<.002f;
}
bool StationCacheMatches(){
 float position[4]{},direction[4]{};bool match=Read(position,uint64_t(image)+0x51c6fd0,16)&&Read(direction,uint64_t(image)+0x51c6ff0,16);
 auto forward=CameraMath::Rotate({written[1][0],written[1][1],written[1][2],written[1][3]},{0,1,0});float expected[]={forward.x,forward.y,forward.z};
 for(unsigned i=0;i<3;++i)match=match&&std::abs(position[i]-written[0][i])<.01f&&std::abs(direction[i]-expected[i])<.01f;
 return match;
}
void AfterUpdate(void* object,uint64_t start){
 ++updates;if(!enabled.load(std::memory_order_acquire)||!configured||uint64_t(object)!=component)return;
 alignas(16) float pose[2][4]{};
 if(!Identity(true)||!Gameplay()||!Read(pose,component+0x50,32)){++refused;return;}
 auto q=Transport::Quaternion{pose[1][0],pose[1][1],pose[1][2],pose[1][3]};float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
 if(!std::isfinite(norm)||std::abs(norm-1)>.01f){++refused;return;}
 auto base=CameraMath::Rotation(q);for(unsigned i=0;i<3;++i)base.m[12+i]=pose[0][i];CameraMath::Matrix head{};uint64_t id{};
 if(!CameraMath::Rigid(base)||!provider(base,head,id)){++refused;return;}
 float values[3][4]{};if(!id||!CrowdCameraMath85::Build(head,values)){++refused;return;}
 Lock lock;alignas(16) float current[2][4]{};
 if(!enabled.load()||epoch!=start||!Identity(true)||!Gameplay()||!Read(current,component+0x50,32)||memcmp(current,pose,32)){++refused;return;}
 memcpy(original,pose,32);memcpy(written,values,32);mouse=base;tracking=id;writeTick=GetTickCount64();
 if(!Store(written)){++refused;return;}owned=true;
 // Execute the unmodified, full native director -> world -> component getters.
 // A failure immediately restores the original output and disables further writes.
 if(!NativeReaders()){++getterFail;Restore();configured=false;return;}
 ++getterPass;++applied;exposed=true;
}
uint64_t __fastcall Updated(void* object,float dt){
 if(updating.test_and_set(std::memory_order_acquire))return native(object,dt);
 struct Flight{~Flight(){updating.clear(std::memory_order_release);}} flight;
 uint64_t start{};{Lock lock;Restore();start=epoch;}
 auto result=native(object,dt); // Preserve the native float argument and full RAX.
 AfterUpdate(object,start);return result;
}
bool Exchange(bool attach) noexcept {__try{auto originalTable=image+TableRva;auto own=table.data();return InterlockedCompareExchangePointer(reinterpret_cast<void**>(component+0x120),attach?own:originalTable,attach?originalTable:own)==(attach?originalTable:own);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
}
bool Initialize(uint8_t* base,void* view,Provider prepare,const std::filesystem::path& output,FILE* log){
 image=base;source=uint64_t(view);provider=prepare;report=log;if(!image||!source||!provider||!report||!Guard85(image)||!Guard91(image))return false;
 configured=GetPrivateProfileIntW(L"Culling",L"GameplayHeadCamera",1,(output/L"ACUnityVR.ini").c_str())!=0;
 uint64_t list{},graph{},renderer{};
 if(!At(uint64_t(image),0x521aad0,collection)||!At(collection,0x40,list)||!At(list,0,component)||!At(component,0x10,entity)||!At(entity,0x60,world)||!At(entity,0x130,handle)||!At(handle,12,token)||!At(uint64_t(image),0x521a768,director)||!At(source,0x440,graph)||!At(graph,0x10,renderer)||!Q(renderer,0x28,world)||!Identity(false))return false;
 // The complete 47-entry callback descriptor is copied, including all native
 // hashes, ordinals, and reserved words. Only the real update function differs.
 uint64_t fn{};if(!At(uint64_t(image)+TableRva,UpdateOffset,fn)||fn!=uint64_t(image)+0x1f3f190||memcmp(image+0x1f3f190,"\x40\x55\x53\x41\x56\x48\x8d\xac\x24\xf0\xfe\xff\xff",13))return false;
 memcpy(table.data(),image+TableRva,TableBytes);native=reinterpret_cast<Update>(fn);auto replacement=reinterpret_cast<uint64_t>(Updated);memcpy(table.data()+UpdateOffset,&replacement,8);
 enabled=true;if(!Exchange(true)){enabled=false;return false;}
 fprintf(report,"EarlyCamera91 attached actual gameplay camera=%llx owner=%llx full47 descriptor; post native1f3f190 camera position50/quaternion60 before visibility. Ini GameplayHeadCamera=%d; unmodified native director getters verify each write; mouse pose restored before source Bind, no getter/code detours.\n",component,entity,int(configured.load()));fflush(report);return true;
}
bool BeforeScene(CameraMath::Matrix& originalBase){Lock lock;++epoch;bool fresh=owned&&Identity(true)&&Gameplay()&&GetTickCount64()-writeTick<=250;
 if(fresh){originalBase=mouse;if(StationCacheMatches())++sceneCacheHead;else ++sceneCacheOther;}
 bool restoredNow=Restore();if(fresh&&restoredNow){++sceneWithHead;return true;}return false;
}
void VisibilityBoundary(){if(!enabled.load())return;
 // This callback is called for many pool entries. Sample at most once per
 // camera update in each exposed/restored state, not once per crowd member.
 auto ticket=(applied.load(std::memory_order_relaxed)<<1)|uint64_t(exposed.load(std::memory_order_relaxed));auto previous=lastVisibilityTicket.load(std::memory_order_relaxed);
 if(previous==ticket||!lastVisibilityTicket.compare_exchange_strong(previous,ticket,std::memory_order_relaxed))return;
 Lock lock;if(owned&&Identity(true)&&Gameplay()){
 ++visibilityWithHead;if(StationCacheMatches())++visibilityCacheHead;else ++visibilityCacheOther;
 }else ++visibilityWithoutHead;}
void Poll(uint64_t sequence){if(!enabled.load())return;auto now=GetTickCount64();if(now-lastStatus<5000)return;Lock lock;
 fprintf(report,"EarlyCamera91 sequence=%llu configured=%d updates=%llu applied=%llu nativeGetterPass=%llu nativeGetterFail=%llu restored=%llu conflicts=%llu refused=%llu sceneAfterHead=%llu visibilityDuringHead=%llu visibilityNative=%llu tracking=%llu\n",sequence,int(configured.load()),updates.load(),applied.load(),getterPass.load(),getterFail.load(),restored.load(),conflicts.load(),refused.load(),sceneWithHead.load(),visibilityWithHead.load(),visibilityWithoutHead.load(),tracking);fflush(report);lastStatus=now;
 fprintf(report,"EarlyCamera91 native station cache matchedHead=%llu other=%llu beforeSceneHead=%llu beforeSceneOther=%llu; read-only ordering evidence, not proof of target crowd coverage\n",visibilityCacheHead.load(),visibilityCacheOther.load(),sceneCacheHead.load(),sceneCacheOther.load());fflush(report);
}
void Stop(){if(!enabled.exchange(false))return;Lock lock;++epoch;Restore();bool tableRestored=Exchange(false);fprintf(report,"EarlyCamera91 stopped descriptorRestored=%d applied=%llu nativeGetterPass=%llu conflicts=%llu; module retained\n",tableRestored,applied.load(),getterPass.load(),conflicts.load());fflush(report);}
}
