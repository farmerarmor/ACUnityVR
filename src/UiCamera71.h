#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include "SharedPair.h"
namespace UiCamera71 {
bool Install(uint8_t* image,const std::filesystem::path& directory,FILE* log);
void SetEyes(void* left,void* right);
void BeginFrame(uint64_t sequence,const Transport::RenderInfo& rendered);
bool Execute(unsigned pass,void* task,void* backend,void* context);
void Stop();
}
