#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdio>
namespace CloudProjection {
bool Install(uint8_t*,ID3D11Device*,ID3D11DeviceContext*,FILE*);
void SetEyes(void*,void*);
void BeginFrame(uint64_t,uint64_t,uint32_t);
void Restore();
// A private CB8 is bound only for original CS146; restore the native binding
// before returning to the engine's unchanged cached state.
struct Scope {
 Scope(ID3D11DeviceContext*,UINT,UINT,UINT);
 ~Scope();
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
 ID3D11DeviceContext* context{};
 Microsoft::WRL::ComPtr<ID3D11Buffer> original;
 bool active{};
};
}
