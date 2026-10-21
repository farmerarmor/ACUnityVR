// Opt-in shader hunting at actual direct/worker draw and dispatch calls.
// Skipping work is the only rendering change; no shader or pipeline substitution.
#include "ShaderHunt.h"
#include "ShaderHuntControl.h"
#include "EffectDrawCapture74.h"
#include "MaterialPrograms75.h"
#ifdef ACUNITYVR_PORTABLE92
#include "ShaderRegistry92.h"
#endif
#ifdef ACUNITYVR_FOG79
#include "VolumetricFog79.h"
#endif
#include <d3d11_1.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <map>
#include <vector>
#include <fstream>
#include <share.h>
#include <cstring>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
namespace ShaderHunt { namespace {
enum Stage : unsigned {VS,PS,GS,HS,DS,CS,Stages};
const char* names[]{"VS","PS","GS","HS","DS","CS"};
constexpr unsigned FilterOrder[]{PS,CS,VS,GS,HS,DS,Stages};
constexpr size_t Limit=4096,CodeBudget=32*1024*1024;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}static SRWLOCK mutex;};
SRWLOCK Lock::mutex=SRWLOCK_INIT;
struct Code {ComPtr<IUnknown> pin;std::vector<uint8_t> bytes;std::string hash;unsigned nativeIndex=UINT_MAX;};
struct Candidate {ComPtr<IUnknown> pin;unsigned stage{},id{},paths{};uint64_t hits{},skips{};};
using Key=std::pair<unsigned,uintptr_t>;
std::map<Key,Code> codes;std::map<Key,size_t> lookup;std::vector<Candidate> candidates;std::vector<size_t> visible;
ShaderHuntControl::State control;std::atomic<bool> active{},acceptCode{},captureRequest{};std::atomic<unsigned> creationCalls{};
ID3D11Device* device{};ID3D11DeviceContext* immediate{};ComPtr<ID3D11DeviceContext1> immediate1;uint8_t* image{};
FILE* report{},*events{};std::filesystem::path directory,package,markDirectory;
uint64_t sequence{},tracking{},skipped{};uint32_t frame{};unsigned nextId{},markId{},markMask{},filterIndex{};bool oldFilter{};
size_t selected=ShaderHuntControl::None,marked=ShaderHuntControl::None,codeBytes{};
BCRYPT_ALG_HANDLE sha{};
using Create=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,IUnknown**);
std::array<Create,Stages> nativeCreate{};std::array<void*,Stages> targets{};std::array<bool,Stages> created{};
using CreateSO=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,const D3D11_SO_DECLARATION_ENTRY*,UINT,const UINT*,UINT,UINT,ID3D11ClassLinkage*,ID3D11GeometryShader**);
CreateSO nativeSO{};void* soTarget{};bool soCreated{};
std::string Field(const std::string& line,const char* key){auto marker=std::string("\"")+key+"\":\"";auto p=line.find(marker);if(p==std::string::npos)return {};p+=marker.size();auto end=line.find('"',p);return end==std::string::npos?std::string{}:line.substr(p,end-p);}
std::string Hash(const void* data,size_t size){if(!sha)return {};BCRYPT_HASH_HANDLE h{};DWORD length{},got{};std::array<UCHAR,32> digest{};
 if(BCryptGetProperty(sha,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&got,0)<0)return {};
 std::vector<UCHAR> object(length);if(BCryptCreateHash(sha,&h,object.data(),length,nullptr,0,0)<0)return {};
 bool ok=BCryptHashData(h,const_cast<PUCHAR>(static_cast<const UCHAR*>(data)),ULONG(size),0)>=0 && BCryptFinishHash(h,digest.data(),ULONG(digest.size()),0)>=0;BCryptDestroyHash(h);std::string hex;
 if(ok)for(auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return hex;}
bool Owned(IUnknown* p) noexcept {__try{if(!p)return false;auto table=*reinterpret_cast<void***>(p);MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(table,&m,sizeof(m)) || m.State!=MEM_COMMIT || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 ID3D11Device* owner{};reinterpret_cast<ID3D11DeviceChild*>(p)->GetDevice(&owner);bool ok=owner==device;if(owner)owner->Release();return ok;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
IUnknown* TypedTessellation(IUnknown* p,unsigned stage) noexcept {__try{if(!Owned(p))return nullptr;void* result{};auto hr=p->QueryInterface(stage==HS?__uuidof(ID3D11HullShader):__uuidof(ID3D11DomainShader),&result);if(FAILED(hr))return nullptr;return static_cast<IUnknown*>(result);}__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}}
bool Write(const std::filesystem::path& path,const void* data,size_t size){FILE* f=_wfsopen(path.c_str(),L"wb",_SH_DENYNO);if(!f)return false;bool ok=!size || fwrite(data,size,1,f)==1;return fclose(f)==0 && ok;}
void Cache(unsigned stage,IUnknown* shader,const void* data,size_t size,unsigned index=UINT_MAX){Key key{stage,reinterpret_cast<uintptr_t>(shader)};
 if(!shader || codes.contains(key) || codes.size()>=Limit || !data || size<32 || size>1024*1024 || codeBytes+size>CodeBudget || memcmp(data,"DXBC",4))return;
 Code c;c.pin=shader;c.bytes.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);c.hash=Hash(data,size);c.nativeIndex=index;codeBytes+=size;codes.emplace(key,std::move(c));}
void Created(unsigned stage,ID3D11Device* d,const void* bytes,SIZE_T size,IUnknown* out){
#ifdef ACUNITYVR_FOG79
 VolumetricFog79::Created(stage,d,bytes,size,out);
#endif
if(stage==PS && d==device && out)MaterialPrograms75::PixelCreated78(d,bytes,size,reinterpret_cast<ID3D11PixelShader*>(out));
 if(d==device && out && acceptCode){Lock lock;if(acceptCode)Cache(stage,out,bytes,size);}}
template<unsigned S> HRESULT STDMETHODCALLTYPE CreateShader(ID3D11Device* d,const void* bytes,SIZE_T size,ID3D11ClassLinkage* link,IUnknown** out){++creationCalls;auto hr=nativeCreate[S](d,bytes,size,link,out);if(SUCCEEDED(hr) && out && *out)Created(S,d,bytes,size,*out);--creationCalls;return hr;}
HRESULT STDMETHODCALLTYPE CreateGeometrySO(ID3D11Device* d,const void* bytes,SIZE_T size,const D3D11_SO_DECLARATION_ENTRY* entries,UINT count,const UINT* strides,UINT strideCount,UINT raster,ID3D11ClassLinkage* link,ID3D11GeometryShader** out){++creationCalls;auto hr=nativeSO(d,bytes,size,entries,count,strides,strideCount,raster,link,out);if(SUCCEEDED(hr) && out && *out)Created(GS,d,bytes,size,*out);--creationCalls;return hr;}
bool Retained(IUnknown* shader,unsigned stage,const uint8_t** bytes,size_t* size,unsigned* index) noexcept {__try{
 if(!image || !shader || stage>=Stages)return false;const unsigned offsets[]{8,0x14,0x20,0x2c,0x38,0x44};auto manager=image+0x524e030;auto count=*reinterpret_cast<uint16_t*>(manager+offsets[stage]+8);auto table=*reinterpret_cast<uint8_t***>(manager+offsets[stage]);if(!table || count>4096)return false;
 for(unsigned i=0;i<count;++i){auto row=table[i];if(!row || *reinterpret_cast<IUnknown**>(row+16)!=shader)continue;auto code=*reinterpret_cast<uint8_t**>(row);auto length=*reinterpret_cast<uint64_t*>(row+8);auto base=reinterpret_cast<uintptr_t>(image),at=reinterpret_cast<uintptr_t>(code);
  if(length<32 || length>1024*1024 || at<base || at-base>0x7825000-length || memcmp(code,"DXBC",4) || *reinterpret_cast<unsigned*>(code+24)!=length)return false;*bytes=code;*size=size_t(length);*index=i;return true;}
 return false;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Resolve(Candidate& c){Key key{c.stage,reinterpret_cast<uintptr_t>(c.pin.Get())};if(codes.contains(key))return;const uint8_t* bytes{};size_t size{};unsigned index{};
 if(Retained(c.pin.Get(),c.stage,&bytes,&size,&index)){Cache(c.stage,c.pin.Get(),bytes,size,index);return;}
#ifdef ACUNITYVR_PORTABLE92
 std::vector<ShaderRegistry92::Entry> imported;if(!ShaderRegistry92::Read(imported))return;
 for(auto& entry:imported)if(entry.device==uint64_t(device)&&entry.shader==key.second&&entry.stage==c.stage){Code code;code.pin=c.pin;code.hash=entry.hash;codes.emplace(key,std::move(code));return;}
#else
 // The startup collector pins VS/PS identities. Import only this exact marked object.
 auto capture=package.parent_path().parent_path()/L"experiments"/L"shader-observe75"/L"captures75"/std::to_wstring(GetCurrentProcessId());std::ifstream list(capture/L"shader-list.jsonl");std::string line;
 for(unsigned n=0;n<4096 && std::getline(list,line);++n){if(line.size()>1024)continue;auto pointer=Field(line,"shader");if(pointer.empty() || std::strtoull(pointer.c_str(),nullptr,16)!=key.second || Field(line,"stage")!=names[c.stage] || std::strtoull(Field(line,"device").c_str(),nullptr,16)!=reinterpret_cast<uintptr_t>(device))continue;
  auto name=Field(line,"file"),expected=Field(line,"sha256");if(expected.size()!=64 || name!=std::string(c.stage==PS?"ps-":"vs-")+expected+".dxbc")return;std::ifstream in(capture/name,std::ios::binary);std::vector<uint8_t> code((std::istreambuf_iterator<char>(in)),{});if(Hash(code.data(),code.size())==expected)Cache(c.stage,c.pin.Get(),code.data(),code.size());return;}
#endif
}
void Identity(FILE* f,const Candidate& c){auto code=codes.find({c.stage,reinterpret_cast<uintptr_t>(c.pin.Get())});fprintf(f,"\"shaderId\":%u,\"stage\":\"%s\",\"shader\":\"%p\",\"nativeIndex\":%u,\"sha256\":\"%s\",\"paths\":%u,\"hits\":%llu,\"skips\":%llu",c.id,names[c.stage],c.pin.Get(),code==codes.end()?UINT_MAX:code->second.nativeIndex,code==codes.end()?"":code->second.hash.c_str(),c.paths,c.hits,c.skips);}
void Refilter(){visible.clear();for(size_t i=0;i<candidates.size();++i)if(FilterOrder[filterIndex]==Stages || candidates[i].stage==FilterOrder[filterIndex])visible.push_back(i);control.count=visible.size();control.selected=visible.empty()?ShaderHuntControl::None:0;selected=control.enabled && !visible.empty()?visible[0]:ShaderHuntControl::None;}
void ResetCatalog(){active=false;captureRequest=false;selected=marked=ShaderHuntControl::None;candidates.clear();lookup.clear();visible.clear();filterIndex=0;control.enabled=false;control.count=0;control.selected=ShaderHuntControl::None;}
void Mark(){if(selected>=candidates.size())return;auto& c=candidates[selected];Resolve(c);++markId;marked=selected;markMask=0;markDirectory=directory/(L"mark-"+std::to_wstring(markId)+L"-shader-"+std::to_wstring(c.id));std::error_code ec;std::filesystem::create_directories(markDirectory,ec);if(ec){marked=ShaderHuntControl::None;return;}
 FILE* f=_wfsopen((markDirectory/L"mark.json").c_str(),L"w",_SH_DENYNO);if(f){fprintf(f,"{");Identity(f,c);fprintf(f,",\"markId\":%u,\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"catalogIndex\":%zu}\n",markId,sequence,tracking,frame,control.selected+1);fclose(f);}
 auto code=codes.find({c.stage,reinterpret_cast<uintptr_t>(c.pin.Get())});if(code!=codes.end()&&!code->second.bytes.empty()){Write(markDirectory/L"shader.dxbc",code->second.bytes.data(),code->second.bytes.size());if(c.stage==PS)Write(markDirectory/L"pixel-shader.dxbc",code->second.bytes.data(),code->second.bytes.size());}
 fprintf(events,"{\"event\":\"mark\",\"markId\":%u,",markId);Identity(events,c);fprintf(events,",\"sequence\":%llu}\n",sequence);fprintf(report,"DELETE mark=%u stage=%s id=%u; direct/worker draw or dispatch details pending\n",markId,names[c.stage],c.id);captureRequest=true;fflush(events);fflush(report);}
void Bindings(ID3D11DeviceContext* c,bool compute,std::array<ComPtr<IUnknown>,Stages>& out){
 if(compute){ComPtr<ID3D11ComputeShader> p;c->CSGetShader(&p,nullptr,nullptr);out[CS]=p;return;}
 ComPtr<ID3D11VertexShader> v;ComPtr<ID3D11PixelShader> p;ComPtr<ID3D11GeometryShader> g;ComPtr<ID3D11HullShader> h;ComPtr<ID3D11DomainShader> d;
 c->VSGetShader(&v,nullptr,nullptr);c->PSGetShader(&p,nullptr,nullptr);c->GSGetShader(&g,nullptr,nullptr);c->HSGetShader(&h,nullptr,nullptr);c->DSGetShader(&d,nullptr,nullptr);out={v,p,g,h,d,nullptr};}
bool Snapshot(void* source,void* output,size_t size) noexcept {__try{if(!source)return false;memcpy(output,source,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Capture(Candidate& c,ID3D11DeviceContext* context,bool worker,unsigned kind,UINT a,UINT b,const char* pass,unsigned view,void* nativeContext,uint64_t cache,bool suppressed){
 unsigned bit=(worker?4u:0u)+(view<3?view:3);if(markMask&(1u<<bit))return;
 auto name=std::wstring(worker?L"worker-view-":L"direct-view-")+std::to_wstring(view);FILE* f=_wfsopen((markDirectory/(name+L".json")).c_str(),L"w",_SH_DENYNO);if(!f)return;
 fprintf(f,"{\"markId\":%u,",markId);Identity(f,c);fprintf(f,",\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"view\":%u,\"kind\":%u,\"a\":%u,\"b\":%u,\"worker\":%s,\"context\":\"%p\",\"nativeContext\":\"%p\",\"cache\":\"%llx\",\"pass\":\"%s\",\"suppressed\":%s",sequence,tracking,frame,view,kind,a,b,worker?"true":"false",context,nativeContext,cache,pass?pass:"unclassified",suppressed?"true":"false");
 if(worker){std::array<uint8_t,0x910> data{};bool ok=Snapshot(reinterpret_cast<void*>(cache),data.data(),data.size()) && Write(markDirectory/(name+L"-bindings.bin"),data.data(),data.size());std::array<uint8_t,0x660> native{};bool nativeOK=Snapshot(nativeContext,native.data(),native.size()) && Write(markDirectory/(name+L"-native.bin"),native.data(),native.size());fprintf(f,",\"bindingsCopied\":%s,\"nativeCopied\":%s,\"deferredGetStateUsed\":false",ok?"true":"false",nativeOK?"true":"false");}
 else {ID3D11Buffer* buffers[14]{};UINT first[14]{},count[14]{};
  if(immediate1){switch(c.stage){case VS:immediate1->VSGetConstantBuffers1(0,14,buffers,first,count);break;case PS:immediate1->PSGetConstantBuffers1(0,14,buffers,first,count);break;case GS:immediate1->GSGetConstantBuffers1(0,14,buffers,first,count);break;case HS:immediate1->HSGetConstantBuffers1(0,14,buffers,first,count);break;case DS:immediate1->DSGetConstantBuffers1(0,14,buffers,first,count);break;case CS:immediate1->CSGetConstantBuffers1(0,14,buffers,first,count);break;}}
  else switch(c.stage){case VS:context->VSGetConstantBuffers(0,14,buffers);break;case PS:context->PSGetConstantBuffers(0,14,buffers);break;case GS:context->GSGetConstantBuffers(0,14,buffers);break;case HS:context->HSGetConstantBuffers(0,14,buffers);break;case DS:context->DSGetConstantBuffers(0,14,buffers);break;case CS:context->CSGetConstantBuffers(0,14,buffers);break;}
  fprintf(f,",\"buffers\":[");bool comma=false;size_t budget{};
  for(unsigned i=0;i<14;++i)if(buffers[i]){D3D11_BUFFER_DESC d{};buffers[i]->GetDesc(&d);bool copied=false;
   if(d.ByteWidth<=65536 && budget+d.ByteWidth<=2*1024*1024){auto sd=d;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=sd.StructureByteStride=0;ComPtr<ID3D11Buffer> staging;
    if(SUCCEEDED(device->CreateBuffer(&sd,nullptr,&staging))){context->CopyResource(staging.Get(),buffers[i]);D3D11_MAPPED_SUBRESOURCE map{};if(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map))){copied=Write(markDirectory/(name+L"-stage-"+std::to_wstring(c.stage)+L"-cb-"+std::to_wstring(i)+L".bin"),map.pData,d.ByteWidth);context->Unmap(staging.Get(),0);budget+=d.ByteWidth;}}}
   fprintf(f,"%s{\"slot\":%u,\"pointer\":\"%p\",\"bytes\":%u,\"firstConstant\":%u,\"constantCount\":%u,\"copied\":%s}",comma?",":"",i,buffers[i],d.ByteWidth,first[i],count[i],copied?"true":"false");buffers[i]->Release();comma=true;}fprintf(f,"]");
  auto resource=[&](ID3D11View* v){if(!v){fprintf(f,"null");return;}ComPtr<ID3D11Resource> r;v->GetResource(&r);ComPtr<ID3D11Texture2D> t;r.As(&t);fprintf(f,"{\"view\":\"%p\",\"resource\":\"%p\"",v,r.Get());if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);fprintf(f,",\"width\":%u,\"height\":%u,\"format\":%u,\"samples\":%u",d.Width,d.Height,unsigned(d.Format),d.SampleDesc.Count);}fprintf(f,"}");};
  ID3D11ShaderResourceView* srvs[128]{};switch(c.stage){case VS:context->VSGetShaderResources(0,128,srvs);break;case PS:context->PSGetShaderResources(0,128,srvs);break;case GS:context->GSGetShaderResources(0,128,srvs);break;case HS:context->HSGetShaderResources(0,128,srvs);break;case DS:context->DSGetShaderResources(0,128,srvs);break;case CS:context->CSGetShaderResources(0,128,srvs);break;}
  fprintf(f,",\"resources\":[");comma=false;for(unsigned i=0;i<128;++i)if(srvs[i]){fprintf(f,"%s{\"slot\":%u,\"binding\":",comma?",":"",i);resource(srvs[i]);fprintf(f,"}");srvs[i]->Release();comma=true;}fprintf(f,"]");
  ID3D11RenderTargetView* targets78[8]{};ComPtr<ID3D11DepthStencilView> depth;context->OMGetRenderTargets(8,targets78,&depth);fprintf(f,",\"targets\":[");for(unsigned i=0;i<8;++i){if(i)fprintf(f,",");resource(targets78[i]);if(targets78[i])targets78[i]->Release();}fprintf(f,"],\"depth\":");resource(depth.Get());
  if(c.stage==CS){ID3D11UnorderedAccessView* uavs[8]{};context->CSGetUnorderedAccessViews(0,8,uavs);fprintf(f,",\"uavs\":[");for(unsigned i=0;i<8;++i){if(i)fprintf(f,",");resource(uavs[i]);if(uavs[i])uavs[i]->Release();}fprintf(f,"]");}
 }
 fprintf(f,"}\n");fclose(f);markMask|=1u<<bit;fprintf(events,"{\"event\":\"markWork\",\"markId\":%u,\"stage\":\"%s\",\"view\":%u,\"worker\":%s,\"kind\":%u,\"sequence\":%llu}\n",markId,names[c.stage],view,worker?"true":"false",kind,sequence);fflush(events);}
} // anonymous
bool Install(ID3D11Device* d,ID3D11DeviceContext* c,uint8_t* base,const std::filesystem::path& out,FILE* log){
 device=d;immediate=c;image=base;report=log;package=out;directory=out/L"shader-hunt"/(L"pid-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));immediate->QueryInterface(IID_PPV_ARGS(&immediate1));std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return false;events=_wfsopen((directory/L"events.jsonl").c_str(),L"w",_SH_DENYNO);if(!events || BCryptOpenAlgorithmProvider(&sha,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 const unsigned slots[]{12,15,13,16,17,18};void* hooks[]{reinterpret_cast<void*>(CreateShader<VS>),reinterpret_cast<void*>(CreateShader<PS>),reinterpret_cast<void*>(CreateShader<GS>),reinterpret_cast<void*>(CreateShader<HS>),reinterpret_cast<void*>(CreateShader<DS>),reinterpret_cast<void*>(CreateShader<CS>)};auto table=*reinterpret_cast<void***>(device);
 for(unsigned i=0;i<Stages;++i){targets[i]=table[slots[i]];MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(targets[i],&m,sizeof(m)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || !(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))){Restore();return false;}
  if(MH_CreateHook(targets[i],hooks[i],reinterpret_cast<void**>(&nativeCreate[i]))!=MH_OK){Restore();return false;}created[i]=true;if(MH_EnableHook(targets[i])!=MH_OK){Restore();return false;}}
 soTarget=table[14];MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(soTarget,&m,sizeof(m)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || !(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE)) || MH_CreateHook(soTarget,reinterpret_cast<void*>(CreateGeometrySO),reinterpret_cast<void**>(&nativeSO))!=MH_OK){Restore();return false;}soCreated=true;if(MH_EnableHook(soTarget)!=MH_OK){Restore();return false;}
 acceptCode=true;fprintf(report,"Shader hunter78 installed OFF: direct and validated worker recording, all draw variants and compute dispatches. End enables; PageUp/PageDown cycle suppressed shader; Delete marks; Home restores/OFF. NumPad7 filters PS -> CS -> VS -> GS -> HS -> DS -> ALL (starts PS). No deferred Get-state calls; retained native DXBC/current-process pinned creation code; existing Material78 PS creation observer shared.\n");fflush(report);return true;}
