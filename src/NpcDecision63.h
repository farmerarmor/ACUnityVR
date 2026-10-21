#pragma once
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <cstdio>
namespace NpcDecision63 {
bool Install(uint8_t* image,void* source,void* rendererWorld,const std::filesystem::path& output,FILE* log);
uint64_t CanonicalManager(uint64_t manager,uint64_t observed);
void BeginFrame(uint64_t sequence);
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame);
void Stop();
}
