#pragma once
#include "GpuCapture.h"
namespace EffectDrawCapture74 {
bool Install(uint8_t*,void*,ID3D11Device*,ID3D11DeviceContext*,FILE*,const std::filesystem::path&);
void SetEyes(void*,void*);
void Arm(uint64_t,uint64_t,uint32_t,const std::filesystem::path&);
void Observe(ID3D11DeviceContext*,unsigned,UINT,UINT,bool commandListReplay=false);
void Finish();
bool Isolate(ID3D11DeviceContext*,unsigned,UINT,UINT,bool commandListReplay=false,ID3D11Buffer* indirectArguments=nullptr);
void Poll(uint32_t mode);
bool HuntBindings78(ID3D11DeviceContext*,uint64_t shaders[6],void** nativeContext,unsigned* view,uint64_t* cache);
bool RemoveFog79(ID3D11DeviceContext*,unsigned kind,UINT vertices,UINT start,bool replay);
void Stop();
}
