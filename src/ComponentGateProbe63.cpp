// F8-only snapshots at the two owned native component task delegates.
// Executor 219f690 passes component in RCX, context in RDX and has no stack arguments.
// The original callback is forwarded once; no camera, visibility or mesh writes.
#include "ComponentGateProbe63.h"
#include "ComponentGateInputs63.h"
#include "ComponentTaskInputs49.h"
#include <atomic>
#include <algorithm>
#include <share.h>
namespace ComponentGateProbe51 {
namespace {
using Task=uint64_t(__fastcall*)(void*,void*,void*,void*);
using ComponentGateInputs63::Snapshot;
constexpr unsigned Limit=1024;
struct Record {Snapshot before{},after{};uint64_t tickBegin{},tickEnd{};unsigned taskIndex{};};
uint8_t* image{};uint64_t source{},surface{};std::atomic<uint64_t> eyes[2]{};
void** slots[2]{};Task native{};FILE* report{};std::filesystem::path directory;
std::atomic<bool> enabled{},capturing{};std::atomic<unsigned> inFlight{},attempts{},finished{};
std::array<Record,Limit> records{};uint64_t captureId{},expectedContext{},expectedOwner{};std::atomic<uint32_t> captureFrame{0xffffffffu};bool f8Down{},pending{};
bool Read(void* dst,uint64_t address,size_t size) noexcept {
 if(!dst || address<0x10000 || address>0x7ffffffeffffULL || !size || size>0x10000 || address+size<address)return false;
 __try{memcpy(dst,reinterpret_cast<const void*>(address),size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Hex(FILE* f,const uint8_t* p,size_t size){fputc('"',f);for(size_t i=0;i<size;++i)fprintf(f,"%02x",p[i]);fputc('"',f);}
void WriteSnapshot(FILE* f,const Snapshot& v){
 fprintf(f,"{\"component\":\"%llx\",\"context\":\"%llx\",\"table\":\"%llx\",\"method\":\"%llx\",\"owner\":\"%llx\",\"primary\":\"%llx\",\"entity\":\"%llx\",\"entityTable\":\"%llx\",\"proxy\":\"%llx\",\"member\":\"%llx\",\"sourceFrame\":%u,\"primaryFrame\":%u,\"errors\":%u,\"identity\":%s,\"cameraRead\":%s,\"entityRead\":%s,\"proxyRead\":%s,\"memberRead\":%s,\"backlinksStable\":%s,\"childCount\":%u,\"childrenRead\":%u,\"geometryMask\":\"%llx\",\"descriptor\":",v.component,v.context,v.table,v.method,v.owner,v.primary,v.entity,v.entityTable,v.proxy,v.member,v.sourceFrame,v.primaryFrame,v.errors,v.identity?"true":"false",v.cameraRead?"true":"false",v.entityRead?"true":"false",v.proxyRead?"true":"false",v.memberRead?"true":"false",v.backlinksStable?"true":"false",v.childCount,v.childrenRead,v.geometryMask);
 Hex(f,v.descriptor,sizeof(v.descriptor));fprintf(f,",\"entityRaw\":");Hex(f,v.entityRaw,sizeof(v.entityRaw));fprintf(f,",\"proxyRaw\":");Hex(f,v.proxyRaw,sizeof(v.proxyRaw));fprintf(f,",\"memberRaw\":");Hex(f,v.memberRaw,sizeof(v.memberRaw));fprintf(f,",\"pose\":");Hex(f,v.pose,sizeof(v.pose));fprintf(f,",\"boundsTransform\":\"%llx\",\"transformRead\":%s,\"transform\":",v.boundsTransform,v.transformRead?"true":"false");Hex(f,v.transform,sizeof(v.transform));fprintf(f,"}");
}
void Flush(){
 if(!pending || inFlight.load())return;
 std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return;
 FILE* f=_wfsopen((directory/(L"gates-"+std::to_wstring(captureId)+L".jsonl")).c_str(),L"wb",_SH_DENYNO);if(!f)return;
 unsigned count=(std::min)(attempts.load(),Limit);
 fprintf(f,"{\"event\":\"begin\",\"schema\":1,\"captureId\":%llu,\"attempts\":%u,\"completed\":%u,\"saved\":%u,\"sourceFrame\":%u,\"note\":\"Read-only native task inputs/outputs for eight validated graphics classes including NPC-associated modifiers; one-frame subset bounded1024. No camera/visibility/LOD changes or extra native calls.\"}\n",captureId,attempts.load(),finished.load(),count,captureFrame.load());
 for(unsigned i=0;i<count;++i){auto& v=records[i];fprintf(f,"{\"event\":\"component\",\"index\":%u,\"taskIndex\":%u,\"tickBegin\":%llu,\"tickEnd\":%llu,\"before\":",i,v.taskIndex,v.tickBegin,v.tickEnd);WriteSnapshot(f,v.before);fprintf(f,",\"after\":");WriteSnapshot(f,v.after);fprintf(f,"}\n");}
 bool ok=ferror(f)==0;fclose(f);pending=false;
 fprintf(report,"F8 component gates51 capture=%llu frame=%u attempts=%u completed=%u saved=%u files=%d; read-only native input evidence\n",captureId,captureFrame.load(),attempts.load(),finished.load(),count,ok);fflush(report);
}
__declspec(noinline) void CapturePhase(Snapshot& snapshot,void* component,void* context){
 ComponentGateInputs63::CaptureInto(snapshot,Read,reinterpret_cast<uint64_t>(image),source,expectedContext,expectedOwner,reinterpret_cast<uint64_t>(component),reinterpret_cast<uint64_t>(context));
}
__declspec(noinline) uint64_t Forward(unsigned task,void* component,void* context,void* third,void* fourth){
 ++inFlight;bool take=false;unsigned index=Limit;uint64_t table{};
 if(enabled.load() && capturing.load() && reinterpret_cast<uint64_t>(context)==expectedContext && Read(&table,reinterpret_cast<uint64_t>(component),8) && ComponentGateInputs63::Known(reinterpret_cast<uint64_t>(image),table)){
  uint32_t frame{};if(Read(&frame,source+0x294,4)){uint32_t unset=0xffffffffu;captureFrame.compare_exchange_strong(unset,frame);if(frame==captureFrame.load()){index=attempts.fetch_add(1);take=index<Limit;}}
 }
 if(take){auto& v=records[index];v.taskIndex=task;v.tickBegin=GetTickCount64();CapturePhase(v.before,component,context);}
 uint64_t result=native(component,context,third,fourth);
 if(take){auto& v=records[index];CapturePhase(v.after,component,context);v.tickEnd=GetTickCount64();++finished;}
 --inFlight;return result;
}
uint64_t __fastcall Graphic(void* a,void* b,void* c,void* d){return Forward(0,a,b,c,d);}
uint64_t __fastcall Zones(void* a,void* b,void* c,void* d){return Forward(1,a,b,c,d);}
bool Writable(void** slot){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(slot,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool Replace(void** slot,void* expected,void* replacement){return slot && Writable(slot) && InterlockedCompareExchangePointer(slot,replacement,expected)==expected;}
}
bool Install(uint8_t* base,void* s,void* screen,const std::filesystem::path& output,FILE* log){
 image=base;source=reinterpret_cast<uint64_t>(s);surface=reinterpret_cast<uint64_t>(screen);report=log;directory=output/L"component-gates51";
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={{0x3096560,"\x90\xf6\x19\x42\x01\x00\x00\x00",8},{0x219f690,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x41\x8d\x40\xff\x48\x8b\xf2\x48\x8b\xf9\x48\x63\xd8\x85\xc0\x78\x19\x48\x8b\x0e\x48\x8b\x97\xb0\x01\x00\x00\x48\x8b\x0c\xd9\xff\x97\xa8\x01\x00\x00\x48\xff\xcb\x79\xe7\x48\x8b\x5c\x24\x30\x48\x8b\x74\x24\x38\x48\x83\xc4\x20\x5f\xc3",73},{0x30a14a8,"\xa0\x25\x24\x42\x01\x00\x00\x00",8},{0x218ceb8,"\x48\x8b\x01\xff\xa0\x38\x01\x00\x00",9},{0x21a4b99,"\x48\x8d\x35\x18\x83\xfe\xff\x48\x8d\x15\xe9\x1d\xef\x00\x48\x8d\x8f\x08\x0d\x00\x00\x48\x89\xb7\xb0\x0e\x00\x00",28},{0x21a4bf4,"\x48\x8d\x15\xd5\x1d\xef\x00\x48\x8d\x8f\x68\x10\x00\x00\x48\x89\xb7\x10\x12\x00\x00",21},{0x218b21a,"\xe8\x71\x98\x01\x00",5},{0x21e11be,"\x48\x89\x91\x90\x08\x00\x00",7},{0x22425d9,"\x49\x8b\x06\x48\x8b\xa8\x90\x08\x00\x00",10},{0x2242769,"\x48\x8b\x02\x48\x8b\x80\x90\x08\x00\x00",10},{0x2242ac8,"\x4c\x8b\x8d\x40\x02\x00\x00\x49\x8b\x8c\x24\x98\x00\x00\x00\xf3\x41\x0f\x10\x94\x24\xbc\x00\x00\x00\x4d\x8b\x89\x28\x04\x00\x00\x48\x8d\x44\x24\x60\x48\x89",39},{0x2241748,"\xe8\xa3\xd5\xf1\xff\x85\xc0",7},{0x21e9490,"\x41\x0f\xb6\x42\x54\xc1\xe0\x12\x33\x41\x10\x25\x00\x00\x04\x00\x31\x41\x10",19},{0x221245a,"\x44\x0f\xb7\x49\x54\x48\x8b\xd9\xb9\xfe\xff\x00\x00\x45\x0f\xb6\xc1\x41\x80\xe0\x01\x41\x3a\xd0\x40\x0f\x95\xc7\x66\x44\x23\xc9\x48\x8b\x4b\x40\x80\xe2\x01\x0f\xb6\xc2\x66\x41\x0b\xc1\x66\x89\x43\x54\x48\x85\xc9\x74\x41\x48",56},{0x30a04f8,"\x20\x24\x24\x42\x01\x00\x00\x00",8},{0x309fa78,"\x90\x65\x22\x42\x01\x00\x00\x00",8},{0x309ed38,"\x20\x60\x22\x42\x01\x00\x00\x00",8},{0x30a77c8,"\xd0\x2e\x2e\x42\x01\x00\x00\x00",8},{0x30a14a8,"\xa0\x25\x24\x42\x01\x00\x00\x00",8},{0x3099448,"\x00\x86\x1e\x42\x01\x00\x00\x00",8},{0x30a7aa8,"\xf0\x2f\x2e\x42\x01\x00\x00\x00",8},{0x30a4dd8,"\x60\xa4\x26\x42\x01\x00\x00\x00",8}};
 for(auto& g:guards)if(memcmp(base+g.rva,g.bytes,g.size)){fprintf(report,"REFUSED component gates51 signature=%zx\n",g.rva);fflush(report);return false;}
 ComponentTaskInputs49::Layout layout;if(!ComponentTaskInputs49::Resolve(Read,reinterpret_cast<uint64_t>(base),source,layout) || layout.surface!=surface)return false;
 expectedContext=layout.context;expectedOwner=layout.state;slots[0]=reinterpret_cast<void**>(layout.slots[0]);slots[1]=reinterpret_cast<void**>(layout.slots[1]);native=reinterpret_cast<Task>(base+0x218ceb8);
 if(!Writable(slots[0]) || !Writable(slots[1]))return false;
 if(!Replace(slots[0],reinterpret_cast<void*>(native),reinterpret_cast<void*>(Graphic)))return false;
 if(!Replace(slots[1],reinterpret_cast<void*>(native),reinterpret_cast<void*>(Zones))){Replace(slots[0],reinterpret_cast<void*>(Graphic),reinterpret_cast<void*>(native));return false;}
 enabled=true;fprintf(report,"Component gates51 installed two writable task delegates=%p/%p context=%llx; original executable vtable unchanged, four GPR args and integer return forwarded once, F8-only one-frame gate snapshots of eight classes including NPC-associated modifiers; no visibility/camera/mesh writes\n",slots[0],slots[1],layout.context);fflush(report);return true;
}
void SetEyes(void* left,void* right){eyes[0]=reinterpret_cast<uint64_t>(left);eyes[1]=reinterpret_cast<uint64_t>(right);}
void Poll(){
 if(!enabled)return;bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
 if(capturing && !down){capturing=false;pending=true;}
 Flush();
 if(down && !f8Down && !pending && !inFlight.load()){attempts=finished=0;captureFrame=0xffffffffu;++captureId;capturing=true;}
 f8Down=down;
}
void Stop(){
 enabled=false;capturing=false;unsigned restored=0;
 void* hooks[2]{reinterpret_cast<void*>(Graphic),reinterpret_cast<void*>(Zones)};
 for(unsigned i=0;i<2;++i){uint64_t current{};if(slots[i] && Read(&current,reinterpret_cast<uint64_t>(slots[i]),8) && (current==reinterpret_cast<uint64_t>(native) || Replace(slots[i],hooks[i],reinterpret_cast<void*>(native))))++restored;}
 if(attempts.load())pending=true;Flush();
 if(report && slots[0]){fprintf(report,"Component gates51 retired slotsRestored=%u/2 inFlight=%u; task forwarding/capture storage retained until exit, original image vtable never patched\n",restored,inFlight.load());fflush(report);}
}
}
