#pragma once
#include "EffectInputs72.h"
#include <d3d11.h>
#include <filesystem>
#include <cstdio>
namespace CrowdDraw82 {
bool Initialize(uint8_t*,ID3D11Device*,const std::filesystem::path&,FILE*);
bool Armed() noexcept;
void Arm(uint64_t,uint64_t,uint32_t,const std::filesystem::path&);
void Observe(const EffectInputs72::Worker&,uint64_t,const std::array<uint64_t,2>&,unsigned,UINT,UINT,bool);
void Finish();
void Stop();
}
