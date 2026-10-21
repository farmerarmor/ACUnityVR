#pragma once
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include <cstdint>
namespace NpcCallback63 {
bool Install(uint8_t* image,void* source,void* rendererWorld,const std::filesystem::path& output,FILE* log);
void ConsiderProxy(uint64_t proxy,const uint8_t* headPlanes);
uint64_t CanonicalVtable(uint64_t entity,uint64_t observed);
void CaptureIfRequested(uint64_t sequence,uint64_t tracking,uint32_t frame);
bool Armed(uint64_t entity,uint64_t handle,uint64_t proxy,uint32_t* registeredState=nullptr,uint32_t* currentState=nullptr);
bool LiveNpcIdentity(uint64_t entity,uint64_t handle,uint64_t proxy,float lo[3],float hi[3],uint32_t* currentState=nullptr);
void Stop();
}