bool Enabled(){return active.load(std::memory_order_relaxed);}bool TakeCaptureRequest(){return captureRequest.exchange(false);}
void BeginFrame(uint64_t id,uint64_t pose,uint32_t sourceFrame){Lock lock;sequence=id;tracking=pose;frame=sourceFrame;uint32_t down{};const int keys[]{VK_END,VK_HOME,VK_PRIOR,VK_NEXT,VK_DELETE};for(unsigned i=0;i<5;++i)if(GetAsyncKeyState(keys[i])&0x8000)down|=1u<<i;
 bool filter=(GetAsyncKeyState(VK_NUMPAD7)&0x8000)!=0,changeFilter=filter && !oldFilter;oldFilter=filter;auto old=selected;bool was=control.enabled;auto pressed=control.Poll(down);
 if(pressed&ShaderHuntControl::Home){ResetCatalog();fprintf(events,"{\"event\":\"resetOff\",\"sequence\":%llu}\n",sequence);fprintf(report,"HOME: all shaders restored; hunter OFF; saved marks retained\n");}
 else {if(changeFilter && control.enabled){filterIndex=(filterIndex+1)%_countof(FilterOrder);Refilter();}else selected=control.enabled && control.selected<visible.size()?visible[control.selected]:ShaderHuntControl::None;active=control.enabled;
  if((pressed&ShaderHuntControl::Delete) && selected<candidates.size())Mark();
  if(selected!=old || was!=control.enabled || changeFilter){fprintf(events,"{\"event\":\"selection\",\"enabled\":%s,\"sequence\":%llu,\"filter\":\"%s\",\"index\":%zu,\"count\":%zu,\"catalogSize\":%zu",control.enabled?"true":"false",sequence,FilterOrder[filterIndex]==Stages?"ALL":names[FilterOrder[filterIndex]],selected<candidates.size()?control.selected+1:0,visible.size(),candidates.size());if(selected<candidates.size()){fprintf(events,",");Identity(events,candidates[selected]);}fprintf(events,"}\n");fprintf(report,"Hunter78 %s filter=%s selected=%zu/%zu stage=%s id=%u\n",control.enabled?"ON":"OFF",FilterOrder[filterIndex]==Stages?"ALL":names[FilterOrder[filterIndex]],selected<candidates.size()?control.selected+1:0,visible.size(),selected<candidates.size()?names[candidates[selected].stage]:"none",selected<candidates.size()?candidates[selected].id:0);}}
 if(pressed || changeFilter || selected!=old || was!=control.enabled){fflush(events);fflush(report);}}
