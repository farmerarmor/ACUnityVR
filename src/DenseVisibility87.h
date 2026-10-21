#pragma once
#include <windows.h>
#include <filesystem>
#include <cstdint>
#include <cstdio>
namespace DenseVisibility87 {
bool Initialize(uint8_t* image,void* source,const std::filesystem::path& output,FILE* log);
void Poll(uint64_t sequence);
bool CorrectionEnabled();
void Stop();
}
