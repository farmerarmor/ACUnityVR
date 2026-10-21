#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace VolumetricFog79 {
struct Program {IUnknown* object{};const void* code{};size_t bytes{};unsigned index{};};
bool Initialize(ID3D11Device*,const Program& vertex,const Program& pixel,const std::filesystem::path&,FILE*);
// Same stage numbers as ShaderHunt78: VS=0, PS=1. Called by existing creation hooks.
void Created(unsigned,ID3D11Device*,const void*,size_t,IUnknown*) noexcept;
bool Enabled() noexcept;
bool Matches(uint64_t vertex,uint64_t pixel) noexcept;
inline bool Shape(unsigned kind,unsigned vertices,unsigned start,unsigned eye,unsigned pass,unsigned subview,bool replay) noexcept {
 return !replay && kind==13 && vertices==6 && start==0 && eye>=1 && eye<=2 && pass==4 && subview==UINT32_MAX;
}
void Record(unsigned eye,bool direct) noexcept;
void Poll(uint32_t mode);
void Stop();
}
