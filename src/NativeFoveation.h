#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include "SharedPair.h"
namespace NativeFoveation {
bool Initialize(ID3D11Device*,ID3D11DeviceContext*,unsigned width,unsigned height,const std::filesystem::path&,FILE*);
bool BeginFrame(uint64_t sequence,uint32_t mode,const Transport::Eye* eyes,bool f4Down);
bool NeedsScope();
#ifdef ACUNITYVR_VISIBILITY_MASK
void SetVisibility(const Transport::Visibility&,uint32_t generation);
#endif
void EndEye(unsigned eye,bool capture);
bool Stop();
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
bool GeometryRequested();
struct CommandScope {
 bool applied{};
 void Apply(ID3D11DeviceContext*,unsigned view,unsigned descriptor,unsigned table,unsigned flags,unsigned laneBegin,unsigned laneEnd);
 ~CommandScope();
 CommandScope()=default;CommandScope(const CommandScope&)=delete;CommandScope& operator=(const CommandScope&)=delete;
};
#endif
struct DrawScope {
 bool applied{};
 void Apply(ID3D11DeviceContext*,unsigned view,unsigned pass,unsigned shader);
 ~DrawScope();
 DrawScope()=default;DrawScope(const DrawScope&)=delete;DrawScope& operator=(const DrawScope&)=delete;
};
}
