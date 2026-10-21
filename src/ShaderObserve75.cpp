#include "ShaderObserve75.h"
#include "BuildGuard.h"
#ifdef ACUNITY_SHADER_REGISTRY92
#include "ShaderRegistry92.h"
#endif
#include <MinHook.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cstdio>
#include <utility>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace ShaderObserve75 {
namespace {
constexpr size_t MaxTargets=16, MaxPrograms=4096, MaxBytes=32*1024*1024, MaxProgramBytes=1024*1024;
using CreateShader=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,IUnknown**);
using CreateDevice=decltype(&D3D11CreateDevice);
using CreateSwap=decltype(&D3D11CreateDeviceAndSwapChain);
struct Target { void* entry{}; CreateShader original{}; };
struct Program { uintptr_t device{}, shader{}; unsigned stage{},bytes{}; ComPtr<IUnknown> pin; std::vector<uint8_t> code; std::string hash; };
std::array<Target,MaxTargets> targets{};
size_t targetCount{};
std::vector<Program> programs;
size_t byteCount{}, vertexCount{}, pixelCount{};
std::mutex installMutex, storeMutex, fileMutex;
std::atomic<bool> accepting{}, stopping{};
std::atomic<bool> stopped92{};
std::atomic<uint64_t> factoryCalls{}, shaderCalls{}, rejected{}, budgetRefused{}, failures{}, active{};
CreateDevice originalDevice{};
CreateSwap originalSwap{};
void *deviceTarget{}, *swapTarget{};
std::filesystem::path outputDirectory;
bool deviceHook{},swapHook{};
struct ActiveCall { ActiveCall(){active.fetch_add(1,std::memory_order_acq_rel);} ~ActiveCall(){active.fetch_sub(1,std::memory_order_acq_rel);} };
uint32_t U32(const uint8_t* data){uint32_t v{};memcpy(&v,data,4);return v;}
std::string Hash(const std::vector<uint8_t>& code){
 BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; DWORD objectSize{},count{}; std::array<UCHAR,32> digest{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof(objectSize),&count,0)>=0;
 std::vector<UCHAR> object(objectSize);
 if(ok)ok=BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(code.data()),static_cast<ULONG>(code.size()),0)>=0 && BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 std::string hex;if(ok)for(auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return hex;
}
void Record(ID3D11Device* device,const void* data,SIZE_T size,IUnknown* shader)noexcept{
 if(!accepting.load(std::memory_order_acquire) || !shader)return;
 try{
  unsigned stage{};if(!ValidDXBC(data,size,stage)){++rejected;return;}
  std::lock_guard guard(storeMutex);if(!accepting.load())return;
  const auto identity=reinterpret_cast<uintptr_t>(shader);
  for(auto& p:programs)if(p.shader==identity){if(p.device!=reinterpret_cast<uintptr_t>(device) || p.stage!=stage)++failures;return;}
  if(programs.size()>=MaxPrograms || size>MaxBytes-byteCount){++budgetRefused;return;}
  Program p;p.device=reinterpret_cast<uintptr_t>(device);p.shader=identity;p.stage=stage;p.pin=shader;p.code.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);p.hash=Hash(p.code);
  if(p.hash.size()!=64){++failures;return;}
  p.bytes=unsigned(size);
#ifdef ACUNITY_SHADER_REGISTRY92
  // Runtime needs hashes and pinned identities, not retained game bytecode.
  std::vector<uint8_t>().swap(p.code);
#endif
  programs.emplace_back(std::move(p));byteCount+=size;if(stage==1)++vertexCount;else ++pixelCount;
 }catch(...){++failures;}
}
template<size_t N>HRESULT STDMETHODCALLTYPE ShaderCreated(ID3D11Device* device,const void* data,SIZE_T size,ID3D11ClassLinkage* linkage,IUnknown** out){
 ActiveCall call;++shaderCalls;const auto fn=targets[N].original;
 const HRESULT hr=fn(device,data,size,linkage,out);
 if(SUCCEEDED(hr) && out && *out)Record(device,data,size,*out);return hr;
}
template<size_t... I>constexpr auto Callbacks(std::index_sequence<I...>){return std::array<CreateShader,sizeof...(I)>{&ShaderCreated<I>...};}
constexpr auto callbacks=Callbacks(std::make_index_sequence<MaxTargets>{});
bool OwnedFunction(void* function){
 MEMORY_BASIC_INFORMATION m{};if(!function || !VirtualQuery(function,&m,sizeof(m)) || m.State!=MEM_COMMIT || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 const auto protection=m.Protect&0xff;return protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
}
void InstallDevice(ID3D11Device* device)noexcept{
 if(!device || stopping.load())return;
 try{
  std::lock_guard guard(installMutex);if(stopping.load())return;
  auto table=*reinterpret_cast<void***>(device);
  for(unsigned slot:{12u,15u}){
   void* entry=table[slot];bool existing=false;for(size_t i=0;i<targetCount;++i)if(targets[i].entry==entry)existing=true;if(existing)continue;
   if(targetCount==MaxTargets || !OwnedFunction(entry)){++failures;continue;}
   const size_t index=targetCount;auto& t=targets[index];t.entry=entry;
   if(MH_CreateHook(entry,reinterpret_cast<void*>(callbacks[index]),reinterpret_cast<void**>(&t.original))!=MH_OK){t.entry=nullptr;++failures;continue;}
   // Stable storage and original trampoline are published before the hook is enabled.
   ++targetCount;
   if(MH_EnableHook(entry)!=MH_OK){++failures;}
  }
 }catch(...){++failures;}
}
HRESULT WINAPI DeviceCreated(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context){
 ActiveCall call;++factoryCalls;const HRESULT hr=originalDevice(adapter,driver,software,flags,levels,count,sdk,device,level,context);
 if(SUCCEEDED(hr)){
  if(device && *device)InstallDevice(*device);
  else if(context && *context){ComPtr<ID3D11Device> d;(*context)->GetDevice(&d);InstallDevice(d.Get());}
 }return hr;
}
HRESULT WINAPI SwapCreated(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context){
 ActiveCall call;++factoryCalls;const HRESULT hr=originalSwap(adapter,driver,software,flags,levels,count,sdk,desc,swap,device,level,context);
 if(SUCCEEDED(hr)){
  if(device && *device)InstallDevice(*device);
  else if(context && *context){ComPtr<ID3D11Device> d;(*context)->GetDevice(&d);InstallDevice(d.Get());}
  else if(swap && *swap){ComPtr<ID3D11Device> d;if(SUCCEEDED((*swap)->GetDevice(IID_PPV_ARGS(&d))))InstallDevice(d.Get());}
 }return hr;
}
bool SavePrograms(){
 std::lock_guard guard(storeMutex);
 FILE* list{};if(_wfopen_s(&list,(outputDirectory/L"shader-list.jsonl").c_str(),L"wb") || !list)return false;
 bool ok=true;
 for(auto& p:programs){
  const std::string filename=(p.stage==1?"vs-":"ps-")+p.hash+".dxbc";auto file=outputDirectory/filename;FILE* f{};
  if(_wfopen_s(&f,file.c_str(),L"wb") || !f){ok=false;break;}
  const bool written=fwrite(p.code.data(),1,p.code.size(),f)==p.code.size();const int close=fclose(f);if(!written || close){ok=false;break;}
  if(fprintf(list,"{\"device\":\"0x%llx\",\"shader\":\"0x%llx\",\"stage\":\"%s\",\"bytes\":%zu,\"sha256\":\"%s\",\"file\":\"%s\"}\n",static_cast<unsigned long long>(p.device),static_cast<unsigned long long>(p.shader),p.stage==1?"VS":"PS",p.code.size(),p.hash.c_str(),filename.c_str())<0){ok=false;break;}
 }
 if(fclose(list))ok=false;return ok;
}
}
bool ValidDXBC(const void* raw,size_t size,unsigned& stage)noexcept{
 stage=~0u;if(!raw || size<36 || size>MaxProgramBytes)return false;
 auto data=static_cast<const uint8_t*>(raw);if(memcmp(data,"DXBC",4) || U32(data+24)!=size)return false;
 const uint32_t chunks=U32(data+28);if(!chunks || chunks>(size-32)/4)return false;
 bool found=false;for(uint32_t i=0;i<chunks;++i){const size_t offset=U32(data+32+i*4);if(offset<32+size_t(chunks)*4 || offset>size-8)return false;const size_t length=U32(data+offset+4);if(length>size-offset-8)return false;
  if(!memcmp(data+offset,"SHDR",4) || !memcmp(data+offset,"SHEX",4)){
   if(found || length<8 || length%4 || U32(data+offset+12)!=length/4)return false;
   stage=(U32(data+offset+8)>>16)&0xffff;if(stage>1)return false;found=true;
  }
 }return found;
}
Summary Stats()noexcept{
 Summary s;s.factories=factoryCalls.load();s.shaderCalls=shaderCalls.load();s.rejected=rejected.load();s.budgetRefused=budgetRefused.load();s.failures=failures.load();s.activeCalls=active.load();s.accepting=accepting.load();
 {std::lock_guard guard(installMutex);s.hookTargets=targetCount;}
 {std::lock_guard guard(storeMutex);s.captured=programs.size();s.vertex=vertexCount;s.pixel=pixelCount;s.bytes=byteCount;}return s;
}
bool SaveStatus(const char* state)noexcept{
#ifdef ACUNITY_SHADER_REGISTRY92
 (void)state;return true;
#else
 try{const auto s=Stats();std::lock_guard guard(fileMutex);std::filesystem::create_directories(outputDirectory);FILE* f{};auto temp=outputDirectory/L"status.tmp";auto file=outputDirectory/L"status.json";if(_wfopen_s(&f,temp.c_str(),L"wb") || !f)return false;
  const int written=fprintf(f,"{\"state\":\"%s\",\"pid\":%lu,\"factories\":%llu,\"shaderCalls\":%llu,\"captured\":%llu,\"vertex\":%llu,\"pixel\":%llu,\"bytes\":%llu,\"rejected\":%llu,\"budgetRefused\":%llu,\"failures\":%llu,\"hookTargets\":%llu,\"activeCalls\":%llu,\"accepting\":%s}\n",state,GetCurrentProcessId(),s.factories,s.shaderCalls,s.captured,s.vertex,s.pixel,s.bytes,s.rejected,s.budgetRefused,s.failures,s.hookTargets,s.activeCalls,s.accepting?"true":"false");const int close=fclose(f);return written>=0 && !close && MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
 }catch(...){return false;}
#endif
}
bool Start(const std::filesystem::path& directory)noexcept{
 try{
  outputDirectory=directory;
#ifndef ACUNITY_SHADER_REGISTRY92
  std::filesystem::create_directories(directory);
#endif
  if(!programs.empty() || deviceTarget || stopping.load())return false;
  auto module=GetModuleHandleW(L"d3d11.dll");if(!module)return false;
  deviceTarget=reinterpret_cast<void*>(GetProcAddress(module,"D3D11CreateDevice"));swapTarget=reinterpret_cast<void*>(GetProcAddress(module,"D3D11CreateDeviceAndSwapChain"));
  if(!OwnedFunction(deviceTarget) || !OwnedFunction(swapTarget)){++failures;SaveStatus("factory-identity-refused");return false;}
  if(MH_Initialize()!=MH_OK){++failures;SaveStatus("hook-init-refused");return false;}
  programs.reserve(1024);
  if(MH_CreateHook(deviceTarget,reinterpret_cast<void*>(DeviceCreated),reinterpret_cast<void**>(&originalDevice))!=MH_OK || MH_CreateHook(swapTarget,reinterpret_cast<void*>(SwapCreated),reinterpret_cast<void**>(&originalSwap))!=MH_OK){++failures;SaveStatus("factory-hook-refused");return false;}
  accepting.store(true,std::memory_order_release);
  deviceHook=MH_EnableHook(deviceTarget)==MH_OK;swapHook=MH_EnableHook(swapTarget)==MH_OK;
  if(!deviceHook || !swapHook){++failures;Stop();return false;}
  if(!SaveStatus("collecting")){Stop();return false;}return true;
 }catch(...){accepting=false;++failures;SaveStatus("startup-exception");return false;}
}
bool Stop()noexcept{
 accepting.store(false,std::memory_order_release);stopping.store(true,std::memory_order_release);bool disabled=true;
 {std::lock_guard guard(installMutex);
  auto queue=[&](void* p){if(!p)return;auto hr=MH_QueueDisableHook(p);if(hr!=MH_OK && hr!=MH_ERROR_DISABLED && hr!=MH_ERROR_NOT_CREATED)disabled=false;};
  queue(deviceTarget);queue(swapTarget);for(size_t i=0;i<targetCount;++i)queue(targets[i].entry);
  if(MH_ApplyQueued()!=MH_OK)disabled=false;
 }
 if(!disabled){++failures;SaveStatus("disable-refused");return false;}
 // Trampolines and pinned shader identities remain alive until process exit.
 while(active.load(std::memory_order_acquire))Sleep(1);
 bool saved=false;
#ifdef ACUNITY_SHADER_REGISTRY92
 saved=true;stopped92=true;
#else
 try{saved=SavePrograms();}catch(...){++failures;}
#endif
 if(!saved)++failures;SaveStatus(saved?"stopped":"save-failed");return saved;
}
}
#ifdef ACUNITY_SHADER_REGISTRY92
extern "C" __declspec(dllexport) uint32_t WINAPI ACUnityShaderRegistrySnapshot(ShaderRegistry92::Entry* out,uint32_t capacity,ShaderRegistry92::State* state){
 using namespace ShaderObserve75;auto s=Stats();if(state)*state={ShaderRegistry92::Version,GetCurrentProcessId(),s.accepting?1u:0u,stopped92.load()?1u:0u,s.captured,s.failures,s.activeCalls};
 std::lock_guard guard(storeMutex);auto count=uint32_t(programs.size());if(!out||capacity<count)return count;
 for(uint32_t i=0;i<count;++i){auto& p=programs[i];out[i]={};out[i].device=p.device;out[i].shader=p.shader;out[i].stage=p.stage==1?0:1;out[i].bytes=p.bytes;memcpy(out[i].hash,p.hash.c_str(),65);}return count;
}
#endif
#ifndef ACUNITY_SHADER_OBSERVE_FIXTURE
namespace {
HMODULE observerModule{};
DWORD WINAPI ObserveThread(void*){
 try{
  wchar_t path[32768]{};if(!GetModuleFileNameW(observerModule,path,_countof(path)))return 1;
  const auto directory=std::filesystem::path(path).parent_path()/L"captures75"/std::to_wstring(GetCurrentProcessId());
  // Only the file hash is consulted before startup. No game camera/entity fields are read.
  std::filesystem::create_directories(directory);
  auto boot=[&](const char* state){FILE* f{};if(!_wfopen_s(&f,(directory/L"status.json").c_str(),L"wb") && f){fprintf(f,"{\"state\":\"%s\",\"pid\":%lu}\n",state,GetCurrentProcessId());fclose(f);}};
  boot("verifying-image");
  if(!acu::VerifyImage()){boot("unsupported-image");return 2;}
  boot("waiting-for-d3d11");
  while(!GetModuleHandleW(L"d3d11.dll")){if(GetAsyncKeyState(VK_F10)&0x8000)return 0;Sleep(10);}
  if(!ShaderObserve75::Start(directory))return 3;
  auto last=GetTickCount64();
  while(!(GetAsyncKeyState(VK_F10)&0x8000)){Sleep(100);if(GetTickCount64()-last>=2000){ShaderObserve75::SaveStatus("collecting");last=GetTickCount64();}}
  return ShaderObserve75::Stop()?0:4;
 }catch(...){return 5;}
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){observerModule=module;DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,ObserveThread,nullptr,0,nullptr);if(!thread)return FALSE;CloseHandle(thread);}return TRUE;}
#endif
