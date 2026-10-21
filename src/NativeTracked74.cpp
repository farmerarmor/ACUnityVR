// Native same-frame stereo: source camera + two independent native eye views.
#include "BuildGuard.h"
#include "GpuCapture.h"
#include "BulkDraw83.h"
#include "DenseVisibility87.h"
#include "PairProducer.h"
#include "UnityCamera.h"
#include "FirstPerson70.h"
#include "ProjectedShadowGuard37.h"
#include "DrawDiagnostics.h"
#include "CullingProbe63.h"
#include "SelectionBoundary46.h"
#include "OcclusionProbe.h"
#include "NativeGpuTiming.h"
#include "NativeFoveation.h"
#include "SourceRendering.h"
#include "GIProjectionTask.h"
#include "UiCamera71.h"
#include "GameDisplay.h"
#include "EntityVisibility41.h"
#include "ComponentGateProbe63.h"
#include "PrimaryCamera52.h"
#include "NpcCamera63.h"
#include "NpcCallback63.h"
#include "NpcDecision63.h"
#include "StationSubmission90.h"
#include "EarlyCamera91.h"
#include "NpcCone63.h"
#include "PrimaryScene52.h"
#include "NativeFault48.h"
#include "RenderSize.h"
#include "DisplaySettings.h"
#include <atomic>
#include <share.h>
namespace {
using acu::Field;using Task=void(__fastcall*)(void*);using PreTask=bool(__fastcall*)(void*);
using Create=void*(__fastcall*)(void*,void*,void*);using Bind=void(__fastcall*)(void*,void*);
struct Scope{SRWLOCK& lock;Scope(SRWLOCK& value):lock(value){AcquireSRWLockExclusive(&lock);}~Scope(){ReleaseSRWLockExclusive(&lock);}};
HMODULE self{};uint8_t *image{},*surface{},*source{},*sourceGraph{},*world{},*sceneGraph{},*eyes[2]{},*graphs[2]{};
void **sourcePreSlot{},**sourcePostSlot{},**sceneSlot{},**eyePostSlots[2]{},**eyePreSlots[2]{},**eyeCullSlots[2]{};
Task nativePost{},nativeScene{},nativeCull{};PreTask nativePre{};
FILE* log{};std::filesystem::path directory;SRWLOCK cameraLock=SRWLOCK_INIT;
std::atomic<unsigned> state{};std::atomic<bool> stop{};
bool ManualStopPressed(){
#ifdef ACUNITYVR_PORTABLE92
    return false;
#else
    return (GetAsyncKeyState(VK_F10)&0x8000)!=0;
#endif
}
uint16_t initialCount{};uint32_t initialActive{},leftFrame{},gpuReadyMask{};
uint64_t started{},lastLog{},sequence{},leftSequence{},trackedPairs{},rejected{};
unsigned captured{};bool pending{},leftHeld{},referenceValid{},trackingStarted{},publishAllowed{};
CameraMath::Matrix base{},ownedPoses[2]{};Transport::Pose reference{};Transport::RenderInfo rendered{};
PairProducer producer;unsigned eyeWidth{},eyeHeight{};float headsetRefresh{};
thread_local bool earlySceneBaseValid{};
thread_local CameraMath::Matrix earlySceneBase{};
bool EarlyHeadPose91(const CameraMath::Matrix& native,CameraMath::Matrix& head,uint64_t& tracking){
    Scope scope(cameraLock);Transport::Tracking pose{};
    if(state!=1 || stop || !referenceValid || !producer.ReadTracking(pose))return false;
    auto anchored=native;FirstPerson70::AnchorCamera(anchored);
    head=UnityCamera::EyeWorld(UnityCamera::LevelBase(anchored),reference,pose.head);tracking=pose.id;
    return CameraMath::Rigid(head);
}
uint8_t* canvasOptions{};LONG originalCanvasPolicy{};bool canvasActive{};unsigned canvasWarmupFrames{};
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool Set(void** slot,void* original,void* hook){return Writable(slot) && InterlockedCompareExchangePointer(slot,hook,original)==original;}
void Restore(void** slot,void* original,void* hook){if(slot)InterlockedCompareExchangePointer(slot,original,hook);}
bool __fastcall SourcePre(void*);void __fastcall SourcePost(void*);void __fastcall EyePost(void*);bool __fastcall EyePre(void*);void __fastcall SceneSetup(void*);void __fastcall EyeCull(void*);
Microsoft::WRL::ComPtr<ID3D11Texture2D> OutputTexture(){
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;auto unknown=acu::OutputInterface(surface);if(!unknown)return texture;
    Microsoft::WRL::ComPtr<ID3D11View> view;Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    if(SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&view)))){view->GetResource(&resource);resource.As(&texture);}return texture;
}
bool ActivateCanvas(){
    if(Field<void*>(surface,0)!=image+0x30aa560 || Field<void*>(source,0x420)!=surface || Field<void*>(source,0x248))return false;
    canvasOptions=Field<uint8_t*>(surface,0x50);
    if(!canvasOptions || !Writable(canvasOptions+0xc0))return false;
    originalCanvasPolicy=Field<LONG>(canvasOptions,0xc0);
    if(originalCanvasPolicy<0 || originalCanvasPolicy>4 || Field<LONG>(source,0x244)!=originalCanvasPolicy)return false;
    if(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(canvasOptions+0xc0),0,originalCanvasPolicy)!=originalCanvasPolicy)return false;
    canvasActive=true;
    fprintf(log,"Native render canvas activated at source frame boundary: policy=%ld->0; next source frame must use full %ux%u before eyes attach\n",originalCanvasPolicy,eyeWidth,eyeHeight);fflush(log);return true;
}
void RestoreCanvas(){
    if(!canvasActive)return;
    bool identity=Field<uint8_t*>(surface,0x50)==canvasOptions && Writable(canvasOptions+0xc0);
    bool restored=identity && InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(canvasOptions+0xc0),originalCanvasPolicy,0)==0;
    fprintf(log,"Native render canvas retired policyRestored=%d original=%ld; native source viewport will regenerate on its next preparation\n",restored,originalCanvasPolicy);fflush(log);canvasActive=false;
}
void Remove(){
    if(state==3)return;
    auto array=Field<void**>(surface,0x28);auto count=Field<uint16_t>(surface,0x32);
    bool owned=count>=initialCount && count<=initialCount+2;
    for(unsigned i=initialCount;owned && i<count;++i)owned=array[i]==eyes[i-initialCount];
    if(!owned){fprintf(log,"Retirement deferred: membership changed\n");fflush(log);stop=true;return;}
    Field<uint16_t>(surface,0x32)=initialCount;Field<uint32_t>(surface,0x48)=initialActive;
    Restore(sceneSlot,reinterpret_cast<void*>(nativeScene),reinterpret_cast<void*>(SceneSetup));
    Restore(sourcePreSlot,reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(SourcePre));
    Restore(sourcePostSlot,reinterpret_cast<void*>(nativePost),reinterpret_cast<void*>(SourcePost));
    for(unsigned i=0;i<2;++i){Restore(eyePostSlots[i],reinterpret_cast<void*>(nativePost),reinterpret_cast<void*>(EyePost));Restore(eyePreSlots[i],reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(EyePre));Restore(eyeCullSlots[i],reinterpret_cast<void*>(nativeCull),reinterpret_cast<void*>(EyeCull));}
    EarlyCamera91::Stop();StationSubmission90::Stop();DenseVisibility87::Stop();BulkDraw83::Stop();FirstPerson70::Stop();NpcCone63::Stop();NpcDecision63::Stop();NpcCallback63::Stop();NpcCamera63::Stop();SelectionBoundary46::Stop();PrimaryCamera52::Stop();ComponentGateProbe51::Stop();EntityVisibility41::Stop();CullingProbe::Stop();SourceRendering::Stop();UiCamera71::Stop();GIProjectionTask::Restore();DrawDiagnostics::Restore();ProjectedShadowGuard::Restore();ProjectedShadowGuard::Report(log);
    RestoreCanvas();producer.Stop();pending=false;state=3;
    fprintf(log,"Retired count=%u active=%u sourceOverride=%p sceneCallback=%d sourcePost=%d retained native views/resources until exit\n",Field<uint16_t>(surface,0x32),Field<uint32_t>(surface,0x48),Field<void*>(source,0x248),*sceneSlot==reinterpret_cast<void*>(nativeScene),*sourcePostSlot==reinterpret_cast<void*>(nativePost));fflush(log);
}
#ifdef ACUNITYVR_CLEAN_RUNTIME
bool SampleGpu(unsigned eye){__try{
 auto renderer=Field<uint8_t*>(image,0x524cfd8);auto backend=renderer?Field<uint8_t*>(renderer,0x10):nullptr;
 auto context=backend?Field<uint8_t*>(backend,0x20):nullptr;auto view=eyes[eye];auto renderState=Field<void*>(view,0x430);
 return context && Field<void*>(context,0x30)==view && Field<void*>(context,0x28)==renderState && RenderSize::Matches(Field<unsigned>(backend,0x4d0),Field<unsigned>(backend,0x4d4));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
#else
struct GpuSample{
    uint64_t sequence,tracking,context;uint32_t frame,eye,flags,pad;
    float pose[16],projection[16],inverseVP[16];uint8_t gpu[0x660],shadowState[0x600];
};
uint64_t sampledTick[2]{};unsigned sampleCount[2]{},gpuSnapshots{},gpuFailures{};
bool ReadGpu(unsigned eye,GpuSample* sample,void** targets){__try{
    auto renderer=Field<uint8_t*>(image,0x524cfd8);auto backend=Field<uint8_t*>(renderer,0x10);auto context=Field<uint8_t*>(backend,0x20);
    auto view=eyes[eye];auto renderState=Field<uint8_t*>(view,0x430);
    if(!context || Field<void*>(context,0x30)!=view || Field<void*>(context,0x28)!=renderState)return false;
    auto width=Field<unsigned>(backend,0x4d0),height=Field<unsigned>(backend,0x4d4);
    if(!RenderSize::Matches(width,height))return false;
    sample->sequence=sequence;sample->tracking=rendered.tracking.id;sample->context=reinterpret_cast<uint64_t>(context);
    sample->frame=Field<uint32_t>(source,0x294);sample->eye=eye;sample->flags=Field<uint32_t>(context,0x3c0);
    memcpy(sample->pose,view,64);memcpy(sample->projection,view+0x320,64);memcpy(sample->inverseVP,view+0x3e0,64);
    memcpy(sample->gpu,context,sizeof(sample->gpu));memcpy(sample->shadowState,renderState+0x240,sizeof(sample->shadowState));
    targets[0]=Field<void*>(backend,0xe48);targets[1]=Field<void*>(backend,0xe50);return true;
}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool SampleGpu(unsigned eye){
    GpuSample sample{};void* targets[2]{};if(!ReadGpu(eye,&sample,targets))return false;
    auto now=GetTickCount64();
    if(rendered.mode!=1 || sampleCount[eye]>=8 || (sampleCount[eye] && now-sampledTick[eye]<5000))return true;
    auto name=L"gpu-"+std::to_wstring(eye)+L"-"+std::to_wstring(sampleCount[eye])+L".bin";
    FILE* f=_wfsopen((directory/name).c_str(),L"wb",_SH_DENYNO);if(f){fwrite(&sample,sizeof(sample),1,f);fclose(f);}
    float projectionError{};for(unsigned i=0;i<16;++i)projectionError=(std::max)(projectionError,std::abs(sample.projection[i]-reinterpret_cast<float*>(sample.gpu+0x60)[i]));
    fprintf(log,"GPU sample eye=%u sequence=%llu pose=%llu context=%p flags=%x projectionError=%.9g shadowTargets=%p/%p SampleSize=%zu\n",eye,sequence,rendered.tracking.id,reinterpret_cast<void*>(sample.context),sample.flags,projectionError,targets[0],targets[1],sizeof(sample));fflush(log);
    sampledTick[eye]=now;++sampleCount[eye];++gpuSnapshots;return true;
}
#endif
bool Projection(uint8_t* view,uint8_t* renderState,unsigned eye){
    CameraMath::Matrix p{},inverse{};
    if(!UnityCamera::Projection(CameraMath::Load(view+0x320),rendered.tracking.eyes[eye],p) || !CameraMath::Inverse(p,inverse))return false;
    auto graphicsWorld=UnityCamera::ToGraphics(ownedPoses[eye]);auto graphicsView=CameraMath::InverseRigid(graphicsWorld);
    auto vp=CameraMath::Multiply(graphicsView,p);auto inverseVP=CameraMath::Multiply(inverse,graphicsWorld);
    CameraMath::Store(view+0x2e0,vp);CameraMath::Store(view+0x320,p);CameraMath::Store(view+0x360,graphicsWorld);
    CameraMath::Store(view+0x3a0,inverse);CameraMath::Store(view+0x3e0,inverseVP);
    // Native frustum consumes the same normal-depth VP. Native state preparation
    // copies VP/pose/projection/frustum to +0/+40/+80/+c0 (21e1678..21e16f9).
    auto frustum=Field<uint8_t*>(view,0x428);
    reinterpret_cast<Bind>(image+0x223bd70)(frustum,view+0x2e0);
    memcpy(renderState,view+0x2e0,64);memcpy(renderState+0x40,view,64);
    memcpy(renderState+0x80,view+0x320,64);memcpy(renderState+0xc0,frustum,0xc0);
    return true;
}
bool Prepare(unsigned eye,bool tracked){
    auto view=eyes[eye];auto renderState=Field<uint8_t*>(view,0x430);
    if(Field<void*>(renderState,0x8208)!=view || Field<void*>(graphs[eye],0x10)!=world)return false;
    memcpy(view,source,0x240);memcpy(view+0x25c,source+0x25c,0x34);memcpy(view+0x29c,source+0x29c,0x34);
    auto pose=tracked?UnityCamera::EyeWorld(UnityCamera::LevelBase(base),reference,rendered.tracking.eyes[eye].pose):base;
    if(!tracked)for(unsigned a=0;a<3;++a)pose.m[12+a]+=(eye?.032f:-.032f)*pose.m[a];
    if(!CameraMath::Rigid(pose))return false;
    ownedPoses[eye]=pose;Field<void*>(view,0x248)=&ownedPoses[eye];Field<uint8_t>(view,0x43c)=0;
    CameraMath::Store(view,pose);CameraMath::Store(view+0x40,pose);
    // Native2226b80 regenerates the normalized viewport only when the cached
    // policy differs from surface settings+c0. Keep its verified policy identity
    // but give only our owned eyes a full canvas before native camera derivation.
    // Source pose/override and executable code stay untouched. A session-scoped
    // native display policy keeps the source and both eyes on the same canvas.
    auto displaySettings=Field<uint8_t*>(surface,0x50);if(!displaySettings)return false;
    auto policy=Field<uint32_t>(displaySettings,0xc0);
    if(policy!=0 || Field<uint32_t>(source,0x244)!=policy)return false;
    Field<uint32_t>(view,0x244)=policy;
    const float fullCanvas[4]{0,0,1,1};memcpy(view+0x29c,fullCanvas,sizeof(fullCanvas));
    // Exact native21d7f50 consists of these two calls. Insert per-eye projection
    // between camera/state derivation and QT setup so culling and GPU agree.
    reinterpret_cast<Bind>(image+0x21e11b0)(renderState,view);
    // Refuse a cropped or padded scene before scheduling culling/rendering.
    auto rect=reinterpret_cast<float*>(view+0x2bc);
    if(rect[0]!=0 || rect[1]!=0 || rect[2]!=float(eyeWidth) || rect[3]!=float(eyeHeight)){
        fprintf(log,"REFUSED native eye canvas eye=%u rect=(%.3f,%.3f,%.3f,%.3f) requested=%ux%u\n",eye,rect[0],rect[1],rect[2],rect[3],eyeWidth,eyeHeight);fflush(log);return false;
    }
    if(sequence<=3){fprintf(log,"Full native canvas eye=%u policy=%u rect=%.0fx%.0f aspect=%.9g\n",eye,policy,rect[2],rect[3],Field<float>(view,0x2cc));fflush(log);}
    if(tracked && !Projection(view,renderState,eye))return false;
    reinterpret_cast<Task>(image+0x21d8830)(renderState);
    return FirstPerson70::Selection(view);
}
bool Attach(){
    initialCount=Field<uint16_t>(surface,0x32);initialActive=Field<uint32_t>(surface,0x48);
    if(initialCount!=1 || (Field<uint16_t>(surface,0x30)&0x3fff)<3 || Field<void**>(surface,0x28)[0]!=source || Field<void*>(source,0x248))return false;
    auto descriptor=Field<uint8_t*>(image,0x4fc5b38);
    if(!descriptor || Field<uint32_t>(descriptor,0x28)!=0x460 || Field<void*>(descriptor,0x50)!=image+0x21eca00)return false;
    base=CameraMath::Load(source);
    for(unsigned eye=0;eye<2;++eye){
        eyes[eye]=static_cast<uint8_t*>(reinterpret_cast<Create>(image+0x26eb1a0)(nullptr,nullptr,descriptor));auto view=eyes[eye];
        if(!view || (reinterpret_cast<uintptr_t>(view)&15))return false;
        graphs[eye]=Field<uint8_t*>(view,0x440);
        if(!graphs[eye] || Field<void*>(graphs[eye],0)!=view || !Field<void*>(view,0x428) || !Field<void*>(view,0x430))return false;
        eyePostSlots[eye]=reinterpret_cast<void**>(graphs[eye]+0x3b78);eyePreSlots[eye]=reinterpret_cast<void**>(graphs[eye]+0x39d0);
        eyeCullSlots[eye]=reinterpret_cast<void**>(graphs[eye]+0x978);
        if(*eyePostSlots[eye]!=reinterpret_cast<void*>(nativePost) || *eyePreSlots[eye]!=reinterpret_cast<void*>(nativePre) || *eyeCullSlots[eye]!=reinterpret_cast<void*>(nativeCull))return false;
        reinterpret_cast<Bind>(image+0x21f5880)(surface,view);reinterpret_cast<Bind>(image+0x2213e20)(view,source+0x250);
        if(!Prepare(eye,false) || !Set(eyePostSlots[eye],reinterpret_cast<void*>(nativePost),reinterpret_cast<void*>(EyePost)) || !Set(eyePreSlots[eye],reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(EyePre)) || !Set(eyeCullSlots[eye],reinterpret_cast<void*>(nativeCull),reinterpret_cast<void*>(EyeCull)))return false;
    }
    if(!SelectionBoundary46::Install(image,eyes[0],eyes[1],[](unsigned eye,void* view){Scope scope(cameraLock);if(state==1 && pending)CullingProbe::AfterSelection(eye,view);},log))return false;
    BulkDraw83::SetEyes(eyes[0],eyes[1]);ComponentGateProbe51::SetEyes(eyes[0],eyes[1]);EntityVisibility41::SetEyes(eyes[0],eyes[1]);OcclusionProbe::SetEyes(eyes[0],eyes[1]);SourceRendering::SetEyes(eyes[0],eyes[1]);DrawDiagnostics::SetEyes(eyes[0],eyes[1]);GIProjectionTask::SetEyes(eyes[0],eyes[1]);UiCamera71::SetEyes(eyes[0],eyes[1]);
    if(!Set(sceneSlot,reinterpret_cast<void*>(nativeScene),reinterpret_cast<void*>(SceneSetup)))return false;
    started=GetTickCount64();state=1;ProjectedShadowGuard::Enable(true);
    fprintf(log,"Attached owned eyes=%p/%p states=%p/%p graphs=%p/%p count=%u source untouched\n",eyes[0],eyes[1],Field<void*>(eyes[0],0x430),Field<void*>(eyes[1],0x430),graphs[0],graphs[1],Field<uint16_t>(surface,0x32));fflush(log);return true;
}
void BoundSource52(void* graph){
    Scope scope(cameraLock);if(state!=1 || graph!=sceneGraph)return;
    if(pending){fprintf(log,"REFUSED next preparation before pair completed\n");fflush(log);stop=true;return;}
    if(Field<void*>(sceneGraph,8)!=source || Field<void*>(source,0x248) || Field<void*>(sourceGraph,0x10)!=world){stop=true;return;}
    producer.Pace();base=earlySceneBaseValid?earlySceneBase:CameraMath::Load(source);rendered={};rendered.eyeMask=3;gpuReadyMask=0;++sequence;
    bool tracked=producer.ReadTracking(rendered.tracking);
    static bool f9Down{};bool f9=(GetAsyncKeyState(VK_F9)&0x8000)!=0;bool recenter=f9&&!f9Down;f9Down=f9;
    if(tracked){
        if(!referenceValid || recenter){reference=CameraMath::LevelReference(rendered.tracking.head);referenceValid=true;fprintf(log,"Tracking reference=%llu base=(%.3f,%.3f,%.3f)\n",rendered.tracking.id,base.m[12],base.m[13],base.m[14]);fflush(log);}
        rendered.mode=1;trackingStarted=true;
    }
    FirstPerson70::BeginFrame(tracked,sequence,base);
    CameraMath::Matrix primaryPoses[2]{};
    if(tracked)for(unsigned eye=0;eye<2;++eye)primaryPoses[eye]=UnityCamera::EyeWorld(UnityCamera::LevelBase(base),reference,rendered.tracking.eyes[eye].pose);
    NpcCone63::BeginFrame(Field<uint32_t>(source,0x294),sequence,rendered.tracking.id,tracked,primaryPoses,rendered.tracking.eyes);NpcDecision63::BeginFrame(sequence);
    PrimaryCamera52::BeginFrame(Field<uint32_t>(source,0x294),sequence,rendered.tracking.id,tracked,primaryPoses,rendered.tracking.eyes);
    BulkDraw83::Publish(Field<uint32_t>(source,0x294),sequence,rendered.tracking.id,tracked,primaryPoses,rendered.tracking.eyes);
    StationSubmission90::Publish(Field<uint32_t>(source,0x294));
    static uint32_t visibilityGeneration{};static Transport::Visibility visibility{};
    if(producer.ReadVisibility(visibilityGeneration,visibility))NativeFoveation::SetVisibility(visibility,visibilityGeneration);
    if(!NativeFoveation::BeginFrame(sequence,rendered.mode,rendered.tracking.eyes,(GetAsyncKeyState(VK_F4)&0x8000)!=0)){stop=true;return;}
    SourceRendering::SetTracked(tracked);
    publishAllowed=tracked || !trackingStarted;
    // Keep one pair pose, then prepare each native eye at its own culling
    // boundary, preserving the engine's per-view query/resource ordering.
    GIProjectionTask::BeginFrame(sequence,rendered.tracking.id,rendered.mode);
    UiCamera71::BeginFrame(sequence,rendered);
    DrawDiagnostics::BeginFrame(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294),rendered.mode);
    CullingProbe::BeginFrame(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294),rendered.mode,DrawDiagnostics::Capturing());
    NpcCallback63::CaptureIfRequested(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294));
    NpcCamera63::CaptureIfRequested(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294));
    NpcCone63::CaptureIfRequested(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294));
    NpcDecision63::CaptureIfRequested(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294));
    BulkDraw83::Poll(sequence,rendered.tracking.id,Field<uint32_t>(source,0x294));DenseVisibility87::Poll(sequence);EarlyCamera91::Poll(sequence);
    CullingProbe::SetSecondary(false,0);
    pending=true;
}
bool SceneRead52(void* out,uint64_t address,size_t bytes) noexcept {__try{if(!out || !address || !bytes || bytes>0x10000)return false;memcpy(out,reinterpret_cast<void*>(address),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void __fastcall SceneSetup(void* graph){
    struct ResetEarly{~ResetEarly(){earlySceneBaseValid=false;}} resetEarly;
    earlySceneBaseValid=graph==sceneGraph && EarlyCamera91::BeforeScene(earlySceneBase);
    ComponentGateProbe51::Poll();SourceRendering::BeginFrame();
    if(state!=1 || graph!=sceneGraph){nativeScene(graph);return;}
    auto result=PrimaryScene52::Execute(SceneRead52,graph,source,Field<void*>(source,0x430),world,reinterpret_cast<Bind>(image+0x21e11b0),reinterpret_cast<Task>(image+0x21d8830),reinterpret_cast<PrimaryScene52::Sort>(image+0x2187ba0),BoundSource52,graph);
    if(result==PrimaryScene52::Result::RefusedBefore){nativeScene(graph);stop=true;fprintf(log,"REFUSED primary scene52 identity before native calls; original forwarded once\n");fflush(log);}
    if(result==PrimaryScene52::Result::RefusedAfter){stop=true;fprintf(log,"REFUSED primary scene52 component-list input after Bind/QT; no double native preparation\n");fflush(log);}
    earlySceneBaseValid=false;
}
void __fastcall EyeCull(void* graph){
    {Scope scope(cameraLock);if(state==1 && pending){unsigned eye=graph==graphs[1]?1:0;
        if(!Prepare(eye,rendered.mode==1)){fprintf(log,"REFUSED per-eye culling preparation eye=%u\n",eye);fflush(log);stop=true;}
    }}
    nativeCull(graph);
    {Scope scope(cameraLock);if(state==1 && pending){unsigned eye=graph==graphs[1]?1:0;CullingProbe::AfterCull(eye,eyes[eye],graph);}}
}
bool __fastcall EyePre(void* graph){
    {
        Scope scope(cameraLock);if(state==1 && pending){
            unsigned eye=graph==graphs[1]?1:0;auto view=eyes[eye];auto renderState=Field<uint8_t*>(view,0x430);
            bool match=Field<void*>(graph,0)==view && !memcmp(renderState,view+0x2e0,64) && !memcmp(renderState+0x80,view+0x320,64);
            if(match)gpuReadyMask|=1u<<eye;else {stop=true;fprintf(log,"REFUSED pre-render cache identity eye=%u\n",eye);fflush(log);}
        }
    }
    if(state==1 && pending)NativeGpuTiming::BeginEye(graph==graphs[1]?1:0);
    if(state==1 && pending)CullingProbe::BeforeRender(graph==graphs[1]?1:0,eyes[graph==graphs[1]?1:0]);
    bool result=nativePre(graph);
    Scope scope(cameraLock);
    if(state==1 && pending){unsigned eye=graph==graphs[1]?1:0;
        if(!SampleGpu(eye)){
#ifndef ACUNITYVR_CLEAN_RUNTIME
            ++gpuFailures;
#endif
            stop=true;gpuReadyMask&=~(1u<<eye);fprintf(log,"REFUSED native GPU context identity eye=%u\n",eye);fflush(log);}
    }
    return result;
}
#ifndef ACUNITYVR_CLEAN_RUNTIME
void Metadata(){
    FILE* f=_wfsopen((directory/(L"tracked-"+std::to_wstring(captured)+L".json")).c_str(),L"w",_SH_DENYNO);if(!f)return;
    fprintf(f,"{\"sourceFrame\":%u,\"trackingId\":%llu,\"sequence\":%llu,\"gpuReadyMask\":%u,\"eyes\":[",leftFrame,rendered.tracking.id,sequence,gpuReadyMask);
    for(unsigned eye=0;eye<2;++eye){fprintf(f,"%s{\"pose\":[",eye?",":"");for(unsigned i=0;i<16;++i)fprintf(f,"%s%.9g",i?",":"",reinterpret_cast<float*>(eyes[eye])[i]);fprintf(f,"],\"projection\":[");for(unsigned i=0;i<16;++i)fprintf(f,"%s%.9g",i?",":"",reinterpret_cast<float*>(eyes[eye]+0x320)[i]);fprintf(f,"]}");}
    fprintf(f,"]}\n");fclose(f);
}
#endif
void __fastcall EyePost(void* graph){
    nativePost(graph);NativeGpuTiming::EndView(graph==graphs[1]?2:1);Scope scope(cameraLock);if(state!=1 || !pending)return;
    bool right=graph==graphs[1];auto texture=OutputTexture();
    DrawDiagnostics::EndEye(right?1:0);
    CullingProbe::EndEye(right?1:0,eyes[right?1:0]);
    
    if(!right){
        leftFrame=Field<uint32_t>(source,0x294);leftSequence=sequence;
        
#ifndef ACUNITYVR_CLEAN_RUNTIME
    if(rendered.mode==1 && captured<2)acu::CaptureOutput(surface,directory/(L"tracked-"+std::to_wstring(captured)+L"-left.bmp"),log);
#endif

        leftHeld=publishAllowed && producer.Left(texture.Get());
        if(!acu::ClearOutput(surface,nullptr))stop=true;
        return;
    }
    bool same=leftFrame==Field<uint32_t>(source,0x294) && leftSequence==sequence && gpuReadyMask==3;
    
#ifndef ACUNITYVR_CLEAN_RUNTIME
    if(rendered.mode==1 && captured<2 && same){acu::CaptureOutput(surface,directory/(L"tracked-"+std::to_wstring(captured)+L"-right.bmp"),log);Metadata();++captured;}
#endif

    NativeGpuTiming::BeginTransport();
    bool published=same && leftHeld && publishAllowed && producer.Right(texture.Get(),leftFrame,rendered);
    if(!published){producer.Cancel();++rejected;}else if(rendered.mode==1)++trackedPairs;
    NativeGpuTiming::EndTransport();
    CullingProbe::EndPair();pending=false;leftHeld=false;
    auto now=GetTickCount64();if(now-lastLog>=5000){fprintf(log,"pairs=%llu tracked=%llu rejected=%llu sourceFrame=%u sequence=%llu/%llu pose=%llu mode=%u gpuMask=%u sameInterval=%d\n",producer.Published(),trackedPairs,rejected,leftFrame,leftSequence,sequence,rendered.tracking.id,rendered.mode,gpuReadyMask,same);fflush(log);lastLog=now;}
    if(!same)stop=true;

    
#ifndef ACUNITYVR_CLEAN_RUNTIME
    if(stop || producer.StopRequested())fprintf(log,"Final eye boundary: GPU samples=%u failures=%u sequence=%llu\n",gpuSnapshots,gpuFailures,sequence);
#endif

    if(stop || producer.StopRequested())Remove();
}
bool __fastcall SourcePre(void* graph){
    if(state==1 && pending)NativeGpuTiming::BeginSource(sequence,rendered.tracking.id,rendered.mode,DrawDiagnostics::Capturing());
    return nativePre(graph);
}
void __fastcall SourcePost(void* graph){
    nativePost(graph);NativeGpuTiming::EndView(0);Scope scope(cameraLock);
    if(state==0){
        if(stop || producer.StopRequested() || ManualStopPressed()){Remove();return;}
        if(!canvasActive){if(!ActivateCanvas()){fprintf(log,"REFUSED source canvas policy identity\n");fflush(log);Remove();}return;}
        auto rect=reinterpret_cast<float*>(source+0x2bc);
        if(Field<LONG>(source,0x244)!=0 || rect[0]!=0 || rect[1]!=0 || rect[2]!=float(eyeWidth) || rect[3]!=float(eyeHeight)){
            if(++canvasWarmupFrames>=4){fprintf(log,"REFUSED native source canvas did not regenerate\n");fflush(log);Remove();}return;
        }
        fprintf(log,"Native source canvas ready=%ux%u aspect=%.9g; original source pose/override preserved\n",eyeWidth,eyeHeight,Field<float>(source,0x2cc));fflush(log);
        if(!Attach()){fprintf(log,"Attachment declined\n");fflush(log);Remove();}return;
    }
    if(state!=1)return;
    if(stop || producer.StopRequested() || ManualStopPressed()){
        stop=true;
        // SceneSetup has already scheduled both owned-eye renders. Retire only
        // at the final right-eye Post boundary after this interval completes.
        if(!pending)Remove();
        return;
    }
    // Original scene output is not an eye. Clear before first owned-eye render;
    // EyePost then clears between owned left and right outputs.
    if(!acu::ClearOutput(surface,nullptr))stop=true;
}
bool Find(){__try{
    surface=Field<uint8_t*>(image,0x523c6d8);if(!surface || Field<uint16_t>(surface,0x32)!=1)return false;
    source=Field<uint8_t*>(Field<void*>(surface,0x28),0);if(!source || Field<void*>(source,0x420)!=surface || Field<void*>(source,0x248))return false;
    sourceGraph=Field<uint8_t*>(source,0x440);if(!sourceGraph || Field<void*>(sourceGraph,0)!=source)return false;
    world=Field<uint8_t*>(sourceGraph,0x10);sceneGraph=Field<uint8_t*>(world,0x1e8);if(!sceneGraph || Field<void*>(sceneGraph,8)!=source)return false;
    sourcePreSlot=reinterpret_cast<void**>(sourceGraph+0x39d0);sourcePostSlot=reinterpret_cast<void**>(sourceGraph+0x3b78);sceneSlot=reinterpret_cast<void**>(sceneGraph+0xce0);
    nativePost=reinterpret_cast<Task>(image+0x21a8d90);nativePre=reinterpret_cast<PreTask>(image+0x21a8db0);nativeScene=reinterpret_cast<Task>(image+0x219f480);nativeCull=reinterpret_cast<Task>(image+0x219f150);
    return Writable(sourcePreSlot) && *sourcePreSlot==reinterpret_cast<void*>(nativePre) && Writable(sourcePostSlot) && Writable(sceneSlot) && *sourcePostSlot==reinterpret_cast<void*>(nativePost) && *sceneSlot==reinterpret_cast<void*>(nativeScene) && acu::OutputInterface(surface);
}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
DWORD WINAPI Run(void*){
    wchar_t path[32768]{};GetModuleFileNameW(self,path,_countof(path));directory=std::filesystem::path(path).parent_path();
    #ifdef ACUNITYVR_CLEAN_RUNTIME
    const auto logName=L"ACUnityTracked74Clean.log";
#else
    #ifdef ACUNITYVR_PORTABLE92
    const auto logName=L"ACUnityVR-native.log";
    #else
    const auto logName=L"ACUnityTracked74.log";
    #endif
#endif
    log=_wfsopen((directory/logName).c_str(),L"w",_SH_DENYNO);if(!log)return 1;image=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if(!NativeFault48::Install(directory,log)){fprintf(log,"REFUSED temporary fault recorder\n");fflush(log);return 11;}
    struct FaultScope{~FaultScope(){NativeFault48::Stop();}} faultScope;

    const struct{size_t rva;const char* bytes;size_t size;} guards[]={
        {0x2290c4c,"\x4c\x8d\x4c\x24\x60\x4d\x8b\xc4\x49\x8b\xd7\x49\x8b\xce\xe8\xc1\x1a\x03\x00",19},
        {0x22c2755,"\x48\x8b\x4f\x28\x33\xc0\x48\x8b\xd7\x48\x89\x87\xf0\x02\x00\x00\x48\x89\x87\x00\x03\x00\x00\x8b\x5b\x1c\x44\x8b\xcb\x4d\x69\xc9\x50\x09\x00\x00\x49\x8b\x84\x09\x38\x3f\x00\x00\x4d\x8b\x84\x09\x30\x3f\x00\x00\x4d\x8b\x8c\x09\x40\x3f\x00\x00\x48\x8b\xce\x48\x89\x44\x24\x28\xe8\x92\x63\x01\x00",73},
        {0x22d8b7d,"\x48\x8b\x87\xc0\x00\x00\x00\x48\x8b\x18\x48\x85\xdb\x74\x06",15},
        {0x219f150,"\x48\x83\xec\x38\x48\x8b\x41\x08\x4c\x8d\x89\xd8\x15\x00\x00\x4c",16},
        {0x2291970,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x49\x8b\x08\x48\x8b\xfa",16},
        {0x2290b40,"\x4c\x8b\xdc\x53\x48\x81\xec\x80\x00\x00\x00\x48\x63\x41\x2c\x49",16},
        {0x219f480,"\x40\x53\x48\x83\xec\x20\x48\x8b\xd9",9},
        {0x21a8d90,"\x48\x8b\x11\x48\x8b\x0d\x3e\x42\x0a\x03\x48\x8b\x01\x48\xff\x60\x18",17},
        {0x21a8db0,"\x48\x8b\x11\x48\x8b\x0d\x1e\x42\x0a\x03\xe9\x71\x2f\x11\x00",15},
        {0x21f5880,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20",10},
        {0x2213e20,"\x40\x53\x55\x56\x48\x83\xec\x20",8},
        {0x21e11b0,"\x4c\x8b\xdc\x53\x57\x41\x57\x48\x81\xec\x40\x01\x00\x00",14},
        {0x21d8830,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10",10},
        {0x223bd70,"\x40\x55\x48\x8b\xec\x48\x83\xec\x10",9},
        {0x26eb1a0,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10",10},{0x219f480,"\x40\x53\x48\x83\xec\x20\x48\x8b\xd9\x48\x8b\x49\x08\x48\x8b\x89\x30\x04\x00\x00\xe8\xb7\x8a\x03\x00\x8b\x93\xe0\x13\x00\x00\x48\x8b\x8b\xd8\x13\x00\x00\xc1\xe2\x03\x45\x33\xc9\x48\x03\xd1\x4c\x8b\xc2\x4c\x2b\xc1\x49\xc1\xf8\x03\x48\x83\xc4\x20\x5b\xe9\xdd\x86\xfe\xff",67},{0x21d7f50,"\x40\x53\x48\x83\xec\x20\x48\x8b\x91\x08\x82\x00\x00\x48\x8b\xd9\xe8\x4b\x92\x00\x00\x48\x8b\xcb\x48\x83\xc4\x20\x5b\xe9\xbe\x08\x00\x00",34},{0x223bd70,"\x40\x55\x48\x8b\xec\x48\x83\xec\x10\x0f\x28\x0a\x0f\x28\x52\x10\x0f\x28\xc1\x0f\xc6\xc1\xff\xf3\x0f\x58\xc8\x0f\x28\xc2\xf3\x0f\x11\x4d\xf0\x0f\xc6\xc2\xff\xf3\x0f\x58\xd0\x0f\x28\x4a\x20\x0f\x28\xc1\xf3\x0f\x11\x55\xf4\x0f\x28\x52\x30\x0f\xc6\xc1\xff\xf3\x0f\x58\xc8\x0f\x28\xc2\xf3\x0f\x11\x4d\xf8\x0f\xc6\xc2\xff\xf3\x0f\x58\xd0\xf3\x0f\x11\x55\xfc\x0f\x28\x65\xf0\x0f\x28\xdc\x0f\x59\xdc\x0f\x28\xcb\x0f\x28\xc3\x0f\xc6\xc3\x55\x0f\xc6\xcb\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd8\x0f\xc6\xdb\x00\x0f\x51\xc3\x0f\x5e\xe0\x0f\x28\x05\x07\xa6\xc3\x00\x0f\x57\xc4\x0f\x29\x65\xf0\x0f\x29\x41\x10\x0f\x28\x0a\x0f\x28\x52\x10\x0f\x28\xc1\x0f\xc6\xc1\xff\xf3\x0f\x5c\xc1\x0f\x28\x4a\x20\xf3\x0f\x11\x45\xf0\x0f\x28\xc2\x0f\xc6\xc2\xff\xf3\x0f\x5c\xc2\x0f\x28\x52\x30\xf3\x0f\x11\x45\xf4\x0f\x28\xc1\x0f\xc6\xc1\xff\xf3\x0f\x5c\xc1\xf3\x0f\x11\x45\xf8\x0f\x28\xc2\x0f\xc6\xc2\xff\xf3\x0f\x5c\xc2\xf3\x0f\x11\x45\xfc\x0f\x28\x5d\xf0\x0f\x28\xd3\x0f\x59\xd3\x0f\x28\xca\x0f\x28\xc2\x0f\xc6\xc2\x55\x0f\xc6\xca\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd0\x0f\xc6\xd2\x00\x0f\x51\xc2\x0f\x5e\xd8\x0f\x28\x05\x7c\xa5\xc3\x00\x0f\x57\xc3\x0f\x29\x5d\xf0\x0f\x29\x41\x20\xf3\x0f\x10\x42\x0c\xf3\x0f\x5c\x42\x04\xf3\x0f\x11\x45\xf0\xf3\x0f\x10\x42\x1c\xf3\x0f\x5c\x42\x14\xf3\x0f\x11\x45\xf4\xf3\x0f\x10\x42\x2c\xf3\x0f\x5c\x42\x24\xf3\x0f\x11\x45\xf8\xf3\x0f\x10\x42\x3c\xf3\x0f\x5c\x42\x34\xf3\x0f\x11\x45\xfc\x0f\x28\x5d\xf0\x0f\x28\xd3\x0f\x59\xd3\x0f\x28\xca\x0f\x28\xc2\x0f\xc6\xc2\x55\x0f\xc6\xca\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd0\x0f\xc6\xd2\x00\x0f\x51\xc2\x0f\x5e\xd8\x0f\x28\x05\x04\xa5\xc3\x00\x0f\x57\xc3\x0f\x29\x5d\xf0\x0f\x29\x41\x30\xf3\x0f\x10\x42\x04\xf3\x0f\x58\x42\x0c\xf3\x0f\x11\x45\xf0\xf3\x0f\x10\x42\x14\xf3\x0f\x58\x42\x1c\xf3\x0f\x11\x45\xf4\xf3\x0f\x10\x42\x24\xf3\x0f\x58\x42\x2c\xf3\x0f\x11\x45\xf8\xf3\x0f\x10\x42\x34\xf3\x0f\x58\x42\x3c\xf3\x0f\x11\x45\xfc\x0f\x28\x5d\xf0\x0f\x28\xd3\x0f\x59\xd3\x0f\x28\xca\x0f\x28\xc2\x0f\xc6\xc2\x55\x0f\xc6\xca\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd0\x0f\xc6\xd2\x00\x0f\x51\xc2\x0f\x5e\xd8\x0f\x28\x05\x8c\xa4\xc3\x00\x0f\x57\xc3\x0f\x29\x5d\xf0\x0f\x29\x41\x40\xf3\x0f\x10\x4a\x08\xf3\x0f\x10\x42\x18\xf3\x0f\x11\x45\xf4\xf3\x0f\x10\x42\x38\xf3\x0f\x11\x4d\xf0\xf3\x0f\x10\x4a\x28\xf3\x0f\x11\x45\xfc\xf3\x0f\x11\x4d\xf8\x0f\x28\x5d\xf0\x0f\x28\xd3\x0f\x59\xd3\x0f\x28\xca\x0f\x28\xc2\x0f\xc6\xc2\x55\x0f\xc6\xca\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd0\x0f\xc6\xd2\x00\x0f\x51\xc2\x0f\x5e\xd8\x0f\x28\x05\x28\xa4\xc3\x00\x0f\x57\xc3\x0f\x29\x5d\xf0\x0f\x29\x01\xf3\x0f\x10\x42\x0c\xf3\x0f\x5c\x42\x08\xf3\x0f\x11\x45\xf0\xf3\x0f\x10\x42\x1c\xf3\x0f\x5c\x42\x18\xf3\x0f\x11\x45\xf4\xf3\x0f\x10\x42\x2c\xf3\x0f\x5c\x42\x28\xf3\x0f\x11\x45\xf8\xf3\x0f\x10\x42\x3c\xf3\x0f\x5c\x42\x38\xf3\x0f\x10\x25\xef\xa3\x49\x01\x48\x83\xc1\x0c\xb8\x06\x00\x00\x00\xf3\x0f\x11\x45\xfc\x0f\x28\x5d\xf0\x0f\x28\xd3\x0f\x59\xd3\x0f\x28\xca\x0f\x28\xc2\x0f\xc6\xc2\x55\x0f\xc6\xca\xaa\xf3\x0f\x58\xc1\xf3\x0f\x58\xd0\x0f\xc6\xd2\x00\x0f\x51\xc2\x0f\x5e\xd8\x0f\x57\x1d\xa0\xa3\xc3\x00\x0f\x29\x59\x44\xf3\x0f\x10\x1d\x94\xa3\xc3\x00\x0f\x1f\x40\x00\x0f\x28\x05\x79\x9b\xc3\x00\x0f\x54\x41\xf4\x0f\x28\xc8\x0f\x29\x45\xf0\xf3\x0f\x10\x55\xf8\x0f\xc6\xc8\x55\x0f\x2f\xc1\x72\x27\x0f\x2f\xc2\x72\x22\xf3\x0f\x10\x09\x0f\x57\xc0\xf3\x0f\x5e\x49\xf4\x0f\x57\xcb\x0f\x28\xd1\x0f\x14\xd0\x0f\x14\x15\x2f\x62\x3b\x01\x0f\x29\x51\x54\xeb\x40\x0f\x2f\xca\x72\x1e\xf3\x0f\x10\x09\x0f\x57\xd2\x0f\x28\xc4\xf3\x0f\x5e\x49\xf8\x0f\x57\xcb\x0f\x14\xcc\x0f\x14\xd1\x0f\x29\x51\x54\xeb\x1d\xf3\x0f\x10\x01\x0f\x57\xc9\xf3\x0f\x5e\x41\xfc\x0f\x57\xc3\x0f\x14\xc8\x0f\x14\x0d\xed\x61\x3b\x01\x0f\x29\x49\x54\x48\x83\xc1\x10\x48\xff\xc8\x0f\x85\x6c\xff\xff\xff\x48\x83\xc4\x10\x5d\xc3",922}};
    const struct{size_t rva;const char* bytes;size_t size;} canvasGuards[]={
        {0x2226be0,"\x48\x8b\x81\x20\x04\x00\x00\x48\x85\xc0\x74\x2d\x48\x8b\x48\x50\x8b\x81\xc0\x00\x00\x00\x39\x87\x44\x02\x00\x00\x74\x1b\x89\x02\x4c\x8b\x87\x20\x04\x00\x00\x44\x8b\xc8\x48\x8b\xd3\x48\x8b\xcf\x49\x83\xc0\x08\xe8\x77\xf9\xfc\xff",57},
        {0x21f6590,"\x48\x83\xec\x28\x41\x83\xf9\x04\x0f\x84\x12\x01\x00\x00\x48\x8b\x81\x20\x04\x00\x00\xf3\x0f\x10\x25\x63\xfe\x4d\x01\x0f\x29\x74\x24\x10\x44\x0f\x29\x04\x24\x80",40},
        {0x22072b0,"\x48\x83\xec\x18\x48\x8b\x81\x20\x04\x00\x00\xf3\x0f\x10\x15\x4d\xf1\x4c\x01\x0f\x57\xe4\x0f\x29\x34\x24\x0f\x28\xca\x0f\x28\xf4\x48\x85\xc0\x74\x14\xf3\x0f\x10\x50\x10\xf3\x0f\x10\x48\x14\xf3\x0f\x10\x60\x08\xf3\x0f\x10",55},
        {0x21e1367,"\x49\x8b\x86\x00\x01\x00\x00\x49\x8d\x55\x50\x8b\x88\x60\x1c\x00\x00\x89\x8b\xd0\x08\x00\x00\x49\x8b\x86\x00\x01\x00\x00\x8b\x88\x64\x1c\x00\x00\x89\x8b\xd4\x08\x00\x00\x48\x8d\x4c\x24\x40\xe8\x85\xc8\xf3\xfe\x48\x8d\x8b\x80\x01\x00\x00\x48\x8d\x94\x24\x80\x00\x00\x00\x45\x0f\x28\x45\x10\x41\x0f\x28\x7d\x20\x45\x0f\x28\x55\x30\x44\x0f\x28\x6c\x24\x50\x44\x0f\x28\x4c\x24\x60\x0f\x28\x6c\x24\x70\x45\x0f\x28\x5d\x40\x0f\x28\x54\x24\x40\x41\x0f\x28\xc5\x41\x0f\xc6\xc5\x00\x41\x0f\x59\xc0\x41\x0f\x28\xcd\x45\x0f\x28\xe5\x0f\x28\xe5\x41\x0f\xc6\xcd\xaa\x45\x0f\xc6\xe5\x55\x0f\xc6\xe5\x55\x41\x0f\x28\xf1\x0f\x28\xda\x41\x0f\x59\xca\x41\x0f\xc6\xf1\x55\x0f\xc6\xda\x55\x0f\x59\xdf\x44\x0f\x59\xe7\x0f\x59\xe7\x44\x0f\x58\xe0\x41\x0f\x28\xc1\x44\x0f\x58\xe1\x41\x0f\x28\xc9\x41\x0f\xc6\xc1\x00\x41\x0f\xc6\xc9\xaa\x0f\x59\xf7\x41\x0f\x59\xc0\x41\x0f\x59\xca\x45\x0f\xc6\xed\xff\x0f\x58\xf0\x0f\x28\xc5\x0f\x58\xf1\x0f\x28\xcd\x0f\xc6\xc5\x00\x0f\xc6\xcd\xaa\x0f\xc6\xed\xff\x41\x0f\x59\xc0\x41\x0f\x59\xca\x45\x0f\xc6\xc9\xff\x0f\x58\xe0\x0f\x28\xc2\x0f\x58\xe1\x0f\x28\xca\x0f\xc6\xc2\x00\x0f\xc6\xca\xaa\x0f\xc6\xd2\xff\x41\x0f\x59\xc0\x41\x0f\x59\xca\x41\x0f\x59\xeb\x0f\x58\xd8\x0f\x58\xec\x0f\x29\xac\x24\xb0\x00\x00\x00\x0f\x58\xd9\x41\x0f\x59\xd3\x45\x0f\x59\xeb\x0f\x58\xda\x45\x0f\x58\xec\x45\x0f\x59\xcb\x0f\x29\x9c\x24\x80\x00\x00\x00\x44\x0f\x29\xac\x24\x90\x00\x00\x00\x44\x0f\x58\xce\x44\x0f\x29\x8c\x24\xa0\x00\x00\x00\xe8\x9e\xa8\x05\x00\x44\x0f\x28\xac\x24\xc0\x00",370},
        {0x21d9990,"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20\x48\x8d\x99\xd0\x2c\x00\x00\x48\x8b\xf9\x48\x8b\xcb\xe8\xb4\x69\x00\x00\x48\x8d\x05\x7d\x3e\x00\x00\x0f\x28\x87\x80\x01\x00\x00\x0f\x29\x87\x10\x2d\x00\x00\x0f\x28\x8f\x90\x01\x00\x00\x0f\x29\x8f\x20\x2d\x00\x00\x0f\x28\x87\xa0\x01\x00\x00\x0f\x29\x87\x30\x2d\x00\x00\x0f\x28\x8f\xb0\x01\x00\x00\x0f\x29\x8f\x40\x2d\x00\x00\x0f\x28\x87\xc0\x01\x00\x00\x0f\x29\x87\x50\x2d\x00\x00\x0f\x28\x8f\xd0\x01\x00\x00\x0f\x29\x8f\x60\x2d\x00\x00\x0f\x28\x87\xe0\x01\x00\x00\x0f\x29\x87\x70\x2d\x00\x00\x0f\x28\x8f\xf0\x01\x00\x00\x0f\x29\x8f\x80\x2d\x00\x00\x0f\x28\x87\x00\x02\x00\x00\x0f\x29\x87\x90\x2d\x00\x00\x0f\x28\x8f\x10\x02\x00\x00\x0f\x29\x8f\xa0\x2d\x00\x00\x0f\x28\x87\x20\x02\x00\x00\x0f\x29\x87\xb0\x2d\x00\x00\x0f\x28\x8f\x30\x02\x00\x00\x0f\x29\x8f\xc0\x2d\x00\x00\xc7\x83\x74\x08\x00\x00\x00\x20\x00\x00\xc7\x83\x78\x08\x00\x00\xff\xdf\xff\xff\xc7\x83\x7c\x08\x00\x00\x00\x00\x00\x00\xc7\x83\x80\x08\x00\x00\xff\xff\xff\xff\xc7\x03\x01\x00\x00\x00\x48\x8b\x5c\x24\x30\x48\x89\xbf\xf0\x34\x00\x00\x48\x89\x87\xa8\x34\x00\x00\xc7\x87\x84\x34\x00\x00\x03\x00\x00\x00\xc7\x87\xd8\x2c\x00\x00\x02\x00\x00\x00\xc7\x87\x40\x35\x00\x00\x20\x00\x00\x00\x48\x83\xc4\x20\x5f\xc3",304},
        {0x21e3db0,"\x48\x8b\xca\xe9\xd8\x5b\xff\xff\xcc",9},
        {0x21e3dc0,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x49\x8d\x98\xd0\x2c\x00\x00\x48\x8b\xf2\x45\x33\xc9\x45\x33\xc0\x48\x8b\xd3\x48\x8b\xf9\xe8\x76\xe6\xfe\xff\x44\x8b\x83\x90\x08\x00\x00\x48\x8b\x83\x88\x08\x00\x00\x48\x8b\x5c\x24\x30\x48\x89\x86\x80\x01\x00\x00\x44\x89\x86\x88\x01\x00\x00\x44\x89\x86\x8c\x01\x00\x00\x66\xc7\x86\x90\x01\x00\x00\x00\x00\x48\x89\xbe\xc8\x01\x00\x00\x48\x8b\x74\x24\x38\x48\x83\xc4\x20\x5f\xc3",109},
        {0x22d56bf,"\x0f\x57\xc0\x48\x8d\x9f\x40\x02\x00\x00\x48\x8d\x4c\x24\x20\xc6\x43\x18\x01\x0f\x28\x0d\x07\x88\x31\x01\x0f\x28\xd1\x0f\x54\xc8\x41\x0f\x55\x96\xa0\x02\x00\x00\x0f\x56\xd1\x0f\x29\x53\x30\x0f\x28\x05\x0b\x0d\xba\x00\x41\x0f\x57\x86\xc0\x02\x00\x00\x0f\x29\x43\x40\xe8\xca\x4e\xf5\xff\x49\x8d\x96\xe0\x01\x00\x00\x48\x8d\x4c\x24\x20\xe8\x59\x66\xf6\xff\x45\x33\xc9\x0f\x28\x44\x24\x20\x0f\x28\x4c\x24\x30\x0f\x29\x43\x50\x0f\x28\x44\x24\x40\x0f\x29\x4b\x60\x0f\x28\x4c\x24\x50\x0f\x29\x43\x70\x0f\x28\x44\x24\x60\x0f\x29\x8b\x80\x00\x00\x00\x0f\x28\x4c\x24\x70\x0f\x29\x83\x90\x00\x00\x00\x0f\x29\x8b\xa0\x00\x00\x00\xc7\x83\x50\x02\x00\x00\x06\x00\x00\x00",164},
};
    for(auto& g:canvasGuards)if(memcmp(image+g.rva,g.bytes,g.size)){fprintf(log,"REFUSED canvas signature=%zx\n",g.rva);fflush(log);return 10;}
    if(!acu::VerifyImage()){fprintf(log,"REFUSED executable hash\n");fflush(log);return 2;}
    for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size)){fprintf(log,"REFUSED signature=%zx\n",g.rva);fflush(log);return 3;}
    auto settings=directory/L"ACUnityVR.ini";
    if(GetPrivateProfileIntW(L"VR",L"NativeBaseline",0,settings.c_str()))return OcclusionProbe::Baseline(image,directory,log);
    eyeWidth=GetPrivateProfileIntW(L"VR",L"EyeWidth",0,settings.c_str());eyeHeight=GetPrivateProfileIntW(L"VR",L"EyeHeight",0,settings.c_str());
    wchar_t refresh[64]{};GetPrivateProfileStringW(L"VR",L"RefreshHz",L"0",refresh,_countof(refresh),settings.c_str());headsetRefresh=float(wcstod(refresh,nullptr));
    if(!RenderSize::Set(eyeWidth,eyeHeight) || !std::isfinite(headsetRefresh) || headsetRefresh<0 || headsetRefresh>1000){fprintf(log,"REFUSED headset display settings; run Start-ACUnityVR after preparing windowed mode\n");fflush(log);return 6;}
    fprintf(log,"Headset requested eye=%ux%u refresh=%.3f Hz; production follows OpenXR whole-frame demand\n",eyeWidth,eyeHeight,headsetRefresh);fflush(log);
    if(!Find() || !CullingProbe::Initialize(image,source,directory,log)){fprintf(log,"REFUSED original source identity or culling capture before display resize\n");fflush(log);return 9;}
    if(!GameDisplay::Initialize(eyeWidth,eyeHeight)){fprintf(log,"REFUSED game window resize; prepare WindowMode=0 with Unity closed, then restart normally\n");fflush(log);return 7;}
    {auto output=OutputTexture();D3D11_TEXTURE2D_DESC desc{};
     auto deadline=GetTickCount64()+10000;
     while(GetTickCount64()<deadline){if(!Find()){Sleep(25);continue;}output=OutputTexture();if(output){output->GetDesc(&desc);if(RenderSize::Matches(desc.Width,desc.Height))break;}Sleep(25);}
     if(!output || !RenderSize::Matches(desc.Width,desc.Height) || desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || desc.SampleDesc.Count!=1){
      fprintf(log,"REFUSED actual output=%ux%u format=%u samples=%u requested=%ux%u\n",desc.Width,desc.Height,unsigned(desc.Format),desc.SampleDesc.Count,eyeWidth,eyeHeight);fflush(log);GameDisplay::Stop();return 8;}
     fprintf(log,"Native output verified=%ux%u RGBA8 sample1; quarter clouds expected=%ux%u\n",desc.Width,desc.Height,RenderSize::Quarter(eyeWidth),RenderSize::Quarter(eyeHeight));fflush(log);
    }
    if(!Find() || !producer.Initialize(eyeWidth,eyeHeight) || !FirstPerson70::Initialize(image,world,directory,log) || !ProjectedShadowGuard::Install(image,log) || !DrawDiagnostics::Install(image,surface,source,directory,log) || !GIProjectionTask::Install(image,log) || !UiCamera71::Install(image,directory,log) || !SourceRendering::Initialize(image,source,log) || !PrimaryCamera52::Initialize(image,source,surface,log) || !EntityVisibility41::Install(image,source,directory,log) || !ComponentGateProbe51::Install(image,source,surface,directory,log) || !NpcCallback63::Install(image,source,world,directory,log) || !NpcCamera63::Install(image,source,world,directory,log) || !NpcDecision63::Install(image,source,world,directory,log) || !NpcCone63::Install(image,source,world,directory,log) || !BulkDraw83::Initialize(image,source,directory,log) || !DenseVisibility87::Initialize(image,source,directory,log) || !EarlyCamera91::Initialize(image,source,EarlyHeadPose91,directory,log)){fprintf(log,"REFUSED native identity, transport or draw diagnostics\n");fflush(log);EarlyCamera91::Stop();StationSubmission90::Stop();DenseVisibility87::Stop();BulkDraw83::Stop();FirstPerson70::Stop();NpcCone63::Stop();NpcDecision63::Stop();NpcCallback63::Stop();NpcCamera63::Stop();SelectionBoundary46::Stop();PrimaryCamera52::Stop();ComponentGateProbe51::Stop();EntityVisibility41::Stop();CullingProbe::Stop();SourceRendering::Stop();UiCamera71::Stop();GIProjectionTask::Restore();DrawDiagnostics::Restore();ProjectedShadowGuard::Restore();producer.Stop();GameDisplay::Stop();return 4;}
    DrawDiagnostics::SetPassOverride([](unsigned pass,void* t,void* b,void* c){return UiCamera71::Execute(pass,t,b,c) || GIProjectionTask::Execute(pass,t,b,c);});
#ifdef ACUNITYVR_CLEAN_RUNTIME
    producer.SetRuntimeVariant(1);
    fprintf(log,"Tracked74 CLEAN comparison: native same-frame stereo ABI%u experiment76; game profiler/captures/hunter not linked. F5 source skip, F6 cloud, F7 GI remain ON. F8 host pair capture; F9 recenter. Same chosen AA/settings and full native canvas.\n",Transport::Version);
#else
    producer.SetRuntimeVariant(2);
    fprintf(log,"Tracked74 PROFILE comparison: same renderer, ABI%u experiment76; sparse native GPU profiling plus F2 early primary head-camera input; F8 worker and direct effect native binding cache and actual GPU constants, plus actual graphics-component/owner/proxy visibility gates plus native mesh queues and component/LOD inputs, scene-depth hierarchy and selected renderer input list, plus parent-group visibility at afterCull/beforeRender/afterEye, actual instance CS17/87 CB8, per-object selection and early entity inputs. F3 profiling ON/OFF, F5 source skip, F6 cloud, F7 GI remain ON. F8 capture; F9 recenter. Accepted colour/geometry VRS stays ON; F4 hidden-area mask follows INI; exact missing per-projector texture guard enabled; F8 adds bounded leaf pipeline counts; F2 compares the early renderer-primary and scoped native crowd head-camera inputs; AA/full resolution unchanged; early getter detours retired; EarlyCamera91 changes the actual gameplay-camera output after native update and restores the mouse base before source Bind.\n",Transport::Version);
#endif
    fflush(log);
    initialCount=Field<uint16_t>(surface,0x32);initialActive=Field<uint32_t>(surface,0x48);
    if(!Set(sourcePreSlot,reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(SourcePre)) || !Set(sourcePostSlot,reinterpret_cast<void*>(nativePost),reinterpret_cast<void*>(SourcePost))){Restore(sourcePreSlot,reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(SourcePre));producer.Stop();EarlyCamera91::Stop();StationSubmission90::Stop();DenseVisibility87::Stop();BulkDraw83::Stop();FirstPerson70::Stop();NpcCone63::Stop();NpcDecision63::Stop();NpcCallback63::Stop();NpcCamera63::Stop();SelectionBoundary46::Stop();PrimaryCamera52::Stop();ComponentGateProbe51::Stop();EntityVisibility41::Stop();CullingProbe::Stop();SourceRendering::Stop();UiCamera71::Stop();GIProjectionTask::Restore();DrawDiagnostics::Restore();ProjectedShadowGuard::Restore();GameDisplay::Stop();return 5;}
    fprintf(log,"Tracked74 native initialization complete; source callbacks active\n");fflush(log);
#ifdef ACUNITYVR_PORTABLE92
    {auto name=L"Local\\ACUnityVRReady-"+std::to_wstring(GetCurrentProcessId());auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());if(event){SetEvent(event);CloseHandle(event);}}
#endif
    while(state!=3){
        if(ManualStopPressed())stop=true;
        if(state==0 && (stop || producer.StopRequested())){Scope scope(cameraLock);if(state==0){
            Restore(sourcePreSlot,reinterpret_cast<void*>(nativePre),reinterpret_cast<void*>(SourcePre));
    Restore(sourcePostSlot,reinterpret_cast<void*>(nativePost),reinterpret_cast<void*>(SourcePost));EarlyCamera91::Stop();StationSubmission90::Stop();DenseVisibility87::Stop();BulkDraw83::Stop();FirstPerson70::Stop();NpcCone63::Stop();NpcDecision63::Stop();NpcCallback63::Stop();NpcCamera63::Stop();SelectionBoundary46::Stop();PrimaryCamera52::Stop();ComponentGateProbe51::Stop();EntityVisibility41::Stop();CullingProbe::Stop();SourceRendering::Stop();UiCamera71::Stop();GIProjectionTask::Restore();DrawDiagnostics::Restore();ProjectedShadowGuard::Restore();RestoreCanvas();producer.Stop();state=3;fprintf(log,"Stopped before attachment; restored\n");fflush(log);
        }}
        Sleep(25);
    }
    GameDisplay::Stop();fprintf(log,"Game window procedure and placement restored; pacing waits=%llu timeouts=%llu\n",producer.PacingWaits(),producer.PacingTimeouts());fflush(log);
    return 0;
}
}
#ifdef ACUNITYVR_PORTABLE92
DWORD WINAPI PortableRun92(void* data){auto result=Run(data);if(result){auto name=L"Local\\ACUnityVRFailed-"+std::to_wstring(GetCurrentProcessId());auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());if(event){SetEvent(event);CloseHandle(event);}}return result;}
#endif
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);
#ifdef ACUNITYVR_PORTABLE92
auto h=CreateThread(nullptr,0,PortableRun92,nullptr,0,nullptr);
#else
auto h=CreateThread(nullptr,0,Run,nullptr,0,nullptr);
#endif
if(h)CloseHandle(h);}return TRUE;}
