#include "NpcCone63.h"
#include "BulkDraw83.h"
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "NpcDecision63.h"
#include "NativeHandleState63.h"
// Diagnostic only: bounded context walk and reads of the already observed native
// crowd activation frames. No native call, handler dispatch, refcount or write.
namespace NpcInputs63 {
struct Snapshot {
 uint32_t status{},depth{},parentDepth{},slot{},poolCount{};
 uint64_t caller{},manager{},state{},descriptor{},reference{},statePool{},frameInput{},positionInput{};
 bool recovered{},parentRecovered{},valid{},positionRead{};
 uint8_t managerRaw[0x3b0]{},poolRaw[0xa8]{},stateRaw[0x360]{},descriptorRaw[0x1b0]{},referenceRaw[0xf0]{},positionRaw[16]{};
};
inline bool Read(void* out,uint64_t p,size_t n) noexcept {
 if(!out || p<0x10000 || p>0x00007ffffffeffffULL || !n || n>0x10000 || p+n<p)return false;
 __try {memcpy(out,reinterpret_cast<const void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T> T Get(const uint8_t* p,size_t offset) noexcept {T v{};memcpy(&v,p+offset,sizeof(v));return v;}
struct Registers {uint64_t rbp{},rsi{},r12{},r13{},r14{},r15{},parentRsp{},parentRbx{},parentRbp{},parentRdi{},parentRsi{},parentR14{};};
__declspec(noinline) inline void Recover(Snapshot& out,Registers& r,uint64_t image) noexcept {
 // No C++ objects with destructors in this leaf. SEH protects only metadata
 // inspection; the observer forwards the original native call outside it.
 __try {
  ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);CONTEXT c{};RtlCaptureContext(&c);
  out.status=1;
  for(unsigned depth=0;depth<24;++depth){
   if(c.Rsp<low || c.Rsp>high-8 || (c.Rsp&7) || !c.Rip){out.status=2;return;}
   if(c.Rip==image+0xfeca24 || c.Rip==image+0xfec9c8 || c.Rip==image+0xfecace){
    if(out.recovered){out.status=3;return;}
    DWORD64 owner{};auto entry=RtlLookupFunctionEntry(c.Rip,&owner,nullptr);if(!entry || owner!=image || entry->BeginAddress!=0xfec740 || entry->EndAddress!=0xfeca48 || entry->UnwindData!=0x3818de0){out.status=4;return;}
    out.recovered=true;out.depth=depth;out.caller=c.Rip;out.manager=c.Rbp;out.state=c.Rsi;out.frameInput=c.R13;out.slot=uint32_t(c.R15);
    r.rbp=c.Rbp;r.rsi=c.Rsi;r.r12=c.R12;r.r13=c.R13;r.r14=c.R14;r.r15=c.R15;
   }
   if(out.recovered && c.Rip==image+0xfed9e7){
    DWORD64 owner{};auto entry=RtlLookupFunctionEntry(c.Rip,&owner,nullptr);if(!entry || owner!=image || entry->BeginAddress!=0xfed7c0 || entry->EndAddress!=0xfeda04 || entry->UnwindData!=0x3817a3c){out.status=9;return;}
    out.parentRecovered=true;out.parentDepth=depth;r.parentRsp=c.Rsp;r.parentRbx=c.Rbx;r.parentRbp=c.Rbp;r.parentRdi=c.Rdi;r.parentRsi=c.Rsi;r.parentR14=c.R14;out.status=0;return;
   }
   DWORD64 module{};auto function=RtlLookupFunctionEntry(c.Rip,&module,nullptr);uint64_t previous=c.Rsp,previousRip=c.Rip;
   if(function){
    if(out.recovered && c.Rip==out.caller && (module!=image || function->BeginAddress!=0xfec740 || function->EndAddress!=0xfeca48 || function->UnwindData!=0x3818de0)){out.status=4;return;}
    PVOID handler{};DWORD64 frame{};RtlVirtualUnwind(0,module,c.Rip,function,&c,&handler,&frame,nullptr);
   }else{if(!Read(&c.Rip,c.Rsp,8)){out.status=5;return;}c.Rsp+=8;}
   if(c.Rsp<=previous || c.Rsp>high || c.Rip==previousRip){out.status=6;return;}
  }
  out.status=7;
 }__except(EXCEPTION_EXECUTE_HANDLER){out.status=8;}
}
inline bool Pointer(uint64_t p) noexcept {return p>=0x10000 && p<=0x00007ffffffeffffULL;}
__declspec(noinline) inline void Capture(Snapshot& out,uint64_t image,uint64_t world,uint64_t entity,uint64_t handle,uint32_t token) noexcept {
 Registers r{};Recover(out,r,image);if(!out.recovered || !out.parentRecovered || out.status)return;
 uint8_t w[0xa00]{},bulk[0x508]{},collection[0xc8]{},pool[0x78]{},h[16]{};
 if(!Read(w,world,sizeof(w)) || Get<uint64_t>(w,0)!=image+0x30225e0){out.status=10;return;}
 uint64_t b=Get<uint64_t>(w,0x998);
 if(!Read(bulk,b,sizeof(bulk)) || BulkDraw83::CanonicalBulk(b,Get<uint64_t>(bulk,0))!=image+0x304ab90){out.status=11;return;}
 uint64_t k=Get<uint64_t>(bulk,0x500);
 if(!Read(collection,k,sizeof(collection)) || NpcCone63::CanonicalCollection(k,Get<uint64_t>(collection,0))!=image+0x2f6b880 || Get<uint64_t>(collection,0xc0)!=out.manager ||
 !Read(out.managerRaw,out.manager,sizeof(out.managerRaw)) || NpcDecision63::CanonicalManager(out.manager,Get<uint64_t>(out.managerRaw,0))!=image+0x2f6bcb0 || Get<uint64_t>(out.managerRaw,0x278)!=world){out.status=12;return;}
 uint64_t p=Get<uint64_t>(out.managerRaw,0x40);out.statePool=p;
 if(!Read(out.poolRaw,p,sizeof(out.poolRaw)) || !Read(pool,p,sizeof(pool)) || memcmp(pool,out.poolRaw,sizeof(pool)) || Get<uint64_t>(pool,0)!=image+0x304c640 || Get<uint64_t>(pool,0x50)!=p || Get<uint64_t>(pool,0x58)!=out.manager || Get<uint64_t>(pool,0x70)!=world){out.status=13;return;}
 out.poolCount=Get<uint32_t>(pool,0x68);uint32_t capacity=Get<uint32_t>(pool,0x6c)&0x1fffffff;
 uint64_t states=Get<uint64_t>(pool,0x60),descriptors=Get<uint64_t>(out.managerRaw,0x258),references=Get<uint64_t>(out.managerRaw,0x384);
 if(!out.poolCount || out.poolCount>4096 || capacity<out.poolCount || capacity>4096 || out.slot>=out.poolCount || r.r15!=out.slot || !Pointer(states) || !Pointer(descriptors) || !Pointer(references)) {out.status=14;return;}
 out.descriptor=descriptors+uint64_t(out.slot)*0x1b0;out.reference=references+uint64_t(out.slot)*0xf0;
 if(out.state!=states+uint64_t(out.slot)*0x360 || r.r12!=uint64_t(out.slot)*0x1b0 || r.r14!=out.reference || r.parentRbx!=out.manager || r.parentRbp!=out.state || r.parentRdi!=out.descriptor || r.parentRsi!=out.slot || r.parentR14!=out.frameInput){out.status=15;return;}
 if(!Read(out.stateRaw,out.state,sizeof(out.stateRaw)) || !Read(out.descriptorRaw,out.descriptor,sizeof(out.descriptorRaw)) || !Read(out.referenceRaw,out.reference,sizeof(out.referenceRaw)) || Get<uint64_t>(out.stateRaw,0x10)!=handle || !Read(h,handle,16) || Get<uint64_t>(h,0)!=entity || !NativeHandleState63::SameLifetime(Get<uint32_t>(h,12),token) || int32_t(token)>=0){out.status=16;return;}
 // fed7c0 forwards its sixth stack argument into fe6f30/fe6da0; both
 // demonstrably load exactly the first vec4 when testing actor distance.
 ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);
 if(r.parentRsp<low || r.parentRsp>high-0x2a0 || !Read(&out.positionInput,r.parentRsp+0x298,8)){out.status=17;return;}
 out.positionRead=Read(out.positionRaw,out.positionInput,16);if(!out.positionRead){out.status=18;return;}
 uint8_t check[0x78]{};uint64_t v{},backlink{},actorHandle{},samePool{},sameDescriptors{},sameReferences{},sameWorld{},sameBulk{},sameCollection{};
 if(!Read(&samePool,out.manager+0x40,8) || samePool!=p || !Read(&sameDescriptors,out.manager+0x258,8) || sameDescriptors!=descriptors || !Read(&sameReferences,out.manager+0x384,8) || sameReferences!=references || !Read(&sameWorld,out.manager+0x278,8) || sameWorld!=world || !Read(&sameBulk,world+0x998,8) || sameBulk!=b || !Read(&sameCollection,b+0x500,8) || sameCollection!=k || !Read(check,p,sizeof(check)) || memcmp(pool+0x50,check+0x50,0x28) || !Read(&v,out.manager,8) || NpcDecision63::CanonicalManager(out.manager,v)!=image+0x2f6bcb0 || !Read(&backlink,k+0xc0,8) || backlink!=out.manager || !Read(&actorHandle,out.state+0x10,8) || actorHandle!=handle || !Read(h,handle,16) || Get<uint64_t>(h,0)!=entity || !NativeHandleState63::SameLifetime(Get<uint32_t>(h,12),token)){out.status=19;return;}
 out.valid=true;
}
}
