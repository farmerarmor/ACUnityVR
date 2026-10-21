// F8-only read-only native geometry/effect recording attribution.
#include "EffectDrawCapture74.h"
#include "CrowdDraw82.h"
#include "BulkDraw83.h"
#include "ShaderHunt.h"
#include "VolumetricFog79.h"
#include "FogDraw79.h"
#include "ShaderHuntBindings78.h"
#include "EffectInputs72.h"
#include "EffectContexts74.h"
#include "EffectBackend74.h"
#include "EffectDispatch75.h"
#include "MaterialPrograms75.h"
#include "MaterialFamilies75.h"
#include "MaterialIndirect75.h"
#include "MaterialDraw75.h"
#include "SmokeDepth75.h"
#include "SourceRendering.h"
#include "EffectSampling74.h"
#include "EffectConstants72.h"
#include <MinHook.h>
#include <atomic>
#include <vector>
#include <share.h>
#include <algorithm>
namespace EffectDrawCapture74 {
namespace {
using acu::Field;
struct Lock{Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}static SRWLOCK mutex;};SRWLOCK Lock::mutex=SRWLOCK_INIT;
std::atomic<bool> active{};bool captureEnabled{};uint8_t* image{};void* surface{};FILE* report{};
std::vector<EffectInputs72::Worker> workers;std::array<uint64_t,2> eyes{};
std::filesystem::path capturePath;uint64_t sequence{},tracking{};uint32_t sourceFrame{};
uint64_t backendNative{},serialD3D{},serialCache{};unsigned attempts{},missed{},errors{},overflows{},genericCopies[2][2]{},eyeCopies[2]{},pathRecords[2][2]{},replaySkipped{},directShaderMismatch{},directCalls{},serialIdentityRefused{},directForeignView{},inputIdentityRefused{};uint64_t calls{};constexpr unsigned Limit=EffectSampling74::RecordLimit;EffectSampling74::State sampling;
struct Shader{uint64_t pointer{};unsigned stage{},index{};const uint8_t* code{};size_t bytes{};};std::vector<Shader> shaders;
struct Record{EffectInputs72::Snapshot input;unsigned kind{},a{},b{},shaderIndex[3]{UINT_MAX,UINT_MAX,UINT_MAX};int gpu{-1};unsigned path{},directConstantMask{},materialGroup{};int smokeDepth{-1};unsigned gpuSlots[6]{2,4,6,2,4,6};};std::vector<Record> records;
EffectConstants72::Pool pool;bool gpuAvailable{};Microsoft::WRL::ComPtr<ID3D11Device> gameDevice;unsigned materialCopies[2][MaterialFamilies75::Groups+1]{},ordinaryCopies[2]{};
using Indirect=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,UINT);
Indirect nativeIndirect[2]{};void* targets[2]{};bool created[2]{};
SmokeDepth75::Mirror depthBindings;SmokeDepth75::Pool smokeDepth;bool depthAvailable{};
using Resources=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView*const*);
Resources nativeResources[2]{};void* resourceTargets[2]{};bool resourceCreated[2]{};
template<unsigned I>void STDMETHODCALLTYPE PSResources(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11ShaderResourceView*const* resources){
 nativeResources[I](c,start,count,resources);if(!active || start>8 || count<=8-start)return;Lock lock;if(active)depthBindings.Set(c,start,count,resources);
}

