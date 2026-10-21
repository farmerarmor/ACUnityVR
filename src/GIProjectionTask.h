#pragma once
#include <cstdint>
#include <cstdio>
namespace GIProjectionTask {
bool Install(uint8_t* image,FILE* log);
void SetEyes(void* left,void* right);
void BeginFrame(uint64_t sequence,uint64_t tracking,uint32_t mode);
bool Execute(unsigned pass,void* task,void* backend,void* context);
void Restore();
}
