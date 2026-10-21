#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace PostAAOptimization {
using DrawFunction=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);
bool Initialize(uint8_t* image,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& directory,FILE* log);
void BeginFrame(uint64_t sequence,uint32_t mode);
bool Draw(ID3D11DeviceContext* context,unsigned view,unsigned passRva,UINT count,UINT start,DrawFunction original,bool capture);
void EndEye(unsigned view);
void Stop();
bool NeedsScope();
}