bool Read(void* out,uint64_t at,size_t size)noexcept{__try{if(!at)return false;memcpy(out,reinterpret_cast<const void*>(at),size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Guard(){const struct{size_t rva;const char* bytes;size_t size;} guards[]={
  {0x22f2c0f,"\x48\x8b\x49\x38\x48\x8b\x93\xe8\x04\x00\x00\x45\x33\xc9\x48\x8b\x01\x45\x33\xc0\xff\x50\x58",23},
  {0x22f2c49,"\x48\x8b\x4b\x38\x48\x8b\x93\xe0\x04\x00\x00\x45\x33\xc9\x48\x8b\x01\x45\x33\xc0\xff\x50\x48",23},
  {0x22f2cd2,"\x48\x8b\x4b\x38\x48\x8b\x93\x08\x05\x00\x00\x45\x33\xc9\x48\x8b\x01\x45\x33\xc0\xff\x90\xb8\x00\x00\x00\x8b\x83\x10\x05\x00\x00",32},
  {0x22f23f3,"\x49\x8d\x81\xb7\x00\x00\x00\x48\x6b\xd2\x0e\x48\x03\xd0\x48\x8d\x04\xd6\x48\x8b\x56\x38\x48\x89\x44\x24\x28\x44\x89\x44\x24\x20\x44\x8b\xc7",35},
  {0x22f7756,"\x48\x8b\x4b\x38\x48\x8b\x56\x28\x48\x8b\x01\x44\x8b\xc7\x48\x8b\x5c\x24\x30\x48\x8b\x74\x24\x38\x48\x83\xc4\x20\x5f\x48\xff\xa0\x40\x01\x00",35},
  {0x22f7660,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x30\x48\x8b\xd9\x48\x8d\x0d\xcf\x32\xdb\x00\x0f\xb6\xfa\x48\x8b\x43\x28",32},
  {0x22f7890,"\x3a\x41\x0f\xb6\xd8\x48\x8b\x01\x45\x33\xc0\x48\x8b\xd7\xff\x90\xd0\x01\x00\x00\x84\xdb\x74\x09\x48\x8b\x07\x48\x8b\xcf\xff\x50",32},
 };for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;return true;}
bool Code(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.AllocationBase==GetModuleHandleW(L"d3d11.dll") && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE));}
void STDMETHODCALLTYPE IndexedIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT offset){Observe(c,39,offset,0);if(!ShaderHunt::Skip(c,39,offset,0,"worker-indirect",3,nullptr) && !Isolate(c,39,offset,0,false,b))nativeIndirect[0](c,b,offset);}
void STDMETHODCALLTYPE DrawIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT offset){Observe(c,40,offset,0);if(!ShaderHunt::Skip(c,40,offset,0,"worker-indirect",3,nullptr))nativeIndirect[1](c,b,offset);}
bool WorkersAttempt(ID3D11DeviceContext* immediate){workers.clear();targets[0]=targets[1]=nullptr;resourceTargets[0]=resourceTargets[1]=nullptr;__try{
 auto renderer=Field<void*>(image,0x524cfd8);auto backend=Field<uint8_t*>(renderer,0x10);
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 backendNative=reinterpret_cast<uintptr_t>(backend);serialD3D=reinterpret_cast<uintptr_t>(immediate);EffectInputs72::Worker serial;if(!EffectContexts74::Serial(Read,backendNative,reinterpret_cast<uintptr_t>(image+0x30a86f0),reinterpret_cast<uintptr_t>(image+0x30aa940),serialD3D,reinterpret_cast<uintptr_t>(surface),serial))return false;
 auto count=Field<uint32_t>(backend,0x10);auto list=Field<void**>(backend,0x18);if(!count || count>64 || !list)return false;
 EffectDispatch75::Snapshot stable;if(!EffectDispatch75::Read(Read,reinterpret_cast<uint64_t>(immediate),stable) || !Code(reinterpret_cast<void*>(stable.entries[8])))return false;resourceTargets[0]=reinterpret_cast<void*>(stable.entries[8]);
 std::array<EffectDispatch75::Snapshot,64> dispatch{};
 for(unsigned i=0;i<count;++i){auto context=static_cast<uint8_t*>(list[i]);if(!context)return false;auto wrapper=Field<uint8_t*>(context,0);if(!wrapper || Field<void*>(wrapper,0x30)!=immediate)return false;
  auto cache=Field<uint8_t*>(wrapper,0x28);auto d3d=Field<ID3D11DeviceContext*>(wrapper,0x38);
  if(!cache || !d3d || Field<void*>(cache,0)!=image+0x30aa940 || Field<void*>(cache,0x30)!=wrapper || Field<void*>(cache,0x38)!=d3d)return false;
  EffectDispatch75::Snapshot methods;if(!EffectDispatch75::Read(Read,reinterpret_cast<uint64_t>(d3d),methods) || !EffectDispatch75::SharedDrawMethods(methods,stable))return false;
  for(unsigned slot:{12u,13u,20u,21u,38u})if(!Code(reinterpret_cast<void*>(methods.entries[slot])))return false;
  if(!Code(reinterpret_cast<void*>(methods.entries[8])) || (i && reinterpret_cast<void*>(methods.entries[8])!=resourceTargets[1]))return false;resourceTargets[1]=reinterpret_cast<void*>(methods.entries[8]);
  for(unsigned j=0;j<2;++j){auto fn=reinterpret_cast<void*>(methods.entries[39+j]);if(!Code(fn) || (i && fn!=targets[j]))return false;targets[j]=fn;}dispatch[i]=methods;
  workers.push_back({reinterpret_cast<uintptr_t>(context),reinterpret_cast<uintptr_t>(wrapper),reinterpret_cast<uintptr_t>(cache),reinterpret_cast<uintptr_t>(d3d)});
 }
 if(!EffectDispatch75::Unchanged(Read,stable) || Field<uint32_t>(backend,0x10)!=count || Field<void**>(backend,0x18)!=list)return false;
 for(unsigned i=0;i<count;++i){const auto& w=workers[i];if(reinterpret_cast<uintptr_t>(list[i])!=w.context || Field<uintptr_t>(reinterpret_cast<void*>(w.context),0)!=w.wrapper || Field<uintptr_t>(reinterpret_cast<void*>(w.wrapper),0x28)!=w.cache || Field<uintptr_t>(reinterpret_cast<void*>(w.wrapper),0x30)!=reinterpret_cast<uintptr_t>(immediate) || Field<uintptr_t>(reinterpret_cast<void*>(w.wrapper),0x38)!=w.d3d || Field<uintptr_t>(reinterpret_cast<void*>(w.cache),0)!=reinterpret_cast<uintptr_t>(image+0x30aa940) || Field<uintptr_t>(reinterpret_cast<void*>(w.cache),0x30)!=w.wrapper || Field<uintptr_t>(reinterpret_cast<void*>(w.cache),0x38)!=w.d3d || !EffectDispatch75::Unchanged(Read,dispatch[i]))return false;}
 EffectInputs72::Worker after;if(!EffectContexts74::Serial(Read,backendNative,reinterpret_cast<uintptr_t>(image+0x30a86f0),reinterpret_cast<uintptr_t>(image+0x30aa940),serialD3D,reinterpret_cast<uintptr_t>(surface),after) || after.context!=serial.context || after.wrapper!=serial.wrapper || after.cache!=serial.cache)return false;
 serialCache=serial.cache;return workers.size()==count;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Workers(ID3D11DeviceContext* immediate){
 for(unsigned attempt=1;attempt<=32;++attempt){if(WorkersAttempt(immediate)){fprintf(report,"Effect74 stable worker dispatch snapshot passed after %u attempt(s)\n",attempt);return true;}Sleep(1);}
 workers.clear();targets[0]=targets[1]=nullptr;fprintf(report,"REFUSED effect74 worker snapshot after32 attempts; original ownership/method checks retained\n");return false;
}
bool Shaders(){__try{
 const unsigned offsets[]{8,0x14,0x20},counts[]{0x10,0x1c,0x28};auto manager=image+0x524e030;
 for(unsigned stage=0;stage<3;++stage){auto n=Field<uint16_t>(manager,counts[stage]);auto table=Field<uint8_t**>(manager,offsets[stage]);if(!table || !n || n>4096)return false;
  for(unsigned i=0;i<n;++i){auto e=table[i];if(!e)continue;auto pointer=Field<uintptr_t>(e,16);auto code=Field<uint8_t*>(e,0);auto size=Field<uint64_t>(e,8);auto at=reinterpret_cast<uintptr_t>(code),base=reinterpret_cast<uintptr_t>(image);
   if(!pointer)continue;if(size<32 || size>1024*1024 || at<base || at-base>0x7825000-size || memcmp(code,"DXBC",4) || Field<uint32_t>(code,24)!=size)return false;
   shaders.push_back({pointer,stage,i,code,size});
  }
 }
 return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Write(const std::filesystem::path& p,const void* data,size_t size){FILE* f=_wfsopen(p.c_str(),L"wb",_SH_DENYNO);if(!f)return false;bool ok=!size || fwrite(data,size,1,f)==1;return fclose(f)==0 && ok;}
}
bool Install(uint8_t* base,void* target,ID3D11Device* device,ID3D11DeviceContext* immediate,FILE* log,const std::filesystem::path& directory){
 image=base;surface=target;report=log;captureEnabled=GetPrivateProfileIntW(L"Diagnostics",L"CaptureEffectInputs",0,(directory/L"ACUnityVR.ini").c_str())!=0;if(!Guard()){fprintf(report,"REFUSED effect74 native code identity\n");return false;}if(!Workers(immediate))return false;if(captureEnabled && !Shaders()){fprintf(report,"REFUSED effect74 fixed shader identity\n");return false;}
 // No deferred context Get-state calls: ownership/type are checked once on COM objects.
 for(auto& w:workers){auto c=reinterpret_cast<ID3D11DeviceContext*>(w.d3d);Microsoft::WRL::ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(c->GetType()!=D3D11_DEVICE_CONTEXT_DEFERRED || owner.Get()!=device)return false;}
 if(!MaterialPrograms75::Initialize(device,directory,log) || !CrowdDraw82::Initialize(base,device,directory,log) || (captureEnabled && !pool.Initialize(device,immediate)))return false;gameDevice=device;
 if(captureEnabled){if(!smokeDepth.Initialize(device,immediate) || !depthBindings.Add(immediate))return false;for(auto& w:workers)if(!depthBindings.Add(reinterpret_cast<ID3D11DeviceContext*>(w.d3d)))return false;
 void* resourceHooks[]{reinterpret_cast<void*>(PSResources<0>),reinterpret_cast<void*>(PSResources<1>)};
 for(unsigned i=0;i<2;++i){if(i && resourceTargets[i]==resourceTargets[0])continue;if(MH_CreateHook(resourceTargets[i],resourceHooks[i],reinterpret_cast<void**>(&nativeResources[i]))!=MH_OK){Stop();return false;}resourceCreated[i]=true;if(MH_EnableHook(resourceTargets[i])!=MH_OK){Stop();return false;}}
 }
 auto table=*reinterpret_cast<void***>(immediate);void* hooks[]{reinterpret_cast<void*>(IndexedIndirect),reinterpret_cast<void*>(DrawIndirect)};
 for(unsigned i=0;i<2;++i)if(targets[i]!=table[39+i]){if(MH_CreateHook(targets[i],hooks[i],reinterpret_cast<void**>(&nativeIndirect[i]))!=MH_OK){Stop();return false;}created[i]=true;if(MH_EnableHook(targets[i])!=MH_OK){Stop();return false;}}
 if(!captureEnabled){fprintf(report,"Effect74 heavy material/depth capture OFF (Diagnostics.CaptureEffectInputs=0); no PS-resource capture hooks, prefix/depth pools or input snapshots; accepted owned-eye removal remains active\n");fflush(report);return true;}
 records.reserve(Limit);fprintf(report,"Effect74 F8 recording capture prepared: workers=%zu plus native serial direct context (native or verified source-render private backend table); registeredShaders=%zu,2048 snapshots,192 worker/64 direct shader/pass families per eye,4 spaced samples per family,48 GPU prefix samples, replay excluded; deferred39/40 coverage; verified tree removal follows INI; F8 smoke PS8 setter-resource copies capped at four total with execution tokens, no deferred Get-state calls\n",workers.size(),shaders.size());fflush(report);return true;
}
bool HuntBindings78(ID3D11DeviceContext* c,uint64_t shader[6],void** nativeContext,unsigned* view,uint64_t* cache){
 if(!ShaderHunt::Enabled() || !shader || !nativeContext || !view || !cache)return false;Lock lock;
 const EffectInputs72::Worker* worker{};for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){worker=&w;break;}if(!worker)return false;
 ShaderHuntBindings78::Snapshot input;if(!ShaderHuntBindings78::Read(Read,*worker,reinterpret_cast<uintptr_t>(surface),eyes,input))return false;
 memcpy(shader,input.shaders,sizeof(input.shaders));*nativeContext=reinterpret_cast<void*>(input.context);*view=input.view;*cache=input.cache;return true;
}
void SetEyes(void* left,void* right){Lock lock;eyes={reinterpret_cast<uintptr_t>(left),reinterpret_cast<uintptr_t>(right)};}
void Arm(uint64_t id,uint64_t pose,uint32_t frame,const std::filesystem::path& path){CrowdDraw82::Arm(id,pose,frame,path);BulkDraw83::ArmSelection(id,pose,frame,path);if(!captureEnabled)return;Lock lock;if(active)return;sequence=id;tracking=pose;sourceFrame=frame;capturePath=path;attempts=missed=errors=overflows=0;memset(genericCopies,0,sizeof(genericCopies));memset(eyeCopies,0,sizeof(eyeCopies));memset(materialCopies,0,sizeof(materialCopies));memset(ordinaryCopies,0,sizeof(ordinaryCopies));calls=0;replaySkipped=directShaderMismatch=directCalls=serialIdentityRefused=directForeignView=inputIdentityRefused=0;memset(pathRecords,0,sizeof(pathRecords));sampling.Reset();records.clear();depthBindings.Reset();gpuAvailable=pool.Reset();depthAvailable=smokeDepth.Reset();active=true;}
void Poll(uint32_t mode){MaterialPrograms75::Poll(mode);VolumetricFog79::Poll(mode);}
bool RemoveFog79(ID3D11DeviceContext* c,unsigned kind,UINT vertices,UINT start,bool replay){
 if(!VolumetricFog79::Enabled() || replay || kind!=13 || vertices!=6 || start)return false;
 const bool direct=reinterpret_cast<uintptr_t>(c)==serialD3D;uint64_t cache=direct?serialCache:0;
 if(!direct)for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){cache=w.cache;break;}
 uint64_t vsKey{},psKey{};if(!cache || !Read(&vsKey,cache+0x4e8,8) || !Read(&psKey,cache+0x4e0,8) || !VolumetricFog79::Matches(vsKey,psKey))return false;
 Lock lock;const EffectInputs72::Worker* worker{};EffectInputs72::Worker serial;
 if(direct){if(!EffectBackend74::Serial(Read,SourceRendering::OwnsBackend,backendNative,reinterpret_cast<uintptr_t>(image+0x30a86f0),reinterpret_cast<uintptr_t>(image+0x30aa940),serialD3D,reinterpret_cast<uintptr_t>(surface),serial))return false;worker=&serial;}
 else for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){worker=&w;break;}if(!worker)return false;
 MaterialDraw75::Identity input;if(!FogDraw79::Read(Read,*worker,reinterpret_cast<uintptr_t>(surface),eyes,kind,vertices,start,replay,input) || input.shaders[0]!=vsKey || input.shaders[1]!=psKey)return false;
 if(direct){Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);if(reinterpret_cast<uintptr_t>(vs.Get())!=vsKey || reinterpret_cast<uintptr_t>(ps.Get())!=psKey)return false;}
 if(!VolumetricFog79::Matches(vsKey,psKey))return false;VolumetricFog79::Record(input.eye,direct);return true;
}
bool Isolate(ID3D11DeviceContext* c,unsigned kind,UINT a,UINT b,bool replay,ID3D11Buffer* indirectArguments){
 if(!MaterialPrograms75::MaskEnabled() || replay || (kind!=20 && kind!=39) || (kind==20 && ((a!=6 && a!=384) || !b)))return false;
 const auto direct=reinterpret_cast<uintptr_t>(c)==serialD3D;uint64_t keyCache=direct?serialCache:0;
 if(!direct)for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){keyCache=w.cache;break;}
 uint64_t vsKey{},psKey{};if(!keyCache || !Read(&vsKey,keyCache+0x4e8,8) || !Read(&psKey,keyCache+0x4e0,8))return false;
 const auto pair=MaterialPrograms75::Pair(vsKey,psKey);if(!MaterialPrograms75::Hidden(pair))return false;
 Lock lock;const EffectInputs72::Worker* worker{};EffectInputs72::Worker serial;
 if(direct){if(!EffectBackend74::Serial(Read,SourceRendering::OwnsBackend,backendNative,reinterpret_cast<uintptr_t>(image+0x30a86f0),reinterpret_cast<uintptr_t>(image+0x30aa940),serialD3D,reinterpret_cast<uintptr_t>(surface),serial))return false;worker=&serial;}
 else for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){worker=&w;break;}
 if(!worker)return false;
 MaterialDraw75::Identity input;if(!MaterialDraw75::Read(Read,*worker,reinterpret_cast<uintptr_t>(surface),eyes,input) || input.shaders[0]!=vsKey || input.shaders[1]!=psKey)return false;
 const auto group=MaterialFamilies75::Group({kind,a,b,input.eye,replay},pair);if(!MaterialPrograms75::Hidden(group))return false;
 if(kind==39 && !MaterialIndirect75::Valid(indirectArguments,gameDevice.Get(),a))return false;
 if(direct){Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);if(reinterpret_cast<uintptr_t>(vs.Get())!=input.shaders[0] || reinterpret_cast<uintptr_t>(ps.Get())!=input.shaders[1])return false;}
 MaterialPrograms75::Count(group,input.eye);return true;
}
void Observe(ID3D11DeviceContext* c,unsigned kind,UINT a,UINT b,bool commandListReplay){
 if(kind==12 && !commandListReplay && CrowdDraw82::Armed())for(const auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){CrowdDraw82::Observe(w,reinterpret_cast<uintptr_t>(surface),eyes,kind,a,b,false);break;}
 if(!active || kind>=41)return;Lock lock;if(!active)return;++calls;
 const auto direct=reinterpret_cast<uintptr_t>(c)==serialD3D;
 if(direct && commandListReplay){++replaySkipped;return;}
 EffectInputs72::Worker serial;const EffectInputs72::Worker* worker{};
 if(direct){++directCalls;if(!EffectBackend74::Serial(Read,SourceRendering::OwnsBackend,backendNative,reinterpret_cast<uintptr_t>(image+0x30a86f0),reinterpret_cast<uintptr_t>(image+0x30aa940),serialD3D,reinterpret_cast<uintptr_t>(surface),serial)){++serialIdentityRefused;++missed;return;}worker=&serial;}
 else for(auto& w:workers)if(w.d3d==reinterpret_cast<uintptr_t>(c)){worker=&w;break;}
 if(!worker){++missed;return;}
 // Small read-only key first. Repeated geometry does not spend the snapshot budget.
 EffectSampling74::Key key;key.kind=kind;key.path=direct?1:0;uint64_t view{};
 if(!Read(&view,worker->context+0x30,8)){++missed;return;}key.eye=view==eyes[0]?1:view==eyes[1]?2:0;
 if(!key.eye && direct)++directForeignView;
 if(!key.eye || !Read(&key.pass,worker->context+0x3c,4) || !Read(&key.subview,worker->context+0x370,4)){++missed;return;}
 const unsigned offsets[]{0x4e8,0x4e0,0x508};for(unsigned stage=0;stage<3;++stage)if(!Read(&key.shaders[stage],worker->cache+offsets[stage],8)){++missed;return;}
 if(!sampling.Select(key))return;if(records.size()==Limit){++overflows;return;}++attempts;
 Record r;r.path=key.path;r.kind=kind;r.a=a;r.b=b;if(!EffectInputs72::Read(Read,*worker,reinterpret_cast<uintptr_t>(surface),eyes,r.input) || r.input.eye!=key.eye || r.input.pass!=key.pass || r.input.subview!=key.subview || r.input.shaders[0]!=key.shaders[0] || r.input.shaders[1]!=key.shaders[1] || r.input.shaders[2]!=key.shaders[2]){++inputIdentityRefused;++missed;return;}
 r.materialGroup=MaterialFamilies75::Group({kind,a,b,r.input.eye,commandListReplay},MaterialPrograms75::Pair(r.input.shaders[0],r.input.shaders[1]));
 if(r.materialGroup){const unsigned particles[]{2,4,8,2,3,8},trees[]{2,3,9,2,3,9};memcpy(r.gpuSlots,r.materialGroup>=6?trees:particles,sizeof(particles));}
 // Direct draws use actual D3D bindings; native caches can retain originals during accepted scoped fixes.
 Microsoft::WRL::ComPtr<ID3D11VertexShader> actualVS;Microsoft::WRL::ComPtr<ID3D11PixelShader> actualPS;Microsoft::WRL::ComPtr<ID3D11GeometryShader> actualGS;
 std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>,EffectConstants72::Slots> directBuffers;
 if(direct){c->VSGetShader(&actualVS,nullptr,nullptr);c->PSGetShader(&actualPS,nullptr,nullptr);c->GSGetShader(&actualGS,nullptr,nullptr);
  if(reinterpret_cast<uintptr_t>(actualVS.Get())!=r.input.shaders[0] || reinterpret_cast<uintptr_t>(actualPS.Get())!=r.input.shaders[1] || reinterpret_cast<uintptr_t>(actualGS.Get())!=r.input.shaders[2]){++directShaderMismatch;return;}
  for(unsigned stage=0;stage<2;++stage)for(unsigned slot=0;slot<3;++slot){auto n=stage*3+slot;auto target=directBuffers[n].GetAddressOf();if(stage==0)c->VSGetConstantBuffers(r.gpuSlots[n],1,target);else c->PSGetConstantBuffers(r.gpuSlots[n],1,target);if(reinterpret_cast<uintptr_t>(directBuffers[n].Get())!=r.input.buffers[stage][r.gpuSlots[n]])r.directConstantMask|=1u<<n;}
 }
 for(unsigned stage=0;stage<3;++stage)for(auto& s:shaders)if(s.stage==stage && s.pointer==r.input.shaders[stage]){r.shaderIndex[stage]=s.index;break;}
 bool sprite=(r.shaderIndex[0]>=141 && r.shaderIndex[0]<=148) || r.shaderIndex[0]==177 || r.shaderIndex[0]==181;
 unsigned copies{};for(auto& previous:records)if(previous.gpu>=0 && previous.input.eye==r.input.eye && previous.input.pass==r.input.pass && previous.input.shaders[0]==r.input.shaders[0] && previous.input.shaders[1]==r.input.shaders[1] && previous.kind==kind && previous.path==r.path)++copies;
 // Reserve24 GPU samples per eye: 7 worker, 7 direct generic, 10 sprite/particle families.
 // Generic samples use the native no-subview marker; cascades still get CPU records.
 const auto eye=r.input.eye-1;const bool reserved=sprite || r.materialGroup;
 if(gpuAvailable && MaterialFamilies75::GpuEligible(eyeCopies[eye],ordinaryCopies[eye],r.materialGroup,materialCopies[eye][r.materialGroup]) && (r.materialGroup || copies<(sprite?4u:1u)) && (reserved || (r.input.subview==UINT_MAX && genericCopies[eye][r.path]<7)) && (direct || (!r.input.shaderDirty && !r.input.bufferDirty[0] && !r.input.bufferDirty[1]))){
  std::array<ID3D11Buffer*,EffectConstants72::Slots> buffers{};for(unsigned stage=0;stage<2;++stage)for(unsigned slot=0;slot<3;++slot)buffers[stage*3+slot]=direct?directBuffers[stage*3+slot].Get():reinterpret_cast<ID3D11Buffer*>(r.input.buffers[stage][r.gpuSlots[stage*3+slot]]);
  if(depthAvailable && smokeDepth.Wants(r.input.eye,r.materialGroup))r.smokeDepth=smokeDepth.Capture(c,depthBindings.Find(c),unsigned(records.size()),r.input.eye,r.materialGroup);
  r.gpu=pool.Capture(c,buffers,unsigned(records.size()),r.materialGroup?1536u:1024u);if(r.gpu>=0){++eyeCopies[eye];if(r.materialGroup)++materialCopies[eye][r.materialGroup];else ++ordinaryCopies[eye];if(!reserved)++genericCopies[eye][r.path];}else if(r.gpu<-1)++errors;
 }
 ++pathRecords[eye][r.path];records.push_back(r);
}
void Finish(){CrowdDraw82::Finish();BulkDraw83::FinishSelection();
 Lock lock;if(!active.exchange(false))return;std::error_code ec;std::filesystem::create_directories(capturePath,ec);if(ec){++errors;return;}
 bool recordsOK=Write(capturePath/L"effect74-records.bin",records.data(),records.size()*sizeof(Record));
 FILE* f=_wfsopen((capturePath/L"effect74.jsonl").c_str(),L"w",_SH_DENYNO);if(!f){++errors;return;}
 fprintf(f,"{\"event\":\"begin\",\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"recordBytes\":%zu,\"snapshotBytes\":%zu,\"cameraOffset\":%zu,\"contextOffset\":%zu,\"cacheOffset\":%zu,\"batchOffset\":%zu,\"gpuAvailable\":%s,\"note\":\"Native worker and direct inputs, not final pixels; direct shaders checked against actual D3D, direct constants copied from actual bindings. Command replay excluded; no deferred Get-state calls. GPU prefix copies precede these draws and require matching execution token. Smoke PS8 setter-resource copies have separate bounded completion tokens.\"}\n",sequence,tracking,sourceFrame,sizeof(Record),sizeof(EffectInputs72::Snapshot),offsetof(EffectInputs72::Snapshot,camera),offsetof(EffectInputs72::Snapshot,gpu),offsetof(EffectInputs72::Snapshot,bindings),offsetof(EffectInputs72::Snapshot,batchData),gpuAvailable?"true":"false");
 unsigned gpuRead{},pending{},shaderSaved{};std::vector<uint64_t> saved;
 for(unsigned i=0;i<records.size();++i){auto& r=records[i];HRESULT hr=S_FALSE;bool files{};
  if(r.gpu>=0){std::array<std::array<uint8_t,EffectConstants72::Prefix>,EffectConstants72::Slots> constants{};hr=pool.Read(unsigned(r.gpu),constants);
   if(hr==S_OK){files=true;auto& item=pool.items[unsigned(r.gpu)];for(unsigned slot=0;slot<6;++slot)if(item.bytes[slot])files=Write(capturePath/(L"effect74-draw-"+std::to_wstring(i)+(slot<3?L"-vs-cb-":L"-ps-cb-")+std::to_wstring(r.gpuSlots[slot])+L".bin"),constants[slot].data(),item.bytes[slot]) && files;if(files)++gpuRead;else ++errors;}else ++pending;
  }
  fprintf(f,"{\"path\":\"%s\",\"directConstantMismatchMask\":%u,\"materialGroup\":%u,\"gpuSlots\":[%u,%u,%u,%u,%u,%u],",r.path?"direct":"worker",r.directConstantMask,r.materialGroup,r.gpuSlots[0],r.gpuSlots[1],r.gpuSlots[2],r.gpuSlots[3],r.gpuSlots[4],r.gpuSlots[5]);
  fprintf(f,"\"event\":\"draw\",\"id\":%u,\"eye\":%u,\"viewFrame\":%u,\"kind\":%u,\"args\":[%u,%u],\"pass\":%u,\"subview\":%u,\"context\":\"%llx\",\"view\":\"%llx\",\"batch\":\"%llx\",\"shaders\":[\"%llx\",\"%llx\",\"%llx\"],\"shaderIndices\":[%u,%u,%u],\"shaderDirty\":%u,\"bufferDirty\":[%u,%u,%u],\"optionalErrors\":%u,\"gpuCopy\":%d,\"gpuHRESULT\":%ld,\"gpuFilesOK\":%s}\n",i,r.input.eye,r.input.frame,r.kind,r.a,r.b,r.input.pass,r.input.subview,r.input.context,r.input.view,r.input.batch,r.input.shaders[0],r.input.shaders[1],r.input.shaders[2],r.shaderIndex[0],r.shaderIndex[1],r.shaderIndex[2],r.input.shaderDirty,r.input.bufferDirty[0],r.input.bufferDirty[1],r.input.bufferDirty[2],r.input.optionalErrors,r.gpu,hr,files?"true":"false");
  for(auto& s:shaders)if(s.stage<3 && s.pointer==r.input.shaders[s.stage] && std::find(saved.begin(),saved.end(),s.pointer)==saved.end()){if(Write(capturePath/(L"effect74-stage-"+std::to_wstring(s.stage)+L"-shader-"+std::to_wstring(s.index)+L".dxbc"),s.code,s.bytes))++shaderSaved;else ++errors;saved.push_back(s.pointer);}
 }
 unsigned depthRead{};for(unsigned i=0;i<(depthAvailable?smokeDepth.count:0);++i){auto& item=smokeDepth.items[i];std::vector<uint8_t> pixels;auto hr=smokeDepth.Read(i,pixels);bool ok=false;
  if(hr==S_OK){ok=Write(capturePath/(L"smoke-depth75-draw-"+std::to_wstring(item.draw)+L".r32f"),pixels.data(),pixels.size());if(ok)++depthRead;else ++errors;}else ++smokeDepth.pending;
  fprintf(f,"{\"event\":\"smokeDepth75\",\"id\":%u,\"draw\":%u,\"eye\":%u,\"materialGroup\":%u,\"view\":\"%llx\",\"resource\":\"%llx\",\"width\":%u,\"height\":%u,\"format\":%u,\"gpuHRESULT\":%ld,\"filesOK\":%s,\"note\":\"PS8 resource explicitly set during this F8 recording epoch. Copied before the consuming draw on the same context and completion-token verified. Deferred automatic hazard nulling is not proven absent.\"}\n",i,item.draw,item.eye,item.group,item.view,item.source,item.desc.Width,item.desc.Height,unsigned(item.desc.Format),hr,ok?"true":"false");
 }
 fprintf(f,"{\"event\":\"smokeDepthStatus75\",\"available\":%s,\"copies\":%u,\"read\":%u,\"unknownSetter\":%u,\"explicitNull\":%u,\"refused\":%u,\"pending\":%u}\n",depthAvailable?"true":"false",depthAvailable?smokeDepth.count:0,depthRead,smokeDepth.unknown,smokeDepth.nullBindings,smokeDepth.refused,smokeDepth.pending);
 fprintf(f,"{\"event\":\"directIdentity\",\"calls\":%u,\"serialRefused\":%u,\"foreignView\":%u,\"snapshotRefused\":%u}\n",directCalls,serialIdentityRefused,directForeignView,inputIdentityRefused);
 fprintf(f,"{\"event\":\"paths\",\"recordsPerEye\":[[%u,%u],[%u,%u]],\"commandReplaySkipped\":%u,\"directShaderMismatch\":%u}\n",pathRecords[0][0],pathRecords[0][1],pathRecords[1][0],pathRecords[1][1],replaySkipped,directShaderMismatch);
 fprintf(f,"{\"event\":\"sampling\",\"drawCalls\":%llu,\"familiesPerEye\":[%u,%u],\"samplesPerEye\":[%u,%u],\"duplicateDrawsFiltered\":%u,\"familyOverflows\":%u}\n",calls,sampling.eyeFamilies[0],sampling.eyeFamilies[1],sampling.eyeSamples[0],sampling.eyeSamples[1],sampling.filtered,sampling.overflows);
 fprintf(f,"{\"event\":\"end\",\"records\":%zu,\"attempts\":%u,\"unmatchedContextOrOwner\":%u,\"errors\":%u,\"overflows\":%u,\"recordsOK\":%s,\"gpuRead\":%u,\"gpuPending\":%u,\"shadersSaved\":%u}\n",records.size(),attempts,missed,errors,overflows,recordsOK?"true":"false",gpuRead,pending,shaderSaved);fclose(f);
 fprintf(report,"Effect74 F8 interval=%llu draws=%zu attempts=%u unmatched=%u errors=%u overflows=%u GPU=%u pending=%u shaderBlobs=%u recordsOK=%d\n",sequence,records.size(),attempts,missed,errors,overflows,gpuRead,pending,shaderSaved,recordsOK);fflush(report);
}
void Stop(){CrowdDraw82::Stop();Lock lock;active=false;VolumetricFog79::Stop();MaterialPrograms75::Stop();for(unsigned i=0;i<2;++i)if(resourceCreated[i]){MH_DisableHook(resourceTargets[i]);resourceCreated[i]=false;}for(unsigned i=0;i<2;++i)if(created[i]){MH_DisableHook(targets[i]);created[i]=false;}if(report){fprintf(report,"Effect74 capture retired; no renderer/effect state changed; private resources retained until game exit\n");fflush(report);}}
}
