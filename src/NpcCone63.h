#pragma once
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include "CameraMath.h"
#include "BinocularVisibility63.h"
namespace NpcCone63 {
bool Install(uint8_t* image,void* source,void* rendererWorld,const std::filesystem::path& output,FILE* log);
void BeginFrame(uint32_t frame,uint64_t sequence,uint64_t tracking,bool tracked,const CameraMath::Matrix poses[2],const Transport::Eye* optics=nullptr);
uint64_t CanonicalCollection(uint64_t collection,uint64_t observed);
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame);
bool CurrentFrustums(uint32_t frame,BinocularVisibility63::Frustums& views,uint64_t& age);
void Stop();
}
