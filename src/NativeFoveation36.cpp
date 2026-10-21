// Private reversible native colour-pass VRS comparison. No deferred-context VRS.
#include "NativeFoveation.h"
#include "FoveationMask.h"
#include "FoveationPolicy.h"
#include "HiddenAreaMask.h"
#include "FoveationSettings.h"
#include "NativeGpuTiming.h"
#include <nvapi.h>
#include <wrl/client.h>
#include <atomic>
#include <share.h>
using Microsoft::WRL::ComPtr;
namespace NativeFoveation {
namespace {
ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;
ComPtr<ID3D11Texture2D> textures[3][2];ComPtr<ID3D11NvShadingRateResourceView> maps[3][2];
FoveationSettings::Settings settings;Transport::Visibility visibility{};uint32_t visibilityGeneration{};
bool visibilityDirty{},hiddenReady{},hiddenEnabled{};unsigned hiddenTiles[2]{},hiddenDraws[2]{},hiddenLists[2]{};
unsigned SelectedMap(bool coarse){return hiddenEnabled&&hiddenReady?(coarse?1u:2u):0u;}
NV_PIXEL_SHADING_RATE PeripheralRate(){return settings.strength==3?NV_PIXEL_X1_PER_2X4_RASTER_PIXELS:settings.strength==2?NV_PIXEL_X1_PER_2X2_RASTER_PIXELS:settings.strength==1?NV_PIXEL_X1_PER_2X1_RASTER_PIXELS:NV_PIXEL_X1_PER_RASTER_PIXEL;}
bool MaskOnlyShader(unsigned pass,unsigned ps){return (pass==0x504a010 && ps==36) || (pass==0x504a038 && ps==392) || (pass==0x504a100 && (ps==509 || ps==504 || ps==479));}
FoveationMask::Mask masks[2];Transport::Eye lastEyes[2]{};
FILE* report{};FILE* output{};unsigned width{},height{},mode{};
std::atomic<bool> active{},requested{},fault{};
bool previousF4{},mapsReady{},bound{},everApplied{};uint64_t sequence{},generation{},lastLog{},totalDraws{},totalResets{};
unsigned draws[2]{},declined[2]{};bool lightingEnabled78{},previousLightingKey78{};unsigned lightingDraws78[2]{};
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
std::atomic<bool> geometryRequested{};unsigned geometryLists[2]{};
#endif
unsigned decisionRecords{};uint64_t decisionSeen{},decisionGeneration=UINT64_MAX;
void Event(const char* name,bool ok){
 if(output)fprintf(output,"{\"event\":\"lightingCoverage78\",\"controlEvent\":\"%s\",\"wallTickMs\":%llu,\"sequence\":%llu,\"mode\":%u,\"enabled\":%s,\"leftDraws\":%u,\"rightDraws\":%u}\n",name,GetTickCount64(),sequence,mode,lightingEnabled78?"true":"false",lightingDraws78[0],lightingDraws78[1]);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 if(output)fprintf(output,"{\"event\":\"hiddenAreaState\",\"controlEvent\":\"%s\",\"wallTickMs\":%llu,\"sequence\":%llu,\"generation\":%llu,\"enabled\":%s,\"available\":%s,\"meshGeneration\":%u,\"colourEnabled\":%s,\"geometryEnabled\":%s,\"leftDraws\":%u,\"rightDraws\":%u,\"leftLists\":%u,\"rightLists\":%u,\"success\":%s}\n",name,GetTickCount64(),sequence,generation,hiddenEnabled?"true":"false",hiddenReady?"true":"false",visibilityGeneration,requested?"true":"false",geometryRequested?"true":"false",hiddenDraws[0],hiddenDraws[1],hiddenLists[0],hiddenLists[1],ok?"true":"false");
 if(output)fprintf(output,"{\"event\":\"geometryState\",\"controlEvent\":\"%s\",\"wallTickMs\":%llu,\"sequence\":%llu,\"generation\":%llu,\"enabled\":%s,\"colourEnabled\":%s,\"leftLists\":%u,\"rightLists\":%u,\"success\":%s}\n",name,GetTickCount64(),sequence,generation,geometryRequested?"true":"false",requested?"true":"false",geometryLists[0],geometryLists[1],ok?"true":"false");
#endif
 if(output){
 fprintf(output,"{\"event\":\"%s\",\"wallTickMs\":%llu,\"sequence\":%llu,\"mode\":%u,\"requested\":%s,\"generation\":%llu,\"success\":%s,\"leftDraws\":%u,\"rightDraws\":%u,\"leftDeclined\":%u,\"rightDeclined\":%u,\"totalDraws\":%llu,\"totalResets\":%llu,\"bound\":%s}\n",name,GetTickCount64(),sequence,mode,requested?"true":"false",generation,ok?"true":"false",draws[0],draws[1],declined[0],declined[1],totalDraws,totalResets,bound?"true":"false");fflush(output);
}}
bool Reset(){
 if(!immediate || !everApplied)return true;
 NV_D3D11_VIEWPORTS_SHADING_RATE_DESC d{};d.version=NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
 auto a=NvAPI_D3D11_RSSetShadingRateResourceView(immediate.Get(),nullptr);
 auto b=NvAPI_D3D11_RSSetViewportsPixelShadingRates(immediate.Get(),&d);++totalResets;
 bool ok=a==NVAPI_OK && b==NVAPI_OK;bound=!ok;
 if(!ok){fault=true;requested=false;fprintf(report,"REFUSED native VRS reset status=%d/%d; full-rate restoration unconfirmed\n",a,b);fflush(report);Event("resetFailure",false);}
 return ok;
}
bool Fail(const char* reason){fault=true;requested=false;fprintf(report,"REFUSED native VRS: %s; retire at normal pair boundary\n",reason);fflush(report);Event("failure",false);return false;}
bool SameFov(const Transport::Eye& a,const Transport::Eye& b){return std::abs(a.left-b.left)<1e-5f && std::abs(a.right-b.right)<1e-5f && std::abs(a.up-b.up)<1e-5f && std::abs(a.down-b.down)<1e-5f;}
bool BuildMaps(const Transport::Eye* eyes){
 ComPtr<ID3D11Texture2D> newTextures[3][2];ComPtr<ID3D11NvShadingRateResourceView> newMaps[3][2];FoveationMask::Mask newMasks[2];
 std::vector<uint8_t> hidden[2];unsigned newHiddenTiles[2]{};bool available=visibility.valid!=0;
 for(unsigned eye=0;eye<2;++eye){auto& e=eyes[eye];
  if(!FoveationMask::Build(width,height,e.left,e.right,e.up,e.down,newMasks[eye],settings.radius))return Fail("invalid headset FOV");
  if(available && !HiddenAreaMask::Build(visibility.eyes[eye],e,width,height,settings.margin,hidden[eye],newHiddenTiles[eye]))available=false;
 }
 for(unsigned eye=0;eye<2;++eye){auto& mask=newMasks[eye];
  for(unsigned kind=0;kind<3;++kind){auto pixels=mask.pixels;
   for(size_t i=0;i<pixels.size();++i){if(!settings.enabled || kind==2)pixels[i]=0;if(kind && available && hidden[eye][i])pixels[i]=2;}
   D3D11_TEXTURE2D_DESC t{};t.Width=mask.width;t.Height=mask.height;t.MipLevels=t.ArraySize=t.SampleDesc.Count=1;t.Format=DXGI_FORMAT_R8_UINT;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   D3D11_SUBRESOURCE_DATA data{pixels.data(),mask.width,0};if(FAILED(device->CreateTexture2D(&t,&data,&newTextures[kind][eye])))return Fail("rate texture creation");
   NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC d{};d.version=NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;d.Format=t.Format;d.ViewDimension=NV_SRRV_DIMENSION_TEXTURE2D;
   if(NvAPI_D3D11_CreateShadingRateResourceView(device.Get(),newTextures[kind][eye].Get(),&d,&newMaps[kind][eye])!=NVAPI_OK)return Fail("rate view creation");
  }
 }
 for(unsigned eye=0;eye<2;++eye){for(unsigned kind=0;kind<3;++kind){textures[kind][eye]=std::move(newTextures[kind][eye]);maps[kind][eye]=std::move(newMaps[kind][eye]);}masks[eye]=std::move(newMasks[eye]);lastEyes[eye]=eyes[eye];hiddenTiles[eye]=available?newHiddenTiles[eye]:0;
  fprintf(output,"{\"event\":\"mask\",\"sequence\":%llu,\"eye\":%u,\"width\":%u,\"height\":%u,\"centreX\":%.9g,\"centreY\":%.9g,\"ellipseRadius\":%.9g,\"fullTiles\":%u,\"coarseTiles\":%u,\"hiddenTiles\":%u,\"hiddenMarginPixels\":%u,\"visibleMeshGeneration\":%u,\"strength\":%u}\n",sequence,eye,masks[eye].width,masks[eye].height,masks[eye].centreX,masks[eye].centreY,settings.radius,masks[eye].fullTiles,masks[eye].coarseTiles,hiddenTiles[eye],settings.margin,visibilityGeneration,settings.strength);
 }
 hiddenReady=available && hiddenTiles[0]>0 && hiddenTiles[1]>0;mapsReady=true;visibilityDirty=false;fflush(output);Event("maskUpdate",true);return true;
}
struct TargetInfo {bool present{},texture{};D3D11_RENDER_TARGET_VIEW_DESC view{};D3D11_TEXTURE2D_DESC resource{};};
struct Decision {unsigned reasons{},viewportCount{},stencilRef{};bool depthBound{},explicitState{};D3D11_VIEWPORT viewport{};D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};D3D11_DEPTH_STENCIL_DESC depthState{};TargetInfo targets[8];};
Decision RenderTargetsMatch(ID3D11DeviceContext* c,bool maskOnly){
 Decision out;ID3D11RenderTargetView* raw[8]{};ComPtr<ID3D11DepthStencilView> dsv;c->OMGetRenderTargets(8,raw,&dsv);bool hasColour=false;
 for(unsigned i=0;i<8;++i){ComPtr<ID3D11RenderTargetView> rt;rt.Attach(raw[i]);if(!rt)continue;hasColour=true;auto& t=out.targets[i];t.present=true;rt->GetDesc(&t.view);
  auto checkedView=t.view;if(maskOnly && checkedView.Format==DXGI_FORMAT_R32_FLOAT)checkedView.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
  if(!FoveationPolicy::SingleFullView(checkedView))out.reasons|=2;
  ComPtr<ID3D11Resource> resource;ComPtr<ID3D11Texture2D> texture;rt->GetResource(&resource);t.texture=SUCCEEDED(resource.As(&texture));if(!t.texture){out.reasons|=4;continue;}
  texture->GetDesc(&t.resource);auto& d=t.resource;
  if(d.Width!=width || d.Height!=height || d.ArraySize!=1 || (d.SampleDesc.Count!=1 && d.SampleDesc.Count!=2) || !(FoveationPolicy::ColourFormat(d.Format) || (maskOnly && d.Format==DXGI_FORMAT_R32_FLOAT)))out.reasons|=4;
 }
 if(!hasColour)out.reasons|=1;
 D3D11_VIEWPORT vp[16]{};UINT count=16;c->RSGetViewports(&count,vp);out.viewportCount=count;if(count)out.viewport=vp[0];
 if(count!=1 || vp[0].TopLeftX!=0 || vp[0].TopLeftY!=0 || vp[0].Width!=float(width) || vp[0].Height!=float(height))out.reasons|=8;
 out.depthBound=bool(dsv);if(dsv)dsv->GetDesc(&out.depthView);
 ComPtr<ID3D11DepthStencilState> state;c->OMGetDepthStencilState(&state,&out.stencilRef);out.explicitState=bool(state);
 if(state)state->GetDesc(&out.depthState);else{out.depthState.DepthEnable=true;out.depthState.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;out.depthState.DepthFunc=D3D11_COMPARISON_LESS;out.depthState.StencilEnable=false;}
 if(!maskOnly)out.reasons|=FoveationPolicy::EffectiveWrites(out.depthBound,out.depthView.Flags,out.depthState);
 return out;
}
void ReportDecision(unsigned view,unsigned pass,unsigned shader,const Decision& d){
 constexpr unsigned shaders[]{276,391,387,30,4,461,36,392,509,504,479,49,301,302,303,304,305};constexpr unsigned slots=sizeof(shaders)/sizeof(shaders[0]);unsigned slot=0;while(slot<slots && shaders[slot]!=shader)++slot;if(slot==slots)return;
 if(decisionGeneration!=generation){decisionGeneration=generation;decisionSeen=0;}
 uint64_t bit=uint64_t(1)<<((view-1)*slots+slot);if(decisionSeen&bit || decisionRecords>=256)return;decisionSeen|=bit;++decisionRecords;
 fprintf(output,"{\"event\":\"drawDecision\",\"wallTickMs\":%llu,\"sequence\":%llu,\"generation\":%llu,\"view\":%u,\"passRva\":%u,\"shader\":%u,\"eligible\":%s,\"reasons\":%u,\"reasonBits\":\"1=noColour,2=colourView,4=colourResource,8=viewport,16=effectiveDepthWrite,32=effectiveStencilWrite\",\"viewportCount\":%u,\"viewport\":[%.9g,%.9g,%.9g,%.9g],\"depthBound\":%s,\"depthViewDimension\":%u,\"depthViewFlags\":%u,\"explicitDepthState\":%s,\"depthEnable\":%s,\"depthWriteMask\":%u,\"stencilEnable\":%s,\"stencilWriteMask\":%u,\"stencilRef\":%u,\"stencilOps\":[%u,%u,%u,%u,%u,%u],\"targets\":[",GetTickCount64(),sequence,generation,view,pass,shader,d.reasons==0?"true":"false",d.reasons,d.viewportCount,d.viewport.TopLeftX,d.viewport.TopLeftY,d.viewport.Width,d.viewport.Height,d.depthBound?"true":"false",unsigned(d.depthView.ViewDimension),d.depthView.Flags,d.explicitState?"true":"false",d.depthState.DepthEnable?"true":"false",unsigned(d.depthState.DepthWriteMask),d.depthState.StencilEnable?"true":"false",d.depthState.StencilWriteMask,d.stencilRef,unsigned(d.depthState.FrontFace.StencilFailOp),unsigned(d.depthState.FrontFace.StencilDepthFailOp),unsigned(d.depthState.FrontFace.StencilPassOp),unsigned(d.depthState.BackFace.StencilFailOp),unsigned(d.depthState.BackFace.StencilDepthFailOp),unsigned(d.depthState.BackFace.StencilPassOp));
 for(unsigned i=0;i<8;++i){auto& t=d.targets[i];if(i)fprintf(output,",");if(!t.present){fprintf(output,"null");continue;}
  auto& r=t.resource;fprintf(output,"{\"viewDimension\":%u,\"viewFormat\":%u,\"viewUnion\":[%u,%u,%u],\"texture\":%s,\"width\":%u,\"height\":%u,\"format\":%u,\"array\":%u,\"samples\":%u}",unsigned(t.view.ViewDimension),unsigned(t.view.Format),t.view.Texture2DArray.MipSlice,t.view.Texture2DArray.FirstArraySlice,t.view.Texture2DArray.ArraySize,t.texture?"true":"false",r.Width,r.Height,unsigned(r.Format),r.ArraySize,r.SampleDesc.Count);
 }
 fprintf(output,"]}\n");fflush(output);
}

}
bool Initialize(ID3D11Device* d,ID3D11DeviceContext* c,unsigned w,unsigned h,const std::filesystem::path& directory,FILE* log){
 if(active || !d || !c || !log || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(owner.Get()!=d)return false;
 report=log;device=d;immediate=c;width=w;height=h;
 auto init=NvAPI_Initialize();NV_D3D1x_GRAPHICS_CAPS caps{};auto status=init==NVAPI_OK?NvAPI_D3D1x_GetGraphicsCapabilities(d,NV_D3D1x_GRAPHICS_CAPS_VER,&caps):init;
 if(status!=NVAPI_OK || !caps.bVariablePixelRateShadingSupported){fprintf(log,"REFUSED game-native VRS capability init=%d caps=%d supported=%u\n",init,status,caps.bVariablePixelRateShadingSupported);fflush(log);return false;}
 output=_wfsopen((directory/L"native-foveation.jsonl").c_str(),L"w",_SH_DENYNO);if(!output)return false;
 active=true;
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 settings=FoveationSettings::Load(directory/L"ACUnityVR.ini");requested=settings.enabled;geometryRequested=settings.enabled&&settings.geometry;hiddenEnabled=settings.hidden;
#else
 requested=false;
#endif
 lightingEnabled78=GetPrivateProfileIntW(L"Foveation",L"ExtendedLighting",0,(directory/L"ACUnityVR.ini").c_str())!=0;previousLightingKey78=false;
 fprintf(report,"Foveation ExtendedLighting=%d; NumPad6 compares only additional PS49/301..305 lighting coverage; actual colour/DSV/viewport write guards retained\n",lightingEnabled78);
 fault=false;previousF4=false;Event("initial",true);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 fprintf(log,"Native configurable VRS; F4 toggles hidden-area mask. Existing immediate context=%p, full %ux%u, AA unchanged. Accepted colour and SolidGBuffer scopes preserved; hidden-only scopes target PS36/392/509/504/479. No NVAPI deferred-context calls; each draw/list resets both states.\n",c,w,h);
#else
 fprintf(log,"Native colour VRS ready starts OFF/F4: existing immediate context=%p, full %ux%u, pixel PS276 lighting + PS391/387/30/4/461 post-effects only. No depth, shadow, UI, compute or command-list VRS. AA unchanged. Each draw resets both NVAPI states; eligibility follows actual DSV/write/read-only flags and accepts exact single-slice colour views. Bounded decision records identify rejections.\n",c,w,h);
#endif
 fprintf(log,"Foveation INI effective enabled=%d geometry=%d CentreRadius=%.4f Strength=%u HiddenAreaMask=%d HiddenAreaMargin=%u; restart required for INI changes\n",settings.enabled,settings.geometry,settings.radius,settings.strength,settings.hidden,settings.margin);fflush(log);return true;
}
bool BeginFrame(uint64_t id,uint32_t renderMode,const Transport::Eye* eyes,bool f4Down){
 if(!active || fault || bound)return false;sequence=id;mode=renderMode;draws[0]=draws[1]=declined[0]=declined[1]=lightingDraws78[0]=lightingDraws78[1]=0;
 const bool lightingKey78=(GetAsyncKeyState(VK_NUMPAD6)&0x8000)!=0;
 if(mode==1 && lightingKey78 && !previousLightingKey78){lightingEnabled78=!lightingEnabled78;decisionSeen=0;Event("lightingToggle",true);fprintf(report,"Extended lighting foveation78 %s/NumPad6 sequence=%llu; baseline VRS/hidden area and AA unchanged\n",lightingEnabled78?"ON":"OFF",sequence);fflush(report);}previousLightingKey78=lightingKey78;
 bool toggle=f4Down&&!previousF4;previousF4=f4Down;uint64_t next=generation+(toggle?1:0);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 if(toggle)hiddenEnabled=!hiddenEnabled;
 bool geometryWanted=settings.enabled&&settings.geometry;bool wanted=settings.enabled;
 geometryLists[0]=geometryLists[1]=hiddenDraws[0]=hiddenDraws[1]=hiddenLists[0]=hiddenLists[1]=0;
 if(!NativeGpuTiming::SetGeometryFoveationState(geometryWanted,next))return Fail("unfinished geometry GPU sample at pair boundary");geometryRequested=geometryWanted;
#else
 bool wanted=toggle?!requested.load():requested.load();
#endif
 if(!NativeGpuTiming::SetFoveationState(wanted,next))return Fail("unfinished GPU sample at pair boundary");
 requested=wanted;generation=next;if(toggle){Event("toggle",true);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 fprintf(report,"Native hidden-area mask %s/F4 generation=%llu sequence=%llu; accepted colour/geometry baseline unchanged, whole pair, AA unchanged\n",hiddenEnabled?"ON":"OFF",generation,id);
#else
 fprintf(report,"Native colour VRS %s/F4 generation=%llu sequence=%llu; whole pair, AA unchanged\n",wanted?"ON":"OFF",generation,id);
#endif
 fflush(report);}
 if(mode==1 && (!mapsReady || visibilityDirty || !SameFov(lastEyes[0],eyes[0]) || !SameFov(lastEyes[1],eyes[1])))if(!BuildMaps(eyes))return false;
 if(!NativeGpuTiming::SetHiddenAreaState(hiddenEnabled,hiddenReady,generation))return Fail("unfinished hidden-area GPU sample at pair boundary");
 return true;
}
void SetVisibility(const Transport::Visibility& value,uint32_t g){
 visibility=value;visibilityGeneration=g;visibilityDirty=true;
 if(output){fprintf(output,"{\"event\":\"visibleMesh\",\"generation\":%u,\"valid\":%u,\"coordinateSpace\":\"view z=-1\",\"eyes\":[",g,value.valid);
  for(unsigned eye=0;eye<2;++eye){const auto& e=value.eyes[eye];fprintf(output,"%s{\"vertices\":[",eye?",":"");
   for(unsigned i=0;i<std::min(e.vertexCount,Transport::VisibilityMaxVertices);++i)fprintf(output,"%s[%.9g,%.9g]",i?",":"",e.vertices[i].x,e.vertices[i].y);
   fprintf(output,"],\"indices\":[");for(unsigned i=0;i<std::min(e.indexCount,Transport::VisibilityMaxIndices);++i)fprintf(output,"%s%u",i?",":"",e.indices[i]);fprintf(output,"]}");
  }fprintf(output,"]}\n");fflush(output);
 }
}
bool NeedsScope(){return active && (requested || (hiddenEnabled&&hiddenReady)) && !fault && mode==1 && mapsReady;}
void DrawScope::Apply(ID3D11DeviceContext* c,unsigned view,unsigned pass,unsigned shader){
 bool accepted=FoveationMask::ShaderAllowed(pass,shader,lightingEnabled78),coarse=requested && accepted,maskOnly=MaskOnlyShader(pass,shader);
 if(!NeedsScope() || c!=immediate.Get() || view<1 || view>2 || (!coarse && !((maskOnly || accepted) && hiddenEnabled && hiddenReady)))return;
 unsigned eye=view-1;if(bound){Fail("overlapping native VRS draw");return;}
 auto decision=RenderTargetsMatch(c,maskOnly);ReportDecision(view,pass,shader,decision);if(decision.reasons){++declined[eye];return;}
 NV_D3D11_VIEWPORT_SHADING_RATE_DESC v{};v.enableVariablePixelShadingRate=true;for(auto& rate:v.shadingRateTable)rate=NV_PIXEL_X1_PER_RASTER_PIXEL;v.shadingRateTable[1]=PeripheralRate();v.shadingRateTable[2]=NV_PIXEL_X0_CULL_RASTER_PIXELS;
 NV_D3D11_VIEWPORTS_SHADING_RATE_DESC d{};d.version=NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;d.numViewports=1;d.pViewports=&v;
 // Mark ownership before either call so partial application is always reset.
 everApplied=true;bound=true;
 auto a=NvAPI_D3D11_RSSetShadingRateResourceView(c,maps[SelectedMap(coarse)][eye].Get());auto b=NvAPI_D3D11_RSSetViewportsPixelShadingRates(c,&d);
 if(a!=NVAPI_OK || b!=NVAPI_OK){Reset();Fail("NVAPI application status");return;}
 applied=true;++totalDraws;if(lightingEnabled78 && pass==0x504a0b0 && FoveationMask::ExtendedLightingShader(shader))++lightingDraws78[eye];if(coarse){++draws[eye];NativeGpuTiming::RecordFoveationDraw(view,pass,shader);}if(hiddenEnabled&&hiddenReady){++hiddenDraws[eye];NativeGpuTiming::RecordHiddenArea(view,false);}
}
DrawScope::~DrawScope(){if(applied)Reset();}
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
bool GeometryRequested(){return NeedsScope() && (geometryRequested || (hiddenEnabled&&hiddenReady));}
void CommandScope::Apply(ID3D11DeviceContext* c,unsigned view,unsigned descriptor,unsigned table,unsigned flags,unsigned laneBegin,unsigned laneEnd){
 if(!GeometryRequested() || c!=immediate.Get() || view<1 || view>2 || descriptor!=0x5049f30 || table!=0x30a90b0 || flags!=3 || laneEnd!=2 || laneBegin>=laneEnd)return;
 unsigned eye=view-1;if(bound){Fail("overlapping geometry VRS");return;}
 NV_D3D11_VIEWPORT_SHADING_RATE_DESC v{};v.enableVariablePixelShadingRate=true;for(auto& rate:v.shadingRateTable)rate=NV_PIXEL_X1_PER_RASTER_PIXEL;v.shadingRateTable[1]=PeripheralRate();v.shadingRateTable[2]=NV_PIXEL_X0_CULL_RASTER_PIXELS;
 NV_D3D11_VIEWPORTS_SHADING_RATE_DESC d{};d.version=NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;d.numViewports=1;d.pViewports=&v;everApplied=true;bound=true;
 bool coarse=geometryRequested;
 auto a=NvAPI_D3D11_RSSetShadingRateResourceView(c,maps[SelectedMap(coarse)][eye].Get());auto b=NvAPI_D3D11_RSSetViewportsPixelShadingRates(c,&d);
 if(a!=NVAPI_OK || b!=NVAPI_OK){Reset();Fail("geometry NVAPI application status");return;}
 applied=true;++totalDraws;if(geometryRequested){++geometryLists[eye];NativeGpuTiming::RecordGeometryFoveationList(view);}if(hiddenEnabled&&hiddenReady){++hiddenLists[eye];NativeGpuTiming::RecordHiddenArea(view,true);}
}
CommandScope::~CommandScope(){if(applied)Reset();}
#endif

void EndEye(unsigned eye,bool capture){
 if(!active)return;if(bound){Reset();Fail("VRS state reached eye boundary");}
 if(eye!=1)return;auto now=GetTickCount64();if(capture || now-lastLog>=5000){Event(capture?"capturePair":"pair",!fault);lastLog=now;}
}
bool Stop(){
 if(!active && !output)return true;active=false;requested=false;
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
 geometryRequested=false;hiddenEnabled=false;
#endif
 bool ok=Reset();Event("stop",ok);
 if(report){fprintf(report,"Native colour VRS retired draws=%llu resets=%llu bound=%d resetAPIok=%d; no GPU flush/wait\n",totalDraws,totalResets,bound,ok);fflush(report);}
 if(output){fclose(output);output=nullptr;}return ok;
}
}
