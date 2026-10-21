#pragma once
#include "UnityCamera.h"
#include <filesystem>
#include <cstdio>
namespace FirstPerson70 {
bool Initialize(unsigned char* image,void* rendererWorld,const std::filesystem::path& directory,FILE* log);
void BeginFrame(bool tracked,uint64_t sequence,CameraMath::Matrix& base);
void AnchorCamera(CameraMath::Matrix& base);
bool Selection(void* view);
void Stop();
}