bool Skip(ID3D11DeviceContext* c,unsigned kind,UINT a,UINT b,const char* pass,unsigned view,void* nativeContext){
 if(!active.load(std::memory_order_relaxed))return false;bool worker=c!=immediate,compute=kind==41 || kind==42;std::array<ComPtr<IUnknown>,Stages> bound;uint64_t cache{};
 if(!worker)Bindings(c,compute,bound);
 else {std::array<uint64_t,Stages> raw{};if(!EffectDrawCapture74::HuntBindings78(c,raw.data(),&nativeContext,&view,&cache))return false;
  if(compute)bound[CS]=reinterpret_cast<IUnknown*>(raw[CS]);else {for(unsigned s:{VS,PS,GS})if(raw[s])bound[s]=reinterpret_cast<IUnknown*>(raw[s]);
   // The two other native cached slots are type-checked, not assumed to be HS/DS.
   for(unsigned slot:{HS,DS})if(raw[slot])for(unsigned s:{HS,DS}){auto typed=TypedTessellation(reinterpret_cast<IUnknown*>(raw[slot]),s);if(typed){if(!bound[s])bound[s].Attach(typed);else typed->Release();}}}}
 Lock lock;if(!control.enabled)return false;bool skip=false;
 for(unsigned stage=0;stage<Stages;++stage){auto shader=bound[stage].Get();if(!shader)continue;Key key{stage,reinterpret_cast<uintptr_t>(shader)};auto found=lookup.find(key);
  if(found==lookup.end() && candidates.size()<Limit && Owned(shader)){Candidate candidate;candidate.pin=shader;candidate.stage=stage;candidate.id=++nextId;auto index=candidates.size();candidates.push_back(std::move(candidate));lookup.emplace(key,index);found=lookup.find(key);if(FilterOrder[filterIndex]==Stages || FilterOrder[filterIndex]==stage){visible.push_back(index);control.Add();}fprintf(events,"{\"event\":\"shader\",");Identity(events,candidates[index]);fprintf(events,"}\n");}
  if(found==lookup.end())continue;auto& candidate=candidates[found->second];++candidate.hits;candidate.paths|=worker?2:1;bool suppressed=selected==found->second;if(suppressed){++candidate.skips;skip=true;}
  if(marked==found->second)Capture(candidate,c,worker,kind,a,b,pass,view,nativeContext,cache,suppressed);
 }
 if(skip)++skipped;return skip;}
void Restore(){active=false;acceptCode=false;captureRequest=false;Lock lock;control.enabled=false;selected=marked=ShaderHuntControl::None;
 for(unsigned i=0;i<Stages;++i)if(created[i]){MH_DisableHook(targets[i]);created[i]=false;}if(soCreated){MH_DisableHook(soTarget);soCreated=false;}
 if(events){fprintf(events,"{\"event\":\"retired\",\"shaders\":%zu,\"marks\":%u,\"skippedWork\":%llu,\"capturedBytecodes\":%zu,\"creationInFlight\":%u}\n",candidates.size(),markId,skipped,codes.size(),creationCalls.load());fclose(events);events=nullptr;}if(report){fprintf(report,"Hunter78 stopped: all shaders restored, shaders=%zu marks=%u skippedWork=%llu; pins/trampolines retained until exit\n",candidates.size(),markId,skipped);fflush(report);}}
} // ShaderHunt
