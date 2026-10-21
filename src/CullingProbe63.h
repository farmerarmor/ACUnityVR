#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace CullingProbe {
bool Initialize(uint8_t* image, void* source, const std::filesystem::path& directory, FILE* log);
void BeginFrame(uint64_t sequence, uint64_t tracking, uint32_t sourceFrame, uint32_t mode, bool capture);
void AfterCull(unsigned eye, void* view, void* graph) noexcept;
void SetSecondary(bool enabled,uint64_t generation);
void AfterSelection(unsigned eye,void* view);
void BeforeRender(unsigned eye,void* view);
void EndEye(unsigned eye,void* view);
void EndPair();
void Stop();
}
