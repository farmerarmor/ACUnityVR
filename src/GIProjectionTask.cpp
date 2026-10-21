// Correct GI volume constants through the verified native IndoorGI callback.
// Exact build only; no shader replacement or D3D resource/binding mutation.
#include "GIProjectionTask.h"
#include "GIRayMath.h"
#include "GpuCapture.h"
#include <atomic>
namespace GIProjectionTask {
namespace {
using acu::Field;using Build=void(__fastcall*)(void*,void*);
uint8_t* image{};uint8_t* backend{};void* eyes[2]{};FILE* report{};Build nativeBuild{},nativeClear{},nativeCull{},nativeMask{},nativeFinish{};
std::atomic<bool> installed{},apply{};std::atomic<unsigned> inFlight{};
bool correction=true;uint64_t sequence{},tracking{},patched[2]{},refused{},sourceCalls{};
float* lastBlock{};void* lastObject{};float before[8]{},after[8]{};
bool Patch(void* object,void* context,unsigned* eye,float* old,float* updated){__try{
 auto ctx=static_cast<uint8_t*>(context);auto view=Field<uint8_t*>(ctx,0x30);auto state=Field<uint8_t*>(ctx,0x28);
 if(!view || !state || Field<void*>(view,0x430)!=state || Field<void*>(state,0x8208)!=view)return false;
 *eye=view==eyes[0]?0:view==eyes[1]?1:2;if(*eye==2){++sourceCalls;lastBlock=nullptr;lastObject=nullptr;return true;}
 if(!apply)return true;
 if(context!=Field<void*>(backend,0x20) || object!=Field<void*>(backend,0x1448) || Field<void*>(ctx,8)!=backend)return false;
 auto block=static_cast<uint8_t*>(object)+0x3e0;float* high=reinterpret_cast<float*>(block+5120);float* pixel=reinterpret_cast<float*>(block+5136);
 auto width=Field<unsigned>(backend,0x4d0),height=Field<unsigned>(backend,0x4d4);
 if(Field<unsigned>(block,5184)>32 || memcmp(ctx+0x60,view+0x320,40))return false;
 memcpy(old,high,32);
 if(!GIRayMath::Correct(reinterpret_cast<float*>(ctx+0x60),high,pixel,width,height,updated,updated+4))return false;
 memcpy(high,updated,32);lastBlock=high;lastObject=object;memcpy(before,old,32);memcpy(after,updated,32);++patched[*eye];return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Eligible(void* b,void* context,void** object){__try{
 if(b!=backend || context!=Field<void*>(backend,0x20))return false;
 auto ctx=static_cast<uint8_t*>(context);auto v=Field<void*>(ctx,0x30);
 if(v!=eyes[0] && v!=eyes[1]){++sourceCalls;lastBlock=nullptr;lastObject=nullptr;return false;}
 auto state=Field<void*>(ctx,0x28);
 if(!v || Field<void*>(ctx,8)!=backend || Field<void*>(v,0x430)!=state || !state || Field<void*>(state,0x8208)!=v)return false;
 *object=Field<void*>(backend,0x1448);return *object!=nullptr;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool RestoreTail(){__try{
 if(lastBlock && lastObject==Field<void*>(backend,0x1448) && !memcmp(lastBlock,after,sizeof(after))){memcpy(lastBlock,before,sizeof(before));lastBlock=nullptr;lastObject=nullptr;return true;}
 // A later native build may already have replaced the corrected values.
 lastBlock=nullptr;lastObject=nullptr;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}

}
bool Install(uint8_t* base,FILE* log){
 image=base;report=log;auto renderer=Field<uint8_t*>(image,0x524cfd8);backend=renderer?Field<uint8_t*>(renderer,0x10):nullptr;
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 const uint8_t finishBytes[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x9a,0x48,0x14,0x00,0x00,0x49,0x8b,0xd0,0x49,0x8b,0xf8,0x48,0x8b,0xcb,0xe8,0x21,0xe7,0xf4,0xff,0x48,0x8b,0xd7,0x48,0x8b,0xcb,0x48,0x8b,0x5c,0x24,0x30,0x48,0x83,0xc4,0x20,0x5f,0xe9,0x5c,0xe7,0xf4,0xff};const uint8_t sequenceBytes[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48,0x8b,0xf9,0xe8,0xbb,0x46,0x00,0x00,0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xe8,0x10,0x36,0xfe,0xff,0x80,0x7b,0x54,0x00,0x75,0x16,0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xe8,0x6f,0x02,0x00,0x00,0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xe8,0x94,0x06,0x00,0x00,0x48,0x8b,0x5c,0x24,0x30,0x48,0x83,0xc4,0x20,0x5f,0xc3};
 if(memcmp(image+0x22908c0,finishBytes,sizeof(finishBytes)) || memcmp(image+0x21df000,sequenceBytes,sizeof(sequenceBytes)))return false;
 const struct{size_t rva;const char* bytes;size_t size;} guards[]{
  {0x21e36d0,"\x48\x8b\xc4\x48\x89\x58\x20\x48\x89\x50\x10\x55\x56\x57\x41\x54",16},
  {0x21c2630,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x50",15},
  {0x21df2a0,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x48",16},
  {0x21df6d0,"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x57",16},
  {0x21df050,"\x48\x89\x5c\x24\x10\x48\x89\x7c\x24\x18\x41\x56\x48\x83\xec\x20",16}};
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;
 nativeBuild=reinterpret_cast<Build>(image+0x21e36d0);nativeClear=reinterpret_cast<Build>(image+0x21c2630);
 nativeCull=reinterpret_cast<Build>(image+0x21df2a0);nativeMask=reinterpret_cast<Build>(image+0x21df6d0);nativeFinish=reinterpret_cast<Build>(image+0x21df050);installed=true;
 fprintf(report,"GI asymmetric rays through verified native task callback, starts ON/F7. Game executable code is unmodified.\n");fflush(report);return true;
}
bool Execute(unsigned pass,void*,void* b,void* context){
 if(pass!=2 || !installed || !apply)return false;void* object{};if(!Eligible(b,context,&object))return false;
 ++inFlight;
 // Exact22908c0 invokes21df000 followed by21df050. Exact21df000 calls
 // builder, clear and (when context+54==0) compute culling then mask.
 nativeBuild(object,context);
 unsigned eye=2;float old[8]{},updated[8]{};bool valid=Patch(object,context,&eye,old,updated);
 if(!valid){++refused;if(refused<5){fprintf(report,"GI task ray correction REFUSED eye=%u sequence=%llu\n",eye,sequence);fflush(report);}}
 else if(eye<2 && patched[eye]<=3){fprintf(report,"GI task rays eye=%u sequence=%llu pose=%llu before=(%.9g,%.9g)/(%.9g,%.9g) corrected=(%.9g,%.9g)/(%.9g,%.9g)\n",eye,sequence,tracking,old[0],old[1],old[4],old[5],updated[0],updated[1],updated[4],updated[5]);fflush(report);}
 nativeClear(object,context);
 if(!Field<uint8_t>(context,0x54)){nativeCull(object,context);nativeMask(object,context);}
 nativeFinish(object,context);--inFlight;return true;
}
void SetEyes(void* left,void* right){eyes[0]=left;eyes[1]=right;}
void BeginFrame(uint64_t id,uint64_t pose,uint32_t mode){
 sequence=id;tracking=pose;static bool held{};bool down=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
 if(down && !held){correction=!correction;fprintf(report,"F7 GI asymmetric ray correction=%d sequence=%llu\n",correction,sequence);fflush(report);}held=down;apply=correction && mode==1;
}
void Restore(){
 apply=false;installed=false;bool tail=backend?RestoreTail():true;
 if(report){fprintf(report,"GI task correction retired tailRestored=%d inFlight=%u corrected=%llu/%llu refused=%llu sourceForwarded=%llu; no game code hook\n",tail,inFlight.load(),patched[0],patched[1],refused,sourceCalls);fflush(report);}
}
}
