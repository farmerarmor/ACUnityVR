#include "MaterialPrograms75.h"
#include "MaterialFamilies75.h"
#include "MaterialProgramSpecs75.h"
#ifdef ACUNITYVR_PORTABLE92
#include "ShaderRegistry92.h"
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <array>
#include <fstream>
#include <atomic>
#include <cstdlib>
#include <cstring>
namespace MaterialPrograms75 {
namespace {
using Microsoft::WRL::ComPtr;
using MaterialProgramSpecs75::specs;
static_assert(sizeof(specs)/sizeof(specs[0])==MaterialFamilies75::Groups);
struct Program {uint64_t pointer{};unsigned group{},stage{};ComPtr<IUnknown> pin;};
constexpr unsigned MaxPrograms78=64;
std::array<Program,MaxPrograms78> programs;std::atomic<unsigned> programCount78{};SRWLOCK programLock78=SRWLOCK_INIT;FILE* report{};
ComPtr<ID3D11Device> programDevice78;
std::atomic<bool> missingSmall78{},observeSmall78{};
std::atomic<bool> active{},cards{},smallSmoke{},chimneySmoke{};std::atomic<unsigned> particles{};std::array<std::array<std::atomic<uint64_t>,2>,MaterialFamilies75::Groups> skipped{};
std::string Field(const std::string& line,const char* key){const std::string marker=std::string("\"")+key+"\":\"";auto begin=line.find(marker);if(begin==std::string::npos)return {};begin+=marker.size();auto end=line.find('"',begin);if(end==std::string::npos)return {};return line.substr(begin,end-begin);}
std::string FileText(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);if(!in)return {};return std::string((std::istreambuf_iterator<char>(in)),{});}
uint64_t Hex(const std::string& s){if(s.size()<3 || s.size()>18 || s.substr(0,2)!="0x")return 0;char* end{};const auto value=std::strtoull(s.c_str()+2,&end,16);return end && !*end?value:0;}
bool HashBytes78(const std::string& code,const char* expected){if(code.size()<32 || code.size()>1024*1024 || code.substr(0,4)!="DXBC")return false;
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD size{},returned{};std::array<UCHAR,32> digest{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&returned,0)>=0;std::vector<UCHAR> object(size);
 if(ok)ok=BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(code.data())),static_cast<ULONG>(code.size()),0)>=0 && BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);std::string hex;
 if(ok)for(auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return ok && hex==expected;
}
bool OwnedShader(IUnknown* shader,ID3D11Device* expected)noexcept{__try{
 if(!shader)return false;auto table=*reinterpret_cast<void***>(shader);MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(table,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || info.AllocationBase!=GetModuleHandleW(L"d3d11.dll"))return false;
 ID3D11Device* owner{};reinterpret_cast<ID3D11DeviceChild*>(shader)->GetDevice(&owner);const bool ok=owner==expected;if(owner)owner->Release();return ok;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool HashFile(const std::filesystem::path& path,const char* expected){return HashBytes78(FileText(path),expected);}
bool AddProgram78(uint64_t pointer,unsigned group,unsigned stage,IUnknown* shader){
 AcquireSRWLockExclusive(&programLock78);auto count=programCount78.load(std::memory_order_relaxed);
 for(unsigned i=0;i<count;++i)if(programs[i].pointer==pointer && programs[i].group==group && programs[i].stage==stage){ReleaseSRWLockExclusive(&programLock78);return true;}
 if(count==MaxPrograms78){ReleaseSRWLockExclusive(&programLock78);return false;}
 auto& p=programs[count];p.pointer=pointer;p.group=group;p.stage=stage;p.pin=shader;programCount78.store(count+1,std::memory_order_release);ReleaseSRWLockExclusive(&programLock78);return true;
}
bool SmallHeader78(const void* data,SIZE_T size)noexcept{__try{
 static constexpr unsigned char prefix[]{0x44,0x58,0x42,0x43,0x2a,0x9d,0x89,0xf6,0x45,0xbb,0x34,0x85,0x3e,0x91,0xb8,0x0d,0x50,0xe1,0x45,0xb2,0x01,0x00,0x00,0x00,0xac,0x13,0x00,0x00,0x03,0x00,0x00,0x00};
 return data && size==5036 && !memcmp(data,prefix,sizeof(prefix));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void RegisterPixel78(ID3D11Device* device,const void* data,SIZE_T size,ID3D11PixelShader* shader)noexcept{
 if(shader && observeSmall78.load(std::memory_order_relaxed) && device==programDevice78.Get() && SmallHeader78(data,size)){
  try{std::string code(static_cast<const char*>(data),size);if(HashBytes78(code,specs[4].ps) && OwnedShader(shader,device) && AddProgram78(reinterpret_cast<uint64_t>(shader),5,1,shader)){
   const bool first=missingSmall78.exchange(false);if(first && report){fprintf(report,"Material78 small chimney shader registered through existing creation hook: exact SHA256/current game device, immutable pinned identity\n");fflush(report);}
  }}catch(...){if(report){fprintf(report,"Material78 late small-smoke registration refused; existing verified registrations retained\n");fflush(report);}}
 }
}
}
bool Initialize(ID3D11Device* device,const std::filesystem::path& directory,FILE* log){
#ifdef ACUNITYVR_PORTABLE92
 report=log;unsigned counts[MaterialFamilies75::Groups][2]{};std::vector<ShaderRegistry92::Entry> imported;
 if(!ShaderRegistry92::Read(imported)){fprintf(report,"REFUSED material75 automatic in-memory shader registry not ready\n");return false;}
 for(auto& entry:imported){if(entry.device!=uint64_t(device))continue;
  for(unsigned g=0;g<MaterialFamilies75::Groups;++g)for(unsigned st=0;st<2;++st){
   if(entry.stage!=st||strcmp(entry.hash,st?specs[g].ps:specs[g].vs))continue;
   auto object=reinterpret_cast<IUnknown*>(entry.shader);
   if(!OwnedShader(object,device)||!AddProgram78(entry.shader,g+1,st,object))return false;++counts[g][st];
  }
 }
#else
 report=log;const auto pid=GetCurrentProcessId();auto capture=directory.parent_path().parent_path()/L"experiments"/L"shader-observe75"/L"captures75"/std::to_wstring(pid);auto status=FileText(capture/L"status.json");
 if(status.find("\"state\":\"stopped\"")==std::string::npos || status.find("\"pid\":"+std::to_string(pid)+",")==std::string::npos || status.find("\"accepting\":false")==std::string::npos || status.find("\"failures\":0,")==std::string::npos || status.find("\"activeCalls\":0,")==std::string::npos){fprintf(report,"REFUSED material75 missing stopped same-process collector\n");return false;}
 std::ifstream in(capture/L"shader-list.jsonl");std::string line;unsigned counts[MaterialFamilies75::Groups][2]{};bool checked[MaterialFamilies75::Groups][2]{};size_t lines{};
 while(std::getline(in,line)){
  if(++lines>4096 || line.size()>1024)return false;const auto stage=Field(line,"stage");const auto sha=Field(line,"sha256");const auto pointer=Hex(Field(line,"shader")),owner=Hex(Field(line,"device"));
  if(owner!=reinterpret_cast<uint64_t>(device))continue;
  for(unsigned g=0;g<MaterialFamilies75::Groups;++g)for(unsigned st=0;st<2;++st){const auto expected=st?specs[g].ps:specs[g].vs;if(stage!=(st?"PS":"VS") || sha!=expected)continue;
   const auto filename=std::string(st?"ps-":"vs-")+expected+".dxbc";if(Field(line,"file")!=filename || !pointer)return false;
   if(!checked[g][st]){if(!HashFile(capture/filename,expected))return false;checked[g][st]=true;}
   auto object=reinterpret_cast<IUnknown*>(pointer);if(!OwnedShader(object,device)){fprintf(report,"REFUSED material75 shader/device ownership group=%u stage=%u\n",g+1,st);return false;}
   if(!AddProgram78(pointer,g+1,st,object))return false;++counts[g][st];
  }
 }
#endif
 // Removal families that are already needed by this build remain mandatory.
 for(unsigned group:{2u,6u,7u})if(!counts[group-1][0] || !counts[group-1][1]){fprintf(report,"REFUSED material75 required removal pair missing group=%u VS=%u PS=%u\n",group,counts[group-1][0],counts[group-1][1]);return false;}
 programDevice78=device;missingSmall78=!counts[4][1];
 if(missingSmall78 && !counts[4][0]){fprintf(report,"REFUSED material78 missing small-smoke shader without verified vertex identity/creation observer\n");return false;}
 observeSmall78=missingSmall78.load();
 for(unsigned g=0;g<MaterialFamilies75::Groups;++g)if(!counts[g][0] || !counts[g][1])fprintf(report,"Material78 absent startup family=%u VS=%u PS=%u policy=%s\n",g+1,counts[g][0],counts[g][1],g==4?"exact small-smoke creation observer armed":"optional diagnostic unavailable");
 cards=GetPrivateProfileIntW(L"Effects",L"RemoveTreeBillboards",1,(directory/L"ACUnityVR.ini").c_str())!=0;
 chimneySmoke=GetPrivateProfileIntW(L"Effects",L"RemoveChimneySmoke",1,(directory/L"ACUnityVR.ini").c_str())!=0;
 fprintf(report,"Material75 effective INI Effects.RemoveTreeBillboards=%u RemoveChimneySmoke=%u; NumPad1 compares trees, NumPad4 compares combined chimney smoke removal\n",cards.load()?1u:0u,chimneySmoke.load()?1u:0u);
 active=true;fprintf(report,"Material75 prepared: available code-hash verified VS/PS pairs on current game device, pinned identities retained; required removal families2/6/7 validated, small-smoke family5 imported or tracked at exact shader creation. NumLock ON: NumPad1 toggles camera-facing geometry candidates; NumPad2 cycles original particle families1/2/3/OFF; NumPad3 toggles the additional depth-faded particle variant. Tree and combined chimney smoke removal follow INI; NumPad4 toggles both identified chimney families together. Legacy isolation controls start OFF. Exact owned-eye indexed shapes or validated game-device indirect argument buffers only; no global suppression. Nine GPU slots per eye reserved for targets, same total24 per eye.\n");fflush(report);return true;
}
unsigned Pair(uint64_t vs,uint64_t ps)noexcept{
 if(!active.load(std::memory_order_relaxed))return 0;
 const auto count=programCount78.load(std::memory_order_acquire);for(unsigned i=0;i<count;++i){const auto& v=programs[i];if(v.stage==0 && v.pointer==vs)for(unsigned j=0;j<count;++j){const auto& p=programs[j];if(p.stage==1 && p.group==v.group && p.pointer==ps)return v.group;}}return 0;
}
void Poll(uint32_t mode){
 if(!active || mode!=1)return;static bool old1{},old2{},old3{},old4{};const bool key1=(GetAsyncKeyState(VK_NUMPAD1)&0x8000)!=0,key2=(GetAsyncKeyState(VK_NUMPAD2)&0x8000)!=0,key3=(GetAsyncKeyState(VK_NUMPAD3)&0x8000)!=0;
 if(key1 && !old1){cards=!cards;fprintf(report,"Material75 camera-facing geometry isolation %s\n",cards?"ON":"OFF");fflush(report);}old1=key1;
 if(key2 && !old2){particles=(particles+1)%4;fprintf(report,"Material75 particle isolation family=%u (0=OFF)\n",particles.load());fflush(report);}old2=key2;
 if(key3 && !old3){smallSmoke=!smallSmoke;fprintf(report,"Material75 extra depth-faded particle isolation %s\n",smallSmoke?"ON":"OFF");fflush(report);}old3=key3;
 const bool key4=(GetAsyncKeyState(VK_NUMPAD4)&0x8000)!=0;if(key4 && !old4){chimneySmoke=!chimneySmoke;fprintf(report,"Material75 combined chimney smoke removal %s (large+small families2/5)\n",chimneySmoke?"ON":"OFF");fflush(report);}old4=key4;
}
void PixelCreated78(ID3D11Device* device,const void* bytes,SIZE_T size,ID3D11PixelShader* shader)noexcept{RegisterPixel78(device,bytes,size,shader);}
bool MaskEnabled()noexcept{return active.load(std::memory_order_relaxed) && (cards.load(std::memory_order_relaxed) || particles.load(std::memory_order_relaxed) || smallSmoke.load(std::memory_order_relaxed) || chimneySmoke.load(std::memory_order_relaxed));}
bool Hidden(unsigned group)noexcept{return active && MaterialFamilies75::Hide(group,cards.load(),particles.load(),smallSmoke.load(),chimneySmoke.load());}
void Count(unsigned group,unsigned eye)noexcept{if(group>=1 && group<=MaterialFamilies75::Groups && eye>=1 && eye<=2)++skipped[group-1][eye-1];}
void Stop(){active=false;missingSmall78=false;observeSmall78=false;chimneySmoke=false;cards=false;particles=0;smallSmoke=false;if(report){for(unsigned g=0;g<MaterialFamilies75::Groups;++g)fprintf(report,"Material75 group%u skipped owned-eye draws=%llu/%llu; masks OFF, pinned shader refs retained until exit\n",g+1,skipped[g][0].load(),skipped[g][1].load());fflush(report);}}
}
