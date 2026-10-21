#pragma once
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include <cstdint>
namespace NpcCamera63 {
bool Install(uint8_t* image,void* source,void* world,const std::filesystem::path& output,FILE* log);
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame);
void Stop();
}
