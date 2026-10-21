#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <d3d11.h>
namespace OcclusionProbe {
bool Initialize(uint8_t* image,void* surface,void* source,const std::filesystem::path& directory,FILE* log);
#ifdef ACUNITYVR_GPU_CONSTANTS40
bool InitializeGpuConstants40(ID3D11Device* device,ID3D11DeviceContext* immediate);
#endif
void SetEyes(void* left,void* right);
void Arm(uint64_t sequence,uint64_t tracking,uint32_t frame,const std::filesystem::path& captureDirectory);
void Observe(ID3D11DeviceContext* context,unsigned kind,UINT x,UINT y,UINT z);
void Finish();
void Stop();
bool InstallWorkerHooks(ID3D11DeviceContext* immediateContext);
void RestoreWorkerHooks();
DWORD Baseline(uint8_t* image,const std::filesystem::path& directory,FILE* log);
}
