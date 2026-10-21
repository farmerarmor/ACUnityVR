#pragma once
#include "BuildGuard.h"
#include <filesystem>
#include <cstdio>
namespace ComponentGateProbe51 {
bool Install(uint8_t* image,void* source,void* surface,const std::filesystem::path& output,FILE* log);
void SetEyes(void* left,void* right);
void Poll();
void Stop();
}
