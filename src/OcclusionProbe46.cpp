#ifdef ACUNITYVR_SHADER_HUNT78
#include "ShaderHunt.h"
#endif
// F8-only native metadata + actual GPU constant buffers. No game bindings or camera writes.
#include "OcclusionProbe.h"
#include "GpuCapture.h"
#include "GeometryConstants46.h"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <share.h>
#include <cstring>
namespace OcclusionProbe {
using acu::Field;
uint8_t* image{};void* surface{};void* views[3]{};FILE* report{};
std::filesystem::path directory,capturePath;
std::atomic<bool> active{};
SRWLOCK captureLock=SRWLOCK_INIT;
struct Lock{Lock(){AcquireSRWLockExclusive(&captureLock);}~Lock(){ReleaseSRWLockExclusive(&captureLock);}};
constexpr unsigned RecordLimit=1024,AttemptLimit=4096;
struct Record{
 int gpuCopy=-1;uint64_t cache{},cb2{},cb8{};uint32_t csDirty{};
 uint64_t context{},d3dContext{},batch{},cluster{},shader{},history{},historySrv{},historyCode{},config{};
 uint32_t view{},viewFrame{},shaderIndex{},kind{},dimensions[3]{},contextType{},pass{},subview{},configIndex{},selectorReturn{};
 uint8_t flags[16]{},configFlags[16]{},options88{},globalDisable{},matrix[64]{},historyInfo[0x50]{};
 uint8_t misc[560]{},passConstants[608]{},contextCamera[240]{};
 uint32_t stack[24]{},stackCount{};
};
GeometryConstants46::Pool gpuPool{};bool gpuCaptureAvailable{};unsigned gpuErrors{};
std::array<Record,RecordLimit> records{};unsigned recordCount{},attempts{},missed{},irrelevant{},errors{},overflows{};
uint64_t sequence{},tracking{};uint32_t sourceFrame{};
struct Shader{void* pointer{};unsigned index{};};std::array<Shader,164> shaders{};
bool Guard(){
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={
  {0x22f23ea,"\x44\x8b\x4b\xf8\x8b\xd7\x48\x8b\xce\x49\x8d\x81\xb7\x00\x00\x00\x48\x6b\xd2\x0e\x48\x03\xd0\x48\x8d\x04\xd6\x48\x8b\x56\x38\x48\x89\x44\x24\x28\x44\x89\x44\x24\x20\x44\x8b\xc7\xe8\x65\x00\xeb\xff",49},
  {0x21a2480,"\x40\x53\x48\x83\xec\x20\x49\x63\xc0\x44\x8b\x44\x24\x50\x41\x8b\xd9\x4c\x8b\x4c\x24\x58\x4c\x8d\x14\x40\x48\x8b\xca\x48\x8d\x05\x6c\x34\xef\x00\x8b\xd3\x48\x83\xc4\x20\x5b\x4a\xff\x24\xd0",47},
  {0x2185914,"\x48\x8b\x01\xff\xa0\x38\x02\x00\x00",9},
  {0x3095988,"\x14\x59\x18\x42\x01\x00\x00\x00",8},
  {0x22f7422,"\xe9\x09\xb9\xff\xff",5},
  {0x22c070c,"\x48\x8b\x43\x18\x48\x8b\x3c\xd0",8},
  {0x22c071c,"\x48\x8b\x0f\x48\x89\x4e\x20",7},
  {0x227a855,"\x8b\x56\x10\x48\x8d\x8e\x60\x04\x00\x00\x48\x89\x46\x18",14},
  {0x22f75b0,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x57",16},
  {0x22f7610,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x41",16},
  {0x22dfa6d,"\x48\x8b\xf2\x48\x8b\xd9\x20\x81\xa0\x04\x00\x00",12},
  {0x22dfb52,"\x4c\x8b\x44\x24\x70\x48\x8b\xd6\x48\x8b\xcb\x4c\x89\x6d\x10\xe8\x4a\x5a\xff\xff",20},
  {0x22dfc30,"\x4c\x8b\x43\x08\x48\x8b\xbb\xb8\x04\x00\x00\x41\x8b\x40\x08\x33\xd2\x41\xf7\x30\x85\xc0\x74\x22\x4c\x8b\x4b\x38\x48\x8b\xd6\x48\x8b\xcb\xc6\x44\x24\x20\x01\xe8",40},
  {0x22dfc9d,"\x4c\x8b\xc7\xe8\xab\x8b\xfb\xff\x48\x8b\xd6\x48",12},
  {0x22dfcb0,"\x4c\x8b\xc7\x48\x8b\xd6\x48\x8b\xcb\xe8\xb2\x91\xfb\xff",14},
  {0x22d6b61,"\x80\x79\x26\x00\x74\x0d\x45\x84\xc0\x75\x08\x84\xd2\x74\x04\x8b\xd0\xeb\x02\x33\xd2\x88\x93\xa0\x04\x00\x00\x84\xd2\x74\x0a\x80",32},
  {0x22d57e1,"\x44\x38\x0d\x54\x77\xf7\x02\x0f\x94\xc0\x20\x87\xa0\x04\x00\x00",16},
  {0x22af9b0,"\x48\x8b\x41\x44\x8d\x14\xd5\x00\x00\x00\x00\x48\x8b\x04\x02\xc3",16},
  {0x22f7012,"\x48\x8b\x43\x28\x48\x39\x90\xf0\x04\x00\x00\x74\x0e\x83\x88\x10\x05\x00\x00\x20\x48\x89\x90\xf0\x04\x00\x00",27},
  {0x22c2a1c,"\x49\x89\xbe\x68\x1a\x00\x00\x41\x89\x4e\x40",11},
  {0x223a9b0,"\x48\x8b\x81\xf8\x06\x00\x00\x42\x8d\x14\xc5\x00\x00\x00\x00\x48\x8b\x04\x02\xc3",20},
  {0x2299112,"\x80\xbf\xa0\x04\x00\x00\x00\xbd\x04\x00\x00\x00\x74\x0b\x4d\x85\xed\x74\x18\x49\x8b\x45\x60\xeb\x14\x48\x8b\x05\x86\x3e\xfb\x02\x48\x85\xc0\x74\x06\x48\x8b\x40\x60\xeb\x02\x33\xc0",45},
  {0x2299188,"\x80\xbf\xa0\x04\x00\x00\x00\x48\x8b\xcb\x74\x29\x80\xbf\xa6\x04\x00\x00\x00\x74\x19\x45\x33\xed\x41\x8b\xd5\x38\x97\xa7\x04\x00\x00\x0f\x95\xc2\x83\xc2\x08\xe8\x3c\xde\x05\x00\xeb\x1d\xba\x0c\x00\x00\x00\xeb\x0e\x0f\xb6\x87\xa6\x04\x00\x00\xf6\xd8\x1b\xd2\x83\xc2\x0b\xe8\x20\xde\x05\x00\x45\x33\xed",75},
  {0x2298a8c,"\x41\x80\xbe\xa3\x04\x00\x00\x00\x74\x33\x41\x80\xbe\xa2\x04\x00\x00\x00\x74\x29\x48\x8b\xcb\x45\x84\xc9\x74\x1a\x45\x33\xed\x41\x8b\xd5\x41\x38\x96\xa7\x04\x00\x00\x0f\x95\xc2\x83\xc2\x0f\xe8\x30\xe5\x05\x00\xeb\x22\xba\x12\x00\x00\x00\xeb\x13\xba\x13\x00\x00\x00\xb8\x11\x00\x00\x00\x45\x84\xc9\x0f\x45\xd0\x48\x8b\xcb\xe8\x0f\xe5\x05\x00\x45\x33\xed",88},
  {0x2299723,"\x44\x38\xbd\xa0\x04\x00\x00\x74\x5e\x48\x85\xf6\x74\x59\x48\x8b\x46\x60\x4c\x8b\x43\x28\xbe\x05\x00",25},
 };
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
 return true;
}
bool ShaderTable(){__try{
 auto m=image+0x524e030;auto count=Field<uint16_t>(m,0x4c);auto table=Field<uint8_t**>(m,0x44);
 if(count<shaders.size() || count>512 || !table)return false;
 // Registration can append shaders. Only read the fixed verified prefix, never the added entries.
 for(unsigned i=0;i<shaders.size();++i){auto entry=table[i];if(!entry)return false;shaders[i]={Field<void*>(entry,0x10),i};}
 const struct{unsigned index;size_t rva,size;} expected[]={
  {8,0x32064d0,10504},
  {9,0x31df7b0,11596},
  {10,0x31f2600,7628},
  {11,0x31e9ad0,5328},
  {12,0x31fbaf0,8268},
  {13,0x3210e80,3624},
  {14,0x3216a40,2016},
  {15,0x3236eb0,9564},
  {16,0x3222680,10484},
  {17,0x322f710,8500},
  {18,0x3234b70,6472},
  {19,0x3231e40,5428},
  {87,0x348b640,6688},
  {88,0x34858a0,3860},
 };
 for(auto& e:expected){auto entry=table[e.index];if(!shaders[e.index].pointer || Field<uint8_t*>(entry,0)!=image+e.rva || Field<uint64_t>(entry,8)!=e.size || memcmp(image+e.rva,"DXBC",4) || Field<uint32_t>(image+e.rva,24)!=e.size)return false;}
 return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Relevant(unsigned i){return (i>=8 && i<=19) || i==87 || i==88;}
// Exact native22dfa40 holds the geometry context in RSI and cluster in RBX.
// Read only the locally unwound copy at the verified return addresses.
__declspec(noinline) bool Recover(ID3D11DeviceContext* d3d,Record* out){__try{
 CONTEXT c{};RtlCaptureContext(&c);auto tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());
 auto low=reinterpret_cast<uintptr_t>(tib->StackLimit),high=reinterpret_cast<uintptr_t>(tib->StackBase),base=reinterpret_cast<uintptr_t>(image);
 for(unsigned frame=0;frame<36 && c.Rip;++frame){
  if(c.Rsp<low || c.Rsp>high-8 || (c.Rsp&7))return false;
  if(c.Rip>=base && c.Rip<base+0x7825000 && out->stackCount<_countof(out->stack))out->stack[out->stackCount++]=unsigned(c.Rip-base);
  auto rva=c.Rip-base;
#ifdef ACUNITYVR_OCCLUSION_CHECK
  extern uintptr_t checkReturn;
  bool found=c.Rip==checkReturn;
#else
  bool found=rva==0x22dfc6a || rva==0x22dfca5 || rva==0x22dfcb0 || rva==0x22dfcbe || rva==0x22dfccd;
#endif
  if(found){
   auto context=reinterpret_cast<uint8_t*>(c.Rsi),cluster=reinterpret_cast<uint8_t*>(c.Rbx);
#ifdef ACUNITYVR_OCCLUSION_CHECK
   rva=0x22dfc6a;
#endif
   auto wrapper=Field<uint8_t*>(context,0);if(!wrapper || Field<void*>(wrapper,0x38)!=d3d || Field<void*>(context,0x20)!=surface)return false;
   auto view=Field<uint8_t*>(context,0x30);unsigned v=3;for(unsigned i=0;i<3;++i)if(views[i] && view==views[i])v=i;
   if(v==3 || Field<void*>(context,0x28)!=Field<void*>(view,0x430) || Field<void*>(Field<void*>(context,0x28),0x8208)!=view)return false;
   auto batch=Field<uint8_t*>(context,0x1a68);if(!batch || batch+0x190!=cluster)return false;
   auto cache=Field<uint8_t*>(wrapper,0x28);auto shader=Field<void*>(cache,0x4f0);unsigned index=UINT_MAX;
   for(auto& s:shaders)if(s.pointer==shader){index=s.index;break;}
   out->context=reinterpret_cast<uintptr_t>(context);out->d3dContext=reinterpret_cast<uintptr_t>(d3d);out->batch=reinterpret_cast<uintptr_t>(batch);out->cluster=reinterpret_cast<uintptr_t>(cluster);out->shader=reinterpret_cast<uintptr_t>(shader);
   out->view=v;out->viewFrame=Field<uint32_t>(view,0x294);out->shaderIndex=index;out->pass=Field<uint32_t>(context,0x3c);out->subview=Field<uint32_t>(context,0x370);out->selectorReturn=unsigned(rva);
   if(!cache || Field<void*>(cache,0x38)!=d3d)return false;
   out->cache=reinterpret_cast<uintptr_t>(cache);out->cb2=Field<uintptr_t>(cache,0x7f8);out->cb8=Field<uintptr_t>(cache,0x828);out->csDirty=Field<uint32_t>(cache,0x89c);
   memcpy(out->misc,cluster+0x270,sizeof(out->misc));memcpy(out->passConstants,context+0x400,sizeof(out->passConstants));memcpy(out->contextCamera,context+0x1e0,sizeof(out->contextCamera));
   memcpy(out->flags,cluster+0x4a0,sizeof(out->flags));memcpy(out->matrix,context+0x1e0,sizeof(out->matrix));out->history=Field<uintptr_t>(cluster,0x4b8);
   if(out->history){auto h=reinterpret_cast<uint8_t*>(out->history);memcpy(out->historyInfo,h+0x20,sizeof(out->historyInfo));out->historySrv=Field<uintptr_t>(h,0x60);out->historyCode=Field<uintptr_t>(h,0);}
   auto options=Field<uint8_t*>(surface,0x50);out->options88=Field<uint8_t>(options,0x88);out->globalDisable=Field<uint8_t>(image,0x524cf3c);out->configIndex=Field<uint32_t>(image,0x524c9f8);
   if(out->configIndex>=256)return false;auto configs=Field<void**>(options,0x6f8);auto config=configs[out->configIndex];out->config=reinterpret_cast<uintptr_t>(config);memcpy(out->configFlags,static_cast<uint8_t*>(config)+0x20,sizeof(out->configFlags));
   return true;
  }
  DWORD64 module{};auto entry=RtlLookupFunctionEntry(c.Rip,&module,nullptr);auto previous=c.Rsp;
  if(entry){PVOID handler{};DWORD64 establisher{};RtlVirtualUnwind(UNW_FLAG_NHANDLER,module,c.Rip,entry,&c,&handler,&establisher,nullptr);}
  else{c.Rip=Field<DWORD64>(reinterpret_cast<void*>(c.Rsp),0);c.Rsp+=8;}
  if(c.Rsp<=previous)return false;
 }
 return false;
 }__except(EXCEPTION_EXECUTE_HANDLER){++errors;return false;}}
bool Initialize(uint8_t* base,void* s,void* source,const std::filesystem::path& output,FILE* log){
 image=base;surface=s;views[0]=source;directory=output;report=log;
 if(!Guard()){fprintf(report,"REFUSED occlusion native code signature\n");fflush(report);return false;}
 if(!ShaderTable()){fprintf(report,"REFUSED occlusion registered shader metadata\n");fflush(report);return false;}
 fprintf(report,"Occlusion shader registration validated total=%u fixedPrefix=%zu; all14 relevant index/code/length/header identities verified\n",unsigned(Field<uint16_t>(image+0x524e030,0x4c)),shaders.size());
 fprintf(report,"Occlusion capture prepared: F8 only, bounded1024 records/4096 attempts; native cached CS metadata includes deferred recording, no deferred Get calls; GPU combined constants46 event queries on F8 only\n");fflush(report);return true;
}
bool InitializeGpuConstants40(ID3D11Device* device,ID3D11DeviceContext* immediate){
 if(!gpuPool.Initialize(device,immediate))return false;
 fprintf(report,"GPU combined constants46 initialized on game device: bounded48 dispatches/96 prefix copies (CS8/10/17/19/87/88 main passes0/2/4; CB8=560 plus retained CB2=608 byte prefix; geometry8/10 consumeCB2/CB8; instance shaders consumeCB8 with CB2 incidental); native cache CS stage5 verified, no deferred Get/Map/GetData, no new driver hooks; readback immediate/nonblocking after eyes finish\n");fflush(report);return true;
}
void SetEyes(void* left,void* right){views[1]=left;views[2]=right;}
void Arm(uint64_t id,uint64_t pose,uint32_t frame,const std::filesystem::path& path){
 Lock lock;if(active)return;sequence=id;tracking=pose;sourceFrame=frame;capturePath=path;recordCount=attempts=missed=irrelevant=errors=overflows=0;gpuErrors=0;gpuCaptureAvailable=gpuPool.device && gpuPool.Reset();active=true;
}
void Observe(ID3D11DeviceContext* c,unsigned kind,UINT x,UINT y,UINT z){
 if(!active)return;Lock lock;if(!active)return;if(attempts>=AttemptLimit){++overflows;return;}++attempts;
 Record r{};if(!Recover(c,&r)){++missed;return;}if(!Relevant(r.shaderIndex)){++irrelevant;return;}
 if(recordCount>=RecordLimit){++overflows;return;}r.kind=kind;r.dimensions[0]=x;r.dimensions[1]=y;r.dimensions[2]=z;r.contextType=unsigned(c->GetType());if(gpuCaptureAvailable && (r.pass==0 || r.pass==2 || r.pass==4) && (r.shaderIndex==8 || r.shaderIndex==10 || r.shaderIndex==17 || r.shaderIndex==19 || r.shaderIndex==87 || r.shaderIndex==88)){
  if(r.csDirty){++gpuErrors;}else{r.gpuCopy=gpuPool.Capture(c,reinterpret_cast<ID3D11Buffer*>(r.cb2),reinterpret_cast<ID3D11Buffer*>(r.cb8),recordCount);if(r.gpuCopy<0)++gpuErrors;}
 }
 records[recordCount++]=r;
}
void Hex(FILE* f,const uint8_t* data,size_t count){fputc('"',f);for(size_t i=0;i<count;++i)fprintf(f,"%02x",data[i]);fputc('"',f);}
void Finish(){
 Lock lock;if(!active)return;active=false;std::error_code ec;std::filesystem::create_directories(capturePath,ec);if(ec){fprintf(report,"Occlusion output directory error\n");fflush(report);return;}
 FILE* f=_wfsopen((capturePath/L"occlusion-dispatches.jsonl").c_str(),L"w",_SH_DENYNO);if(!f){fprintf(report,"Occlusion output file error\n");fflush(report);return;}
 fprintf(f,"{\"event\":\"begin\",\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"shaderEvidence\":\"native binding cache before Dispatch; deferred records are recording metadata, not execution timing\",\"recordLimit\":%u,\"attemptLimit\":%u}\n",sequence,tracking,sourceFrame,RecordLimit,AttemptLimit);
 for(unsigned i=0;i<recordCount;++i){auto& r=records[i];
  fprintf(f,"{\"event\":\"dispatch\",\"id\":%u,\"view\":%u,\"viewFrame\":%u,\"shaderIndex\":%u,\"kind\":%u,\"dimensions\":[%u,%u,%u],\"contextType\":%u,\"pass\":%u,\"subview\":%u,\"context\":\"%llx\",\"d3dContext\":\"%llx\",\"batch\":\"%llx\",\"cluster\":\"%llx\",\"nativeCachedShader\":\"%llx\",\"history\":\"%llx\",\"historySrv\":\"%llx\",\"historyFirstPointer\":\"%llx\",\"selectorReturnRva\":%u,\"configIndex\":%u,\"config\":\"%llx\",\"options88\":%u,\"globalDisable\":%u,\"clusterFlags\":",i,r.view,r.viewFrame,r.shaderIndex,r.kind,r.dimensions[0],r.dimensions[1],r.dimensions[2],r.contextType,r.pass,r.subview,r.context,r.d3dContext,r.batch,r.cluster,r.shader,r.history,r.historySrv,r.historyCode,r.selectorReturn,r.configIndex,r.config,r.options88,r.globalDisable);
  Hex(f,r.flags,sizeof(r.flags));fprintf(f,",\"configFlags\":");Hex(f,r.configFlags,sizeof(r.configFlags));fprintf(f,",\"matrix1e0\":");Hex(f,r.matrix,sizeof(r.matrix));fprintf(f,",\"miscCulling\":");Hex(f,r.misc,sizeof(r.misc));fprintf(f,",\"passConstants\":");Hex(f,r.passConstants,sizeof(r.passConstants));fprintf(f,",\"contextCamera\":");Hex(f,r.contextCamera,sizeof(r.contextCamera));fprintf(f,",\"history20to70\":");Hex(f,r.historyInfo,sizeof(r.historyInfo));fprintf(f,",\"stackRvas\":[");for(unsigned j=0;j<r.stackCount;++j)fprintf(f,"%s%u",j?",":"",r.stack[j]);fprintf(f,"]}\n");
 }
 fprintf(f,"{\"event\":\"end\",\"records\":%u,\"attempts\":%u,\"noVerifiedScheduleFrame\":%u,\"irrelevantShader\":%u,\"errors\":%u,\"overflows\":%u}\n",recordCount,attempts,missed,irrelevant,errors,overflows);fclose(f);
 FILE* g=_wfsopen((capturePath/L"gpu-constants.jsonl").c_str(),L"w",_SH_DENYNO);unsigned gpuRead=0,gpuPending=0;
 if(g){fprintf(g,"{\"event\":\"begin\",\"sequence\":%llu,\"available\":%s,\"copyLimit\":48,\"evidence\":\"GPU copies plus unique execution token recorded on native context before Dispatch; event/token verification and Map only on game immediate context\"}\n",sequence,gpuCaptureAvailable?"true":"false");
  for(unsigned n=0;n<recordCount;++n){auto& r=records[n];if(r.gpuCopy<0)continue;
   std::array<unsigned char,GeometryConstants46::PassBytes> pass{};std::array<unsigned char,GeometryConstants46::MiscBytes> misc{};
   auto hr=gpuPool.Read(unsigned(r.gpuCopy),&pass,&misc);bool files=false;
   if(hr==S_OK){files=true;const struct{unsigned slot;const void* data;size_t size;} blobs[]={{2,pass.data(),pass.size()},{8,misc.data(),misc.size()}};
    for(auto& b:blobs){auto file=capturePath/(L"dispatch-"+std::to_wstring(n)+L"-cs-cb-"+std::to_wstring(b.slot)+L".bin");FILE* out=_wfsopen(file.c_str(),L"wb",_SH_DENYNO);if(out){files=(fwrite(b.data,b.size,1,out)==1)&&files;fclose(out);}else files=false;}
    if(files)++gpuRead;else ++gpuErrors;
   }else ++gpuPending;
   auto& item=gpuPool.items[unsigned(r.gpuCopy)];
   fprintf(g,"{\"event\":\"copy\",\"dispatch\":%u,\"view\":%u,\"pass\":%u,\"subview\":%u,\"shaderIndex\":%u,\"cache\":\"%llx\",\"buffers\":[\"%llx\",\"%llx\"],\"sourceBytes\":[%u,%u],\"csDirty\":%u,\"HRESULT\":%ld,\"filesOK\":%s}\n",n,r.view,r.pass,r.subview,r.shaderIndex,r.cache,r.cb2,r.cb8,item.sourceBytes[0],item.sourceBytes[1],r.csDirty,hr,files?"true":"false");
  }
  fprintf(g,"{\"event\":\"end\",\"recorded\":%u,\"read\":%u,\"notReady\":%u,\"errors\":%u}\n",gpuPool.count,gpuRead,gpuPending,gpuErrors);fclose(g);
 }else ++gpuErrors;
 fprintf(report,"F8 GPU combined constants46 interval=%llu recorded=%u read=%u notReady=%u errors=%u\n",sequence,gpuPool.count,gpuRead,gpuPending,gpuErrors);fflush(report);
 fprintf(report,"F8 occlusion capture interval=%llu records=%u attempts=%u noSchedule=%u otherShaders=%u errors=%u overflows=%u\n",sequence,recordCount,attempts,missed,irrelevant,errors,overflows);fflush(report);
}
void Stop(){Lock lock;active=false;}

#ifndef ACUNITYVR_OCCLUSION_CHECK
using Scene=void(__fastcall*)(void*);using Post=void(__fastcall*)(void*);
using Dispatch=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);
using Indirect=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,UINT);
Scene baselineScene{};Post baselinePost{};Dispatch baselineDispatch{};Indirect baselineIndirect{};
void** sceneSlot{},**postSlot{};void* sceneGraph{};void* sourceGraph{};void* hookTargets[2]{};uint8_t hookCode[2][16]{};
std::atomic<bool> stopRequested{},stopped{};unsigned created{};bool f8Down{};uint64_t baselineSequence{};
void __fastcall BaselineScene(void* graph);
void __fastcall BaselinePost(void* graph);
void STDMETHODCALLTYPE HookDispatch(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){Observe(c,41,x,y,z);baselineDispatch(c,x,y,z);}
void STDMETHODCALLTYPE HookIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT offset){Observe(c,42,offset,0,0);baselineIndirect(c,b,offset);}
void Retire(){
 Stop();RestoreWorkerHooks();if(sceneSlot)InterlockedCompareExchangePointer(sceneSlot,reinterpret_cast<void*>(baselineScene),reinterpret_cast<void*>(&BaselineScene));
 if(postSlot)InterlockedCompareExchangePointer(postSlot,reinterpret_cast<void*>(baselinePost),reinterpret_cast<void*>(&BaselinePost));
 for(unsigned i=0;i<created;++i)MH_QueueDisableHook(hookTargets[i]);if(created)MH_ApplyQueued();
 if(report){for(unsigned i=0;i<created;++i)fprintf(report,"Baseline D3D retirement target=%p bytesRestored=%d\n",hookTargets[i],memcmp(hookTargets[i],hookCode[i],16)==0);
  fprintf(report,"Baseline retired sceneRestored=%d postRestored=%d; native camera/display/settings unchanged, resources/trampolines retained until game exit\n",sceneSlot && *sceneSlot==reinterpret_cast<void*>(baselineScene),postSlot && *postSlot==reinterpret_cast<void*>(baselinePost));fflush(report);}
 stopped=true;
}
void __fastcall BaselineScene(void* graph){
 baselineScene(graph);if(graph!=sceneGraph || stopRequested)return;
 bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0,pressed=down&&!f8Down;f8Down=down;
 if(pressed){auto frame=Field<uint32_t>(views[0],0x294);auto id=++baselineSequence;Arm(id,0,frame,directory/(L"baseline-"+std::to_wstring(id)));fprintf(report,"F8 normal-game occlusion capture armed id=%llu sourceFrame=%u\n",id,frame);fflush(report);}
}
void __fastcall BaselinePost(void* graph){baselinePost(graph);if(graph!=sourceGraph)return;Finish();if(stopRequested)Retire();}
bool BaselineIdentity(uint8_t* base,void** s,void** source,ID3D11DeviceContext** context){__try{
 *s=Field<void*>(base,0x523c6d8);auto surf=*s;if(!surf || Field<uint16_t>(surf,0x32)!=1 || Field<uint32_t>(surf,0x48)!=1)return false;
 *source=Field<void**>(surf,0x28)[0];if(!*source || Field<void*>(*source,0x248) || Field<void*>(*source,0x420)!=surf)return false;
 sourceGraph=Field<void*>(*source,0x440);auto world=Field<void*>(sourceGraph,0x10);sceneGraph=Field<void*>(world,0x1e8);
 sceneSlot=reinterpret_cast<void**>(static_cast<uint8_t*>(sceneGraph)+0xce0);postSlot=reinterpret_cast<void**>(static_cast<uint8_t*>(sourceGraph)+0x3b78);
 baselineScene=reinterpret_cast<Scene>(base+0x219f480);baselinePost=reinterpret_cast<Post>(base+0x21a8d90);
 if(*sceneSlot!=reinterpret_cast<void*>(baselineScene) || *postSlot!=reinterpret_cast<void*>(baselinePost))return false;
 auto renderer=Field<void*>(base,0x524cfd8);auto backend=Field<void*>(renderer,0x10);auto serial=static_cast<uint8_t*>(backend)+0x3980;auto wrapper=Field<void*>(serial,0);*context=Field<ID3D11DeviceContext*>(wrapper,0x30);return *context!=nullptr;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
Indirect workerIndirect{};void* workerTarget{};uint8_t workerCode[16]{};bool workerCreated{};
void STDMETHODCALLTYPE HookWorkerIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT offset){Observe(c,42,offset,0,0);
#ifdef ACUNITYVR_SHADER_HUNT78
 if(ShaderHunt::Skip(c,42,offset,0,"worker-compute-indirect",3,nullptr))return;
#endif
 workerIndirect(c,b,offset);}
bool WorkerTargets(ID3D11DeviceContext* immediateContext,void** indirectTarget,unsigned* count){__try{
 auto renderer=Field<void*>(image,0x524cfd8);auto backend=Field<uint8_t*>(renderer,0x10);
 auto n=Field<uint32_t>(backend,0x10);auto contexts=Field<void**>(backend,0x18);if(!n || n>64 || !contexts)return false;
 auto immediateVtable=*reinterpret_cast<void***>(immediateContext);void* target{};
 for(unsigned i=0;i<n;++i){
  auto wrapper=Field<void*>(contexts[i],0);if(Field<void*>(wrapper,0x30)!=immediateContext)return false;
  auto deferred=Field<ID3D11DeviceContext*>(wrapper,0x38);if(!deferred || deferred->GetType()!=D3D11_DEVICE_CONTEXT_DEFERRED)return false;
  auto table=*reinterpret_cast<void***>(deferred);if(table[41]!=immediateVtable[41])return false;
  if(i && table[42]!=target)return false;target=table[42];
 }
 *indirectTarget=target;*count=n;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool InstallWorkerHooks(ID3D11DeviceContext* immediateContext){
 void* target{};unsigned count{};if(!WorkerTargets(immediateContext,&target,&count))return false;
 auto table=*reinterpret_cast<void***>(immediateContext);if(target==table[42]){fprintf(report,"Occlusion coverage: %u native worker contexts share both existing dispatch methods\n",count);fflush(report);return true;}
 MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(target,&m,sizeof(m)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || !(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE)))return false;
 workerTarget=target;memcpy(workerCode,target,16);
 if(MH_CreateHook(target,reinterpret_cast<void*>(HookWorkerIndirect),reinterpret_cast<void**>(&workerIndirect))!=MH_OK)return false;workerCreated=true;
 if(MH_EnableHook(target)!=MH_OK){RestoreWorkerHooks();return false;}
 fprintf(report,"Occlusion coverage: %u native worker contexts verified; direct Dispatch shared, extra deferred DispatchIndirect target=%p, forwards once; F8 metadata only\n",count,target);fflush(report);return true;
}
void RestoreWorkerHooks(){
 if(!workerCreated)return;auto status=MH_DisableHook(workerTarget);workerCreated=false;
 if(report){fprintf(report,"Occlusion worker D3D retirement target=%p status=%d bytesRestored=%d\n",workerTarget,int(status),memcmp(workerTarget,workerCode,16)==0);fflush(report);}
}
DWORD Baseline(uint8_t* base,const std::filesystem::path& output,FILE* log){
 void* s{};void* source{};ID3D11DeviceContext* context{};
 if(!BaselineIdentity(base,&s,&source,&context)){fprintf(log,"REFUSED baseline native identity/layout\n");fflush(log);return 20;}
 if(!Initialize(base,s,source,output,log))return 20;
 if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){fprintf(log,"REFUSED baseline immediate context type\n");fflush(log);return 20;}
 auto status=MH_Initialize();if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED)return 21;
 if(!InstallWorkerHooks(context)){Retire();return 27;}
 auto vt=*reinterpret_cast<void***>(context);unsigned slots[]{41,42};void* hooks[]{reinterpret_cast<void*>(HookDispatch),reinterpret_cast<void*>(HookIndirect)};void** originals[]{reinterpret_cast<void**>(&baselineDispatch),reinterpret_cast<void**>(&baselineIndirect)};
 for(unsigned i=0;i<2;++i){hookTargets[i]=vt[slots[i]];MEMORY_BASIC_INFORMATION m{};
  if(!VirtualQuery(hookTargets[i],&m,sizeof(m)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll")){Retire();return 22;}
  memcpy(hookCode[i],hookTargets[i],16);if(MH_CreateHook(hookTargets[i],hooks[i],originals[i])!=MH_OK){Retire();return 23;}++created;
 }
 for(unsigned i=0;i<created;++i)if(MH_QueueEnableHook(hookTargets[i])!=MH_OK){Retire();return 24;}
 if(MH_ApplyQueued()!=MH_OK){Retire();return 25;}
 if(InterlockedCompareExchangePointer(postSlot,reinterpret_cast<void*>(BaselinePost),reinterpret_cast<void*>(baselinePost))!=reinterpret_cast<void*>(baselinePost) || InterlockedCompareExchangePointer(sceneSlot,reinterpret_cast<void*>(BaselineScene),reinterpret_cast<void*>(baselineScene))!=reinterpret_cast<void*>(baselineScene)){Retire();return 26;}
 fprintf(log,"NORMAL GAME baseline active; original native render functions preserved. No XR/session/owned eyes/window resize/settings edits. F8 captures one native frame; F10 stops manually. Two shared D3D method hooks plus one deferred indirect hook when distinct; no native executable patch.\n");fflush(log);
 while(!stopped){if(GetAsyncKeyState(VK_F10)&0x8000)stopRequested=true;Sleep(25);}return 0;
}
#endif
}
