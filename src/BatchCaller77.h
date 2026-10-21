#pragma once
#include <cstdint>
namespace BatchCaller77 {
// The exact guarded22f7880 frame saves the incoming RDI at returnSlot+0x28.
// Its caller22a5420 holds the native job in RDI until the list has executed.
// No recyclable command-list cache and no mutation of caller or native job state.
template<class Read> bool Recover(uintptr_t slot,uintptr_t low,uintptr_t high,uintptr_t innerReturn,uintptr_t outerReturn,uintptr_t immediate,uintptr_t list,uintptr_t& result,Read read){
 result=0;if(!slot || (slot&7) || slot<low || high<low || slot>high || high-slot<0x38 || !immediate || !list)return false;
 uintptr_t inner{},outer{},job{},wrapper{},actualList{},context{};
 if(!read(slot,inner) || inner!=innerReturn || !read(slot+0x30,outer) || outer!=outerReturn || !read(slot+0x28,job) || !job || (job&7) || job>UINTPTR_MAX-0x30)return false;
 if(!read(job+0x28,actualList) || actualList!=list || !read(job+0x20,wrapper) || !wrapper || (wrapper&7) || wrapper>UINTPTR_MAX-0x38 || !read(wrapper+0x30,context) || context!=immediate)return false;
 uintptr_t againInner{},againOuter{},againJob{},againWrapper{},againList{},againContext{};
 if(!read(slot,againInner) || againInner!=inner || !read(slot+0x30,againOuter) || againOuter!=outer || !read(slot+0x28,againJob) || againJob!=job || !read(job+0x28,againList) || againList!=list || !read(job+0x20,againWrapper) || againWrapper!=wrapper || !read(wrapper+0x30,againContext) || againContext!=immediate)return false;
 result=job;return true;
}
}
