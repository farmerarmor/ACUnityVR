#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace NativeGpuTiming {
#ifdef ACUNITYVR_CLEAN_RUNTIME
inline void BeginSource(uint64_t,uint64_t,uint32_t,bool){}
inline void EndView(unsigned){}
inline void BeginEye(unsigned){}
inline void BeginTransport(){}
inline void EndTransport(){}
#else
bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& directory,FILE* log);
void BeginSource(uint64_t sequence,uint64_t tracking,uint32_t mode,bool capture);
void EndView(unsigned view);
void BeginEye(unsigned eye);
void BeginPass(unsigned view,unsigned pass);
void EndPass(unsigned view,unsigned pass);
void BeginTransport();
void EndTransport();
unsigned BeginEvent(unsigned kind,unsigned taskRva,unsigned phase,ID3D11DeviceContext* context=nullptr,unsigned view=UINT_MAX,unsigned shaderIndex=UINT_MAX);
void EndEvent(unsigned token);
void AddNativeStack(unsigned token);
struct NativeBatchInfo {
 unsigned descriptorRva{},tableRva{},finishRva{},flags{},laneBegin{},laneEnd{};
 uintptr_t job{},payload0{},payload1{};char name[64]{};
};
void AddNativeBatch(unsigned token,const NativeBatchInfo& info,uintptr_t commandList,bool restoreState);

void RecordNullPixelDraw(ID3D11DeviceContext* context,unsigned view,unsigned taskRva,unsigned drawKind,unsigned vertexShaderIndex);
void Stop();
void MarkCapture();
void SetPostAA(bool requested);
void RecordPostAADraw();
bool Recording();
#ifdef ACUNITYVR_NATIVE_FOVEATION
bool SetFoveationState(bool enabled,uint64_t generation);
void RecordFoveationDraw(unsigned view,unsigned pass,unsigned shader);
#ifdef ACUNITYVR_GEOMETRY_FOVEATION
#ifdef ACUNITYVR_VISIBILITY_MASK
bool SetHiddenAreaState(bool enabled,bool available,uint64_t generation);
void RecordHiddenArea(unsigned view,bool commandList);
#endif
bool SetGeometryFoveationState(bool enabled,uint64_t generation);
void RecordGeometryFoveationList(unsigned view);
#endif
#endif
#ifdef ACUNITYVR_OCCLUSION_COMPARE
bool SetOcclusionState(bool enabled,unsigned nativeDisable,uint64_t generation);
#endif
#endif
}
