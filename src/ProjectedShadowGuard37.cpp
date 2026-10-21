// Forward valid native projected-shadow jobs. Skip the previously observed
// missing atlas and the exact per-projector texture-holder null fault from
// cutscene dumps5900/8148. Read-only inputs; no resource writes or exception
// handler around native rendering.
#include "ProjectedShadowGuard37.h"
#include "GpuCapture.h"
#include <atomic>
namespace ProjectedShadowGuard {
using acu::Field;
using Start=void(__fastcall*)(void*,void*,void*);
using Worker=void(__fastcall*)(void*,void*,void*,unsigned,unsigned);
uint8_t *image{},*backend{};void **startDescriptor{},**descriptors[8]{};
void *startClone[5]{},*workerClone[5]{};void **startTable{},**workerTable{};
Start nativeStart{};Worker nativeWorker{};
FILE* guardLog{};
std::atomic<uint64_t> missingProjector{};
std::atomic<bool> enabled{};std::atomic<uint64_t> missing{},starts{},ready{};
struct Prep{uint64_t view,state,pool,targetBefore[2],targetAfter[2];uint32_t flags,frame;};
Prep observations[48]{};unsigned observed{},writeIndex{};SRWLOCK observationLock=SRWLOCK_INIT;
bool Writable(void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_READWRITE;}
bool ObserveContext(void* context,Prep* p){__try{
 auto state=Field<uint8_t*>(context,0x28);auto view=Field<uint8_t*>(context,0x30);
 if(!state || !view || Field<void*>(view,0x430)!=state || Field<void*>(state,0x8208)!=view)return false;
 p->view=reinterpret_cast<uint64_t>(view);p->state=reinterpret_cast<uint64_t>(state);p->pool=Field<uint64_t>(state,0x8650);p->frame=Field<uint32_t>(view,0x294);return true;
}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void __fastcall Prepare(void* task,void* b,void* context){
 if(!enabled || b!=backend){nativeStart(task,b,context);return;}
 Prep p{};p.flags=Field<uint32_t>(b,0x528);p.targetBefore[0]=Field<uint64_t>(b,0xe48);p.targetBefore[1]=Field<uint64_t>(b,0xe50);
 bool identity=ObserveContext(context,&p);nativeStart(task,b,context);
 p.targetAfter[0]=Field<uint64_t>(b,0xe48);p.targetAfter[1]=Field<uint64_t>(b,0xe50);++starts;if(p.targetAfter[0] || p.targetAfter[1])++ready;
 if(identity && TryAcquireSRWLockExclusive(&observationLock)){observations[writeIndex++%48]=p;if(observed<48)++observed;ReleaseSRWLockExclusive(&observationLock);}
}
enum class ProjectorInput { Unchecked, Ready, MissingBundle, MissingTextureHolder };
struct ProjectorProbe {uint64_t view{},state{},bundle{};unsigned index{};};
ProjectorInput InspectProjector(void* task,void* context,ProjectorProbe* p){__try{
 //2290c4c forwards this exact descriptor/context to22c2720. The latter
 //reads state=context+28, bundle=state+3f40+950*index, then22d8b30
 //dereferences the texture-holder at bundle+c0 (22d8b7d..22d8b84).
 unsigned index=8;
 for(unsigned i=0;i<8;++i)if(task==descriptors[i]){index=i;break;}
 if(index==8 || !context || Field<uint32_t>(task,0x1c)!=index)return ProjectorInput::Unchecked;
 auto state=Field<uint8_t*>(context,0x28);auto view=Field<uint8_t*>(context,0x30);
 if(!state || !view || Field<void*>(view,0x430)!=state || Field<void*>(state,0x8208)!=view)return ProjectorInput::Unchecked;
 auto bundle=Field<uint8_t*>(state,0x3f40+0x950*index);
 p->view=reinterpret_cast<uint64_t>(view);p->state=reinterpret_cast<uint64_t>(state);p->bundle=reinterpret_cast<uint64_t>(bundle);p->index=index;
 if(!bundle)return ProjectorInput::MissingBundle;
 //A nonnull holder containing a null texture is handled by native code;
 //do not reject it or inspect guessed COM/resource layouts.
 return Field<void*>(bundle,0xc0)?ProjectorInput::Ready:ProjectorInput::MissingTextureHolder;
}__except(EXCEPTION_EXECUTE_HANDLER){return ProjectorInput::Unchecked;}}
void __fastcall Guard(void* task,void* b,void* context,unsigned lane,unsigned lanes){
 if(enabled && b==backend){
  if(!Field<void*>(b,0xe48) && !Field<void*>(b,0xe50)){++missing;return;}
  ProjectorProbe p{};auto result=InspectProjector(task,context,&p);
  if(result==ProjectorInput::MissingBundle || result==ProjectorInput::MissingTextureHolder){
   auto n=missingProjector.fetch_add(1)+1;
   //At most8 worker records per process; no per-frame disk tracing.
   if(guardLog && n<=8){fprintf(guardLog,"Projected-shadow missing input skipped=%llu projector=%u reason=%s view=%llx state=%llx bundle=%llx lane=%u/%u\n",n,p.index,result==ProjectorInput::MissingBundle?"bundle":"textureHolder",p.view,p.state,p.bundle,lane,lanes);fflush(guardLog);}
   return;
  }
 }
 //Probe exceptions only make the check inapplicable. Native faults remain
 //visible in crash dumps, and every ready/inapplicable job forwards once.
 nativeWorker(task,b,context,lane,lanes);
}
bool Install(uint8_t* base,void* log){
 guardLog=static_cast<FILE*>(log);
 image=base;auto renderer=Field<uint8_t*>(image,0x524cfd8);if(!renderer)return false;backend=Field<uint8_t*>(renderer,0x10);
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 startDescriptor=reinterpret_cast<void**>(image+0x5049be8);startTable=reinterpret_cast<void**>(image+0x30a8f98);workerTable=reinterpret_cast<void**>(image+0x30a8fc0);
 nativeStart=reinterpret_cast<Start>(image+0x2291970);nativeWorker=reinterpret_cast<Worker>(image+0x2290b40);
 if(*startDescriptor!=startTable || startTable[2]!=reinterpret_cast<void*>(nativeStart) || workerTable[3]!=reinterpret_cast<void*>(nativeWorker) || !Writable(startDescriptor))return false;
 for(unsigned i=0;i<8;++i){descriptors[i]=reinterpret_cast<void**>(image+0x5049c10+0x40*i);if(*descriptors[i]!=workerTable || !Writable(descriptors[i]) || Field<uint32_t>(descriptors[i],0x1c)!=i)return false;}
 memcpy(startClone,startTable,sizeof(startClone));memcpy(workerClone,workerTable,sizeof(workerClone));startClone[2]=reinterpret_cast<void*>(Prepare);workerClone[3]=reinterpret_cast<void*>(Guard);
 if(InterlockedCompareExchangePointer(startDescriptor,startClone,startTable)!=startTable)return false;
 for(unsigned i=0;i<8;++i)if(InterlockedCompareExchangePointer(descriptors[i],workerClone,workerTable)!=workerTable){Restore();return false;}
 return true;
}
void Enable(bool value){enabled=value;}
void Restore(){enabled=false;if(startDescriptor)InterlockedCompareExchangePointer(startDescriptor,startTable,startClone);for(auto p:descriptors)if(p)InterlockedCompareExchangePointer(p,workerTable,workerClone);}
uint64_t MissingTargets(){return missing.load();}
void Report(void* output){auto log=static_cast<FILE*>(output);if(!log)return;
 AcquireSRWLockExclusive(&observationLock);fprintf(log,"Projected-shadow diagnostic: prepare=%llu targetReady=%llu missingTargetWorkerCalls=%llu observations=%u missingProjectorWorkerCalls=%llu\n",starts.load(),ready.load(),missing.load(),observed,missingProjector.load());
 for(unsigned i=0;i<observed;++i){auto& p=observations[(writeIndex-observed+i)%48];fprintf(log,"ShadowPrep view=%llx frame=%u state=%llx pool=%llx flags=%x targets=%llx/%llx -> %llx/%llx\n",p.view,p.frame,p.state,p.pool,p.flags,p.targetBefore[0],p.targetBefore[1],p.targetAfter[0],p.targetAfter[1]);}
 ReleaseSRWLockExclusive(&observationLock);fflush(log);
}
}
