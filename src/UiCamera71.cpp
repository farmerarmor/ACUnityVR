#include "UiCamera71.h"
#include "UiProjection71.h"
#include "UiProjectionScope65.h"
#include "GpuCapture.h"
#include "SourceRendering.h"
#include "UiMode71.h"
#include <share.h>
#include <atomic>
#include <cwchar>
namespace UiCamera71 {
namespace {
using acu::Field;uint8_t *image{},*backend{};void* eyes[2]{};FILE* report{};
UiProjection71::Settings settings,hudSettings,menuSettings;std::filesystem::path directory71;UiMode71::State mode71{};bool modeValid71{},lastMenu71{};unsigned modeLogs71{},modeCaptures71{};Transport::Eye optics[2]{};UiProjectionScope65::Task native{};
bool installed{},enabled{},apply{};uint64_t sequence{},tracking{},applied[2]{},refused{},conflicts{};std::atomic<unsigned> inFlight{};
float Number(const std::filesystem::path& p,const wchar_t* key,float fallback,float lo,float hi){
 wchar_t text[80]{};GetPrivateProfileStringW(L"UI",key,L"",text,_countof(text),p.c_str());if(!*text)return fallback;
 wchar_t* end{};double value=wcstod(text,&end);while(end && (*end==L' ' || *end==L'\t'))++end;
 return end && end!=text && !*end && std::isfinite(value) && value>=lo && value<=hi?float(value):fallback;
}
bool Read(void* b,void* context,unsigned* eye,CameraMath::Matrix* before){__try{
 if(!b || b!=backend || context!=Field<void*>(backend,0x20) || !(Field<void*>(backend,0)==image+0x30a86f0 || SourceRendering::OwnsBackend(backend)))return false;
 auto ctx=static_cast<uint8_t*>(context);auto view=Field<void*>(ctx,0x30);auto state=Field<void*>(ctx,0x28);
 *eye=view==eyes[0]?0:view==eyes[1]?1:2;if(*eye>1)return false;
 if(Field<void*>(ctx,8)!=backend || Field<void*>(backend,0x4c8)!=view || !state || Field<void*>(view,0x430)!=state || Field<void*>(state,0x8208)!=view)return false;
 auto width=Field<unsigned>(backend,0x4d0),height=Field<unsigned>(backend,0x4d4);*before=CameraMath::Load(backend+0x1400);
 return RenderSize::Matches(width,height) && UiProjection71::NativeMatrix(*before,width,height);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
}
bool Install(uint8_t* base,const std::filesystem::path& directory,FILE* log){
 image=base;report=log;auto renderer=Field<uint8_t*>(image,0x524cfd8);backend=renderer?Field<uint8_t*>(renderer,0x10):nullptr;
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 const struct{size_t rva;const char* bytes;size_t size;} guards[]{
  {0x1dcb66,"\x48\x8b\x05\x7b\xe1\x00\x05\x48\x8b\xf9\x48\x8b\x50\x30\x66\x83\x7a\x1a\x00",19},
  {0x7ba7f1,"\x0f\xb7\x5d\x1a\x85\xdb\x74\x3e\x8d\x34\xdd\x00\x00\x00\x00\x48\x8b\x45\x10\x8d\x76\xf8\xff\xcb\x48\x8b\x3c\x06\x48",29},
  {0x22909f0,"\xe9\x8b\x39\x01\x00",5},
  {0x22a4380,"\x40\x55\x56\x57\x41\x57\x48\x8d\x6c\x24\x88\x48\x81\xec\x78\x01\x00\x00",18},
  {0x22a0de0,"\x48\x89\x5c\x24\x18\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8d\x6c\x24\xd9\x48\x81\xec\x00\x01\x00\x00",28},
  {0x22a0ee2,"\x49\x8d\x96\x00\x14\x00\x00\x48\x8b\xcf\xe8\xcf\xf9\x02\x00",15},
  {0x22a0ef1,"\x48\x8d\x15\xf8\x0f\xf8\x02\x48\x8d\x4d\x97\xe8\x1f\xcd\xe7\xfe\x4c\x8d\x4c\x24\x50\x4c\x8d\x45\x97\x0f\x57\xf6\x48\x8d\x55\x97\x48\x8b\xcf\x0f\x29\x74\x24\x50\xe8\xd2\xde\x02\x00",45},
  {0x22a1788,"\x49\x8b\x96\xc8\x04\x00\x00\x48\x81\xc2\x20\x03\x00\x00\x48\x8b\xcf\xe8\x22\xf1\x02\x00",22},
  {0x22d08c0,"\x48\x8b\xc4\x48\x89\x58\x08\x57\x48\x81\xec\x10\x02\x00\x00\x0f\x28\x02\x0f\x29\x70\xe8\x0f\x29",24},
 };
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
 native=reinterpret_cast<UiProjectionScope65::Task>(image+0x22909f0);auto p=directory/L"ACUnityVR.ini";
 directory71=directory;
 const auto legacyWidth=Number(p,L"WidthDegrees",60,30,140),legacyDistance=Number(p,L"DistanceMeters",2,.5f,10);
 hudSettings.widthDegrees=Number(p,L"HUDWidthDegrees",legacyWidth,30,140);hudSettings.distance=Number(p,L"HUDDistanceMeters",legacyDistance,.5f,10);
 menuSettings.widthDegrees=Number(p,L"MenuWidthDegrees",legacyWidth,30,100);menuSettings.distance=Number(p,L"MenuDistanceMeters",legacyDistance,.5f,10);
 settings=menuSettings;
 enabled=Number(p,L"Enabled",1,0,1)==1;installed=true;
 fprintf(report,"UI71 scoped native local camera ON=%d F12 comparison HUD=%.3fdeg/%.3fm menu=%.3fdeg/%.3fm logical=16:9; native menu-stack detection; world-marker camera unchanged\n",enabled,hudSettings.widthDegrees,hudSettings.distance,menuSettings.widthDegrees,menuSettings.distance);fflush(report);return true;
}
void SetEyes(void* left,void* right){eyes[0]=left;eyes[1]=right;}
bool ReadMode71(void* out,uint64_t address,size_t size) noexcept {if(!out || address<0x10000 || !size || size>0x1000)return false;__try{memcpy(out,reinterpret_cast<void*>(address),size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void BeginFrame(uint64_t id,const Transport::RenderInfo& rendered){
 sequence=id;tracking=rendered.tracking.id;
 modeValid71=UiMode71::Capture(ReadMode71,reinterpret_cast<uint64_t>(image),mode71);bool menu=modeValid71?mode71.menu:true;settings=menu?menuSettings:hudSettings;
 if((id<=3 || menu!=lastMenu71) && modeLogs71<32){++modeLogs71;fprintf(report,"UI71 mode=%s valid=%d pages=%u width=%.3fdeg distance=%.3fm sequence=%llu\n",menu?"menu":"HUD",modeValid71,mode71.pageCount,settings.widthDegrees,settings.distance,id);fflush(report);}lastMenu71=menu;
 static bool f8Held71{};bool f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(f8&&!f8Held71){auto file=_wfsopen((directory71/(L"ui-mode71-"+std::to_wstring(++modeCaptures71)+L".json")).c_str(),L"w",_SH_DENYNO);if(file){fprintf(file,"{\"sequence\":%llu,\"valid\":%s,\"menu\":%s,\"pageCount\":%u,\"widthDegrees\":%.9g,\"distanceMeters\":%.9g}\n",id,modeValid71?"true":"false",menu?"true":"false",mode71.pageCount,settings.widthDegrees,settings.distance);fclose(file);}}f8Held71=f8;
static bool held{};bool down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
 if(down && !held){enabled=!enabled;fprintf(report,"UI71 F12 enabled=%d sequence=%llu\n",enabled,sequence);fflush(report);}held=down;
 apply=installed && enabled && rendered.mode==1 && rendered.tracking.valid && tracking!=0;memcpy(optics,rendered.tracking.eyes,sizeof(optics));
}
bool Execute(unsigned pass,void* task,void* b,void* context){
 // Index16 is the existing exact UI3DPass finish callback (descriptor504a1e0).
 if(pass!=16 || !apply)return false;unsigned eye=2;CameraMath::Matrix before{},changed{};
 if(!Read(b,context,&eye,&before) || !UiProjection71::Build(before,optics,eye,settings,changed)){
  ++refused;if(refused<5){fprintf(report,"UI71 refused eye=%u sequence=%llu\n",eye,sequence);fflush(report);}return false;
 }
 ++inFlight;bool restored=false;
 __try{restored=UiProjectionScope65::Invoke(reinterpret_cast<float*>(backend+0x1400),before,changed,native,task,b,context);}
 __finally{--inFlight;}
 if(!restored){++conflicts;apply=false;enabled=false;fprintf(report,"UI71 disabled after native projection replacement sequence=%llu\n",sequence);fflush(report);}
 ++applied[eye];if(applied[eye]<=3){fprintf(report,"UI71 applied eye=%u sequence=%llu pose=%llu before=(%.9g,%.9g,%.9g,%.9g) after=(%.9g,%.9g,%.9g,%.9g) scopedRestore=%d\n",eye,sequence,tracking,before.m[0],before.m[5],before.m[8],before.m[9],changed.m[0],changed.m[5],changed.m[8],changed.m[9],restored);fflush(report);}return true;
}
void Stop(){apply=false;installed=false;if(report){fprintf(report,"UI71 retired applied=%llu/%llu refused=%llu conflicts=%llu inFlight=%u\n",applied[0],applied[1],refused,conflicts,inFlight.load());fflush(report);}}
}
