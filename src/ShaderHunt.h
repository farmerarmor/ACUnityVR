#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace ShaderHunt {
bool Install(ID3D11Device*,ID3D11DeviceContext*,uint8_t* image,const std::filesystem::path&,FILE*);
bool Enabled();
bool TakeCaptureRequest();
void BeginFrame(uint64_t sequence,uint64_t tracking,uint32_t frame);
bool Skip(ID3D11DeviceContext*,unsigned kind,UINT a,UINT b,const char* pass,unsigned view,void* nativeContext);
void Restore();
}
