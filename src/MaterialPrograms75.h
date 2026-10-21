#pragma once
#include <d3d11.h>
#include <filesystem>
#include <cstdio>
#include <cstdint>
namespace MaterialPrograms75 {
bool Initialize(ID3D11Device*,const std::filesystem::path&,FILE*);
unsigned Pair(uint64_t vs,uint64_t ps) noexcept;
void Poll(uint32_t mode);
void PixelCreated78(ID3D11Device*,const void*,SIZE_T,ID3D11PixelShader*) noexcept;
bool MaskEnabled() noexcept;
bool Hidden(unsigned group) noexcept;
void Count(unsigned group,unsigned eye) noexcept;
void Stop();
}
