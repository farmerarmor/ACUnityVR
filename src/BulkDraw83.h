#pragma once
#include <windows.h>
#include "UnityCamera.h"
#include <filesystem>
#include <cstdio>
#include <cstdint>
namespace BulkDraw83 {
bool Initialize(uint8_t*,void*,const std::filesystem::path&,FILE*);
void Publish(uint32_t,uint64_t,uint64_t,bool,const CameraMath::Matrix[2],const Transport::Eye[2]);
bool CurrentCamera(uint32_t frame,CameraMath::Matrix&,uint64_t& age,uint64_t& sequence,uint64_t& tracking);
void SetEyes(void*,void*);
void ArmSelection(uint64_t,uint64_t,uint32_t,const std::filesystem::path&);
void FinishSelection();
void Poll(uint64_t,uint64_t,uint32_t);
// Recognize only this observer's exact live owned primary table for other guards.
uint64_t CanonicalBulk(uint64_t object,uint64_t table);
void Stop();
}
