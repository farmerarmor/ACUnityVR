#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstdio>

namespace NativeGroup88 {
bool Initialize(uint8_t* image,FILE* log);
// Exact private native B8 body; camera replacements are confined to its copy.
uint64_t Invoke(void* a,void* b,void* c,void* d,const uint8_t head[32]);
uint64_t CameraReads();
}
