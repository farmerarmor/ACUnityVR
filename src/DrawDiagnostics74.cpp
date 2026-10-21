// Read-only observation of lighting/post-effect draws on the existing game device.
// F8 arms one complete native interval; rendering continues until manual F10.
#include "DrawDiagnostics.h"
#include "EffectDrawCapture74.h"
#include "DispatchRoute76.h"
#include "TerminalCapture64.h"
#include "LightingGrid67.h"
#include "TileCapture68.h"
#include "LightingTiles69.h"
#include "ShaderHunt.h"
#include "VolumetricFog79.h"
#include "CloudProjection.h"
#include "GpuCapture.h"
#include "GpuSceneDepth46.h"
#include "NativeGpuTiming.h"
#ifdef ACUNITYVR_NATIVE_FOVEATION
#include "NativeFoveation.h"
#include "RenderSize.h"
#endif
#ifdef ACUNITYVR_OCCLUSION_CAPTURE
#include "OcclusionProbe.h"
#endif
#ifdef ACUNITYVR_POST_AA
#include "PostAAOptimization.h"
#endif
#include <d3d11_1.h>
#include <MinHook.h>
#include "BatchCaller77.h"
#include "DispatchKnown77.h"
#include <array>
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
#include <intrin.h>
#endif
#include <atomic>
#include <cmath>
#include <share.h>
#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>
using Microsoft::WRL::ComPtr;
namespace DrawDiagnostics {
using acu::Field;using Task=void(__fastcall*)(void*,void*,void*);
uint8_t *image{},*backend{};void* views[3]{};void* surface{};FILE* log{};FILE* events{};
std::filesystem::path directory,captureDirectory;ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> immediate;ComPtr<ID3D11DeviceContext1> immediate1;
PassOverride passOverride{};
std::atomic<bool> enabled{},capturing{};std::atomic<unsigned> inFlight{};
uint64_t sequence{},tracking{};uint32_t sourceFrame{};unsigned draws{},otherContexts{},errors{},budgetSkipped{},captureCount{};
size_t stagedBytes{},bytesByView[3]{};unsigned drawsByView[3]{};constexpr size_t Budget=16*1024*1024;constexpr unsigned DrawLimit=256;
LightingTiles69::Pool lightingTiles69;
TileCapture68::Pool tileCapture68;
LightingGrid67::Pool lightingGrid67;bool gridGeometry67[3]{};
TerminalCapture64::Pool terminalPixels64;size_t terminalBufferBytes64[3]{};unsigned terminalDraws64[3]{},terminalSkipped64{};constexpr size_t TerminalBufferBudget64=64*1024*1024;constexpr unsigned TerminalDrawLimit64=96;
struct Pass{size_t descriptor,table,start,finish;const char* name;size_t lanes=0x76b00,worker=0x1e6d4e0;void* clone[5]{};Task nativeStart{},native{};};
Pass passes[]{
 {0x5049e90,0x30a9010,0x22919c0,0x22909d0,"ReflectionDeferredLighting",0x76b00,0x1e6d4e0},
 {0x5049eb8,0x30a9038,0x1e6d4e0,0x22909e0,"ReflectionFinalPass",0x76b00,0x1e6d4e0},
 {0x504a088,0x30a91f0,0x2291940,0x22908c0,"IndoorGI",0x76b00,0x1e6d4e0},
 {0x504a0b0,0x30a9218,0x2291910,0x22908a0,"DeferredLighting",0x76b00,0x1e6d4e0},
 {0x504a150,0x30a92b8,0x1e6d4e0,0x2290930,"PostEffects",0x76b00,0x1e6d4e0},
 {0x504a100,0x30a9268,0x2291900,0x2290890,"SkyBeforeTransparent",0x76b00,0x1e6d4e0}
#ifdef ACUNITYVR_FULL_PASS_PROFILE
 , {0x5049e50,0x30a8fe8,0x2291a30,0x1e6d4e0,"ReflectionGBuffer",0x2290580,0x2290d00},
 {0x5049ee0,0x30a8e80,0x22919e0,0x2290a30,"ZoneRenderPass",0x76b00,0x1e6d4e0},
 {0x5049f08,0x30a9060,0x1e6d4e0,0x2290a80,"ZoneSynchronizeMaskPass",0x76b00,0x1e6d4e0},
 {0x5049f30,0x30a90b0,0x2291a40,0x1e6d4e0,"SolidGBuffer",0x2290580,0x22910a0},
 {0x5049fc0,0x30a9128,0x1e6d4e0,0x2290810,"AfterDrawOpaque",0x76b00,0x1e6d4e0},
 {0x5049fe8,0x30a9150,0x22919d0,0x1e6d4e0,"SetupDepthLinearizePass",0x76b00,0x1e6d4e0},
 {0x504a010,0x30a9178,0x1e6d4e0,0x2290800,"DepthBufferLinearize",0x76b00,0x1e6d4e0},
 {0x504a038,0x30a91c8,0x2291950,0x2290900,"MSAAResolve",0x76b00,0x1e6d4e0},
 {0x504a060,0x30a91a0,0x1e6d4e0,0x22907b0,"DownScaleSceneDepth",0x76b00,0x1e6d4e0},
 {0x504a0d8,0x30a9240,0x1e6d4e0,0x22908b0,"Emissive",0x76b00,0x1e6d4e0},
 {0x504a1e0,0x30a9380,0x1e6d4e0,0x22909f0,"UI3DPass",0x76b00,0x1e6d4e0},
 {0x504a208,0x30a92e0,0x2291960,0x1e6d4e0,"preUIAndVideo",0x76b00,0x1e6d4e0},
 {0x504a230,0x30a9308,0x1e6d4e0,0x2290a00,"videoBlitPass",0x76b00,0x1e6d4e0},
 {0x504a2c0,0x30a9438,0x1e6d4e0,0x2290920,"NonAsyncComputeVisibility",0x76b00,0x1e6d4e0}
#endif
 ,{0x504a2e8,0x30a9460,0x2291910,0x2290910,"LightingTileClassifier"}
};
struct Scope{Pass* pass{};void* context{};unsigned view{},phase{};};thread_local Scope current;
// D3D11 command-list replay may re-enter these method hooks with no public
// shader binding. Time that work at ExecuteCommandList, never as direct draws.
thread_local unsigned commandListReplayDepth{};
std::atomic<uint64_t> replayDrawsSkipped{},emptyShaderDrawsSkipped{};
std::atomic<bool> fastPaths77{true};bool fastBatchSetting77=true,fastDispatchSetting77=true;
std::atomic<uint64_t> fastBatchHits77{},slowBatchAttempts77{},knownDispatchHits77{},fullDispatchChecks77{};
FILE* performance77{};
bool ReadPointer77(uintptr_t address,uintptr_t& value)noexcept{__try{value=*reinterpret_cast<const uintptr_t*>(address);return true;}__except(EXCEPTION_EXECUTE_HANDLER){value=0;return false;}}
struct BufferCopy{ComPtr<ID3D11Buffer> staging;unsigned draw,stage,slot,size;};
std::vector<BufferCopy> copies;
GpuSceneDepth46::Pool sceneDepth;bool sceneDepthAvailable{};
void CaptureSceneDepth46(ID3D11DeviceContext*,ID3D11ComputeShader*,unsigned);
struct TextureCopy{ComPtr<ID3D11Texture2D> staging;D3D11_TEXTURE2D_DESC desc;unsigned view,draw;};
std::vector<TextureCopy> textureCopies;bool cloudCopied[3]{};
#ifdef ACUNITYVR_SUN_CAPTURE
struct SunTextureCopy{ComPtr<ID3D11Texture2D> staging;D3D11_TEXTURE2D_DESC desc{};unsigned view{};};
std::vector<SunTextureCopy> sunCopies;bool sunCopied[3]{};
void CaptureSunTexture(ID3D11DeviceContext* c,unsigned view,ID3D11PixelShader* ps);
#endif
ComPtr<ID3D11PixelShader> cloudComposite;
Pass cloudUnscoped{0,0,0,0,"QuarterHDRUnscoped"};
Pass finalUnscoped{0,0,0,0,"FinalOutputUnscoped"};
#ifdef ACUNITYVR_DEPTH_PROFILE
Pass nullPixelUnscoped{0,0,0,0,"NullPixelUnscoped"};
struct NullCaptureKey{uintptr_t color{},depth{},streamOutput{};D3D11_VIEWPORT viewport{};bool Same(const NullCaptureKey& b)const{return color==b.color && depth==b.depth && streamOutput==b.streamOutput && !memcmp(&viewport,&b.viewport,sizeof(viewport));}};
std::array<std::vector<NullCaptureKey>,3> nullCaptureKeys;
#endif
ComPtr<ID3D11Resource> finalOutput;
struct PreserveScope{Scope prior;PreserveScope():prior(current){}~PreserveScope(){current=prior;}};

struct Snapshot{uint64_t sequence,tracking,view,state,context;uint32_t sourceFrame,viewFrame,viewIndex,phase;uint8_t camera[0x240],normal[0x140],gpu[0x660],lighting[0x600];};
bool Read(void* context,unsigned index,unsigned phase,Snapshot* s){__try{
 auto c=static_cast<uint8_t*>(context);auto v=Field<uint8_t*>(c,0x30);auto state=Field<uint8_t*>(c,0x28);
 if(index>=3 || v!=views[index] || !state || Field<void*>(v,0x430)!=state || Field<void*>(state,0x8208)!=v)return false;
 s->sequence=sequence;s->tracking=tracking;s->view=reinterpret_cast<uint64_t>(v);s->state=reinterpret_cast<uint64_t>(state);s->context=reinterpret_cast<uint64_t>(c);
 s->sourceFrame=sourceFrame;s->viewFrame=Field<uint32_t>(v,0x294);s->viewIndex=index;s->phase=phase;
 memcpy(s->camera,v,0x240);memcpy(s->normal,v+0x2e0,0x140);memcpy(s->gpu,c,0x660);memcpy(s->lighting,state+0x240,0x600);return true;
}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Identify(void* context,unsigned* index){__try{
 auto v=Field<void*>(context,0x30);for(unsigned i=0;i<3;++i)if(views[i] && v==views[i]){*index=i;return true;}return false;
}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void SaveSnapshot(Pass& pass,void* context,unsigned index,unsigned phase){
 Snapshot s{};if(!Read(context,index,phase,&s)){++errors;return;}
 auto name=std::to_wstring(index)+L"-"+std::wstring(pass.name,pass.name+strlen(pass.name))+L"-"+std::to_wstring(phase)+L".bin";
 FILE* f=_wfsopen((captureDirectory/name).c_str(),L"wb",_SH_DENYNO);if(f){fwrite(&s,sizeof(s),1,f);fclose(f);}else ++errors;
 fprintf(events,"{\"event\":\"pass\",\"name\":\"%s\",\"view\":%u,\"phase\":%u,\"context\":\"%p\",\"snapshotSize\":%zu}\n",pass.name,index,phase,context,sizeof(s));
}
bool EnsureDispatchCoverage();
void ExecutePass(unsigned i,unsigned phase,void* task,void* b,void* context){
 ++inFlight;auto& p=passes[i];unsigned index{};bool scoped=enabled && (capturing || lightingTiles69.NeedsScope() || lightingGrid67.NeedsScope() || ShaderHunt::Enabled() || NativeGpuTiming::Recording()
#ifdef ACUNITYVR_POST_AA
 || PostAAOptimization::NeedsScope()
#endif
#ifdef ACUNITYVR_NATIVE_FOVEATION
 || NativeFoveation::NeedsScope()
#endif
 ) && b==backend && context==Field<void*>(backend,0x20) && Identify(context,&index);
 bool record=scoped && capturing && phase==4;auto previous=current;if(scoped){if(record)SaveSnapshot(p,context,index,0);current={&p,context,index,phase};}
 unsigned token=scoped?NativeGpuTiming::BeginEvent(0,unsigned(p.descriptor),phase,nullptr,index):UINT_MAX;
 if(scoped && phase==4 && i<6)NativeGpuTiming::BeginPass(index,i);
 if(scoped)EnsureDispatchCoverage();
 if(phase==2)p.nativeStart(task,b,context);
 else if(!passOverride || !passOverride(i,task,b,context))p.native(task,b,context);
 if(scoped && phase==4 && i<6)NativeGpuTiming::EndPass(index,i);
 NativeGpuTiming::EndEvent(token);
 if(scoped){current=previous;if(record)SaveSnapshot(p,context,index,1);}--inFlight;
}
template<unsigned I>void __fastcall PassStart(void* t,void* b,void* c){ExecutePass(I,2,t,b,c);}
template<unsigned I>void __fastcall PassFinish(void* t,void* b,void* c){ExecutePass(I,4,t,b,c);}
template<size_t... I>auto StartWrappers(std::index_sequence<I...>){return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&PassStart<unsigned(I)>)...};}
template<size_t... I>auto FinishWrappers(std::index_sequence<I...>){return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&PassFinish<unsigned(I)>)...};}
void Resource(ID3D11View* view){
 if(!view){fprintf(events,"null");return;}ComPtr<ID3D11Resource> r;view->GetResource(&r);ComPtr<ID3D11Texture2D> t;r.As(&t);
 if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);fprintf(events,"{\"view\":\"%p\",\"resource\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u,\"array\":%u,\"mips\":%u,\"samples\":%u}",view,t.Get(),d.Width,d.Height,unsigned(d.Format),d.ArraySize,d.MipLevels,d.SampleDesc.Count);}
 else fprintf(events,"{\"view\":\"%p\",\"resource\":\"%p\"}",view,r.Get());
}
void Buffers(unsigned stage,bool terminal=false){
 auto& used=terminal?terminalBufferBytes64[current.view]:bytesByView[current.view];const auto limit=terminal?TerminalBufferBudget64:Budget;
 ID3D11Buffer* bound[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};UINT first[_countof(bound)]{},count[_countof(bound)]{};
 if(immediate1){
  if(stage==0)immediate1->VSGetConstantBuffers1(0,_countof(bound),bound,first,count);
  if(stage==1)immediate1->PSGetConstantBuffers1(0,_countof(bound),bound,first,count);
  if(stage==2)immediate1->CSGetConstantBuffers1(0,_countof(bound),bound,first,count);
  if(stage==5)immediate1->GSGetConstantBuffers1(0,_countof(bound),bound,first,count);
 }else{
  if(stage==0)immediate->VSGetConstantBuffers(0,_countof(bound),bound);
  if(stage==1)immediate->PSGetConstantBuffers(0,_countof(bound),bound);
  if(stage==2)immediate->CSGetConstantBuffers(0,_countof(bound),bound);
  if(stage==5)immediate->GSGetConstantBuffers(0,_countof(bound),bound);
 }
 bool comma=false;fprintf(events,"[");
 for(unsigned slot=0;slot<_countof(bound);++slot)if(bound[slot]){
  auto b=bound[slot];D3D11_BUFFER_DESC d{};b->GetDesc(&d);bool copied=false;
  if(d.ByteWidth<=65536 && used+d.ByteWidth<=limit){
   auto stagingDesc=d;stagingDesc.Usage=D3D11_USAGE_STAGING;stagingDesc.BindFlags=0;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;stagingDesc.MiscFlags=0;stagingDesc.StructureByteStride=0;
   BufferCopy copy{};copy.draw=draws;copy.stage=stage;copy.slot=slot;copy.size=d.ByteWidth;
   if(SUCCEEDED(device->CreateBuffer(&stagingDesc,nullptr,&copy.staging))){immediate->CopyResource(copy.staging.Get(),b);copies.push_back(std::move(copy));stagedBytes+=d.ByteWidth;used+=d.ByteWidth;copied=true;}else ++errors;
  }else ++budgetSkipped;
  fprintf(events,"%s{\"slot\":%u,\"buffer\":\"%p\",\"bytes\":%u,\"firstConstant\":%u,\"constantCount\":%u,\"copied\":%s}",comma?",":"",slot,b,d.ByteWidth,first[slot],count[slot],copied?"true":"false");comma=true;b->Release();
 }
 fprintf(events,"]");
}
void Resources(unsigned stage){
 ID3D11ShaderResourceView* bound[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
 if(stage==0)immediate->VSGetShaderResources(0,_countof(bound),bound);else if(stage==1)immediate->PSGetShaderResources(0,_countof(bound),bound);else if(stage==3)immediate->GSGetShaderResources(0,_countof(bound),bound);else immediate->CSGetShaderResources(0,_countof(bound),bound);
 fprintf(events,"[");bool comma=false;
 for(unsigned i=0;i<_countof(bound);++i)if(bound[i]){D3D11_SHADER_RESOURCE_VIEW_DESC desc{};bound[i]->GetDesc(&desc);fprintf(events,"%s{\"slot\":%u,\"format\":%u,\"dimension\":%u,\"binding\":",comma?",":"",i,unsigned(desc.Format),unsigned(desc.ViewDimension));Resource(bound[i]);fprintf(events,"}");bound[i]->Release();comma=true;}
 fprintf(events,"]");
}
bool ReadCloudShader(ID3D11PixelShader** pointer){__try{
 auto manager=image+0x524e030;auto array=Field<uint8_t**>(manager,0x14);auto count=Field<uint16_t>(manager,0x1c);
 if(!array || count<=538 || count>4096)return false;
 auto entry=array[538];if(!entry)return false;auto code=Field<uint8_t*>(entry,0);auto bytes=Field<uint64_t>(entry,8);
 if(code!=image+0x334a340 || bytes!=6828 || memcmp(code,"DXBC",4))return false;
 *pointer=Field<ID3D11PixelShader*>(entry,0x10);return *pointer!=nullptr;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool InstallClouds(){ID3D11PixelShader* pointer{};if(!ReadCloudShader(&pointer))return false;cloudComposite=pointer;return true;}
bool CloudSizeMatches(unsigned width,unsigned height,unsigned index){
 if(index>=3)return false;if(index)return RenderSize::CloudMatches(width,height);
 // The untouched source camera can remain letterboxed. Observe its actual
 // native viewport, while owned VR eyes require the full headset dimensions.
 auto w=Field<float>(views[0],0x2c4),h=Field<float>(views[0],0x2c8);
 if(!std::isfinite(w) || !std::isfinite(h) || w<1 || h<1 || w>RenderSize::MaxWidth || h>RenderSize::MaxEyeHeight)return false;
 auto sw=unsigned(w),sh=unsigned(h);if(float(sw)!=w || float(sh)!=h)return false;
 return width==RenderSize::Quarter(sw) && height==RenderSize::Quarter(sh);
}
bool QuarterHDR(ID3D11View* target){
 if(!target)return false;ComPtr<ID3D11Resource> resource;target->GetResource(&resource);
 ComPtr<ID3D11Texture2D> texture;resource.As(&texture);if(!texture)return false;
 D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
 unsigned index{};if(!Identify(Field<void*>(backend,0x20),&index))return false;
 return d.Format==DXGI_FORMAT_R16G16B16A16_FLOAT && CloudSizeMatches(d.Width,d.Height,index) && d.SampleDesc.Count==1 && d.ArraySize==1;
}
bool CloudProducer(ID3D11DeviceContext* c,unsigned kind){
 if(c!=immediate.Get())return false;
 if(kind<41){ComPtr<ID3D11RenderTargetView> target;c->OMGetRenderTargets(1,&target,nullptr);return QuarterHDR(target.Get());}
 ID3D11UnorderedAccessView* targets[D3D11_PS_CS_UAV_REGISTER_COUNT]{};c->CSGetUnorderedAccessViews(0,_countof(targets),targets);
 bool matched=false;for(auto target:targets)if(target){matched=QuarterHDR(target)||matched;target->Release();}return matched;
}
void UnorderedAccess(){
 ID3D11UnorderedAccessView* bound[D3D11_PS_CS_UAV_REGISTER_COUNT]{};immediate->CSGetUnorderedAccessViews(0,_countof(bound),bound);
 fprintf(events,"[");bool comma=false;
 for(unsigned i=0;i<_countof(bound);++i)if(bound[i]){fprintf(events,"%s{\"slot\":%u,\"binding\":",comma?",":"",i);Resource(bound[i]);fprintf(events,"}");bound[i]->Release();comma=true;}fprintf(events,"]");
}
void CloudInput(ID3D11DeviceContext* c,ID3D11PixelShader* ps){
 if(ps!=cloudComposite.Get() || current.view>=3 || cloudCopied[current.view])return;
 ComPtr<ID3D11ShaderResourceView> srv;c->PSGetShaderResources(3,1,&srv);if(!srv)return;
 ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;resource.As(&texture);if(!texture)return;
 D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
 if(d.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || !CloudSizeMatches(d.Width,d.Height,current.view) || d.SampleDesc.Count!=1 || d.ArraySize!=1 || d.MipLevels!=1){++errors;return;}
 if(size_t(d.Width)*d.Height*8+bytesByView[current.view]>Budget){++budgetSkipped;return;}
 auto staging=d;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.MiscFlags=0;
 TextureCopy copy{};copy.desc=d;copy.view=current.view;copy.draw=draws;
 if(FAILED(device->CreateTexture2D(&staging,nullptr,&copy.staging))){++errors;return;}
 c->CopyResource(copy.staging.Get(),texture.Get());textureCopies.push_back(std::move(copy));cloudCopied[current.view]=true;stagedBytes+=size_t(d.Width)*d.Height*8;bytesByView[current.view]+=size_t(d.Width)*d.Height*8;
 fprintf(events,"{\"event\":\"cloudInput\",\"draw\":%u,\"view\":%u,\"nativePixelShaderIndex\":538,\"resource\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u}\n",draws,current.view,texture.Get(),d.Width,d.Height,unsigned(d.Format));
}
bool IsFinalOutput(ID3D11DeviceContext* c,unsigned kind){
 if(c!=immediate.Get() || kind>=41 || !finalOutput)return false;
 ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};c->OMGetRenderTargets(_countof(targets),targets,nullptr);
 bool found=false;for(auto target:targets)if(target){ComPtr<ID3D11Resource> r;target->GetResource(&r);found=found || r.Get()==finalOutput.Get();target->Release();}return found;
}
void RasterState(ID3D11DeviceContext* c){
 ComPtr<ID3D11RasterizerState> rs;c->RSGetState(&rs);D3D11_RASTERIZER_DESC d{};
 if(rs)rs->GetDesc(&d);else {d.FillMode=D3D11_FILL_SOLID;d.CullMode=D3D11_CULL_BACK;d.DepthClipEnable=TRUE;}
 fprintf(events,",\"rasterizer\":{\"fill\":%u,\"cull\":%u,\"frontCCW\":%s,\"depthClip\":%s,\"scissorEnable\":%s,\"multisample\":%s,\"depthBias\":%d}",unsigned(d.FillMode),unsigned(d.CullMode),d.FrontCounterClockwise?"true":"false",d.DepthClipEnable?"true":"false",d.ScissorEnable?"true":"false",d.MultisampleEnable?"true":"false",d.DepthBias);
 D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};UINT count=_countof(rects);c->RSGetScissorRects(&count,rects);fprintf(events,",\"scissors\":[");
 for(UINT i=0;i<count;++i)fprintf(events,"%s[%ld,%ld,%ld,%ld]",i?",":"",rects[i].left,rects[i].top,rects[i].right,rects[i].bottom);fprintf(events,"]");
 ComPtr<ID3D11BlendState> blend;float factor[4]{};UINT mask{};c->OMGetBlendState(&blend,factor,&mask);D3D11_BLEND_DESC bd{};
 if(blend)blend->GetDesc(&bd);else bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
 auto& rt=bd.RenderTarget[0];fprintf(events,",\"blend0\":{\"enabled\":%s,\"src\":%u,\"dst\":%u,\"op\":%u,\"srcAlpha\":%u,\"dstAlpha\":%u,\"alphaOp\":%u,\"writeMask\":%u,\"sampleMask\":%u}",rt.BlendEnable?"true":"false",unsigned(rt.SrcBlend),unsigned(rt.DestBlend),unsigned(rt.BlendOp),unsigned(rt.SrcBlendAlpha),unsigned(rt.DestBlendAlpha),unsigned(rt.BlendOpAlpha),unsigned(rt.RenderTargetWriteMask),mask);
}
void GeometryBuffer(ID3D11Buffer* b,unsigned stage,unsigned slot,bool terminal=false){
 auto& used=terminal?terminalBufferBytes64[current.view]:bytesByView[current.view];const auto limit=terminal?TerminalBufferBudget64:Budget;
 if(!b){fprintf(events,"null");return;}D3D11_BUFFER_DESC d{};b->GetDesc(&d);bool copied=false;
 if(d.ByteWidth<=1024*1024 && used+d.ByteWidth<=limit){
  auto desc=d;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;desc.StructureByteStride=0;
  BufferCopy copy{};copy.draw=draws;copy.stage=stage;copy.slot=slot;copy.size=d.ByteWidth;
  if(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&copy.staging))){immediate->CopyResource(copy.staging.Get(),b);copies.push_back(std::move(copy));stagedBytes+=d.ByteWidth;used+=d.ByteWidth;copied=true;}else ++errors;
 }else ++budgetSkipped;
 fprintf(events,"{\"buffer\":\"%p\",\"bytes\":%u,\"copied\":%s}",b,d.ByteWidth,copied?"true":"false");
}
void Geometry(ID3D11DeviceContext* c,bool terminal=false){
 D3D11_PRIMITIVE_TOPOLOGY topology{};c->IAGetPrimitiveTopology(&topology);fprintf(events,",\"geometry\":{\"topology\":%u,\"vertices\":[",unsigned(topology));
 ID3D11Buffer* vb[4]{};UINT strides[4]{},offsets[4]{};c->IAGetVertexBuffers(0,4,vb,strides,offsets);bool comma=false;
 for(unsigned i=0;i<4;++i)if(vb[i]){fprintf(events,"%s{\"slot\":%u,\"stride\":%u,\"offset\":%u,\"data\":",comma?",":"",i,strides[i],offsets[i]);GeometryBuffer(vb[i],3,i,terminal);fprintf(events,"}");vb[i]->Release();comma=true;}
 ComPtr<ID3D11Buffer> ib;DXGI_FORMAT format{};UINT offset{};c->IAGetIndexBuffer(&ib,&format,&offset);fprintf(events,"],\"indexFormat\":%u,\"indexOffset\":%u,\"indices\":",unsigned(format),offset);GeometryBuffer(ib.Get(),4,0,terminal);fprintf(events,"}");
}
#ifdef ACUNITYVR_SHADER_PROFILE
struct NativeShader {unsigned stage{},index{};uint8_t* code{};size_t size{};IUnknown* shader{};};
std::unordered_map<IUnknown*,NativeShader> nativeShaders;std::unordered_set<IUnknown*> savedShaders;size_t savedShaderBytes{};
bool ShaderEntry(unsigned stage,unsigned index,NativeShader* result,unsigned* count){__try{
 const unsigned offsets[]{8,0x14,0x44,0x20},countOffsets[]{0x10,0x1c,0x4c,0x28};auto manager=image+0x524e030;
 *count=Field<uint16_t>(manager,countOffsets[stage]);if(!*count || *count>4096)return false;
 auto table=Field<uint8_t**>(manager,offsets[stage]);if(!table || index>=*count)return false;auto entry=table[index];if(!entry)return true;
 auto code=Field<uint8_t*>(entry,0);auto size=Field<uint64_t>(entry,8);auto shader=Field<IUnknown*>(entry,0x10);
 auto address=reinterpret_cast<uintptr_t>(code),begin=reinterpret_cast<uintptr_t>(image);
 if(!shader)return true;
 if(size<32 || size>1024*1024 || address<begin || address-begin>0x7825000-size || memcmp(code,"DXBC",4) || Field<uint32_t>(code,24)!=size)return false;
 *result={stage,index,code,size_t(size),shader};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool InitializeShaderLookup(const std::filesystem::path& output){
 FILE* f=_wfsopen((output/L"native-shaders.jsonl").c_str(),L"w",_SH_DENYNO);if(!f)return false;
 for(unsigned stage=0;stage<4;++stage){unsigned count{};NativeShader first{};if(!ShaderEntry(stage,0,&first,&count)){fclose(f);return false;}
  for(unsigned index=0;index<count;++index){NativeShader shader{};if(!ShaderEntry(stage,index,&shader,&count)){fclose(f);return false;}if(!shader.shader)continue;
   nativeShaders.emplace(shader.shader,shader);fprintf(f,"{\"stage\":%u,\"index\":%u,\"shader\":\"%p\",\"rva\":%llu,\"size\":%zu}\n",stage,index,shader.shader,static_cast<unsigned long long>(shader.code-image),shader.size);
  }
 }fclose(f);return true;
}
void SaveNativeShader(IUnknown* shader,unsigned stage){
 auto it=nativeShaders.find(shader);fprintf(events,",\"nativeShader%u\":",stage);if(it==nativeShaders.end()){fprintf(events,"null");return;}
 auto& b=it->second;bool saved=savedShaders.contains(shader);
 if(!saved && savedShaderBytes+b.size<=32*1024*1024){auto name=std::to_wstring(stage)+L"-shader-"+std::to_wstring(b.index)+L".dxbc";FILE* f=_wfsopen((captureDirectory/name).c_str(),L"wb",_SH_DENYNO);if(f){saved=fwrite(b.code,b.size,1,f)==1;fclose(f);if(saved){savedShaders.insert(shader);savedShaderBytes+=b.size;}}}
 fprintf(events,"{\"index\":%u,\"rva\":%llu,\"size\":%zu,\"saved\":%s}",b.index,static_cast<unsigned long long>(b.code-image),b.size,saved?"true":"false");
}
#ifdef ACUNITYVR_BATCH_PROFILE
// Exact mapped code proves job+0x10 is the task descriptor and job+0x28 the
// command list. Tag only; never change the descriptor, lanes, list or payload.
unsigned batchCaptures[3]{};std::atomic<uint64_t> batchRecovered{},batchMissed{};
bool ReadBatch(void* job,NativeGpuTiming::NativeBatchInfo* info){__try{
 auto j=static_cast<uint8_t*>(job);auto d=Field<uint8_t*>(j,0x10);auto base=reinterpret_cast<uintptr_t>(image);auto address=reinterpret_cast<uintptr_t>(d);
 if(address<base || address>=base+0x7825000)return false;
 info->job=reinterpret_cast<uintptr_t>(job);info->descriptorRva=unsigned(address-base);info->flags=Field<unsigned>(d,0x18);info->laneBegin=Field<unsigned>(j,8);info->laneEnd=Field<unsigned>(j,0xc);
 info->payload0=Field<uintptr_t>(j,0x40);info->payload1=Field<uintptr_t>(j,0x48);auto table=Field<uint8_t*>(d,0);auto finish=Field<uintptr_t>(table,0x20);auto tableAddress=reinterpret_cast<uintptr_t>(table);
 if(tableAddress>=base && tableAddress<base+0x7825000)info->tableRva=unsigned(tableAddress-base);
 if(finish>=base && finish<base+0x7825000)info->finishRva=unsigned(finish-base);
 for(auto& p:passes)if(info->descriptorRva==p.descriptor){info->tableRva=unsigned(p.table);info->finishRva=unsigned(p.finish);break;}
 auto name=Field<const char*>(d,0x10);if(name)for(unsigned i=0;i<sizeof(info->name)-1;++i){char x=name[i];if(!x)break;info->name[i]=(x>=32 && x<=126 && x!='"' && x!='\\')?x:'?';}
 return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){*info={};return false;}}
// Virtual unwinding changes only this local CONTEXT. NHANDLER invokes no handler.
// https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlvirtualunwind
__declspec(noinline) bool ReadBatchFromCaller(NativeGpuTiming::NativeBatchInfo* info){__try{
 CONTEXT context{};RtlCaptureContext(&context);auto tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());
 auto low=reinterpret_cast<uintptr_t>(tib->StackLimit),high=reinterpret_cast<uintptr_t>(tib->StackBase);auto base=reinterpret_cast<uintptr_t>(image);
 for(unsigned frame=0;frame<28 && context.Rip;++frame){
  if(context.Rsp<low || context.Rsp>high-8 || (context.Rsp&7))return false;
  // Native22f7880 preserves the caller's RDI. At its return address22a5464,
  // recovered RDI is the original native job, proven by exact mapped code.
  if(context.Rip==base+0x22a5464)return ReadBatch(reinterpret_cast<void*>(context.Rdi),info);
  DWORD64 module{};auto entry=RtlLookupFunctionEntry(context.Rip,&module,nullptr);auto previous=context.Rsp;
  if(entry){PVOID handler{};DWORD64 establisher{};RtlVirtualUnwind(UNW_FLAG_NHANDLER,module,context.Rip,entry,&context,&handler,&establisher,nullptr);}
  else{context.Rip=Field<DWORD64>(reinterpret_cast<void*>(context.Rsp),0);context.Rsp+=8;}
  if(context.Rsp<=previous)return false;
 }
 return false;
 }__except(EXCEPTION_EXECUTE_HANDLER){*info={};return false;}}
bool ReadBatchFast77(void* returnSlot,ID3D11DeviceContext* context,ID3D11CommandList* list,NativeGpuTiming::NativeBatchInfo* info){
 auto tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());uintptr_t job{};auto slot=reinterpret_cast<uintptr_t>(returnSlot),base=reinterpret_cast<uintptr_t>(image);
 auto recover=[&](uintptr_t& result){return BatchCaller77::Recover(slot,reinterpret_cast<uintptr_t>(tib->StackLimit),reinterpret_cast<uintptr_t>(tib->StackBase),base+0x22f78a4,base+0x22a5464,reinterpret_cast<uintptr_t>(context),reinterpret_cast<uintptr_t>(list),result,ReadPointer77);};
 if(!recover(job) || !ReadBatch(reinterpret_cast<void*>(job),info))return false;
 uintptr_t again{};if(!recover(again) || again!=job){*info={};return false;}return true;
}
bool CopyBatchBytes(uintptr_t pointer,void* data,size_t count){
 MEMORY_BASIC_INFORMATION region{};if(!pointer || !VirtualQuery(reinterpret_cast<void*>(pointer),&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 auto begin=reinterpret_cast<uintptr_t>(region.BaseAddress);if(pointer<begin || pointer-begin>region.RegionSize || count>region.RegionSize-(pointer-begin))return false;
 __try{memcpy(data,reinterpret_cast<void*>(pointer),count);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#ifdef ACUNITYVR_SUN_CAPTURE
struct SunCpuPointers{uintptr_t root{},cull{},list{};unsigned family{},cascade{};};
bool ReadSunCpu(unsigned descriptor,SunCpuPointers* out){__try{
 if(descriptor!=0x5049ac0 && descriptor!=0x5049b00 && descriptor!=0x5049b40 && descriptor!=0x5049b80)return false;
 auto d=image+descriptor;out->family=Field<unsigned>(d,0x28);out->cascade=Field<unsigned>(d,0x2c);if(out->family>5 || out->cascade>8)return false;
 out->root=Field<uintptr_t>(backend,0x4a0);out->cull=Field<uintptr_t>(backend,0x13e8);auto array=Field<uintptr_t>(reinterpret_cast<void*>(out->cull),0x8220);
 if(!array)return false;out->list=array+0x58*(out->family+6*out->cascade);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){*out={};return false;}}
void CaptureSunCpu(unsigned view,unsigned index,const NativeGpuTiming::NativeBatchInfo& info){
 SunCpuPointers pointers{};if(!ReadSunCpu(info.descriptorRva,&pointers))return;uint8_t bytes[0x700]{};
 auto prefix=L"batch-view-"+std::to_wstring(view)+L"-"+std::to_wstring(index);
 const struct{const wchar_t* name;uintptr_t pointer;size_t size;} blocks[]={{L"sun-root",pointers.root,0x700},{L"cull-state",pointers.cull,0x100},{L"cull-header",pointers.list,0x58},{L"descriptor-full",reinterpret_cast<uintptr_t>(image)+info.descriptorRva,0x40}};
 for(auto& block:blocks)if(CopyBatchBytes(block.pointer,bytes,block.size)){
  FILE* f=_wfsopen((captureDirectory/(prefix+L"-"+block.name+L".bin")).c_str(),L"wb",_SH_DENYNO);if(f){fwrite(bytes,block.size,1,f);fclose(f);}
 }
 fprintf(events,"{\"event\":\"sunCpu\",\"view\":%u,\"batchId\":%u,\"descriptorRva\":%u,\"root\":\"%llx\",\"cullState\":\"%llx\",\"listHeader\":\"%llx\",\"family\":%u,\"cascade\":%u,\"note\":\"CPU state at native list execution; may differ from deferred recording time\"}\n",view,index,info.descriptorRva,static_cast<unsigned long long>(pointers.root),static_cast<unsigned long long>(pointers.cull),static_cast<unsigned long long>(pointers.list),pointers.family,pointers.cascade);
}
#endif
void CaptureBatch(unsigned view,ID3D11CommandList* list,BOOL restore,const NativeGpuTiming::NativeBatchInfo& info){
 if(!capturing || !info.descriptorRva || view>=3 || batchCaptures[view]>=64)return;
 unsigned index=++batchCaptures[view];auto& b=info;uint8_t bytes[256]{};
#ifdef ACUNITYVR_SUN_CAPTURE
 CaptureSunCpu(view,index,info);
#endif
 const struct{const wchar_t* label;uintptr_t pointer;size_t bytes;} blocks[]={{L"job",b.job,0x60},{L"descriptor",reinterpret_cast<uintptr_t>(image)+b.descriptorRva,0x28},{L"payload0",b.payload0,0x100},{L"payload1",b.payload1,0x100}};
 for(auto& block:blocks)if(CopyBatchBytes(block.pointer,bytes,block.bytes)){
  auto path=captureDirectory/(L"batch-view-"+std::to_wstring(view)+L"-"+std::to_wstring(index)+L"-"+block.label+L".bin");FILE* f=_wfsopen(path.c_str(),L"wb",_SH_DENYNO);if(f){fwrite(bytes,block.bytes,1,f);fclose(f);}
 }
 fprintf(events,"{\"event\":\"commandList\",\"view\":%u,\"batchId\":%u,\"descriptorRva\":%u,\"finishRva\":%u,\"name\":\"%s\",\"flags\":%u,\"laneBegin\":%u,\"laneEnd\":%u,\"list\":\"%p\",\"restoreContextState\":%s,\"payload0\":\"%llx\",\"payload1\":\"%llx\"}\n",view,index,b.descriptorRva,b.finishRva,b.name,b.flags,b.laneBegin,b.laneEnd,list,restore?"true":"false",static_cast<unsigned long long>(b.payload0),static_cast<unsigned long long>(b.payload1));
}
#endif
struct ShaderGpuScope{
 unsigned token=UINT_MAX;
 ShaderGpuScope(ID3D11DeviceContext* c,unsigned kind){
  if(c!=immediate.Get() || !NativeGpuTiming::Recording())return;
  if(commandListReplayDepth){++replayDrawsSkipped;return;}unsigned view{};
  if(!Identify(Field<void*>(backend,0x20),&view))return;
  IUnknown* shader{};ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11ComputeShader> cs;
  if(kind<41){c->PSGetShader(&ps,nullptr,nullptr);shader=ps.Get();}else{c->CSGetShader(&cs,nullptr,nullptr);shader=cs.Get();}
  if(!shader){++emptyShaderDrawsSkipped;
#ifdef ACUNITYVR_DEPTH_PROFILE
   #ifndef ACUNITYVR_BATCH_PROFILE
   if(kind<41){ComPtr<ID3D11VertexShader> vs;c->VSGetShader(&vs,nullptr,nullptr);auto it=nativeShaders.find(vs.Get());unsigned index=it==nativeShaders.end()?UINT_MAX:it->second.index;NativeGpuTiming::RecordNullPixelDraw(c,view,current.pass?unsigned(current.pass->descriptor):0,kind,index);}
#endif
#endif
   return;}
  auto it=nativeShaders.find(shader);unsigned index=it==nativeShaders.end()?UINT_MAX:it->second.index;
  token=NativeGpuTiming::BeginEvent(kind<41?3:4,current.pass?unsigned(current.pass->descriptor):0,kind,c,view,index);
 }
 ~ShaderGpuScope(){NativeGpuTiming::EndEvent(token);}
};
#else
struct ShaderGpuScope{ShaderGpuScope(ID3D11DeviceContext*,unsigned){}};
#endif
#ifdef ACUNITYVR_SUN_CAPTURE
void CaptureSunTexture(ID3D11DeviceContext* c,unsigned view,ID3D11PixelShader* ps){
 if(view>=3 || !view || sunCopied[view] || c!=immediate.Get() || !ps)return;
 auto found=nativeShaders.find(ps);if(found==nativeShaders.end() || (found->second.index!=276 && found->second.index!=278))return;
 ComPtr<ID3D11ShaderResourceView> srv;c->PSGetShaderResources(14,1,&srv);if(!srv)return;
 ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return;
 D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);if(d.Format!=DXGI_FORMAT_R32_TYPELESS || d.SampleDesc.Count!=1 || d.ArraySize!=1 || d.MipLevels!=1 || d.Width<1 || d.Height<1 || d.Width>4096 || d.Height>4096)return;
 auto staging=d;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.MiscFlags=0;
 SunTextureCopy copy{};copy.desc=d;copy.view=view;auto result=device->CreateTexture2D(&staging,nullptr,&copy.staging);
 if(FAILED(result)){fprintf(events,"{\"event\":\"sunTextureError\",\"view\":%u,\"HRESULT\":%ld}\n",view,result);return;}
 c->CopyResource(copy.staging.Get(),texture.Get());sunCopies.push_back(std::move(copy));sunCopied[view]=true;
 fprintf(events,"{\"event\":\"sunTexture\",\"view\":%u,\"draw\":%u,\"shaderIndex\":%u,\"slot\":14,\"resource\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u,\"bytesPerPixel\":4}\n",view,draws,found->second.index,texture.Get(),d.Width,d.Height,unsigned(d.Format));
}
#endif
#ifdef ACUNITYVR_DEPTH_PROFILE
bool CaptureNullScope(ID3D11DeviceContext* c,unsigned kind){
 if(kind>=41 || c!=immediate.Get())return false;auto context=Field<void*>(backend,0x20);unsigned view{};if(!Identify(context,&view) || nullCaptureKeys[view].size()>=64)return false;
 ComPtr<ID3D11PixelShader> ps;c->PSGetShader(&ps,nullptr,nullptr);if(ps)return false;
 ComPtr<ID3D11RenderTargetView> color;ComPtr<ID3D11DepthStencilView> depth;c->OMGetRenderTargets(1,&color,&depth);ComPtr<ID3D11Buffer> stream;c->SOGetTargets(1,&stream);if(!depth && !stream)return false;
 NullCaptureKey key{reinterpret_cast<uintptr_t>(color.Get()),reinterpret_cast<uintptr_t>(depth.Get()),reinterpret_cast<uintptr_t>(stream.Get()),{}};UINT n=1;c->RSGetViewports(&n,&key.viewport);
 for(const auto& previous:nullCaptureKeys[view])if(previous.Same(key))return false;nullCaptureKeys[view].push_back(key);current={&nullPixelUnscoped,context,view};return true;
}
#endif
void Observe(ID3D11DeviceContext* c,unsigned kind,UINT a,UINT b,bool suppressed=false){
 EffectDrawCapture74::Observe(c,kind,a,b,commandListReplayDepth!=0);
 if(!capturing)return;PreserveScope scope;
 if(!current.pass && CloudProducer(c,kind)){auto context=Field<void*>(backend,0x20);unsigned view{};if(Identify(context,&view))current={&cloudUnscoped,context,view};}
 if(!current.pass && IsFinalOutput(c,kind)){auto context=Field<void*>(backend,0x20);unsigned view{};if(Identify(context,&view))current={&finalUnscoped,context,view};}
#ifdef ACUNITYVR_DEPTH_PROFILE
 #ifndef ACUNITYVR_BATCH_PROFILE
 if(!current.pass)CaptureNullScope(c,kind);
#endif
#endif
 if(!current.pass || current.view>=3)return;
 if(c!=immediate.Get()){++otherContexts;return;}
 const bool finalTarget64=IsFinalOutput(c,kind);const bool ui64=kind<41 && (!strcmp(current.pass->name,"UI3DPass") || !strcmp(current.pass->name,"preUIAndVideo") || !strcmp(current.pass->name,"videoBlitPass"));
 const bool terminal64=kind<41 && (finalTarget64 || ui64 || !strcmp(current.pass->name,"PostEffects"));
 if(terminal64){if(terminalDraws64[current.view]>=TerminalDrawLimit64){++terminalSkipped64;return;}++terminalDraws64[current.view];}else if(drawsByView[current.view]>=DrawLimit)return;

 ++draws;++drawsByView[current.view];
 Snapshot camera{};if(Read(current.context,current.view,2,&camera)){
  auto name=L"draw-"+std::to_wstring(draws)+L"-camera.bin";FILE* f=_wfsopen((captureDirectory/name).c_str(),L"wb",_SH_DENYNO);if(f){fwrite(&camera,sizeof(camera),1,f);fclose(f);}else ++errors;
 }else ++errors;
 ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11ComputeShader> cs;ComPtr<ID3D11GeometryShader> gs;
 c->GSGetShader(&gs,nullptr,nullptr);c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);c->CSGetShader(&cs,nullptr,nullptr);
  CaptureSceneDepth46(c,cs.Get(),kind);
 if(kind<41 && !suppressed)CloudInput(c,ps.Get());
#ifdef ACUNITYVR_SUN_CAPTURE
 if(kind<41 && !suppressed)CaptureSunTexture(c,current.view,ps.Get());
#endif
 ID3D11RenderTargetView* rt[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};ComPtr<ID3D11DepthStencilView> depth;c->OMGetRenderTargets(_countof(rt),rt,&depth);
 D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};UINT n=_countof(vp);c->RSGetViewports(&n,vp);
 fprintf(events,"{\"event\":\"draw\",\"id\":%u,\"pass\":\"%s\",\"view\":%u,\"kind\":%u,\"a\":%u,\"b\":%u,\"shaderSuppressed\":%s,\"vs\":\"%p\",\"ps\":\"%p\",\"cs\":\"%p\",\"targets\":[",draws,current.pass->name,current.view,kind,a,b,suppressed?"true":"false",vs.Get(),ps.Get(),cs.Get());
 for(unsigned i=0;i<_countof(rt);++i){if(i)fprintf(events,",");Resource(rt[i]);if(rt[i])rt[i]->Release();}
 fprintf(events,"],\"depth\":");Resource(depth.Get());fprintf(events,",\"viewports\":[");
 for(UINT i=0;i<n;++i)fprintf(events,"%s[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]",i?",":"",vp[i].TopLeftX,vp[i].TopLeftY,vp[i].Width,vp[i].Height,vp[i].MinDepth,vp[i].MaxDepth);
 fprintf(events,"]");
#ifdef ACUNITYVR_SHADER_PROFILE
 SaveNativeShader(vs.Get(),0);SaveNativeShader(ps.Get(),1);SaveNativeShader(cs.Get(),2);SaveNativeShader(gs.Get(),3);
#endif
 fprintf(events,",\"terminal64\":%s,\"finalTarget64\":%s,\"ui64\":%s",terminal64?"true":"false",finalTarget64?"true":"false",ui64?"true":"false");
 RasterState(c);if(terminal64)Geometry(c,true);else if(current.pass==&finalUnscoped)Geometry(c);
#ifdef ACUNITYVR_SHADER_PROFILE
 auto gridVs=nativeShaders.find(vs.Get()),gridPs=nativeShaders.find(ps.Get());
 if(kind==20 && !strcmp(current.pass->name,"DeferredLighting") && gridVs!=nativeShaders.end() && gridPs!=nativeShaders.end() && gridVs->second.index==57 && gridPs->second.index==301 && !gridGeometry67[current.view]){gridGeometry67[current.view]=true;Geometry(c,true);}
#endif

 fprintf(events,",\"buffersGS\":");if(kind<41 && current.pass->descriptor==0x504a0b0)Buffers(5);else fprintf(events,"[]");
 fprintf(events,",\"buffersVS\":");if(kind<41)Buffers(0,terminal64);else fprintf(events,"[]");fprintf(events,",\"buffersPS\":");if(kind<41)Buffers(1,terminal64);else fprintf(events,"[]");fprintf(events,",\"buffersCS\":");if(kind>=41)Buffers(2);else fprintf(events,"[]");
#ifdef ACUNITYVR_DEPTH_PROFILE
 if(current.pass==&nullPixelUnscoped){fprintf(events,",\"resourcesVS\":");Resources(0);}
#endif
 fprintf(events,",\"resourcesGS\":");if(kind<41 && current.pass->descriptor==0x504a0b0){Resources(3);auto gridGS=nativeShaders.find(gs.Get());if(kind==20 && gridVs!=nativeShaders.end() && gridVs->second.index==57 && gridGS!=nativeShaders.end() && gridGS->second.index>=1 && gridGS->second.index<=5)tileCapture68.Input(c,current.view,draws,(RenderSize::eyeWidth+15)/16,(RenderSize::eyeHeight+15)/16);}else fprintf(events,"[]");
 fprintf(events,",\"resourcesPS\":");Resources(1);fprintf(events,",\"resourcesCS\":");Resources(2);fprintf(events,",\"unorderedAccessCS\":");if(kind>=41)UnorderedAccess();else fprintf(events,"[]");fprintf(events,",\"stack\":[");
 void* stack[28]{};auto count=CaptureStackBackTrace(0,_countof(stack),stack,nullptr);bool comma=false;
 for(USHORT i=0;i<count;++i){auto address=reinterpret_cast<uintptr_t>(stack[i]);auto base=reinterpret_cast<uintptr_t>(image);if(address>=base && address<base+0x7825000){fprintf(events,"%s\"%llx\"",comma?",":"",static_cast<unsigned long long>(address-base));comma=true;}}
 fprintf(events,"]}\n");
 if(terminal64){
  if(finalTarget64)for(unsigned slot=0;slot<2;++slot){auto status=terminalPixels64.Input(c,current.view,draws,slot);if(status!=TerminalCapture64::Status::NoTexture && status!=TerminalCapture64::Status::Duplicate)fprintf(events,"{\"event\":\"terminal64InputRequest\",\"draw\":%u,\"view\":%u,\"slot\":%u,\"status\":%u}\n",draws,current.view,slot,unsigned(status));}
  if(ui64){auto status=terminalPixels64.Target(c,current.view,draws);if(status!=TerminalCapture64::Status::Duplicate)fprintf(events,"{\"event\":\"terminal64UiTargetRequest\",\"draw\":%u,\"view\":%u,\"status\":%u}\n",draws,current.view,unsigned(status));}
 }
}
bool Suppressed(ID3D11DeviceContext* c,unsigned kind,UINT a,UINT b){
 if(!ShaderHunt::Enabled() || commandListReplayDepth)return false;
 if(c!=immediate.Get())return ShaderHunt::Skip(c,kind,a,b,"worker-recording",3,nullptr);
 auto context=current.context;unsigned view=current.pass?current.view:3;
 if(!context){auto serial=Field<void*>(backend,0x20);unsigned index{};if(Identify(serial,&index)){context=serial;view=index;}}
 return ShaderHunt::Skip(c,kind,a,b,current.pass?current.pass->name:nullptr,view,context);
}
using Indexed=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);Indexed indexed{};
using Draw=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);Draw draw{};
using IndexedInst=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);IndexedInst indexedInst{};
using Inst=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);Inst inst{};
using Indirect=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,UINT);Indirect indexedIndirect{},indirect{},dispatchIndirect{};
using Auto=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*);Auto drawAuto{};
using Dispatch=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);Dispatch dispatch{};
#ifdef ACUNITYVR_NATIVE_FOVEATION
struct FoveationDrawScope {
 NativeFoveation::DrawScope scope;
 explicit FoveationDrawScope(ID3D11DeviceContext* c){
  if(c!=immediate.Get() || commandListReplayDepth || !current.pass || current.phase!=4 || !NativeFoveation::NeedsScope())return;
#ifdef ACUNITYVR_VISIBILITY_MASK
  if(current.pass->descriptor!=0x504a0b0 && current.pass->descriptor!=0x504a150 && current.pass->descriptor!=0x504a010 && current.pass->descriptor!=0x504a038 && current.pass->descriptor!=0x504a100)return;
#else
  if(current.pass->descriptor!=0x504a0b0 && current.pass->descriptor!=0x504a150)return;
#endif
  ComPtr<ID3D11PixelShader> ps;c->PSGetShader(&ps,nullptr,nullptr);auto it=nativeShaders.find(ps.Get());
  if(it!=nativeShaders.end())scope.Apply(c,current.view,unsigned(current.pass->descriptor),it->second.index);
 }
};
#else
struct FoveationDrawScope {explicit FoveationDrawScope(ID3D11DeviceContext*){}};
#endif
void STDMETHODCALLTYPE HookIndexed(ID3D11DeviceContext* c,UINT n,UINT s,INT b){ShaderGpuScope timing(c,12);bool skip=Suppressed(c,12,n,s);Observe(c,12,n,s,skip);if(!skip){FoveationDrawScope foveation(c);indexed(c,n,s,b);}}
void STDMETHODCALLTYPE HookDraw(ID3D11DeviceContext* c,UINT n,UINT s){
 ShaderGpuScope timing(c,13);bool skip=Suppressed(c,13,n,s) || EffectDrawCapture74::RemoveFog79(c,13,n,s,commandListReplayDepth!=0);Observe(c,13,n,s,skip);if(skip)return;FoveationDrawScope foveation(c);
#ifdef ACUNITYVR_POST_AA
 if(current.pass && PostAAOptimization::Draw(c,current.view,unsigned(current.pass->descriptor),n,s,draw,capturing.load()))return;
#endif
 draw(c,n,s);
}
void STDMETHODCALLTYPE HookIndexedInst(ID3D11DeviceContext* c,UINT n,UINT i,UINT s,INT b,UINT a){
 ShaderGpuScope timing(c,20);bool skip=Suppressed(c,20,n,i) || EffectDrawCapture74::Isolate(c,20,n,i,commandListReplayDepth!=0);Observe(c,20,n,i,skip);if(skip)return;FoveationDrawScope foveation(c);
 unsigned vsIndex=UINT_MAX,psIndex=UINT_MAX;
 if(c==immediate.Get() && !commandListReplayDepth && current.pass && current.phase==4 && current.pass->descriptor==0x504a0b0 && current.view>0 && current.view<3){
  ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);
  auto v=nativeShaders.find(vs.Get()),p=nativeShaders.find(ps.Get());if(v!=nativeShaders.end())vsIndex=v->second.index;if(p!=nativeShaders.end())psIndex=p->second.index;
 }
 lightingGrid67.Execute(c,current.view,current.pass?unsigned(current.pass->descriptor):0,vsIndex,psIndex,n,i,s,b,a,indexedInst,capturing.load());
}
void STDMETHODCALLTYPE HookInst(ID3D11DeviceContext* c,UINT n,UINT i,UINT s,UINT a){ShaderGpuScope timing(c,21);bool skip=Suppressed(c,21,n,i);Observe(c,21,n,i,skip);if(!skip){FoveationDrawScope foveation(c);inst(c,n,i,s,a);}}
void STDMETHODCALLTYPE HookIndexedIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT o){ShaderGpuScope timing(c,39);bool skip=Suppressed(c,39,o,0) || EffectDrawCapture74::Isolate(c,39,o,0,commandListReplayDepth!=0,b);Observe(c,39,o,0,skip);if(!skip){FoveationDrawScope foveation(c);indexedIndirect(c,b,o);}}
void STDMETHODCALLTYPE HookIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT o){ShaderGpuScope timing(c,40);bool skip=Suppressed(c,40,o,0);Observe(c,40,o,0,skip);if(!skip){FoveationDrawScope foveation(c);indirect(c,b,o);}}
void STDMETHODCALLTYPE HookDrawAuto(ID3D11DeviceContext* c){ShaderGpuScope timing(c,38);bool skip=Suppressed(c,38,0,0);Observe(c,38,0,0,skip);if(!skip){FoveationDrawScope foveation(c);drawAuto(c);}}
Dispatch CurrentDispatch(){auto native=DispatchRoute76::Scope<Dispatch>::Current();return native?native:dispatch;}
void STDMETHODCALLTYPE ClassifierDispatch69(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){Observe(c,41,x,y);CurrentDispatch()(c,x,y,z);}
void DispatchBody(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){
 if(Suppressed(c,41,x,y)){Observe(c,41,x,y,true);return;}
 if(c==immediate.Get() && !commandListReplayDepth && current.pass && current.phase==4 && current.pass->descriptor==0x504a2e8 && current.view>0 && current.view<3){
  ComPtr<ID3D11ComputeShader> shader;c->CSGetShader(&shader,nullptr,nullptr);auto it=nativeShaders.find(shader.Get());unsigned index=it!=nativeShaders.end()?it->second.index:UINT_MAX;
  ShaderGpuScope timing(c,41);lightingTiles69.Execute(c,current.context,current.view,unsigned(current.pass->descriptor),index,x,y,z,ClassifierDispatch69,capturing.load());return;
 }

#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 OcclusionProbe::Observe(c,41,x,y,z);
#endif
CloudProjection::Scope constants(c,x,y,z);ShaderGpuScope timing(c,41);Observe(c,41,x,y);CurrentDispatch()(c,x,y,z);}
constexpr unsigned DispatchVariantLimit76=8;
struct DispatchVariant76 {void* target{};Dispatch native{};bool created{};std::atomic<uint64_t> calls{},nested{};};
std::array<DispatchVariant76,DispatchVariantLimit76> dispatchVariants76;
std::atomic<unsigned> dispatchVariantCount76{};SRWLOCK dispatchVariantLock76=SRWLOCK_INIT;
void DispatchRouted76(Dispatch native,ID3D11DeviceContext* c,UINT x,UINT y,UINT z,std::atomic<uint64_t>* nested=nullptr){
 DispatchRoute76::Scope<Dispatch> route(native);if(!route.Outermost()){if(nested)++*nested;native(c,x,y,z);return;}DispatchBody(c,x,y,z);
}
void STDMETHODCALLTYPE HookDispatch(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){DispatchRouted76(dispatch,c,x,y,z);}
template<unsigned I>void STDMETHODCALLTYPE DispatchVariantHook76(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){auto& item=dispatchVariants76[I];const auto n=++item.calls;if(n==1 && log){fprintf(log,"ComputeDispatch76 variant%u first call target=%p context=%p groups=%u/%u/%u\n",I,item.target,c,x,y,z);fflush(log);}DispatchRouted76(item.native,c,x,y,z,&item.nested);}
void* dispatchHooks76[]{reinterpret_cast<void*>(DispatchVariantHook76<0>),reinterpret_cast<void*>(DispatchVariantHook76<1>),reinterpret_cast<void*>(DispatchVariantHook76<2>),reinterpret_cast<void*>(DispatchVariantHook76<3>),reinterpret_cast<void*>(DispatchVariantHook76<4>),reinterpret_cast<void*>(DispatchVariantHook76<5>),reinterpret_cast<void*>(DispatchVariantHook76<6>),reinterpret_cast<void*>(DispatchVariantHook76<7>)};
void STDMETHODCALLTYPE HookDispatchIndirect(ID3D11DeviceContext* c,ID3D11Buffer* b,UINT o){
#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 OcclusionProbe::Observe(c,42,o,0,0);
#endif
ShaderGpuScope timing(c,42);bool skip=Suppressed(c,42,o,0);Observe(c,42,o,0,skip);if(!skip)dispatchIndirect(c,b,o);}
#ifdef ACUNITYVR_FULL_PASS_PROFILE
using ExecuteList=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11CommandList*,BOOL);
using Resolve=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,ID3D11Resource*,UINT,DXGI_FORMAT);
ExecuteList executeList{};Resolve resolve{};
void STDMETHODCALLTYPE HookExecuteList(ID3D11DeviceContext* c,ID3D11CommandList* list,BOOL restore){
#ifdef ACUNITYVR_BATCH_PROFILE
 bool geometryCandidate=false;
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 geometryCandidate=c==immediate.Get() && NativeFoveation::GeometryRequested() && _ReturnAddress()==image+0x22f78a4;
#endif
 NativeGpuTiming::NativeBatchInfo batch{};if(c==immediate.Get() && (NativeGpuTiming::Recording() || capturing || geometryCandidate)){
  bool recovered=fastPaths77 && fastBatchSetting77 && ReadBatchFast77(_AddressOfReturnAddress(),c,list,&batch);
  if(recovered)++fastBatchHits77;else{++slowBatchAttempts77;recovered=ReadBatchFromCaller(&batch);}
  if(recovered)++batchRecovered;else ++batchMissed;
 }
#endif
 auto token=NativeGpuTiming::BeginEvent(1,
#ifdef ACUNITYVR_BATCH_PROFILE
 batch.descriptorRva,
#else
 0,
#endif
 0,c);
#ifdef ACUNITYVR_BATCH_PROFILE
 if(c==immediate.Get()){NativeGpuTiming::AddNativeBatch(token,batch,reinterpret_cast<uintptr_t>(list),restore!=FALSE);unsigned view{};if(Identify(Field<void*>(backend,0x20),&view))CaptureBatch(view,list,restore,batch);}
#endif
#ifdef ACUNITYVR_DEPTH_PROFILE
 NativeGpuTiming::AddNativeStack(token);
#endif
 bool serial=c==immediate.Get();
 {
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 NativeFoveation::CommandScope foveation;unsigned view{};
 if(geometryCandidate && Identify(Field<void*>(backend,0x20),&view))foveation.Apply(c,view,batch.descriptorRva,batch.tableRva,batch.flags,batch.laneBegin,batch.laneEnd);
#endif
 if(serial)++commandListReplayDepth;executeList(c,list,restore);if(serial)--commandListReplayDepth;
 }
 NativeGpuTiming::EndEvent(token);
}
void STDMETHODCALLTYPE HookResolve(ID3D11DeviceContext* c,ID3D11Resource* dst,UINT dstSub,ID3D11Resource* src,UINT srcSub,DXGI_FORMAT format){auto token=NativeGpuTiming::BeginEvent(2,0,0,c);resolve(c,dst,dstSub,src,srcSub,format);NativeGpuTiming::EndEvent(token);}
constexpr unsigned HookCount=11;
#else
constexpr unsigned HookCount=9;
#endif
void* targets[HookCount]{};uint8_t originalCode[HookCount][16]{};unsigned created{};bool descriptorsInstalled[_countof(passes)]{};
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool D3DCode(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.AllocationBase==GetModuleHandleW(L"d3d11.dll") && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE));}
bool DispatchSnapshot76(void*** tableOut,void** targetOut){__try{
 if(!immediate)return false;auto table=*reinterpret_cast<void***>(immediate.Get());MEMORY_BASIC_INFORMATION m{};
 if(!table || !VirtualQuery(table,&m,sizeof(m)) || m.State!=MEM_COMMIT || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll") || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 auto target=table[41];if(!D3DCode(target) || *reinterpret_cast<void***>(immediate.Get())!=table || table[41]!=target)return false;*tableOut=table;*targetOut=target;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool EnsureDispatchCoverage(){
 if(!enabled || created<8)return false;
 if(fastPaths77 && fastDispatchSetting77){uintptr_t knownTable{},knownTarget{};
  auto known=[](uintptr_t p){if(p==reinterpret_cast<uintptr_t>(targets[7]))return true;for(unsigned i=0;i<dispatchVariantCount76.load(std::memory_order_acquire);++i)if(p==reinterpret_cast<uintptr_t>(dispatchVariants76[i].target))return true;return false;};
  if(DispatchKnown77::ReadKnown(reinterpret_cast<uintptr_t>(immediate.Get()),knownTable,knownTarget,ReadPointer77,known)){++knownDispatchHits77;return true;}
 }
 ++fullDispatchChecks77;void** table{};void* target{};if(!DispatchSnapshot76(&table,&target))return false;
 if(target==targets[7])return true;for(unsigned i=0;i<dispatchVariantCount76.load(std::memory_order_acquire);++i)if(dispatchVariants76[i].target==target)return true;
 AcquireSRWLockExclusive(&dispatchVariantLock76);bool ok=false;
 if(!enabled || !DispatchSnapshot76(&table,&target)){ReleaseSRWLockExclusive(&dispatchVariantLock76);return false;}
 if(target==targets[7]){ReleaseSRWLockExclusive(&dispatchVariantLock76);return true;}
 unsigned count=dispatchVariantCount76.load(std::memory_order_relaxed);for(unsigned i=0;i<count;++i)if(dispatchVariants76[i].target==target){ReleaseSRWLockExclusive(&dispatchVariantLock76);return true;}
 if(count<DispatchVariantLimit76){auto& item=dispatchVariants76[count];item.target=target;auto status=MH_CreateHook(target,dispatchHooks76[count],reinterpret_cast<void**>(&item.native));
  if(status==MH_OK){void** againTable{};void* againTarget{};
   if(DispatchSnapshot76(&againTable,&againTarget) && againTable==table && againTarget==target && MH_EnableHook(target)==MH_OK){item.created=true;dispatchVariantCount76.store(count+1,std::memory_order_release);ok=true;if(log){fprintf(log,"ComputeDispatch76 variant%u installed from current owned immediate context official slot41 target=%p; primary=%p; normal/query variants share one outer correction scope\n",count,target,targets[7]);fflush(log);}}
   else {MH_RemoveHook(target);item.native=nullptr;item.target=nullptr;}
  }
 }
 static unsigned refusals{};if(!ok && log && (++refusals<=8 || (refusals&(refusals-1))==0)){fprintf(log,"REFUSED ComputeDispatch76 new target=%p variants=%u refusals=%u; previous native entry points retained\n",target,count,refusals);fflush(log);}ReleaseSRWLockExclusive(&dispatchVariantLock76);return ok;
}
bool Install(uint8_t* base,void* target,void* source,const std::filesystem::path& output,FILE* report){
 image=base;surface=target;views[0]=source;directory=output/L"draw-captures";log=report;
 auto performanceIni=output/L"ACUnityVR.ini";fastBatchSetting77=GetPrivateProfileIntW(L"Performance",L"FastBatchLookup",1,performanceIni.c_str())!=0;fastDispatchSetting77=GetPrivateProfileIntW(L"Performance",L"FastDispatchCheck",1,performanceIni.c_str())!=0;
 performance77=_wfsopen((output/L"cpu-fastpaths77.jsonl").c_str(),L"w",_SH_DENYNO);
 auto renderer=Field<uint8_t*>(image,0x524cfd8);if(!renderer)return false;backend=Field<uint8_t*>(renderer,0x10);
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 auto unknown=acu::OutputInterface(surface);ComPtr<ID3D11View> v;ComPtr<ID3D11Resource> r;
 if(!unknown || FAILED(unknown->QueryInterface(IID_PPV_ARGS(&v))))return false;v->GetResource(&r);r->GetDevice(&device);device->GetImmediateContext(&immediate);
 if(!immediate || immediate->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;immediate.As(&immediate1);
 #ifdef ACUNITYVR_SHADER_PROFILE
 if(!InitializeShaderLookup(output))return false;

#endif

#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 if(!OcclusionProbe::Initialize(image,surface,source,output,log))return false;
#ifdef ACUNITYVR_GPU_CONSTANTS40
 if(!OcclusionProbe::InitializeGpuConstants40(device.Get(),immediate.Get()))return false;
#endif
#endif
 if(!InstallClouds() || !NativeGpuTiming::Initialize(device.Get(),immediate.Get(),output,log))return false;
#ifdef ACUNITYVR_POST_AA
 if(!PostAAOptimization::Initialize(image,device.Get(),immediate.Get(),output,log))return false;
#endif
 for(auto& p:passes){auto slot=reinterpret_cast<void**>(image+p.descriptor);auto table=reinterpret_cast<void**>(image+p.table);
  if(!Writable(slot) || *slot!=table || table[0]!=image+0x2287f60 || table[1]!=image+p.lanes || table[2]!=image+p.start || table[3]!=image+p.worker || table[4]!=image+p.finish)return false;
  memcpy(p.clone,table,sizeof(p.clone));p.nativeStart=reinterpret_cast<Task>(table[2]);p.native=reinterpret_cast<Task>(table[4]);
 }
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={
 {0x22af9d0,"\x48\x8b\x41\x20\x8d\x14\xd5\x00\x00\x00\x00\x48\x8b\x04\x02\xc3",16},
 {0x33e52c0,"\x44\x58\x42\x43\x99\xf9\x54\xa0\x88\x76\x1d\xbe\x2f\x06\xe9\xcc\xbd\xec\x46\x45\x01\x00\x00\x00\x60\x09\x00\x00\x05\x00\x00\x00",32},
 {0x33e76c0,"\x44\x58\x42\x43\xcb\xfe\x15\x03\x9b\x64\x58\x59\x55\x2d\x1b\x82\x4f\xde\xd8\xba\x01\x00\x00\x00\x60\x09\x00\x00\x05\x00\x00\x00",32},
 {0x33e8a20,"\x44\x58\x42\x43\xfd\xeb\x68\x2a\x0d\xec\x53\x97\xf8\x7f\x0c\xad\x95\xaa\x37\x69\x01\x00\x00\x00\x60\x09\x00\x00\x05\x00\x00\x00",32},
 {0x33ea1b0,"\x44\x58\x42\x43\xfe\x20\x8b\x70\xd4\xe2\x63\xff\x14\x68\xcd\x71\xaa\x1f\x18\xb8\x01\x00\x00\x00\xa4\x09\x00\x00\x05\x00\x00\x00",32},
 {0x33eb290,"\x44\x58\x42\x43\xe1\xe4\x11\xeb\x1a\x03\x5b\xe2\xbf\x9e\xf5\x04\xaf\x65\xaf\x4e\x01\x00\x00\x00\xb0\x09\x00\x00\x05\x00\x00\x00",32},
 {0x327d420,"\x44\x58\x42\x43\x5f\x69\x15\x2f\xb5\xf7\x84\x3a\x75\xe8\xdf\x18\xff\x36\x47\xc7\x01\x00\x00\x00\x3c\x04\x00\x00\x05\x00\x00\x00\x34\x00\x00\x00\x10\x01\x00\x00\x20\x01\x00\x00\x30\x01\x00\x00\xa0\x03\x00\x00\x52\x44\x45\x46\xd4\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x3c\x00\x00\x00\x00\x05\x53\x43\x00\x01\x00\x00\xa2\x00\x00\x00\x52\x44\x31\x31\x3c\x00\x00\x00\x18\x00\x00\x00\x20\x00\x00\x00\x28\x00\x00\x00\x24\x00\x00\x00\x0c\x00\x00\x00\x00\x00\x00\x00\x7c\x00\x00\x00\x02\x00\x00\x00\x05\x00\x00\x00\x04\x00\x00\x00\xff\xff\xff\xff\x00\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x91\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00\x04\x00\x00\x00\xff\xff\xff\xff\x00\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x67\x5f\x4d\x69\x6e\x4d\x61\x78\x44\x65\x70\x74\x68\x54\x65\x78\x74\x75\x72\x65\x00\x67\x5f\x4d\x69\x6e\x4d\x61\x78\x44\x65\x70\x74\x68\x4f\x75\x74\x00\x4d\x69\x63\x72\x6f\x73\x6f\x66\x74\x20\x28\x52\x29\x20\x48\x4c\x53\x4c\x20\x53\x68\x61\x64\x65\x72\x20\x43\x6f\x6d\x70\x69\x6c\x65\x72\x20\x39\x2e\x32\x39\x2e\x39\x35\x32\x2e\x33\x31\x31\x31\x00\xab\x49\x53\x47\x4e\x08\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x4f\x53\x47\x4e\x08\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x53\x48\x45\x58\x68\x02\x00\x00\x50\x00\x05\x00\x9a\x00\x00\x00\x6a\x08\x00\x01\x58\x18\x00\x04\x00\x70\x10\x00\x00\x00\x00\x00\x55\x55\x00\x00\x9c\x18\x00\x04\x00\xe0\x11\x00\x00\x00\x00\x00\x55\x55\x00\x00\x5f\x00\x00\x02\x32\x00\x02\x00\x68\x00\x00\x02\x04\x00\x00\x00\x9b\x00\x00\x04\x10\x00\x00\x00\x10\x00\x00\x00\x01\x00\x00\x00\x29\x00\x00\x09\x32\x00\x10\x00\x00\x00\x00\x00\x46\x00\x02\x00\x02\x40\x00\x00\x02\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x36\x00\x00\x08\xc2\x00\x10\x00\x01\x00\x00\x00\x02\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x36\x00\x00\x08\xc2\x00\x10\x00\x00\x00\x00\x00\x02\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x7f\x96\x18\x4b\x00\x00\x00\x00\x36\x00\x00\x05\x12\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x00\x00\x00\x00\x30\x00\x00\x01\x21\x00\x00\x07\x22\x00\x10\x00\x02\x00\x00\x00\x0a\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x04\x00\x00\x00\x03\x00\x04\x03\x1a\x00\x10\x00\x02\x00\x00\x00\x36\x00\x00\x05\x12\x00\x10\x00\x03\x00\x00\x00\x0a\x00\x10\x00\x02\x00\x00\x00\x36\x00\x00\x05\x62\x00\x10\x00\x02\x00\x00\x00\xa6\x0b\x10\x00\x00\x00\x00\x00\x36\x00\x00\x05\x82\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x00\x00\x00\x00\x30\x00\x00\x01\x21\x00\x00\x07\x42\x00\x10\x00\x03\x00\x00\x00\x3a\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x04\x00\x00\x00\x03\x00\x04\x03\x2a\x00\x10\x00\x03\x00\x00\x00\x36\x00\x00\x05\x22\x00\x10\x00\x03\x00\x00\x00\x3a\x00\x10\x00\x02\x00\x00\x00\x1e\x00\x00\x07\x32\x00\x10\x00\x01\x00\x00\x00\x46\x00\x10\x00\x00\x00\x00\x00\x46\x00\x10\x00\x03\x00\x00\x00\x2d\x00\x00\x89\xc2\x00\x00\x80\x43\x55\x15\x00\x32\x00\x10\x00\x01\x00\x00\x00\x46\x0e\x10\x00\x01\x00\x00\x00\x46\x7e\x10\x00\x00\x00\x00\x00\x33\x00\x00\x07\x22\x00\x10\x00\x02\x00\x00\x00\x1a\x00\x10\x00\x02\x00\x00\x00\x0a\x00\x10\x00\x01\x00\x00\x00\x34\x00\x00\x07\x42\x00\x10\x00\x02\x00\x00\x00\x2a\x00\x10\x00\x02\x00\x00\x00\x1a\x00\x10\x00\x01\x00\x00\x00\x1e\x00\x00\x07\x82\x00\x10\x00\x02\x00\x00\x00\x3a\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x01\x00\x00\x00\x16\x00\x00\x01\x36\x00\x00\x05\xc2\x00\x10\x00\x00\x00\x00\x00\x56\x09\x10\x00\x02\x00\x00\x00\x1e\x00\x00\x07\x12\x00\x10\x00\x02\x00\x00\x00\x0a\x00\x10\x00\x02\x00\x00\x00\x01\x40\x00\x00\x01\x00\x00\x00\x16\x00\x00\x01\x36\x00\x00\x05\xf2\x00\x10\x00\x00\x00\x00\x00\xe6\x0a\x10\x00\x00\x00\x00\x00\xa4\x00\x00\x06\xf2\xe0\x11\x00\x00\x00\x00\x00\x46\x05\x02\x00\x46\x0e\x10\x00\x00\x00\x00\x00\x3e\x00\x00\x01\x53\x54\x41\x54\x94\x00\x00\x00\x1a\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x16\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",1084},
 
  {0x22909d0,"\xe9\xdb\x37\x01\x00",5},{0x22909e0,"\xe9\xab\x38\x01\x00",5},
  {0x22908c0,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20",10},
  {0x22908a0,"\xe9\xcb\x34\x01\x00",5},{0x2290930,"\x33\xc9\x49\x8b\xc0\x4c\x8b\xd2",8},
  {0x2290890,"\xe9\x2b\x34\x01\x00",5}};

#ifdef ACUNITYVR_FULL_PASS_PROFILE
 const struct{size_t rva;const char* bytes;size_t size;} profileGuards[]={
  {0x22919c0,"\xe9\xab\x1e\x04\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22909d0,"\xe9\xdb\x37\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22909e0,"\xe9\xab\x38\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2291940,"\x48\x8b\x8a\x48\x14\x00\x00\x49\x8b\xd0\xe9\xd1\xb7\xf4\xff\xcc",16},
  {0x22908c0,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x48\x8b\x9a\x48\x14\x00",16},
  {0x2291910,"\x80\xba\xf0\x04\x00\x00\x00\x4d\x8b\xc8\x48\x8b\xc2\x74\x12\x4c",16},
  {0x22908a0,"\xe9\xcb\x34\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290930,"\x33\xc9\x49\x8b\xc0\x4c\x8b\xd2\x49\x89\x88\xf0\x02\x00\x00\x49",16},
  {0x2291900,"\x48\x8b\xca\x49\x8b\xd0\xe9\x95\x78\x04\x00\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290890,"\xe9\x2b\x34\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2291a30,"\xe9\xdb\x1f\x04\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22919e0,"\x40\x53\x48\x83\xec\x20\x49\x8b\x40\x30\x49\x8b\xd8\x48\x8b\x88",16},
  {0x2290a30,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x49\x8b\x40\x30\x49\x8b",16},
  {0x2290a80,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x49\x8b\x40\x30\x49\x8b",16},
  {0x2291a40,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x49\x8b\x48\x20\x49\x8b",16},
  {0x2290810,"\xe9\x9b\x33\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22919d0,"\xe9\x0b\x1f\x04\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290800,"\xe9\xbb\x32\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2291950,"\xe9\xab\x1d\x04\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290900,"\xe9\x8b\x36\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22907b0,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x48\x8b\x8a\xd8\x13\x00",16},
  {0x22908b0,"\xe9\x5b\x36\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x22909f0,"\xe9\x8b\x39\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2291960,"\x48\x8b\xca\x49\x8b\xd0\xe9\xc5\x03\x00\x00\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290a00,"\xe9\xeb\x45\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
  {0x2290920,"\xe9\x0b\x38\x01\x00\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc",16},
 };
 for(auto& g:profileGuards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
#endif
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
 #ifdef ACUNITYVR_BATCH_PROFILE
 const struct{size_t rva;const char* bytes;size_t size;} batchGuards[]={
  {0x22f7880,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x48\x8b\x49\x30\x48\x8b",16},
  // Exact immediate ExecuteCommandList call; batch unwind continues to22a5464/RDI.
  {0x22f7895,"\x48\x8b\x01\x45\x33\xc0\x48\x8b\xd7\xff\x90\xd0\x01\x00\x00",15},
  {0x22f788e,"\x48\x8b\x3a\x41\x0f\xb6\xd8",7},
  {0x22a543b,"\x48\x8b\xf9\x48\x8b\x70\x20",7},
  {0x22a545a,"\x48\x89\x5c\x24\x30\xe8\x1c\x24\x05\x00",10},
  {0x22a5420,"\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7c\x24\x20\x41",16},
  {0x22a5442,"\x48\x8b\x41\x10\xf6\x40\x18\x01",8},
  {0x22a544f,"\x48\x8d\x51\x28\x48\x8b\x49\x20\x41\xb0\x01",11},
  {0x22a54d0,"\x48\x8b\x4f\x10\x48\x8b\x17\x48\x8b\x01\xff\x50\x20",13}
 };
 for(auto& g:batchGuards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
#ifdef ACUNITYVR_SUN_CAPTURE
 const struct{size_t rva;const char* bytes;size_t size;} sunGuards[]={
  {0x229126b,"\x48\x63\x41\x2c",4},
  {0x229127b,"\x48\x63\x41\x28",4},
  {0x229128b,"\x48\x8b\x82\xe8\x13\x00\x00",7},
  {0x229129d,"\x48\x6b\xdb\x58\x48\x03\x98\x20\x82\x00\x00",11},
  {0x229139a,"\x49\x8b\x87\xa0\x04\x00\x00",7},
  {0x22c53cf,"\x49\x8d\x96\xf0\x01\x00\x00",7},
  {0x22c53f5,"\x49\x8d\x96\xf0\x02\x00\x00",7}
 };
 for(auto& g:sunGuards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
 auto sunTable=reinterpret_cast<void**>(image+0x30a8f70);if(sunTable[0]!=image+0x2287f60 || sunTable[1]!=image+0x2290580 || sunTable[2]!=image+0x1e6d4e0 || sunTable[3]!=image+0x2291260 || sunTable[4]!=image+0x1e6d4e0)return false;
 for(size_t rva:{0x5049ac0u,0x5049b00u,0x5049b40u,0x5049b80u})if(Field<void*>(image,rva)!=sunTable)return false;
#endif
#endif
 auto status=MH_Initialize();if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED)return false;
#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 if(!OcclusionProbe::InstallWorkerHooks(immediate.Get())){Restore();return false;}
 IUnknown* fogVertex79{};IUnknown* fogPixel79{};for(auto& [object,shader]:nativeShaders){if(shader.stage==0 && shader.index==166)fogVertex79=object;if(shader.stage==1 && shader.index==568)fogPixel79=object;}
 if(!VolumetricFog79::Initialize(device.Get(),{fogVertex79,image+0x341f260,664,166},{fogPixel79,image+0x341b740,5204,568},output,log)){Restore();return false;}
 if(!EffectDrawCapture74::Install(image,surface,device.Get(),immediate.Get(),log,output)){Restore();return false;}
#endif
 if(!lightingTiles69.Initialize(image,backend,device.Get(),immediate.Get(),RenderSize::eyeWidth,RenderSize::eyeHeight,output,log)){Restore();return false;}
 if(!lightingGrid67.Initialize(image,device.Get(),immediate.Get(),RenderSize::eyeWidth,RenderSize::eyeHeight,output,log)){Restore();return false;}
 if(!sceneDepth.Initialize(device.Get(),immediate.Get())){fprintf(log,"REFUSED scene-depth46 private capture resources\n");Restore();return false;}
 if(!ShaderHunt::Install(device.Get(),immediate.Get(),image,output,log) || !CloudProjection::Install(image,device.Get(),immediate.Get(),log)){Restore();return false;}
#ifdef ACUNITYVR_NATIVE_FOVEATION
 if(!NativeFoveation::Initialize(device.Get(),immediate.Get(),RenderSize::eyeWidth,RenderSize::eyeHeight,output,log)){Restore();return false;}
#endif
 auto vt=*reinterpret_cast<void***>(immediate.Get());unsigned slots[]{12,13,20,21,38,39,40,41,42
#ifdef ACUNITYVR_FULL_PASS_PROFILE
 ,58,57
#endif
 };
 void* hooks[]{reinterpret_cast<void*>(HookIndexed),reinterpret_cast<void*>(HookDraw),reinterpret_cast<void*>(HookIndexedInst),reinterpret_cast<void*>(HookInst),reinterpret_cast<void*>(HookDrawAuto),reinterpret_cast<void*>(HookIndexedIndirect),reinterpret_cast<void*>(HookIndirect),reinterpret_cast<void*>(HookDispatch),reinterpret_cast<void*>(HookDispatchIndirect)
#ifdef ACUNITYVR_FULL_PASS_PROFILE
 ,reinterpret_cast<void*>(HookExecuteList),reinterpret_cast<void*>(HookResolve)
#endif
 };
 void** originals[]{reinterpret_cast<void**>(&indexed),reinterpret_cast<void**>(&draw),reinterpret_cast<void**>(&indexedInst),reinterpret_cast<void**>(&inst),reinterpret_cast<void**>(&drawAuto),reinterpret_cast<void**>(&indexedIndirect),reinterpret_cast<void**>(&indirect),reinterpret_cast<void**>(&dispatch),reinterpret_cast<void**>(&dispatchIndirect)
#ifdef ACUNITYVR_FULL_PASS_PROFILE
 ,reinterpret_cast<void**>(&executeList),reinterpret_cast<void**>(&resolve)
#endif
 };
 for(unsigned i=0;i<_countof(slots);++i){targets[i]=vt[slots[i]];if(!D3DCode(targets[i])){Restore();return false;}memcpy(originalCode[i],targets[i],16);if(MH_CreateHook(targets[i],hooks[i],originals[i])!=MH_OK){Restore();return false;}++created;}
  for(unsigned i=0;i<created;++i)if(MH_QueueEnableHook(targets[i])!=MH_OK){Restore();return false;}
 if(MH_ApplyQueued()!=MH_OK){Restore();return false;}
 auto starts=StartWrappers(std::make_index_sequence<_countof(passes)>{});auto finishes=FinishWrappers(std::make_index_sequence<_countof(passes)>{});
 for(unsigned i=0;i<_countof(passes);++i){auto& p=passes[i];
#ifdef ACUNITYVR_FULL_PASS_PROFILE
  if(p.start!=0x1e6d4e0)p.clone[2]=starts[i];
#endif
  if(p.finish!=0x1e6d4e0)p.clone[4]=finishes[i];
  if(InterlockedCompareExchangePointer(reinterpret_cast<void**>(image+p.descriptor),p.clone,image+p.table)!=image+p.table){Restore();return false;}descriptorsInstalled[i]=true;
 }
 #ifdef ACUNITYVR_BATCH_PROFILE
 fprintf(log,"Read-only native caller recovery installed: guarded exact caller-frame lookup with bounded virtual-unwind fallback; existingD3D11hooks only; nativejob/list/finish code unmodified\n");
#endif
 fprintf(log,"Performance77 effective FastBatchLookup=%d FastDispatchCheck=%d; NumPad5 compares CPU paths together; sparse profiling controlled independently by Diagnostics.GpuProfiling/F3\n",fastBatchSetting77,fastDispatchSetting77);
 copies.reserve(1024);textureCopies.reserve(3);enabled=true;fprintf(log,"Draw diagnostic installed: existing game immediate context=%p; F8 captures lighting/sky/cloud and final output bindings/constants, rasterizer/scissors and final-output geometry; no renderer settings changed\n",immediate.Get());fflush(log);return true;
}
void SetPassOverride(PassOverride function){passOverride=function;}
void SetEyes(void* left,void* right){views[1]=left;views[2]=right;EffectDrawCapture74::SetEyes(left,right);lightingTiles69.SetEyes(left,right);CloudProjection::SetEyes(left,right);}
void BeginFrame(uint64_t id,uint64_t pose,uint32_t frame,uint32_t mode){
 if(!enabled)return;
 static bool previousFastKey{};const bool fastKey=(GetAsyncKeyState(VK_NUMPAD5)&0x8000)!=0;
 const auto tick=GetTickCount64();bool changed=false;if(mode==1 && fastKey && !previousFastKey){fastPaths77=!fastPaths77.load();changed=true;fprintf(log,"CPU fast paths77 %s/NumPad5; render selection and settings unchanged\n",fastPaths77?"ON":"OFF");fflush(log);}previousFastKey=fastKey;
 static uint64_t lastPerformance{};if(performance77 && (changed || tick-lastPerformance>=5000)){DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);fprintf(performance77,"{\"event\":\"%s\",\"wallTickMs\":%llu,\"sequence\":%llu,\"mode\":%u,\"enabled\":%s,\"gameForeground\":%s,\"fastBatchHits\":%llu,\"slowBatchAttempts\":%llu,\"knownDispatchHits\":%llu,\"fullDispatchChecks\":%llu}\n",changed?"toggle":"counts",tick,id,mode,fastPaths77?"true":"false",foreground==GetCurrentProcessId()?"true":"false",fastBatchHits77.load(),slowBatchAttempts77.load(),knownDispatchHits77.load(),fullDispatchChecks77.load());fflush(performance77);lastPerformance=tick;}
 EnsureDispatchCoverage();
 EffectDrawCapture74::Poll(mode);
 lightingGrid67.BeginFrame(id,mode);lightingTiles69.BeginFrame(id,mode);
#ifdef ACUNITYVR_POST_AA
 PostAAOptimization::BeginFrame(id,mode);
#endif
 CloudProjection::BeginFrame(id,pose,mode);ShaderHunt::BeginFrame(id,pose,frame);bool marked=ShaderHunt::TakeCaptureRequest();
 static bool previous{},initial73{};bool f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0;bool pressed=f8&&!previous;previous=f8;
 const bool initial=mode==1 && !initial73;
 if(!enabled || capturing || (!pressed && !marked && !initial))return;
 finalOutput.Reset();auto output=acu::OutputInterface(surface);ComPtr<ID3D11View> outputView;
#ifdef ACUNITYVR_SUN_CAPTURE
 sunCopies.clear();memset(sunCopied,0,sizeof(sunCopied));
#endif
 if(!output || FAILED(output->QueryInterface(IID_PPV_ARGS(&outputView))))return;outputView->GetResource(&finalOutput);
 #ifdef ACUNITYVR_SHADER_PROFILE
 savedShaders.clear();savedShaderBytes=0;
#endif
 tileCapture68.Reset();memset(gridGeometry67,0,sizeof(gridGeometry67));terminalPixels64.Reset();memset(terminalBufferBytes64,0,sizeof(terminalBufferBytes64));memset(terminalDraws64,0,sizeof(terminalDraws64));terminalSkipped64=0;
 sequence=id;tracking=pose;sourceFrame=frame;draws=otherContexts=errors=budgetSkipped=0;stagedBytes=0;memset(bytesByView,0,sizeof(bytesByView));memset(drawsByView,0,sizeof(drawsByView));memset(cloudCopied,0,sizeof(cloudCopied));
#ifdef ACUNITYVR_DEPTH_PROFILE
 for(auto& keys:nullCaptureKeys)keys.clear();
#ifdef ACUNITYVR_BATCH_PROFILE
 memset(batchCaptures,0,sizeof(batchCaptures));
#endif
#endif
 captureDirectory=directory/(L"interval-"+std::to_wstring(id));std::error_code ec;std::filesystem::create_directories(captureDirectory,ec);if(ec)return;
 events=_wfsopen((captureDirectory/L"draws.jsonl").c_str(),L"w",_SH_DENYNO);if(!events)return;
 NativeGpuTiming::MarkCapture();
#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 OcclusionProbe::Arm(id,pose,frame,captureDirectory);
#endif
 fprintf(events,"{\"event\":\"begin\",\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"mode\":%u,\"bufferBudgetPerView\":%zu,\"drawLimitPerView\":%u}\n",sequence,tracking,sourceFrame,mode,Budget,DrawLimit);sceneDepthAvailable=sceneDepth.Reset();
 EffectDrawCapture74::Arm(sequence,tracking,sourceFrame,captureDirectory);
 if(initial){initial73=true;fprintf(log,"Effect74 initial tracked interval capture requested; rendering remains untimed\n");}
 capturing=true;
 fprintf(log,"F8 draw capture armed interval=%llu sourceFrame=%u pose=%llu\n",sequence,sourceFrame,tracking);fflush(log);
}
void CaptureSceneDepth46(ID3D11DeviceContext* c,ID3D11ComputeShader* shader,unsigned kind){
 if(kind!=41 || c!=immediate.Get() || current.view<1 || current.view>2 || !current.pass || strcmp(current.pass->name,"DownScaleSceneDepth") || !sceneDepthAvailable)return;
 auto found=nativeShaders.find(shader);if(found==nativeShaders.end() || found->second.index!=37 || found->second.stage!=2 || found->second.code!=image+0x327d420 || found->second.size!=1084)return;
 auto eye=current.view-1;if(sceneDepth.items[eye].requested)return;
 ComPtr<ID3D11ShaderResourceView> srv;c->CSGetShaderResources(0,1,&srv);if(!srv)return;
 ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return;
 const float width=Field<float>(views[current.view],0x2c4),height=Field<float>(views[current.view],0x2c8);
 // Native viewport at+2bc is x,y,width,height: dimensions are+2c4/+2c8.
 if(!std::isfinite(width+height) || width<1 || height<1 || width>16384 || height>8192 || width!=float(unsigned(width)) || height!=float(unsigned(height)))return;
 auto hr=sceneDepth.Capture(c,texture.Get(),eye,unsigned(width),unsigned(height));
 fprintf(events,"{\"event\":\"sceneDepthCopy46\",\"view\":%u,\"draw\":%u,\"shaderIndex\":37,\"slot\":0,\"HRESULT\":%ld,\"requested\":%s}\n",current.view,draws,hr,hr==S_OK?"true":"false");
}
void FlushSceneDepth46(){
 for(unsigned eye=0;eye<2;++eye){auto& item=sceneDepth.items[eye];if(!item.requested || item.saved)continue;
  D3D11_MAPPED_SUBRESOURCE map{};auto hr=sceneDepth.Map(eye,&map);bool files{};
  if(hr==S_OK){auto path=captureDirectory/(L"scene-depth-view-"+std::to_wstring(eye+1)+L".rg32f");auto f=_wfsopen(path.c_str(),L"wb",_SH_DENYNO);
   if(f){files=map.RowPitch>=item.desc.Width*8;for(unsigned y=0;files && y<item.desc.Height;++y)files=fwrite(static_cast<const uint8_t*>(map.pData)+size_t(y)*map.RowPitch,8,item.desc.Width,f)==item.desc.Width;files=fclose(f)==0 && files;}
   sceneDepth.Unmap(eye);item.saved=files;}
  fprintf(events,"{\"event\":\"sceneDepthRead46\",\"view\":%u,\"width\":%u,\"height\":%u,\"format\":16,\"samples\":1,\"resource\":\"%llx\",\"token\":%llu,\"HRESULT\":%ld,\"filesOK\":%s,\"note\":\"Native quarter-depth min/max hierarchy input at CS37, GPU execution token and completion verified; no assumed depth encoding\"}\n",eye+1,item.desc.Width,item.desc.Height,item.resource,item.token,hr,files?"true":"false");
 }
}
void FlushBuffers(){
 FlushSceneDepth46();
#ifdef ACUNITYVR_SUN_CAPTURE
 for(auto& copy:sunCopies){D3D11_MAPPED_SUBRESOURCE mapped{};auto result=immediate->Map(copy.staging.Get(),0,D3D11_MAP_READ,0,&mapped);
  if(SUCCEEDED(result)){FILE* f=_wfsopen((captureDirectory/(L"sun-shadow-view-"+std::to_wstring(copy.view)+L".r32")).c_str(),L"wb",_SH_DENYNO);if(f){for(unsigned y=0;y<copy.desc.Height;++y)fwrite(static_cast<uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(copy.desc.Width)*4,1,f);fclose(f);}else ++errors;immediate->Unmap(copy.staging.Get(),0);}
  else fprintf(events,"{\"event\":\"sunTextureMapError\",\"view\":%u,\"HRESULT\":%ld}\n",copy.view,result);
 }
 sunCopies.clear();
#endif
 for(auto& copy:textureCopies){D3D11_MAPPED_SUBRESOURCE map{};
  if(SUCCEEDED(immediate->Map(copy.staging.Get(),0,D3D11_MAP_READ,0,&map))){
   auto name=L"cloud-input-view-"+std::to_wstring(copy.view)+L".rgba16f";FILE* f=_wfsopen((captureDirectory/name).c_str(),L"wb",_SH_DENYNO);
   if(f){for(unsigned y=0;y<copy.desc.Height;++y)fwrite(static_cast<uint8_t*>(map.pData)+size_t(y)*map.RowPitch,size_t(copy.desc.Width)*8,1,f);fclose(f);}else ++errors;
   immediate->Unmap(copy.staging.Get(),0);
  }else ++errors;
 }
 textureCopies.clear();
 for(auto& copy:copies){D3D11_MAPPED_SUBRESOURCE map{};
  if(SUCCEEDED(immediate->Map(copy.staging.Get(),0,D3D11_MAP_READ,0,&map))){
   auto file=L"draw-"+std::to_wstring(copy.draw)+L"-stage-"+std::to_wstring(copy.stage)+L"-slot-"+std::to_wstring(copy.slot)+L".bin";
   FILE* f=_wfsopen((captureDirectory/file).c_str(),L"wb",_SH_DENYNO);if(f){fwrite(map.pData,copy.size,1,f);fclose(f);}else ++errors;immediate->Unmap(copy.staging.Get(),0);
  }else ++errors;
 }
 copies.clear();
}
void EndEye(unsigned eye){
#ifdef ACUNITYVR_NATIVE_FOVEATION
 NativeFoveation::EndEye(eye,capturing.load());
#endif
#ifdef ACUNITYVR_POST_AA
 PostAAOptimization::EndEye(eye+1);
#endif
 if(!capturing)return;fprintf(events,"{\"event\":\"eyeComplete\",\"eye\":%u,\"sourceFrame\":%u}\n",eye,Field<uint32_t>(views[0],0x294));acu::CaptureOutput(surface,captureDirectory/(eye?L"right.bmp":L"left.bmp"),log);
 tileCapture68.Dump(immediate.Get(),captureDirectory,events);terminalPixels64.Dump(immediate.Get(),captureDirectory,events);FlushBuffers();if(!eye)return;
 #ifdef ACUNITYVR_OCCLUSION_CAPTURE
 OcclusionProbe::Finish();
 EffectDrawCapture74::Finish();
#endif
 fprintf(events,"{\"event\":\"terminal64End\",\"drawsPerView\":[%u,%u,%u],\"drawLimit\":%u,\"terminalSkipped\":%u,\"bufferBytesPerView\":[%zu,%zu,%zu],\"bufferBudgetPerView\":%zu,\"textureBytes\":%zu,\"textureBudget\":%zu,\"texturesCopied\":%u,\"textureErrors\":%u}\n",terminalDraws64[0],terminalDraws64[1],terminalDraws64[2],TerminalDrawLimit64,terminalSkipped64,terminalBufferBytes64[0],terminalBufferBytes64[1],terminalBufferBytes64[2],TerminalBufferBudget64,terminalPixels64.bytes,TerminalCapture64::Pool::BudgetBytes,terminalPixels64.copied,terminalPixels64.errors);
 capturing=false;fprintf(events,"{\"event\":\"end\",\"draws\":%u,\"otherContexts\":%u,\"errors\":%u,\"budgetSkipped\":%u,\"stagedBytes\":%zu}\n",draws,otherContexts,errors,budgetSkipped,stagedBytes);fclose(events);events=nullptr;++captureCount;
 fprintf(log,"F8 draw capture completed interval=%llu draws=%u otherContexts=%u errors=%u bytes=%zu budgetSkipped=%u\n",sequence,draws,otherContexts,errors,stagedBytes,budgetSkipped);fflush(log);
}
bool Capturing(){return capturing.load();}
void Restore(){
 enabled=false;capturing=false;passOverride=nullptr;EffectDrawCapture74::Stop();lightingTiles69.Stop();lightingGrid67.Stop();
#ifdef ACUNITYVR_OCCLUSION_CAPTURE
 OcclusionProbe::Stop();OcclusionProbe::RestoreWorkerHooks();
#endif
#ifdef ACUNITYVR_POST_AA
 PostAAOptimization::Stop();
#endif
#ifdef ACUNITYVR_NATIVE_FOVEATION
 NativeFoveation::Stop();
#endif
 NativeGpuTiming::Stop();CloudProjection::Restore();ShaderHunt::Restore();
 for(unsigned i=0;i<_countof(passes);++i)if(descriptorsInstalled[i]){auto& p=passes[i];InterlockedCompareExchangePointer(reinterpret_cast<void**>(image+p.descriptor),image+p.table,p.clone);descriptorsInstalled[i]=false;}
 AcquireSRWLockExclusive(&dispatchVariantLock76);
 for(unsigned i=0;i<created;++i)MH_QueueDisableHook(targets[i]);
 for(unsigned i=0;i<dispatchVariantCount76.load();++i)if(dispatchVariants76[i].created){MH_QueueDisableHook(dispatchVariants76[i].target);dispatchVariants76[i].created=false;if(log)fprintf(log,"ComputeDispatch76 variant%u retired calls=%llu nestedForwarded=%llu\n",i,dispatchVariants76[i].calls.load(),dispatchVariants76[i].nested.load());}
 if(created)MH_ApplyQueued();
 ReleaseSRWLockExclusive(&dispatchVariantLock76);
#ifdef ACUNITYVR_BATCH_PROFILE
 if(log)fprintf(log,"Read-only batch caller profile retired recovered=%llu missed=%llu; native job code unmodified\n",batchRecovered.load(),batchMissed.load());
#endif
 if(log)for(unsigned i=0;i<created;++i)fprintf(log,"D3D hook retirement target=%p bytesRestored=%d\n",targets[i],memcmp(targets[i],originalCode[i],16)==0);
 if(performance77){fprintf(performance77,"{\"event\":\"stop\",\"wallTickMs\":%llu,\"fastBatchHits\":%llu,\"slowBatchAttempts\":%llu,\"knownDispatchHits\":%llu,\"fullDispatchChecks\":%llu}\n",GetTickCount64(),fastBatchHits77.load(),slowBatchAttempts77.load(),knownDispatchHits77.load(),fullDispatchChecks77.load());fclose(performance77);performance77=nullptr;}
 if(events){fprintf(events,"{\"event\":\"retired\"}\n");fclose(events);events=nullptr;}
#ifdef ACUNITYVR_SUN_CAPTURE
 sunCopies.clear();
#endif
 copies.clear();textureCopies.clear();if(log){fprintf(log,"Shader profile excluded command-list replay draws=%llu empty shader bindings=%llu\n",replayDrawsSkipped.load(),emptyShaderDrawsSkipped.load());fprintf(log,"Draw diagnostic retired captures=%u inFlight=%u; original descriptor/D3D code restored; resources/trampolines retained until game exit\n",captureCount,inFlight.load());fflush(log);}
}
}
