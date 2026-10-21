// Correct only CS146 cloud rays on the verified game immediate context.
// Existing game device; no native executable hook, shader replacement or CPU writes.
#include "CloudProjection.h"
#include "CloudRayMath.h"
#include "RenderSize.h"
#include "GpuCapture.h"
#include <d3d11_1.h>
#include <atomic>
namespace CloudProjection {
namespace {
using acu::Field;using Microsoft::WRL::ComPtr;
uint8_t *image{},*backend{};void* eyes[2]{};FILE* report{};
ComPtr<ID3D11DeviceContext> immediate;ComPtr<ID3D11DeviceContext1> immediate1;
ComPtr<ID3D11ComputeShader> shader;ComPtr<ID3D11Buffer> correctedBuffer;
std::atomic<bool> installed{},apply{};std::atomic<unsigned> inFlight{};
bool correction=true;uint64_t sequence{},tracking{},patched[2]{},sourceCalls{},refused{},restoreFailures{},bindingChecks{};
struct Inputs{float projection[16],world[16],native[16];unsigned eye{};};
bool Identity(ID3D11ComputeShader** result){__try{
 auto renderer=Field<uint8_t*>(image,0x524cfd8);backend=renderer?Field<uint8_t*>(renderer,0x10):nullptr;
 if(!backend || Field<void*>(backend,0)!=image+0x30a86f0)return false;
 auto object=Field<uint8_t*>(backend,0x1450);if(!object || Field<void*>(object,0)!=image+0x30a87f0 || Field<void*>(image+0x30a87f0,8)!=image+0x22bf820)return false;
 auto manager=image+0x524e030;auto array=Field<uint8_t**>(manager,0x44);auto count=Field<uint16_t>(manager,0x4c);
 if(!array || count<=146 || count>4096)return false;auto entry=array[146];if(!entry)return false;
 auto code=Field<uint8_t*>(entry,0);if(code!=image+0x33ebc40 || Field<uint64_t>(entry,8)!=6512 || memcmp(code,"DXBC",4))return false;
 *result=Field<ID3D11ComputeShader*>(entry,0x10);return *result!=nullptr;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
int Read(Inputs* inputs){__try{
 auto ctx=Field<uint8_t*>(backend,0x20);if(!ctx)return -1;
 auto view=Field<uint8_t*>(ctx,0x30);inputs->eye=view==eyes[0]?0:view==eyes[1]?1:2;
 if(inputs->eye==2){++sourceCalls;return 0;}
 auto state=Field<void*>(ctx,0x28);auto object=Field<uint8_t*>(backend,0x1450);
 if(!state || Field<void*>(ctx,8)!=backend || Field<void*>(view,0x430)!=state || Field<void*>(state,0x8208)!=view ||
    !object || Field<void*>(object,0)!=image+0x30a87f0 || Field<uint32_t>(object,0x13f8)!=4 || Field<void*>(object,0x1400)!=object+0x1420 || !Field<void*>(object,0x1410))return -2;
 if(memcmp(ctx+0x60,view+0x320,64))return -3;
 if(!CloudRayMath::SameCamera(reinterpret_cast<float*>(view+0x360),reinterpret_cast<float*>(ctx+0x270)))return -4;
 memcpy(inputs->projection,ctx+0x60,64);memcpy(inputs->world,ctx+0x270,64);memcpy(inputs->native,object+0x1420,64);return 1;
 }__except(EXCEPTION_EXECUTE_HANDLER){return -5;}}
void Refused(const char* reason,int detail=0){++refused;if(refused<=6){fprintf(report,"Cloud correction REFUSED reason=%s detail=%d sequence=%llu; original dispatch preserved\n",reason,detail,sequence);fflush(report);}}
}
bool Install(uint8_t* base,ID3D11Device* device,ID3D11DeviceContext* context,FILE* log){
 image=base;report=log;ID3D11ComputeShader* pointer{};
 if(!device || !context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || !Identity(&pointer))return false;
 const uint8_t guard[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
 if(memcmp(image+0x2274ce0,guard,sizeof(guard)))return false;
 shader=pointer;immediate=context;context->QueryInterface(IID_PPV_ARGS(&immediate1));
 D3D11_BUFFER_DESC desc{};desc.ByteWidth=64;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 if(FAILED(device->CreateBuffer(&desc,nullptr,&correctedBuffer)))return false;
 installed=true;fprintf(report,"Cloud correction installed: verified nativeCS146 CB8, existing game device/immediate=%p; starts ON/F6; original binding restored after each dispatch\n",context);fflush(report);return true;
}
void SetEyes(void* left,void* right){eyes[0]=left;eyes[1]=right;}
void BeginFrame(uint64_t id,uint64_t pose,uint32_t mode){
 sequence=id;tracking=pose;static bool held{};bool down=(GetAsyncKeyState(VK_F6)&0x8000)!=0;
 if(down && !held){correction=!correction;fprintf(report,"F6 cloud projection correction=%d sequence=%llu\n",correction,sequence);fflush(report);}held=down;apply=correction && mode==1;
}
Scope::Scope(ID3D11DeviceContext* ctx,UINT x,UINT y,UINT z){
 if(!installed || !apply || ctx!=immediate.Get())return;
 ComPtr<ID3D11ComputeShader> bound;ctx->CSGetShader(&bound,nullptr,nullptr);if(bound.Get()!=shader.Get())return;
 Inputs inputs{};auto eligibility=Read(&inputs);if(!eligibility)return;if(eligibility<0){Refused("camera/native identity",eligibility);return;}
 ComPtr<ID3D11UnorderedAccessView> target;ctx->CSGetUnorderedAccessViews(0,1,&target);
 if(!target){Refused("missing UAV0");return;}ComPtr<ID3D11Resource> resource;target->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;resource.As(&texture);
 if(!texture){Refused("UAV0 not Texture2D");return;}D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
 if(!RenderSize::CloudMatches(d.Width,d.Height) || d.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || d.ArraySize!=1 || d.MipLevels!=1 || d.SampleDesc.Count!=1 || x!=(d.Width+15)/16 || y!=(d.Height+15)/16 || z!=1){Refused("UAV description or dispatch");return;}
 UINT first{},count{};if(immediate1)immediate1->CSGetConstantBuffers1(8,1,&original,&first,&count);else ctx->CSGetConstantBuffers(8,1,&original);
 if(!original || (immediate1 && (first!=0 || count!=4096))){Refused("original CB8 window",int(count));return;}
 D3D11_BUFFER_DESC desc{};original->GetDesc(&desc);if(desc.ByteWidth!=64 || !(desc.BindFlags&D3D11_BIND_CONSTANT_BUFFER)){Refused("original CB8 description",int(desc.ByteWidth));return;}
 float updated[16]{};if(!CloudRayMath::Correct(inputs.projection,inputs.world,inputs.native,d.Width,d.Height,updated)){Refused("ray inputs");return;}
 ++inFlight;context=ctx;ctx->UpdateSubresource(correctedBuffer.Get(),0,nullptr,updated,0,0);auto buffer=correctedBuffer.Get();ctx->CSSetConstantBuffers(8,1,&buffer);active=true;++patched[inputs.eye];
 if(patched[inputs.eye]<=3){fprintf(report,"Cloud rays eye=%u sequence=%llu pose=%llu correctedDir=(%.9g,%.9g,%.9g) right=(%.9g,%.9g,%.9g) up=(%.9g,%.9g,%.9g) originalCB=%p privateCB=%p\n",inputs.eye,sequence,tracking,updated[4],updated[5],updated[6],updated[8],updated[9],updated[10],updated[12],updated[13],updated[14],original.Get(),buffer);fflush(report);}
}
Scope::~Scope(){
 if(!active)return;auto buffer=original.Get();context->CSSetConstantBuffers(8,1,&buffer);
 if(bindingChecks<6){ComPtr<ID3D11Buffer> actual;context->CSGetConstantBuffers(8,1,&actual);bool restored=actual.Get()==buffer;if(!restored)++restoreFailures;++bindingChecks;fprintf(report,"Cloud native binding restored=%d sequence=%llu\n",restored,sequence);fflush(report);}
 --inFlight;
}
void Restore(){
 apply=false;installed=false;if(report){fprintf(report,"Cloud correction retired inFlight=%u corrected=%llu/%llu refused=%llu sourceForwarded=%llu bindingChecks=%llu restoreFailures=%llu; private CB retained; original native buffers/CPU data untouched\n",inFlight.load(),patched[0],patched[1],refused,sourceCalls,bindingChecks,restoreFailures);fflush(report);}
}
}
