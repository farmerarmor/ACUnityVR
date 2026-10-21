// Sparse timestamps on Unity's existing immediate context. Never wait or flush.
#include "NativeGpuTiming.h"
#include <windows.h>
#include <wrl/client.h>
#include <array>
#include <share.h>
#include <cstring>
#ifdef ACUNITYVR_LEAF_PIPELINE
#include "LeafPipeline.h"
#endif
using Microsoft::WRL::ComPtr;
namespace NativeGpuTiming {
namespace {
constexpr unsigned RingSize=8,Marks=44;bool detailedProfiling=true;
#ifdef ACUNITYVR_DEPTH_PROFILE
constexpr uint64_t SamplingMs=1500;bool profilingEnabled=false,previousF3{};
#else
constexpr uint64_t SamplingMs=500;
#endif
#ifdef ACUNITYVR_SHADER_PROFILE
constexpr unsigned EventLimit=512;
#elif defined(ACUNITYVR_FULL_PASS_PROFILE)
constexpr unsigned EventLimit=192;
#else
constexpr unsigned EventLimit=0;
#endif
struct NullPixelKey {
 ComPtr<ID3D11RenderTargetView> color;ComPtr<ID3D11DepthStencilView> depth;ComPtr<ID3D11DepthStencilState> state;
 UINT ref{},viewportCount{};D3D11_VIEWPORT viewport{};unsigned view{},taskRva{};ComPtr<ID3D11Buffer> streamOutput;
 bool Same(const NullPixelKey& b)const{return color.Get()==b.color.Get() && depth.Get()==b.depth.Get() && state.Get()==b.state.Get() && ref==b.ref && viewportCount==b.viewportCount && view==b.view && taskRva==b.taskRva && bool(streamOutput)==bool(b.streamOutput) && !memcmp(&viewport,&b.viewport,sizeof(viewport));}
};
struct NullPixelInfo {
 uintptr_t depthResource{},colorResource{};D3D11_TEXTURE2D_DESC depth{},color{};D3D11_DEPTH_STENCIL_DESC state{};UINT ref{},viewportCount{};D3D11_VIEWPORT viewport{};unsigned draws{};bool mixedVS{},mixedStreamOutput{},explicitState{};uintptr_t streamOutput{};unsigned streamOutputBytes{};
};
struct Event {
 ComPtr<ID3D11Query> begin,end;LONGLONG cpuBegin{},cpuEnd{};
 unsigned kind{},taskRva{},phase{},view{},threadBegin{},threadEnd{},shaderIndex=UINT_MAX;bool closed{};NullPixelInfo nullPixel{};std::array<unsigned,12> nativeStack{};unsigned stackCount{};NativeBatchInfo batch{};uintptr_t commandList{};bool restoreState{};
#ifdef ACUNITYVR_LEAF_PIPELINE
 ComPtr<ID3D11Query> leafQuery;bool leafBegun{},leafEnded{};
#endif
};
struct Sample {
 ComPtr<ID3D11Query> disjoint;
 std::array<ComPtr<ID3D11Query>,Marks> marks;
 std::array<LONGLONG,Marks> cpu{};
#ifdef ACUNITYVR_LEAF_PIPELINE
 LeafPipeline::State leaf;bool leafRequested{};
#endif
 std::array<Event,EventLimit> events;unsigned eventCount{},eventDrops{},nullPixelToken=UINT_MAX;NullPixelKey nullPixelKey;
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
 std::array<ComPtr<ID3D11Query>,3> pipeline;unsigned pipelineOpen{},pipelineEnded{};
#endif
 uint64_t mask{},sequence{},tracking{},wallTickMs{};DWORD foregroundPID{};bool gameForeground{};uint32_t mode{};
 bool pending{},capture{},postAARequested{};unsigned postAADraws{};
#ifdef ACUNITYVR_NATIVE_FOVEATION
 bool foveationEnabled{};uint64_t foveationGeneration{};unsigned foveationDraws[3]{};
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 bool geometryFoveationEnabled{};uint64_t geometryFoveationGeneration{};unsigned geometryFoveationLists[3]{};
#ifdef ACUNITYVR_VISIBILITY_MASK
 bool hiddenAreaEnabled{},hiddenAreaAvailable{};uint64_t hiddenAreaGeneration{};unsigned hiddenAreaDraws[3]{},hiddenAreaLists[3]{};
#endif
#endif
#endif
#ifdef ACUNITYVR_OCCLUSION_COMPARE
 bool occlusionEnabled=true;unsigned nativeOcclusionDisable{};uint64_t occlusionGeneration{};
#endif
};
std::array<Sample,RingSize> ring;
ComPtr<ID3D11DeviceContext> immediate;
Sample* active{};FILE* output{};FILE* report{};uint64_t lastSample{},completed{},dropped{},disjoints{};LONGLONG cpuFrequency{};unsigned currentView{};bool postAARequested{};
#ifdef ACUNITYVR_OCCLUSION_COMPARE
bool occlusionEnabled=true;unsigned nativeOcclusionDisable{};uint64_t occlusionGeneration{};
#endif
#ifdef ACUNITYVR_NATIVE_FOVEATION
bool foveationEnabled{};uint64_t foveationGeneration{};
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
bool geometryFoveationEnabled{};uint64_t geometryFoveationGeneration{};
#ifdef ACUNITYVR_VISIBILITY_MASK
bool hiddenAreaEnabled{},hiddenAreaAvailable{};uint64_t hiddenAreaGeneration{};
#endif
#endif
#endif
void Mark(unsigned index){
 if(!active || index>=Marks || (active->mask&(uint64_t(1)<<index)))return;
 LARGE_INTEGER now{};QueryPerformanceCounter(&now);active->cpu[index]=now.QuadPart;
 immediate->End(active->marks[index].Get());active->mask|=uint64_t(1)<<index;
}
void BeginPipeline(unsigned view){
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
 if(active && view<3){immediate->Begin(active->pipeline[view].Get());active->pipelineOpen|=1u<<view;}
#else
 (void)view;
#endif
}
void EndPipeline(unsigned view){
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
 if(active && view<3 && (active->pipelineOpen&(1u<<view))){immediate->End(active->pipeline[view].Get());active->pipelineOpen&=~(1u<<view);active->pipelineEnded|=1u<<view;}
#else
 (void)view;
#endif
}
void CloseNullPixel(){if(active && active->nullPixelToken!=UINT_MAX){unsigned token=active->nullPixelToken;active->nullPixelToken=UINT_MAX;EndEvent(token);active->nullPixelKey={};}}
void Describe(ID3D11View* view,uintptr_t& pointer,D3D11_TEXTURE2D_DESC& desc){
 if(!view)return;ComPtr<ID3D11Resource> resource;view->GetResource(&resource);pointer=reinterpret_cast<uintptr_t>(resource.Get());ComPtr<ID3D11Texture2D> texture;if(SUCCEEDED(resource.As(&texture)))texture->GetDesc(&desc);
}
void Poll(){
 for(auto& s:ring)if(s.pending){
  D3D11_QUERY_DATA_TIMESTAMP_DISJOINT frequency{};
  HRESULT status=immediate->GetData(s.disjoint.Get(),&frequency,sizeof(frequency),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if(status==S_FALSE)continue;
  if(FAILED(status) || frequency.Disjoint || !frequency.Frequency){s.pending=false;++disjoints;continue;}
  std::array<UINT64,Marks> ticks{};bool ready=true;
  for(unsigned i=0;i<Marks;++i)if(s.mask&(uint64_t(1)<<i)){
   status=immediate->GetData(s.marks[i].Get(),&ticks[i],sizeof(ticks[i]),D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(status!=S_OK){ready=false;if(FAILED(status)){s.pending=false;++disjoints;}break;}
  }
  std::array<std::array<UINT64,2>,EventLimit> eventTicks{};
  if(ready)for(unsigned i=0;i<s.eventCount;++i)if(s.events[i].closed){
   for(unsigned j=0;j<2;++j){
    auto query=j?s.events[i].end.Get():s.events[i].begin.Get();
    status=immediate->GetData(query,&eventTicks[i][j],sizeof(UINT64),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(status!=S_OK){ready=false;if(FAILED(status)){s.pending=false;++disjoints;}break;}
   }if(!ready)break;
  }
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
  std::array<D3D11_QUERY_DATA_PIPELINE_STATISTICS,3> pipelineData{};
  if(ready)for(unsigned view=0;view<3;++view)if(s.pipelineEnded&(1u<<view)){
   status=immediate->GetData(s.pipeline[view].Get(),&pipelineData[view],sizeof(pipelineData[view]),D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(status!=S_OK){ready=false;if(FAILED(status)){s.pending=false;++disjoints;}break;}
  }
#endif
#ifdef ACUNITYVR_LEAF_PIPELINE
  std::array<D3D11_QUERY_DATA_PIPELINE_STATISTICS,EventLimit> leafData;
  if(ready)for(unsigned i=0;i<s.eventCount;++i)if(s.events[i].leafEnded){
   status=immediate->GetData(s.events[i].leafQuery.Get(),&leafData[i],sizeof(leafData[i]),D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(status!=S_OK){ready=false;if(FAILED(status)){s.pending=false;++disjoints;}break;}
  }
#endif
  if(!ready)continue;
  double interval=(s.mask&129)==129?double(ticks[7]-ticks[0])*1000.0/double(frequency.Frequency):0;
  fprintf(output,"{\"sequence\":%llu,\"tracking\":%llu,\"mode\":%u,\"capture\":%s,\"gpuFrequency\":%llu,\"intervalGpuMs\":%.6f,\"segments\":[",s.sequence,s.tracking,s.mode,s.capture?"true":"false",frequency.Frequency,interval);
  bool comma=false;
  for(unsigned segment=0;segment<Marks/2;++segment){unsigned a=segment*2,b=a+1;
   if(!(s.mask&(uint64_t(1)<<a)) || !(s.mask&(uint64_t(1)<<b)) || ticks[b]<ticks[a])continue;
   fprintf(output,"%s{\"segment\":%u,\"beginMs\":%.6f,\"endMs\":%.6f,\"gpuMs\":%.6f,\"cpuMs\":%.6f}",comma?",":"",segment,double(ticks[a]-ticks[0])*1000.0/double(frequency.Frequency),double(ticks[b]-ticks[0])*1000.0/double(frequency.Frequency),double(ticks[b]-ticks[a])*1000.0/double(frequency.Frequency),double(s.cpu[b]-s.cpu[a])*1000.0/double(cpuFrequency));comma=true;
  }
  fprintf(output,"],\"eventDrops\":%u,\"events\":[",s.eventDrops);comma=false;
  for(unsigned i=0;i<s.eventCount;++i){auto& e=s.events[i];auto a=eventTicks[i][0],b=eventTicks[i][1];
   if(!e.closed || b<a || a<ticks[0])continue;
   fprintf(output,"%s{\"kind\":%u,\"taskRva\":%u,\"phase\":%u,\"view\":%u,\"threadBegin\":%u,\"threadEnd\":%u,\"shaderIndex\":%u,\"beginMs\":%.6f,\"endMs\":%.6f,\"gpuMs\":%.6f,\"cpuMs\":%.6f",comma?",":"",e.kind,e.taskRva,e.phase,e.view,e.threadBegin,e.threadEnd,e.shaderIndex,double(a-ticks[0])*1000.0/double(frequency.Frequency),double(b-ticks[0])*1000.0/double(frequency.Frequency),double(b-a)*1000.0/double(frequency.Frequency),double(e.cpuEnd-e.cpuBegin)*1000.0/double(cpuFrequency));
#ifdef ACUNITYVR_LEAF_PIPELINE
   if(e.leafEnded){auto& data=leafData[i];fprintf(output,",\"leafPipeline\":{\"IAVertices\":%llu,\"IAPrimitives\":%llu,\"VSInvocations\":%llu,\"GSInvocations\":%llu,\"GSPrimitives\":%llu,\"CInvocations\":%llu,\"CPrimitives\":%llu,\"PSInvocations\":%llu,\"HSInvocations\":%llu,\"DSInvocations\":%llu,\"CSInvocations\":%llu}",data.IAVertices,data.IAPrimitives,data.VSInvocations,data.GSInvocations,data.GSPrimitives,data.CInvocations,data.CPrimitives,data.PSInvocations,data.HSInvocations,data.DSInvocations,data.CSInvocations);}
#endif
   if(e.kind==1 || e.kind==5){fprintf(output,",\"nativeStack\":[");for(unsigned k=0;k<e.stackCount;++k)fprintf(output,"%s\"%x\"",k?",":"",e.nativeStack[k]);fprintf(output,"]");}
   if(e.kind==1){auto& batch=e.batch;fprintf(output,",\"commandList\":\"%llx\",\"restoreContextState\":%s,\"nativeBatch\":{\"descriptorRva\":%u,\"tableRva\":%u,\"finishRva\":%u,\"flags\":%u,\"laneBegin\":%u,\"laneEnd\":%u,\"job\":\"%llx\",\"payload0\":\"%llx\",\"payload1\":\"%llx\",\"name\":\"%s\"}",static_cast<unsigned long long>(e.commandList),e.restoreState?"true":"false",batch.descriptorRva,batch.tableRva,batch.finishRva,batch.flags,batch.laneBegin,batch.laneEnd,static_cast<unsigned long long>(batch.job),static_cast<unsigned long long>(batch.payload0),static_cast<unsigned long long>(batch.payload1),batch.name);}
   if(e.kind==5){auto& n=e.nullPixel;fprintf(output,",\"nullPixelSpan\":{\"draws\":%u,\"firstVSIndex\":%u,\"mixedVS\":%s,\"depthResource\":\"%llx\",\"depthWidth\":%u,\"depthHeight\":%u,\"depthFormat\":%u,\"depthSamples\":%u,\"colorResource\":\"%llx\",\"colorWidth\":%u,\"colorHeight\":%u,\"colorFormat\":%u,\"colorSamples\":%u,\"explicitDepthState\":%s,\"depthEnable\":%s,\"depthWriteMask\":%u,\"depthFunc\":%u,\"stencilEnable\":%s,\"stencilReadMask\":%u,\"stencilWriteMask\":%u,\"stencilRef\":%u,\"viewportCount\":%u,\"viewport\":[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]}",n.draws,e.shaderIndex,n.mixedVS?"true":"false",static_cast<unsigned long long>(n.depthResource),n.depth.Width,n.depth.Height,unsigned(n.depth.Format),n.depth.SampleDesc.Count,static_cast<unsigned long long>(n.colorResource),n.color.Width,n.color.Height,unsigned(n.color.Format),n.color.SampleDesc.Count,n.explicitState?"true":"false",n.state.DepthEnable?"true":"false",unsigned(n.state.DepthWriteMask),unsigned(n.state.DepthFunc),n.state.StencilEnable?"true":"false",n.state.StencilReadMask,n.state.StencilWriteMask,n.ref,n.viewportCount,n.viewport.TopLeftX,n.viewport.TopLeftY,n.viewport.Width,n.viewport.Height,n.viewport.MinDepth,n.viewport.MaxDepth);}
   if(e.kind==5){auto& n=e.nullPixel;fprintf(output,",\"streamOutput\":{\"buffer\":\"%llx\",\"bytes\":%u,\"mixedBuffers\":%s}",static_cast<unsigned long long>(n.streamOutput),n.streamOutputBytes,n.mixedStreamOutput?"true":"false");}
   fprintf(output,"}");comma=true;
  }
  fprintf(output, "],\"wallTickMs\":%llu,\"foregroundPID\":%lu,\"gameForeground\":%s",s.wallTickMs,s.foregroundPID,s.gameForeground?"true":"false");
#ifdef ACUNITYVR_NATIVE_FOVEATION
  fprintf(output,",\"foveationEnabled\":%s,\"foveationGeneration\":%llu,\"foveationDraws\":[%u,%u,%u]",s.foveationEnabled?"true":"false",s.foveationGeneration,s.foveationDraws[0],s.foveationDraws[1],s.foveationDraws[2]);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
  fprintf(output,",\"geometryFoveationEnabled\":%s,\"geometryFoveationGeneration\":%llu,\"geometryFoveationLists\":[%u,%u,%u]",s.geometryFoveationEnabled?"true":"false",s.geometryFoveationGeneration,s.geometryFoveationLists[0],s.geometryFoveationLists[1],s.geometryFoveationLists[2]);
#ifdef ACUNITYVR_VISIBILITY_MASK
 fprintf(output,",\"hiddenAreaEnabled\":%s,\"hiddenAreaAvailable\":%s,\"hiddenAreaGeneration\":%llu,\"hiddenAreaDraws\":[%u,%u,%u],\"hiddenAreaLists\":[%u,%u,%u]",s.hiddenAreaEnabled?"true":"false",s.hiddenAreaAvailable?"true":"false",s.hiddenAreaGeneration,s.hiddenAreaDraws[0],s.hiddenAreaDraws[1],s.hiddenAreaDraws[2],s.hiddenAreaLists[0],s.hiddenAreaLists[1],s.hiddenAreaLists[2]);
#endif
#endif
#endif
#ifdef ACUNITYVR_OCCLUSION_COMPARE
  fprintf(output,",\"occlusionEnabled\":%s,\"nativeOcclusionDisable\":%u,\"occlusionGeneration\":%llu",s.occlusionEnabled?"true":"false",s.nativeOcclusionDisable,s.occlusionGeneration);
#endif
#ifdef ACUNITYVR_LEAF_PIPELINE
  fprintf(output,",\"leafPipelineRequested\":%s,\"leafPipelineBegun\":%u,\"leafPipelineOverlaps\":%u,\"leafPipelineForcedClose\":%u",s.leafRequested?"true":"false",s.leaf.begun,s.leaf.overlaps,s.leaf.forced);
#endif
  fprintf(output,",\"postAARequested\":%s,\"postAADraws\":%u",s.postAARequested?"true":"false",s.postAADraws);
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
  fprintf(output,",\"pipeline\":[");comma=false;
  for(unsigned view=0;view<3;++view)if(s.pipelineEnded&(1u<<view)){auto& p=pipelineData[view];fprintf(output,"%s{\"view\":%u,\"PSInvocations\":%llu,\"VSInvocations\":%llu,\"CSInvocations\":%llu,\"IAPrimitives\":%llu}",comma?",":"",view,p.PSInvocations,p.VSInvocations,p.CSInvocations,p.IAPrimitives);comma=true;}
  fprintf(output,"]");
#endif
  fprintf(output,"}\n");fflush(output);s.pending=false;++completed;
 }
}
}
bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& directory,FILE* log){
 if(!device || !context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 ComPtr<ID3D11Device> owner;context->GetDevice(&owner);if(owner.Get()!=device)return false;
 immediate=context;report=log;
#ifdef ACUNITYVR_DEPTH_PROFILE
 detailedProfiling=GetPrivateProfileIntW(L"Diagnostics",L"GpuDetailed",1,(directory/L"ACUnityVR.ini").c_str())!=0;fprintf(report,"Diagnostics.GpuDetailed effective=%d; coarse whole-eye/pass sampling remains available when OFF\n",detailedProfiling);
 profilingEnabled=GetPrivateProfileIntW(L"Diagnostics",L"GpuProfiling",0,(directory/L"ACUnityVR.ini").c_str())!=0;fprintf(report,"Diagnostics.GpuProfiling effective=%d; F3 toggles sparse profiling independently of correction/VRS scopes\n",profilingEnabled);
#endif
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);cpuFrequency=f.QuadPart;
 for(auto& s:ring){D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};if(FAILED(device->CreateQuery(&desc,&s.disjoint)))return false;
  desc.Query=D3D11_QUERY_TIMESTAMP;for(auto& mark:s.marks)if(FAILED(device->CreateQuery(&desc,&mark)))return false;
  for(auto& e:s.events)if(FAILED(device->CreateQuery(&desc,&e.begin)) || FAILED(device->CreateQuery(&desc,&e.end)))return false;
#ifdef ACUNITYVR_LEAF_PIPELINE
  desc.Query=D3D11_QUERY_PIPELINE_STATISTICS;
  for(auto& e:s.events)if(FAILED(device->CreateQuery(&desc,&e.leafQuery)))return false;
#endif
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
  desc.Query=D3D11_QUERY_PIPELINE_STATISTICS;for(auto& query:s.pipeline)if(FAILED(device->CreateQuery(&desc,&query)))return false;
#endif
 }
 output=_wfsopen((directory/L"native-gpu-timing.jsonl").c_str(),L"w",_SH_DENYNO);if(!output)return false;
 fprintf(output,"{\"event\":\"schema\",\"segments\":{\"0\":\"source\",\"1\":\"left\",\"2\":\"right\",\"3\":\"rightPairPublish\",\"4to21\":\"4+view*6+pass: reflectionLighting,reflectionFinal,indoorGI,deferredLighting,postEffects,sky\"},\"passPolicy\":\"first occurrence per view\",\"samplingMs\":%llu,\"queryRing\":8,\"readMode\":\"DONOTFLUSH\"}\n",SamplingMs);fflush(output);
 fprintf(output,"{\"event\":\"eventSchema\",\"kinds\":{\"0\":\"native serial callback\",\"1\":\"ExecuteCommandList\",\"2\":\"ResolveSubresource\",\"3\":\"direct draw by native PS index\",\"4\":\"dispatch by native CS index\",\"5\":\"contiguous null pixel-shader span by targets/viewport/stencil state; includes uninstrumented work\"},\"phases\":{\"2\":\"start\",\"4\":\"finish\"},\"eventLimit\":%u,\"nesting\":\"callback spans can contain command-list spans; do not sum overlapping times\"}\n",EventLimit);fflush(output);
 fprintf(report,"Native GPU timing installed on existing game immediate context=%p; sparse whole-interval timestamps, nonblocking query reads\n",immediate.Get());
#ifdef ACUNITYVR_DEPTH_PROFILE
#ifdef ACUNITYVR_BATCH_PROFILE
 fprintf(report,"Native batch task attribution ON: sampling every%llums, F3 profiling ON/OFF; original rendering/list order unchanged; null-PS spans omitted, task descriptor labels attached to existing list timers.\n",SamplingMs);
#else
 fprintf(report,"Depth/command-list attribution ON: sampling every%llums, F3 profiling ON/OFF, rendering unchanged; contiguous null-PS spans and native stacks, bounded queries on existing game device.\n",SamplingMs);
#endif
#endif
#ifdef ACUNITYVR_LEAF_PIPELINE
 fprintf(report,"F8-only leaf pipeline attribution: draws/compute/native command lists, 512-event bound on existing game context; whole-eye pipeline counters omitted to avoid same-type nesting. Capture timings include diagnostic overhead; no rendering change.\n");
#endif
 fflush(report);return true;
}
void BeginSource(uint64_t sequence,uint64_t tracking,uint32_t mode,bool capture){
 if(!output || active)return;Poll();auto now=GetTickCount64();
#ifdef ACUNITYVR_DEPTH_PROFILE
 bool down=(GetAsyncKeyState(VK_F3)&0x8000)!=0;if(down&&!previousF3){profilingEnabled=!profilingEnabled;fprintf(output,"{\"event\":\"profilingToggle\",\"wallTickMs\":%llu,\"sequence\":%llu,\"enabled\":%s}\n",now,sequence,profilingEnabled?"true":"false");fflush(output);fprintf(report,"Sparse GPU profiling %s/F3; rendering unchanged; sampling every%llums when ON\n",profilingEnabled?"ON":"OFF",SamplingMs);fflush(report);}previousF3=down;if(!profilingEnabled && !capture)return;
#endif
#ifdef ACUNITYVR_LEAF_PIPELINE
 if(mode!=1 || (!capture && now-lastSample<SamplingMs))return;
#else
 if(mode!=1 || now-lastSample<SamplingMs)return;
#endif
 lastSample=now;
 for(auto& s:ring)if(!s.pending){active=&s;currentView=0;s.mask=0;s.eventCount=0;s.eventDrops=0;s.nullPixelToken=UINT_MAX;s.nullPixelKey={};
#if defined(ACUNITYVR_SHADER_PROFILE) && !defined(ACUNITYVR_LEAF_PIPELINE)
 s.pipelineOpen=s.pipelineEnded=0;
#endif
#ifdef ACUNITYVR_OCCLUSION_COMPARE
 s.occlusionEnabled=occlusionEnabled;s.nativeOcclusionDisable=nativeOcclusionDisable;s.occlusionGeneration=occlusionGeneration;
#endif
#ifdef ACUNITYVR_LEAF_PIPELINE
 s.leaf={};s.leafRequested=capture;
#endif
#ifdef ACUNITYVR_NATIVE_FOVEATION
 s.foveationEnabled=foveationEnabled;s.foveationGeneration=foveationGeneration;memset(s.foveationDraws,0,sizeof(s.foveationDraws));
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 s.geometryFoveationEnabled=geometryFoveationEnabled;s.geometryFoveationGeneration=geometryFoveationGeneration;memset(s.geometryFoveationLists,0,sizeof(s.geometryFoveationLists));
#ifdef ACUNITYVR_VISIBILITY_MASK
 s.hiddenAreaEnabled=hiddenAreaEnabled;s.hiddenAreaAvailable=hiddenAreaAvailable;s.hiddenAreaGeneration=hiddenAreaGeneration;memset(s.hiddenAreaDraws,0,sizeof(s.hiddenAreaDraws));memset(s.hiddenAreaLists,0,sizeof(s.hiddenAreaLists));
#endif
#endif
#endif
 s.wallTickMs=now;s.foregroundPID=0;GetWindowThreadProcessId(GetForegroundWindow(),&s.foregroundPID);s.gameForeground=s.foregroundPID==GetCurrentProcessId();s.sequence=sequence;s.tracking=tracking;s.mode=mode;s.capture=capture;s.postAARequested=postAARequested;s.postAADraws=0;immediate->Begin(s.disjoint.Get());BeginPipeline(0);Mark(0);return;}
 ++dropped;
}
void EndView(unsigned view){CloseNullPixel();if(view<3){EndPipeline(view);Mark(view*2+1);}}
void BeginEye(unsigned eye){CloseNullPixel();if(eye<2){currentView=eye+1;BeginPipeline(eye+1);Mark((eye+1)*2);}}
void BeginPass(unsigned view,unsigned pass){CloseNullPixel();if(view<3 && pass<6)Mark(8+(view*6+pass)*2);}
void EndPass(unsigned view,unsigned pass){CloseNullPixel();if(view<3 && pass<6)Mark(9+(view*6+pass)*2);}
#ifdef ACUNITYVR_LEAF_PIPELINE
void ForceCloseLeaf(){
 if(!active)return;auto token=active->leaf.ForceClose();
 if(token<active->eventCount){auto& e=active->events[token];immediate->End(e.leafQuery.Get());e.leafEnded=true;}
}
#endif
void BeginTransport(){CloseNullPixel();Mark(6);}
void EndTransport(){
 if(!active)return;CloseNullPixel();
#ifdef ACUNITYVR_LEAF_PIPELINE
 ForceCloseLeaf();
#endif
 for(unsigned v=0;v<3;++v)EndPipeline(v);Mark(7);immediate->End(active->disjoint.Get());active->pending=true;active=nullptr;
}
unsigned BeginEvent(unsigned kind,unsigned taskRva,unsigned phase,ID3D11DeviceContext* context,unsigned view,unsigned shaderIndex){
 // Unity serial GPU tasks are dependency-ordered but migrate among job threads.
 // The caller proves the native serial-context/view identity; D3D hooks prove
 // this is the existing game immediate context. Do not pin them to SourcePre's thread.
 if(!EventLimit || !active || (!detailedProfiling && !active->capture) || (context && context!=immediate.Get()) || (!context && view>=3))return UINT_MAX;
 if(kind!=5)CloseNullPixel();
 if(active->eventCount>=EventLimit){++active->eventDrops;return UINT_MAX;}
 unsigned token=active->eventCount++;auto& e=active->events[token];e.kind=kind;e.shaderIndex=shaderIndex;e.taskRva=taskRva;e.phase=phase;e.view=view<3?view:currentView;e.closed=false;e.nullPixel={};e.nativeStack={};e.stackCount=0;e.batch={};e.commandList=0;e.restoreState=false;e.threadBegin=GetCurrentThreadId();
 LARGE_INTEGER now{};QueryPerformanceCounter(&now);e.cpuBegin=now.QuadPart;immediate->End(e.begin.Get());
#ifdef ACUNITYVR_LEAF_PIPELINE
 e.leafBegun=e.leafEnded=false;
 if(active->leaf.Begin(token,kind,active->leafRequested)){immediate->Begin(e.leafQuery.Get());e.leafBegun=true;}
#endif
 return token;
}
void EndEvent(unsigned token){
 if(!active || token>=active->eventCount)return;if(token!=active->nullPixelToken)CloseNullPixel();
 auto& e=active->events[token];
#ifdef ACUNITYVR_LEAF_PIPELINE
 if(e.leafBegun && active->leaf.End(token)){immediate->End(e.leafQuery.Get());e.leafEnded=true;}
#endif
 immediate->End(e.end.Get());LARGE_INTEGER now{};QueryPerformanceCounter(&now);e.cpuEnd=now.QuadPart;e.threadEnd=GetCurrentThreadId();e.closed=true;
}
void AddNativeStack(unsigned token){
 if(!active || token>=active->eventCount)return;auto& e=active->events[token];void* stack[28]{};auto count=CaptureStackBackTrace(0,_countof(stack),stack,nullptr);auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 for(USHORT i=0;i<count && e.stackCount<e.nativeStack.size();++i){auto address=reinterpret_cast<uintptr_t>(stack[i]);if(address>=base && address<base+0x7825000)e.nativeStack[e.stackCount++]=unsigned(address-base);}
}
void AddNativeBatch(unsigned token,const NativeBatchInfo& info,uintptr_t commandList,bool restoreState){
 if(!active || token>=active->eventCount)return;auto& e=active->events[token];e.batch=info;e.commandList=commandList;e.restoreState=restoreState;
}
void RecordNullPixelDraw(ID3D11DeviceContext* c,unsigned view,unsigned taskRva,unsigned drawKind,unsigned vsIndex){
 if(!active || c!=immediate.Get() || view>=3)return;
 NullPixelKey key{};key.view=view;key.taskRva=taskRva;c->OMGetRenderTargets(1,&key.color,&key.depth);c->OMGetDepthStencilState(&key.state,&key.ref);key.viewportCount=1;c->RSGetViewports(&key.viewportCount,&key.viewport);c->SOGetTargets(1,&key.streamOutput);
 if(active->nullPixelToken!=UINT_MAX && active->nullPixelKey.Same(key)){auto& e=active->events[active->nullPixelToken];++e.nullPixel.draws;e.nullPixel.mixedVS|=e.shaderIndex!=vsIndex;e.nullPixel.mixedStreamOutput|=e.nullPixel.streamOutput!=reinterpret_cast<uintptr_t>(key.streamOutput.Get());return;}
 CloseNullPixel();unsigned token=BeginEvent(5,taskRva,drawKind,c,view,vsIndex);if(token==UINT_MAX)return;
 active->nullPixelToken=token;active->nullPixelKey=std::move(key);auto& e=active->events[token];auto& n=e.nullPixel;auto& k=active->nullPixelKey;
 n.draws=1;n.streamOutput=reinterpret_cast<uintptr_t>(k.streamOutput.Get());if(k.streamOutput){D3D11_BUFFER_DESC d{};k.streamOutput->GetDesc(&d);n.streamOutputBytes=d.ByteWidth;}n.ref=k.ref;n.viewportCount=k.viewportCount;n.viewport=k.viewport;n.explicitState=k.state!=nullptr;
 if(k.state)k.state->GetDesc(&n.state);else{n.state.DepthEnable=TRUE;n.state.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;n.state.DepthFunc=D3D11_COMPARISON_LESS;}
 Describe(k.depth.Get(),n.depthResource,n.depth);Describe(k.color.Get(),n.colorResource,n.color);AddNativeStack(token);
}
#ifdef ACUNITYVR_OCCLUSION_COMPARE
bool SetOcclusionState(bool enabled,unsigned nativeDisable,uint64_t generation){
 if(active || nativeDisable>1 || enabled!=(nativeDisable==0))return false;
 occlusionEnabled=enabled;nativeOcclusionDisable=nativeDisable;occlusionGeneration=generation;return true;
}
#endif
#ifdef ACUNITYVR_NATIVE_FOVEATION
bool SetFoveationState(bool enabled,uint64_t generation){if(active)return false;foveationEnabled=enabled;foveationGeneration=generation;return true;}
void RecordFoveationDraw(unsigned view,unsigned,unsigned){if(active && view<3)++active->foveationDraws[view];}
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
#ifdef ACUNITYVR_VISIBILITY_MASK
bool SetHiddenAreaState(bool enabled,bool available,uint64_t generation){if(active)return false;hiddenAreaEnabled=enabled;hiddenAreaAvailable=available;hiddenAreaGeneration=generation;return true;}
void RecordHiddenArea(unsigned view,bool command){if(active && view<3){if(command)++active->hiddenAreaLists[view];else ++active->hiddenAreaDraws[view];}}
#endif
bool SetGeometryFoveationState(bool enabled,uint64_t generation){if(active)return false;geometryFoveationEnabled=enabled;geometryFoveationGeneration=generation;return true;}
void RecordGeometryFoveationList(unsigned view){if(active && view<3)++active->geometryFoveationLists[view];}
#endif
#endif
bool Recording(){return active!=nullptr;}
void MarkCapture(){if(active)active->capture=true;}
void SetPostAA(bool requested){postAARequested=requested;if(active)active->postAARequested=requested;}
void RecordPostAADraw(){if(active)++active->postAADraws;}
void Stop(){
 if(active){CloseNullPixel();
#ifdef ACUNITYVR_LEAF_PIPELINE
 ForceCloseLeaf();
#endif
 for(unsigned v=0;v<3;++v)EndPipeline(v);immediate->End(active->disjoint.Get());active->pending=true;active=nullptr;}if(output){Poll();
  unsigned pending=0;for(auto& s:ring)pending+=s.pending?1:0;
  fprintf(output,"{\"event\":\"stop\",\"wallTickMs\":%llu,\"completed\":%llu,\"ringDrops\":%llu,\"disjointOrError\":%llu,\"pending\":%u}\n",GetTickCount64(),completed,dropped,disjoints,pending);fclose(output);output=nullptr;
  fprintf(report,"Native GPU timing retired completed=%llu ringDrops=%llu disjointOrError=%llu pending=%u; no GPU wait/flush\n",completed,dropped,disjoints,pending);fflush(report);
 }
}
}
