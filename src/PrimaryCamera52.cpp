// Early native renderer-primary camera input comparison. No gameplay camera entity writes.
#include "PrimaryCamera52.h"
#include "GpuCapture.h"
#include "PrimarySourceRanges52.h"
#include <atomic>
namespace PrimaryCamera52 {
namespace {
uint8_t* image{},*source{},*surface{};FILE* report{};PrimarySourceRanges52::Ownership owned;
std::atomic<bool> enabled{};bool correction=true,f2Down{};uint64_t lastStatus{},applied{},refused{};
using Frustum=void(__fastcall*)(void*,void*);Frustum build{};
#ifdef ACUNITYVR_PRIMARY52_CHECK
bool checkF2{};
#endif
bool F2Down(){
#ifdef ACUNITYVR_PRIMARY52_CHECK
 return checkF2;
#else
 return (GetAsyncKeyState(VK_F2)&0x8000)!=0;
#endif
}
bool SafeRestore() noexcept {__try{return PrimarySourceRanges52::Restore(owned);}__except(EXCEPTION_EXECUTE_HANDLER){owned.active=false;return false;}}
bool Identity() noexcept {__try{
 return owned.view==source && !acu::Field<void*>(source,0x248) && acu::Field<void*>(source,0x420)==surface && acu::Field<void*>(source,0x430)==owned.state && acu::Field<void*>(source,0x428)==owned.frustum &&
 acu::Field<void*>(owned.state,0x8208)==source && acu::Field<void*>(owned.state,0x890)==source &&
 !memcmp(owned.state,source+0x2e0,64) && !memcmp(owned.state+0x40,source,64) && !memcmp(owned.state+0x80,source+0x320,64) && !memcmp(owned.state+0xc0,owned.frustum,0xc0);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
}
bool Initialize(uint8_t* base,uint8_t* s,uint8_t* screen,FILE* log){image=base;source=s;surface=screen;report=log;owned.view=source;owned.state=acu::Field<uint8_t*>(source,0x430);owned.frustum=acu::Field<uint8_t*>(source,0x428);build=reinterpret_cast<Frustum>(image+0x223bd70);enabled=true;return owned.state && owned.frustum;}
void BeginFrame(uint32_t frame,uint64_t sequence,uint64_t tracking,bool tracked,const CameraMath::Matrix poses[2],const Transport::Eye optics[2]){
 if(!enabled)return;bool down=F2Down();if(down && !f2Down){correction=!correction;fprintf(report,"Primary camera52 F2 correction=%d sequence=%llu\n",correction,sequence);fflush(report);}f2Down=down;
 // Native Bind has refreshed the game-camera ranges at this exact boundary.
 owned.active=false;bool changed=false;BinocularComponentInput50::Result result;
 if(correction && tracked){
  if(Identity() && acu::Field<uint32_t>(source,0x294)==frame && BinocularComponentInput50::Build(CameraMath::Load(source+0x320),poses,optics,result)){
   alignas(16) uint8_t frustum[0xc0]{};build(frustum,&result.viewProjection);changed=PrimarySourceRanges52::Apply(owned,result,frustum);if(changed)++applied;else ++refused;
  }else ++refused;
 }
 auto now=GetTickCount64();if(now-lastStatus>=5000){fprintf(report,"Primary camera52 frame=%u sequence=%llu tracking=%llu mode=%d correction=%d appliedNow=%d applied=%llu refused=%llu; source renderer head-centre/binocular input before nativeQT, Bind/QT/sort once; game camera entity unchanged\n",frame,sequence,tracking,tracked,correction,changed,applied,refused);fflush(report);lastStatus=now;}
}
void Stop(){if(!enabled.exchange(false))return;bool active=owned.active;bool restored=SafeRestore();if(report){fprintf(report,"Primary camera52 retired owned=%d restored=%d; nine source ranges restored only if still owned, next nativeBind regenerates game camera; no override pointer used\n",active,restored);fflush(report);}}
}
