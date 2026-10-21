#include "VolumetricFog79.h"
#include <windows.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstring>
#include <vector>
#include <share.h>
namespace VolumetricFog79 { namespace {
using Microsoft::WRL::ComPtr;
constexpr const char* Hashes[]{"bab218b17a6f8643aae989c756a8cb0f004b48146d78e695c63344ec62ea6cf5","9cd08220adac0c03ee688f7ab6a91fbe49c0af3ed79a71936fac29126c236edd"};
constexpr size_t Sizes[]{664,5204};constexpr unsigned Indices[]{166,568};
constexpr uint8_t Headers[2][32]{
 {0x44,0x58,0x42,0x43,0x6e,0xc8,0x93,0x28,0xa6,0xd1,0x75,0x1a,0x78,0xde,0x64,0x4c,0xe7,0x5b,0x67,0x6d,0x01,0,0,0,0x98,0x02,0,0,0x05,0,0,0},
 {0x44,0x58,0x42,0x43,0x62,0x64,0xf3,0x1e,0x83,0xe9,0x0f,0x37,0x48,0x86,0x0d,0x91,0x0b,0x39,0xe9,0x63,0x01,0,0,0,0x54,0x14,0,0,0x05,0,0,0}};
struct Entry {uint64_t pointer{};unsigned stage{};ComPtr<IUnknown> pin;};
constexpr unsigned Limit=64;std::array<Entry,Limit> programs;std::atomic<unsigned> count{};SRWLOCK registryLock=SRWLOCK_INIT;
ComPtr<ID3D11Device> gameDevice;std::atomic<bool> active{},removeFog{};std::atomic<uint64_t> skipped[2]{};std::atomic<unsigned> paths[2]{},seen{};FILE* report{},*events{};BCRYPT_ALG_HANDLE algorithm{};bool oldKey{};
bool Header(unsigned stage,const void* data,size_t bytes) noexcept {__try{return stage<2 && data && bytes==Sizes[stage] && !memcmp(data,Headers[stage],32);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Hash(unsigned stage,const void* data,size_t size){if(!Header(stage,data,size) || !algorithm)return false;BCRYPT_HASH_HANDLE h{};DWORD length{},got{};std::array<UCHAR,32> digest{};
 if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&got,0)<0)return false;std::vector<UCHAR> object(length);
 if(BCryptCreateHash(algorithm,&h,object.data(),length,nullptr,0,0)<0)return false;
 bool ok=BCryptHashData(h,const_cast<PUCHAR>(static_cast<const UCHAR*>(data)),ULONG(size),0)>=0 && BCryptFinishHash(h,digest.data(),ULONG(digest.size()),0)>=0;BCryptDestroyHash(h);std::string hex;
 if(ok)for(auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return ok && hex==Hashes[stage];}
bool Owned(IUnknown* shader,unsigned stage)noexcept{__try{
 if(!shader || stage>1)return false;auto table=*reinterpret_cast<void***>(shader);MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(table,&m,sizeof(m)) || m.State!=MEM_COMMIT || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 void* typed{};if(FAILED(shader->QueryInterface(stage?__uuidof(ID3D11PixelShader):__uuidof(ID3D11VertexShader),&typed)))return false;static_cast<IUnknown*>(typed)->Release();
 ID3D11Device* owner{};reinterpret_cast<ID3D11DeviceChild*>(shader)->GetDevice(&owner);bool ok=owner==gameDevice.Get();if(owner)owner->Release();return ok;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Add(unsigned stage,IUnknown* shader){AcquireSRWLockExclusive(&registryLock);auto n=count.load(std::memory_order_relaxed);bool ok=false;
 for(unsigned i=0;i<n;++i)if(programs[i].stage==stage && programs[i].pointer==reinterpret_cast<uint64_t>(shader)){ok=true;break;}
 if(!ok && active && n<Limit){auto& row=programs[n];row.pointer=reinterpret_cast<uint64_t>(shader);row.stage=stage;row.pin=shader;count.store(n+1,std::memory_order_release);ok=true;}ReleaseSRWLockExclusive(&registryLock);return ok;}
void Event(const char* what){if(!events)return;fprintf(events,"{\"event\":\"%s\",\"wallTickMs\":%llu,\"enabled\":%s,\"registeredPrograms\":%u,\"skipped\":[%llu,%llu],\"paths\":[%u,%u]}\n",what,GetTickCount64(),removeFog?"true":"false",count.load(),skipped[0].load(),skipped[1].load(),paths[0].load(),paths[1].load());fflush(events);}
} // anonymous
bool Initialize(ID3D11Device* device,const Program& vertex,const Program& pixel,const std::filesystem::path& directory,FILE* log){
 report=log;gameDevice=device;if(!device || BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 if(vertex.index!=Indices[0] || pixel.index!=Indices[1] || !Hash(0,vertex.code,vertex.bytes) || !Hash(1,pixel.code,pixel.bytes)){fprintf(report,"REFUSED fog79 exact native VS166/PS568 code identities\n");return false;}
 if((vertex.object && !Owned(vertex.object,0)) || (pixel.object && !Owned(pixel.object,1))){fprintf(report,"REFUSED fog79 shader stage/device ownership\n");return false;}
 events=_wfsopen((directory/L"fog-removal79.jsonl").c_str(),L"w",_SH_DENYNO);if(!events)return false;active=true;
 if((vertex.object && !Add(0,vertex.object)) || (pixel.object && !Add(1,pixel.object))){Stop();return false;}
 removeFog=GetPrivateProfileIntW(L"Effects",L"RemoveVolumetricFog",0,(directory/L"ACUnityVR.ini").c_str())!=0;Event("initial");
 fprintf(report,"Fog79 prepared RemoveVolumetricFog=%u: exact VS166/PS568 SHA256/current device, available pinned programs=%u; missing identities use existing VS/PS creation callbacks. NumPad8 compares only the owned-eye six-vertex volumetric fog composite (pass4/subviewFFFFFFFF). General shadows, upstream fog volume and other effects remain native.\n",removeFog?1u:0u,count.load());fflush(report);return true;
}
void Created(unsigned stage,ID3D11Device* device,const void* data,size_t size,IUnknown* shader) noexcept {
 if(!active.load(std::memory_order_relaxed) || stage>1 || device!=gameDevice.Get() || !shader || !Header(stage,data,size))return;
 try{if(Hash(stage,data,size) && Owned(shader,stage))Add(stage,shader);}catch(...){/* Unrecognized/failed registrations retain native work. */}
}
bool Enabled() noexcept {return active.load(std::memory_order_relaxed) && removeFog.load(std::memory_order_relaxed);}
bool Matches(uint64_t vertex,uint64_t pixel) noexcept {if(!Enabled() || !vertex || !pixel)return false;bool v=false,p=false;auto n=count.load(std::memory_order_acquire);
 for(unsigned i=0;i<n;++i){const auto& row=programs[i];if(row.stage==0 && row.pointer==vertex)v=true;if(row.stage==1 && row.pointer==pixel)p=true;}return v && p;}
void Record(unsigned eye,bool direct) noexcept {if(eye<1 || eye>2)return;++skipped[eye-1];paths[eye-1].fetch_or(direct?1u:2u);seen.fetch_or(1u<<(eye-1));}
void Poll(uint32_t mode){if(!active)return;bool key=(GetAsyncKeyState(VK_NUMPAD8)&0x8000)!=0;if(mode==1 && key && !oldKey){removeFog=!removeFog;Event("toggle");fprintf(report,"Fog79 volumetric fog removal %s; normal shadows preserved\n",removeFog?"ON":"OFF");fflush(report);}oldKey=key;
 auto mask=seen.exchange(0);static unsigned reported{};if((mask&~reported)!=0){reported|=mask;Event("applied");fprintf(report,"Fog79 observed owned-eye suppression mask=%u counts=%llu/%llu paths=%u/%u\n",reported,skipped[0].load(),skipped[1].load(),paths[0].load(),paths[1].load());fflush(report);}}
void Stop(){active=false;removeFog=false;if(events){Event("stop");fclose(events);events=nullptr;}/* Immutable program pins remain until process exit. */}
}
