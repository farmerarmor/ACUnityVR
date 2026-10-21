// Read-only attribution of the exact crowd family marked in the headset.
// Existing draw hooks only; bounded CPU snapshots at F8, no GPU commands or game writes.
#include "CrowdDraw82.h"
#include "MaterialDraw75.h"
#ifdef ACUNITYVR_PORTABLE92
#include "ShaderRegistry92.h"
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <fstream>
#include <string>
#include <vector>
#include <share.h>
namespace CrowdDraw82 { namespace {
using Microsoft::WRL::ComPtr;
constexpr const char* Hashes[2]={"b5d72b84e6de6061b73a907620e8ed4a2d4d60ad9100bf8ac7a5852a88930440","afa2449fb45ec47b1a5aa289eee06fecb395fccc6abfcc5dd1304556c005577d"};
constexpr unsigned Sizes[2]={3556,2516},ProgramLimit=32,Limit=16,PerEye=8,Frames=40,Objects=16;
constexpr unsigned CensusPerEye=256,CensusLimit=CensusPerEye*2;
struct Program {uint64_t pointer{};unsigned stage{};ComPtr<IUnknown> pin;};
std::array<Program,ProgramLimit> programs;unsigned programCount{};uint8_t* image{};FILE* report{};std::atomic<bool> prepared{},armed{};
SRWLOCK mutex=SRWLOCK_INIT;struct Lock{Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
struct Frame {uint64_t rip{},rsp{},regs[8]{};uint8_t stack[0x100]{};unsigned native{},stackBytes{};};
struct Object {uint64_t pointer{},vtable{};unsigned bytes{};uint8_t data[0x300]{};};
struct Record {uint64_t context{},wrapper{},cache{},d3d{},view{},state{},vs{},ps{};unsigned eye{},frame{},pass{},subview{},indices{},start{},frameCount{},objectCount{},unwindStatus{};uint8_t camera[0x440]{},native[0x1b00]{},bindings[0x910]{};Frame frames[Frames]{};Object objects[Objects]{};};
std::array<Record,Limit> records;unsigned count{},perEye[2]{},refused{};uint64_t sequence{},tracking{};uint32_t sourceFrame{};std::filesystem::path capturePath;
struct CensusRecord {uint64_t context{},view{},vs{},ps{},cb10{};unsigned eye{},frame{},indices{},start{},pass{},subview{},errors{};uint8_t constants[0xa20]{},camera[0x440]{};};
std::array<CensusRecord,CensusLimit> census;unsigned censusCount{},censusPerEye[2]{},censusSeen[2]{},censusOverflow[2]{};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(p<0x10000 || p>0x00007ffffffeffffULL || n>0x10000 || p+n<p)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
std::string Field(const std::string& line,const char* key){auto marker=std::string("\"")+key+"\":\"";auto b=line.find(marker);if(b==std::string::npos)return {};b+=marker.size();auto e=line.find('"',b);return e==std::string::npos?std::string{}:line.substr(b,e-b);}
uint64_t Hex(const std::string& s){if(s.size()<3 || s.size()>18 || s.substr(0,2)!="0x")return 0;char* end{};auto n=std::strtoull(s.c_str()+2,&end,16);return end && !*end?n:0;}
std::string Text(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(in)),{});}
bool Hash(const std::filesystem::path& p,unsigned stage){auto bytes=Text(p);if(bytes.size()!=Sizes[stage] || bytes.substr(0,4)!="DXBC")return false;BCRYPT_ALG_HANDLE a{};BCRYPT_HASH_HANDLE h{};std::array<UCHAR,32> digest{};
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;auto status=BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0);bool ok=status>=0 && BCryptHashData(h,reinterpret_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0)>=0 && BCryptFinishHash(h,digest.data(),ULONG(digest.size()),0)>=0;if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);std::string hex;for(auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return ok && hex==Hashes[stage];}
bool Owned(uint64_t pointer,unsigned stage,ID3D11Device* device) noexcept {__try{
 auto shader=reinterpret_cast<IUnknown*>(pointer);uint64_t table{};MEMORY_BASIC_INFORMATION m{};if(!Read(&table,pointer,8) || !VirtualQuery(reinterpret_cast<void*>(table),&m,sizeof(m)) || m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll"))return false;
 void* typed{};if(FAILED(shader->QueryInterface(stage?__uuidof(ID3D11PixelShader):__uuidof(ID3D11VertexShader),&typed)))return false;static_cast<IUnknown*>(typed)->Release();ID3D11Device* owner{};reinterpret_cast<ID3D11DeviceChild*>(shader)->GetDevice(&owner);bool ok=owner==device;if(owner)owner->Release();return ok;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Matches(uint64_t vs,uint64_t ps){bool v=false,p=false;for(unsigned i=0;i<programCount;++i){auto& r=programs[i];if(!r.stage && r.pointer==vs)v=true;if(r.stage==1 && r.pointer==ps)p=true;}return v&&p;}
bool Native(uint64_t p){return p>=uint64_t(image) && p-uint64_t(image)<0x7825000;}
void AddObject(Record& r,uint64_t p){if(r.objectCount>=Objects || p%8 || p<0x10000 || Native(p))return;for(unsigned i=0;i<r.objectCount;++i)if(r.objects[i].pointer==p)return;uint64_t vt{};if(!Read(&vt,p,8) || !Native(vt))return;auto& o=r.objects[r.objectCount];if(!Read(o.data,p,sizeof(o.data)))return;uint64_t again{};if(!Read(&again,p,8) || again!=vt)return;o.pointer=p;o.vtable=vt;o.bytes=sizeof(o.data);++r.objectCount;}
// Local-only virtual unwinding, no handlers/debugger; all memory reads are bounded.
__declspec(noinline) void Trace(Record* record) noexcept {__try{
 CONTEXT c{};RtlCaptureContext(&c);auto tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());auto low=uint64_t(tib->StackLimit),high=uint64_t(tib->StackBase);auto& r=*record;
 for(unsigned i=0;i<Frames && c.Rip;++i){
  if((c.Rsp&7) || c.Rsp<low || c.Rsp>high-8){r.unwindStatus=1;break;}auto& f=r.frames[r.frameCount++];f.rip=c.Rip;f.rsp=c.Rsp;f.native=Native(c.Rip);uint64_t regs[8]={c.Rbx,c.Rbp,c.Rsi,c.Rdi,c.R12,c.R13,c.R14,c.R15};memcpy(f.regs,regs,sizeof(regs));
  if(f.native){if(c.Rsp<=high-sizeof(f.stack) && Read(f.stack,c.Rsp,sizeof(f.stack)))f.stackBytes=sizeof(f.stack);for(auto p:regs)AddObject(r,p);}
  DWORD64 module{};auto entry=RtlLookupFunctionEntry(c.Rip,&module,nullptr);auto old=c.Rsp;
  if(entry){PVOID handler{};DWORD64 establisher{};RtlVirtualUnwind(UNW_FLAG_NHANDLER,module,c.Rip,entry,&c,&handler,&establisher,nullptr);}
  else {if(!Read(&c.Rip,c.Rsp,8)){r.unwindStatus=2;break;}c.Rsp+=8;}
  if(c.Rsp<=old){r.unwindStatus=3;break;}
 }
 }__except(EXCEPTION_EXECUTE_HANDLER){record->unwindStatus=4;}}
bool Write(const std::filesystem::path& path,const void* data,size_t n){FILE* f=_wfsopen(path.c_str(),L"wb",_SH_DENYNO);if(!f)return false;bool ok=fwrite(data,n,1,f)==1;return fclose(f)==0&&ok;}
void SaveCensus(){bool ok=!censusCount || Write(capturePath/L"crowd-census89.bin",census.data(),sizeof(CensusRecord)*censusCount);auto f=_wfsopen((capturePath/L"crowd-census89.json").c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"records\":%u,\"capacityPerEye\":%u,\"recordBytes\":%zu,\"constantsOffset\":%zu,\"cameraOffset\":%zu,\"seen\":[%u,%u],\"stored\":[%u,%u],\"overflow\":[%u,%u],\"filesOK\":%s,\"note\":\"Exact current marked PS/VS at owned worker DrawIndexed. Full native CPU CB10 payload plus native camera; current buffer wrapper+28 checked against cached VS slot10 as in native2152f20. No GPU readbacks or deferred Get-state. Unused tail transforms may be stale; correlate native index/instance counts.\",\"draws\":[",sequence,tracking,sourceFrame,censusCount,CensusPerEye,sizeof(CensusRecord),offsetof(CensusRecord,constants),offsetof(CensusRecord,camera),censusSeen[0],censusSeen[1],censusPerEye[0],censusPerEye[1],censusOverflow[0],censusOverflow[1],ok?"true":"false");
 for(unsigned i=0;i<censusCount;++i){auto& c=census[i];fprintf(f,"%s{\"id\":%u,\"eye\":%u,\"frame\":%u,\"indices\":%u,\"start\":%u,\"pass\":%u,\"subview\":%u,\"errors\":%u,\"context\":\"%llx\",\"view\":\"%llx\",\"cb10\":\"%llx\"}",i?",":"",i,c.eye,c.frame,c.indices,c.start,c.pass,c.subview,c.errors,c.context,c.view,c.cb10);}fprintf(f,"]}\n");fclose(f);
 if(report){fprintf(report,"Crowd census89 interval=%llu stored=%u/%u seen=%u/%u overflow=%u/%u filesOK=%u\n",sequence,censusPerEye[0],censusPerEye[1],censusSeen[0],censusSeen[1],censusOverflow[0],censusOverflow[1],unsigned(ok));fflush(report);}
}
}
bool Initialize(uint8_t* base,ID3D11Device* device,const std::filesystem::path& directory,FILE* log){
 image=base;report=log;if(!GetPrivateProfileIntW(L"Diagnostics",L"CaptureMarkedCrowd",0,(directory/L"ACUnityVR.ini").c_str()))return true;
#ifdef ACUNITYVR_PORTABLE92
 unsigned found[2]{};std::vector<ShaderRegistry92::Entry> imported;if(!ShaderRegistry92::Read(imported))return false;
 for(auto& entry:imported){auto st=entry.stage;if(entry.device!=uint64_t(device)||st>1||strcmp(entry.hash,Hashes[st]))continue;
  if(!Owned(entry.shader,st,device))return false;
  bool duplicate=false;for(unsigned i=0;i<programCount;++i)if(programs[i].pointer==entry.shader&&programs[i].stage==st)duplicate=true;
  if(!duplicate){if(programCount==ProgramLimit)return false;auto& p=programs[programCount++];p.pointer=entry.shader;p.stage=st;p.pin=reinterpret_cast<IUnknown*>(entry.shader);++found[st];}
 }
#else
 auto capture=directory.parent_path().parent_path()/L"experiments"/L"shader-observe75"/L"captures75"/std::to_wstring(GetCurrentProcessId());auto status=Text(capture/L"status.json");
 if(status.find("\"state\":\"stopped\"")==std::string::npos || status.find("\"pid\":"+std::to_string(GetCurrentProcessId())+",")==std::string::npos || status.find("\"accepting\":false")==std::string::npos || status.find("\"activeCalls\":0,")==std::string::npos || status.find("\"failures\":0,")==std::string::npos)return false;
 std::ifstream in(capture/L"shader-list.jsonl");std::string line;unsigned lines{},found[2]{};bool checked[2]{};
 while(std::getline(in,line)){
  if(++lines>4096 || line.size()>1024)return false;unsigned st=Field(line,"stage")=="VS"?0:Field(line,"stage")=="PS"?1:2;if(st==2 || Field(line,"sha256")!=Hashes[st] || Hex(Field(line,"device"))!=uint64_t(device))continue;
  auto file=std::string(st?"ps-":"vs-")+Hashes[st]+".dxbc";auto pointer=Hex(Field(line,"shader"));if(Field(line,"file")!=file || !pointer || (!checked[st] && !Hash(capture/file,st)) || !Owned(pointer,st,device))return false;checked[st]=true;
  bool duplicate=false;for(unsigned i=0;i<programCount;++i)if(programs[i].pointer==pointer && programs[i].stage==st)duplicate=true;
  if(!duplicate){if(programCount==ProgramLimit)return false;auto& p=programs[programCount++];p.pointer=pointer;p.stage=st;p.pin=reinterpret_cast<IUnknown*>(pointer);++found[st];}
 }
#endif
 if(!found[0] || !found[1]){fprintf(log,"REFUSED CrowdDraw82 marked crowd programs absent VS=%u PS=%u\n",found[0],found[1]);return false;}
 prepared=true;fprintf(log,"CrowdDraw82 prepared exact marked crowd VS/PS hashes and current device: programs=%u/%u; F8-only max8 stack traces plus256 CPU CB10/camera census records per owned eye; explicit overflow counts, no new hooks/GPU commands/culling writes\n",found[0],found[1]);fflush(log);return true;
}
bool Armed() noexcept {return armed.load(std::memory_order_relaxed);}
void Arm(uint64_t id,uint64_t pose,uint32_t frame,const std::filesystem::path& path){if(!prepared)return;Lock lock;armed=false;count=refused=censusCount=0;perEye[0]=perEye[1]=0;memset(censusPerEye,0,sizeof(censusPerEye));memset(censusSeen,0,sizeof(censusSeen));memset(censusOverflow,0,sizeof(censusOverflow));sequence=id;tracking=pose;sourceFrame=frame;capturePath=path;armed=true;}
void Observe(const EffectInputs72::Worker& w,uint64_t surface,const std::array<uint64_t,2>& eyes,unsigned kind,UINT a,UINT b,bool replay){
 if(!armed.load(std::memory_order_relaxed) || replay || kind!=12 || !a)return;uint64_t vs{},ps{};if(!Read(&vs,w.cache+0x4e8,8) || !Read(&ps,w.cache+0x4e0,8) || !Matches(vs,ps))return;
 Lock lock;if(!armed)return;MaterialDraw75::Identity s;if(!MaterialDraw75::Read(Read,w,surface,eyes,s) || !Matches(s.shaders[0],s.shaders[1])){++refused;return;}
 auto eye=s.eye-1;++censusSeen[eye];
 if(censusCount<CensusLimit && censusPerEye[eye]<CensusPerEye){auto& c=census[censusCount++];c={};c.context=w.context;c.view=s.view;c.vs=vs;c.ps=ps;c.eye=s.eye;c.indices=a;c.start=b;++censusPerEye[eye];
  uint64_t nativeBuffer{},buffer{};if(!Read(c.constants,w.context+0x9d0,sizeof(c.constants)) || !Read(c.camera,s.view,sizeof(c.camera)) || !Read(&c.cb10,w.cache+0x608,8) || !Read(&nativeBuffer,w.context+0x9c0,8) || !Read(&buffer,nativeBuffer+0x28,8) || c.cb10!=buffer || !c.cb10)c.errors|=1;
  c.frame=EffectInputs72::Get<unsigned>(c.camera,0x294);if(!Read(&c.pass,w.context+0x3c,4) || !Read(&c.subview,w.context+0x370,4))c.errors|=2;
  MaterialDraw75::Identity after;if(!MaterialDraw75::Read(Read,w,surface,eyes,after) || after.view!=s.view || after.state!=s.state || after.shaders[0]!=vs || after.shaders[1]!=ps)c.errors|=4;
 }else ++censusOverflow[eye];
 if(count==Limit || perEye[eye]>=PerEye)return;
 auto& r=records[count];r={};r.context=w.context;r.wrapper=w.wrapper;r.cache=w.cache;r.d3d=w.d3d;r.view=s.view;r.state=s.state;r.vs=vs;r.ps=ps;r.eye=s.eye;r.indices=a;r.start=b;
 if(!Read(r.native,w.context,sizeof(r.native)) || !Read(r.bindings,w.cache,sizeof(r.bindings)) || !Read(r.camera,s.view,sizeof(r.camera))){++refused;return;}
 r.frame=EffectInputs72::Get<unsigned>(r.camera,0x294);r.pass=EffectInputs72::Get<unsigned>(r.native,0x3c);r.subview=EffectInputs72::Get<unsigned>(r.native,0x370);
 Trace(&r);MaterialDraw75::Identity after;if(!MaterialDraw75::Read(Read,w,surface,eyes,after) || after.view!=s.view || after.state!=s.state || after.shaders[0]!=vs || after.shaders[1]!=ps){++refused;return;}
 ++count;++perEye[s.eye-1];
}
void Finish(){if(!prepared)return;Lock lock;if(!armed.exchange(false))return;SaveCensus();bool ok=true;auto path=capturePath/L"marked-crowd82.bin";if(count)ok=Write(path,records.data(),sizeof(Record)*count);FILE* f=_wfsopen((capturePath/L"marked-crowd82.json").c_str(),L"w",_SH_DENYNO);
 if(f){fprintf(f,"{\"imageBase\":\"%llx\",\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"records\":%u,\"perEye\":[%u,%u],\"refused\":%u,\"recordBytes\":%zu,\"frameBytes\":%zu,\"objectBytes\":%zu,\"cameraOffset\":%zu,\"nativeOffset\":%zu,\"bindingsOffset\":%zu,\"framesOffset\":%zu,\"objectsOffset\":%zu,\"filesOK\":%s,\"note\":\"Exact marked skinned crowd pair at owned worker DrawIndexed. Frames include RIP, RSP and recovered nonvolatile RBX RBP RSI RDI R12 R13 R14 R15; volatile registers are deliberately omitted. Objects only where first qword belongs to game image. No deferred Get-state calls, native writes or GPU commands.\",\"draws\":[",uint64_t(image),sequence,tracking,sourceFrame,count,perEye[0],perEye[1],refused,sizeof(Record),sizeof(Frame),sizeof(Object),offsetof(Record,camera),offsetof(Record,native),offsetof(Record,bindings),offsetof(Record,frames),offsetof(Record,objects),ok?"true":"false");
  for(unsigned i=0;i<count;++i){auto& r=records[i];fprintf(f,"%s{\"id\":%u,\"eye\":%u,\"viewFrame\":%u,\"indices\":%u,\"start\":%u,\"pass\":%u,\"subview\":%u,\"context\":\"%llx\",\"view\":\"%llx\",\"vs\":\"%llx\",\"ps\":\"%llx\",\"frames\":%u,\"objects\":%u,\"unwindStatus\":%u,\"nativeStack\":[",i?",":"",i,r.eye,r.frame,r.indices,r.start,r.pass,r.subview,r.context,r.view,r.vs,r.ps,r.frameCount,r.objectCount,r.unwindStatus);bool comma=false;for(unsigned j=0;j<r.frameCount;++j)if(r.frames[j].native){fprintf(f,"%s\"%llx\"",comma?",":"",r.frames[j].rip-uint64_t(image));comma=true;}fprintf(f,"]}");}fprintf(f,"]}\n");fclose(f);}
 if(report){fprintf(report,"CrowdDraw82 interval=%llu marked draw traces=%u/%u refused=%u filesOK=%u; renderer unchanged\n",sequence,perEye[0],perEye[1],refused,unsigned(ok));fflush(report);}}
void Stop(){prepared=false;Lock lock;armed=false;/* immutable pinned identities retained until process exit */}
}
