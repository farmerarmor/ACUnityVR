#pragma once
#include "BuildGuard.h"
#include "BinocularComponentInput50.h"
#include <cstdio>
namespace PrimaryCamera52 {
bool Initialize(uint8_t* image,uint8_t* source,uint8_t* surface,FILE* log);
void BeginFrame(uint32_t frame,uint64_t sequence,uint64_t tracking,bool tracked,const CameraMath::Matrix poses[2],const Transport::Eye optics[2]);
void Stop();
}
