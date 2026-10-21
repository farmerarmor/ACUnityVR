#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace EntityVisibility41 {
bool Install(uint8_t* image,void* source,const std::filesystem::path& directory,FILE* log);
void SetEyes(void* left,void* right);
void Stop();
}
