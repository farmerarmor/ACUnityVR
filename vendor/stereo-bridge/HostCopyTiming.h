#pragma once
#include <d3d11.h>
#include <cstdint>
namespace HostCopyTiming {
enum class Phase:unsigned {AcquirePair,SharedFlush,WaitFrame,WaitSwapchain,SwapchainFlush,EndFrame,Count};
bool Initialize(ID3D11Device*,ID3D11DeviceContext*);
void BeginIteration(uint64_t frame,uint32_t mode,uint32_t variant,bool foreground);
void FreshPair(uint64_t frame,uint32_t mode);
void BeginShared();void EndShared();void BeginSwapchain();void EndSwapchain();
void Capture();void EndIteration();void Stop();
struct CpuScope {explicit CpuScope(Phase);~CpuScope();Phase phase;int64_t begin{};};
}
