#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
namespace ShaderRegistry92 {
constexpr uint32_t Version=1,Limit=4096;
constexpr wchar_t Module[]=L"ACUnityShaderRegistry.dll";
struct Entry {uint64_t device{},shader{};uint32_t stage{},bytes{};char hash[65]{};}; // stage0 VS,1 PS
struct State {uint32_t version{},pid{},accepting{},stopped{};uint64_t count{},failures{},active{};};
using Snapshot=uint32_t(WINAPI*)(Entry*,uint32_t,State*);
inline bool Read(std::vector<Entry>& entries){
#ifdef ACUNITY_REGISTRY92_FIXTURE
 auto module=GetModuleHandleW(nullptr);
#else
 auto module=GetModuleHandleW(Module);
#endif
 auto get=module?reinterpret_cast<Snapshot>(GetProcAddress(module,"ACUnityShaderRegistrySnapshot")):nullptr;
 if(!get)return false;State state{};auto count=get(nullptr,0,&state);
 if(state.version!=Version||state.pid!=GetCurrentProcessId()||state.accepting||!state.stopped||state.failures||state.active||!count||count>Limit||state.count!=count)return false;
 entries.resize(count);State after{};auto actual=get(entries.data(),count,&after);
 if(actual!=count||after.version!=Version||after.pid!=state.pid||after.accepting||!after.stopped||after.failures||after.active||after.count!=count)return false;
 for(auto& e:entries)if(!e.device||!e.shader||e.stage>1||e.bytes<32||e.bytes>1024*1024||e.hash[64])return false;
 return true;
}
}
