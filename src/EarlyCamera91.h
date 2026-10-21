#pragma once
#include "UnityCamera.h"
#include <filesystem>
#include <cstdio>
namespace EarlyCamera91 {
using Provider=bool(*)(const CameraMath::Matrix&,CameraMath::Matrix&,uint64_t&);
bool Initialize(uint8_t*,void*,Provider,const std::filesystem::path&,FILE*);
// Restore before native renderer Bind. Return the original mouse-camera base,
// because native preparation may already have copied the temporary head pose.
bool BeforeScene(CameraMath::Matrix&);
void VisibilityBoundary();
void Poll(uint64_t);
void Stop();
}
