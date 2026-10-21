// Owned observer and optional private camera at the marked BulkWorld render callback.
// One full34-method private primary vtable; only render slot100 is observed.
#include "BulkDraw83.h"
#include "BulkCamera84.h"
#include "NativeHandleState63.h"
#include "CrowdSelection89.h"
#include <array>
#include <atomic>
#include <cstring>
#include <intrin.h>
#include <share.h>
namespace BulkDraw83 {namespace {
#include "BulkDraw83Signatures.h"
using Callback=uint64_t(__fastcall*)(void*,void*,void*,void*);
constexpr unsigned Count=34,Slot=32,Limit=128;
uint8_t* image{};uint64_t source{},world{},bulk{},handle{};uint32_t handleToken{};std::array<std::atomic<uint64_t>,2> eyes{};Callback native{};void* clone[Count+1]{};FILE* report{};std::filesystem::path directory;
std::atomic<bool> enabled{},installed{},correction{true};BulkCamera84::Published published;bool compareDown{},allowComparison{};std::atomic<uint64_t> corrected[4]{},cameraRefused{};std::atomic<unsigned> inFlight{};SRWLOCK mutex=SRWLOCK_INIT;
struct Lock{Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};struct Flight{Flight(){++inFlight;}~Flight(){--inFlight;}};
struct View {uint64_t pointer{},frustum{},state{};unsigned frame{},errors{};uint8_t raw[0x440]{},planes[0xc0]{};};
struct Record {uint64_t id{},tickBefore{},tickAfter{},caller{},owner{},viewPointer{},sourceFrame{};unsigned status{},whichView{},active{},bufferIndex{};uint64_t buffer{},root{},returned{};uint8_t headerBefore[0xc8]{},headerAfter[0xc8]{};View views[4]{},effective{};uint64_t headAge{},headSequence{},headTracking{};uint32_t cameraStatus{},applied{};};
std::array<Record,Limit> records,snapshot;uint64_t total{},lastStatus{},capture{};bool keyDown{};std::atomic<uint64_t> callViews[5]{},refused{};
CrowdSelection89::Capture selectionCapture;
bool allowSelectionCapture{};
bool Read(void* out,uint64_t p,size_t n) noexcept {if(p<0x10000 || p>0x00007ffffffeffffULL || p+n<p || n>0x10000)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>T Get(const uint8_t* p,size_t n){T v{};memcpy(&v,p+n,sizeof(v));return v;}
bool Q(uint64_t p,size_t at,uint64_t expected){uint64_t q{};return Read(&q,p+at,8) && q==expected;}
bool Root(bool own){uint64_t renderer{},graph{},currentHandle{};uint8_t h[16]{};
 return Q(world,0,uint64_t(image)+0x30225e0) && Q(world,0x998,bulk) && Q(bulk,0,own?uint64_t(clone+1):uint64_t(image)+0x304ab90) && Read(&currentHandle,bulk+0x4f8,8) && Q(currentHandle,0,uint64_t(image)+0x2f6b980) && Q(currentHandle,0x80,bulk) && Read(&graph,source+0x440,8) && Q(graph,0,source) && Read(&renderer,graph+0x10,8) && Q(renderer,0,uint64_t(image)+0x3094180) && Q(renderer,0x28,world) && Read(&currentHandle,source+0x250,8) && currentHandle==handle && Read(h,handle,16) && Get<uint64_t>(h,0)==world && NativeHandleState63::SameLifetime(Get<uint32_t>(h,12),handleToken);
}
bool SetTable(bool attach) noexcept {__try{return InterlockedCompareExchangePointer(reinterpret_cast<void**>(bulk),attach?static_cast<void*>(clone+1):static_cast<void*>(image+0x304ab90),attach?static_cast<void*>(image+0x304ab90):static_cast<void*>(clone+1))==(attach?static_cast<void*>(image+0x304ab90):static_cast<void*>(clone+1));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void ReadView(View& v,uint64_t p){v.pointer=p;if(!p || !Read(v.raw,p,sizeof(v.raw))){v.errors|=1;return;}v.frustum=Get<uint64_t>(v.raw,0x428);v.state=Get<uint64_t>(v.raw,0x430);v.frame=Get<unsigned>(v.raw,0x294);if(!Q(v.state,0x8208,p))v.errors|=2;if(!Read(v.planes,v.frustum,sizeof(v.planes)))v.errors|=4;uint64_t f{},s{};if(!Read(&f,p+0x428,8) || f!=v.frustum || !Read(&s,p+0x430,8) || s!=v.state)v.errors|=8;}
bool Before(Record& r,void* a,void* b){r.tickBefore=GetTickCount64();r.owner=uint64_t(a);r.viewPointer=uint64_t(b);if(uint64_t(a)!=bulk || !Root(true)){r.status=1;return false;}
 r.whichView=uint64_t(b)==source?1:uint64_t(b)==eyes[0].load()?2:uint64_t(b)==eyes[1].load()?3:4;
 unsigned sourceFrame32{};Read(&sourceFrame32,source+0x294,4);r.sourceFrame=sourceFrame32;
 ReadView(r.views[0],uint64_t(b));ReadView(r.views[1],source);ReadView(r.views[2],eyes[0].load());ReadView(r.views[3],eyes[1].load());
 uint8_t active{};if(!Read(&active,bulk+0x480,1)){r.status=2;return false;}r.active=active;
 uint64_t root{};uint32_t index{};if(!Read(&root,bulk+0x478,8) || !Read(&index,root+0x34,4) || index>1){r.status=3;return false;}r.root=root;r.bufferIndex=index^1;r.buffer=bulk+0x2e8+uint64_t(r.bufferIndex)*0xc8;
 if(!Read(r.headerBefore,r.buffer,sizeof(r.headerBefore))){r.status=4;return false;}return true;
}
__declspec(noinline) uint64_t __fastcall Observe(void* a,void* b,void* c,void* d){
 if(!enabled.load(std::memory_order_acquire))return native(a,b,c,d);Flight flight;Record r{};r.caller=uint64_t(_ReturnAddress());bool valid=Before(r,a,b);
 BulkCamera84::Input privateInput;void* argument=b;
 if(valid && r.active && r.whichView>=1 && r.whichView<=3 && correction.load()){
  BulkCamera84::Published p;{Lock guard;p=published;}r.headSequence=p.sequence;r.headTracking=p.tracking;r.headAge=r.tickBefore>=p.tick?r.tickBefore-p.tick:UINT64_MAX;
  r.cameraStatus=1;
  if(BulkCamera84::Fresh(p,r.tickBefore,uint32_t(r.sourceFrame)) && !r.views[0].errors){
   alignas(16) uint8_t original[0x460]{};r.cameraStatus=2;
   if(Read(original,uint64_t(b),sizeof(original)) && Get<uint64_t>(original,0x428)==r.views[0].frustum && Get<uint64_t>(original,0x430)==r.views[0].state && BulkCamera84::Build(p,original,privateInput)){
    argument=privateInput.view;r.applied=1;r.cameraStatus=0;ReadView(r.effective,uint64_t(argument));r.effective.errors&=~2u; // private view intentionally retains original state/backlink

   }
  }
  if(!r.applied)++cameraRefused;
 }
 auto token=valid && r.active?selectionCapture.Begin(Read,r.buffer,uint64_t(argument),r.viewPointer,r.whichView):CrowdSelection89::Token{};
 uint64_t result=native(a,argument,c,d); // Exact original callback once; only validated private camera input may differ.
 if(token)selectionCapture.End(Read,token,valid && Root(true));
 if(r.applied)++corrected[r.whichView];
 r.returned=result;r.tickAfter=GetTickCount64();if(valid && (!Root(true) || !Read(r.headerAfter,r.buffer,sizeof(r.headerAfter))))r.status=5;
 {Lock guard;r.id=++total;records[(r.id-1)%Limit]=r;}++callViews[r.whichView<5?r.whichView:4];if(r.status)++refused;return result;
}
bool Write(const std::filesystem::path& p,const void* data,size_t n){FILE* f=_wfsopen(p.c_str(),L"wb",_SH_DENYNO);if(!f)return false;bool ok=!n || fwrite(data,n,1,f)==1;return fclose(f)==0&&ok;}
void Save(uint64_t sequence,uint64_t tracking,uint32_t frame){unsigned n{};uint64_t end{};{Lock guard;end=total;n=unsigned(end<Limit?end:Limit);for(unsigned i=0;i<n;++i)snapshot[i]=records[(end-n+i)%Limit];}
 std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return;auto prefix=directory/(L"bulk-"+std::to_wstring(++capture)+L"-sequence-"+std::to_wstring(sequence));bool ok=Write(std::filesystem::path(prefix.wstring()+L".bin"),snapshot.data(),size_t(n)*sizeof(Record));FILE* f=_wfsopen((prefix.wstring()+L".json").c_str(),L"w",_SH_DENYNO);if(f){
 fprintf(f,"{\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"records\":%u,\"total\":%llu,\"recordBytes\":%zu,\"viewBytes\":%zu,\"headerBeforeOffset\":%zu,\"headerAfterOffset\":%zu,\"viewsOffset\":%zu,\"effectiveOffset\":%zu,\"headOffset\":%zu,\"filesOK\":%s,\"callViews\":[%llu,%llu,%llu,%llu,%llu],\"refused\":%llu,\"note\":\"Actual BulkWorld virtual100 entry/exit. NumPad9 private camera comparison. Owned source/eye callback only, max100ms/four frames published head input; original callback once. Private copied view/frustum, original state pointer preserved; no shared camera/culling/buffer writes. Per-buffer selected list+90/count98 and instance array+0/count40 are native.\"}\n",sequence,tracking,frame,n,end,sizeof(Record),sizeof(View),offsetof(Record,headerBefore),offsetof(Record,headerAfter),offsetof(Record,views),offsetof(Record,effective),offsetof(Record,headAge),ok?"true":"false",callViews[0].load(),callViews[1].load(),callViews[2].load(),callViews[3].load(),callViews[4].load(),refused.load());fclose(f);}
 if(report){fprintf(report,"BulkDraw83 saved sequence=%llu total=%llu records=%u filesOK=%u\n",sequence,end,n,unsigned(ok));fflush(report);}
}
}
bool Initialize(uint8_t* base,void* primary,const std::filesystem::path& output,FILE* log){image=base;source=uint64_t(primary);directory=output/L"bulk-draw83";report=log;if(!Guard83(base,log))return false;uint64_t graph{},renderer{};uint8_t h[16]{};
 allowSelectionCapture=GetPrivateProfileIntW(L"Diagnostics",L"CaptureMarkedCrowd",0,(output/L"ACUnityVR.ini").c_str())!=0;
 if(!Read(&graph,source+0x440,8) || !Read(&renderer,graph+0x10,8) || !Read(&world,renderer+0x28,8) || !Read(&bulk,world+0x998,8) || !Read(&handle,source+0x250,8) || !Read(h,handle,16))return false;handleToken=Get<uint32_t>(h,12);if(!NativeHandleState63::Live(handleToken) || Get<uint64_t>(h,0)!=world || !Root(false))return false;
 correction=GetPrivateProfileIntW(L"Culling",L"BulkHeadCamera",0,(output/L"ACUnityVR.ini").c_str())!=0;allowComparison=GetPrivateProfileIntW(L"Culling",L"BulkHeadCameraCompare",0,(output/L"ACUnityVR.ini").c_str())!=0;memcpy(clone,image+0x304ab88,sizeof(clone));native=reinterpret_cast<Callback>(base+0x1f143a0);clone[Slot+1]=reinterpret_cast<void*>(Observe);enabled=true;if(!SetTable(true)){enabled=false;return false;}installed=true;
 fprintf(log,"BulkDraw83 world handle=%llx initialState=%08x: pointer/owner/live/high flags must match; low24 native reference count may change\n",handle,handleToken);
 fprintf(log,"BulkDraw83 attached actual BulkWorld=%llx primary full34 table slot100: marked crowd path1f143a0 ->2241d80 ->22e14d0 ->2299a80/22de9f0 ->228f230; private copied head camera enabled=%u, NumPad9 compares; rolling128 camera/batch snapshots, F8 saves; zero shared camera/visibility/renderer writes\n",bulk,unsigned(correction.load()));fflush(log);return true;
}
void Publish(uint32_t frame,uint64_t sequence,uint64_t tracking,bool tracked,const CameraMath::Matrix poses[2],const Transport::Eye optics[2]){
 if(!enabled)return;BulkCamera84::Published p;p.tick=GetTickCount64();p.frame=frame;p.sequence=sequence;p.tracking=tracking;
 if(tracked && tracking && Root(true)){
  CameraMath::Matrix depth{};
  if(Read(depth.m,source+0x320,64) && BinocularComponentInput50::Build(depth,poses,optics,p.camera)){
   reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x223bd70)(p.frustum,&p.camera.viewProjection);p.valid=true;
  }
 }
 Lock guard;published=p;
}
bool CurrentCamera(uint32_t frame,CameraMath::Matrix& pose,uint64_t& age,uint64_t& sequence,uint64_t& tracking){
 if(!enabled.load() || !Root(true))return false;BulkCamera84::Published p;{Lock guard;p=published;}
 auto now=GetTickCount64();age=now>=p.tick?now-p.tick:UINT64_MAX;
 if(!BulkCamera84::Fresh(p,now,frame))return false;
 pose=p.camera.pose;sequence=p.sequence;tracking=p.tracking;return true;
}
uint64_t CanonicalBulk(uint64_t object,uint64_t table){
 if(!enabled.load(std::memory_order_acquire) || !installed.load(std::memory_order_acquire) || object!=bulk || table!=uint64_t(clone+1) || !Root(true))return table;
 // All other methods remain the verified native entries; no foreign table is accepted.
 for(unsigned i=0;i<Count;++i)if(clone[i+1]!=(i==Slot?reinterpret_cast<void*>(Observe):static_cast<void*>(image+Methods[i])))return table;
 return uint64_t(image)+0x304ab90;
}
void SetEyes(void* left,void* right){eyes[0]=uint64_t(left);eyes[1]=uint64_t(right);}
void ArmSelection(uint64_t sequence,uint64_t tracking,uint32_t frame,const std::filesystem::path& path){if(enabled.load() && allowSelectionCapture)selectionCapture.Arm(sequence,tracking,frame,path);}
void FinishSelection(){selectionCapture.Finish();}
void Poll(uint64_t sequence,uint64_t tracking,uint32_t frame){if(!enabled)return;bool compare=(GetAsyncKeyState(VK_NUMPAD9)&0x8000)!=0;if(allowComparison && compare && !compareDown){correction=!correction.load();fprintf(report,"BulkCamera84 NumPad9 private head input=%u sequence=%llu\n",unsigned(correction.load()),sequence);fflush(report);}compareDown=compare;uint64_t completed{};{Lock guard;completed=total;}if(!capture && tracking && completed>=8 && eyes[0].load() && eyes[1].load())Save(sequence,tracking,frame);bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(down && !keyDown)Save(sequence,tracking,frame);keyDown=down;auto now=GetTickCount64();if(now-lastStatus>=5000){uint64_t n{};{Lock guard;n=total;}fprintf(report,"BulkDraw83 total=%llu views source/left/right/other=%llu/%llu/%llu/%llu refused=%llu\n",n,callViews[1].load(),callViews[2].load(),callViews[3].load(),callViews[4].load(),refused.load());fflush(report);fprintf(report,"BulkCamera84 correction=%u applied source/left/right=%llu/%llu/%llu staleOrRefused=%llu\n",unsigned(correction.load()),corrected[1].load(),corrected[2].load(),corrected[3].load(),cameraRefused.load());fflush(report);lastStatus=now;}}
void Stop(){if(!enabled.exchange(false))return;selectionCapture.Stop();bool identity=Root(true);bool restored=identity && SetTable(false);installed=false;if(report){fprintf(report,"BulkDraw83 retired identity=%u tableRestored=%u inFlight=%u; clone retained until process exit\n",unsigned(identity),unsigned(restored),inFlight.load());fflush(report);}}
}
