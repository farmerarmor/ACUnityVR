#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace DrawDiagnostics {
bool Install(uint8_t* image,void* surface,void* source,const std::filesystem::path& directory,FILE* log);
void SetEyes(void* left,void* right);
using PassOverride=bool(*)(unsigned pass,void* task,void* backend,void* context);
void SetPassOverride(PassOverride function);
void BeginFrame(uint64_t sequence,uint64_t trackingId,uint32_t sourceFrame,uint32_t mode=1);
void EndEye(unsigned eye);
bool Capturing();
void Restore();
}
