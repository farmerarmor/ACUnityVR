#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace MaterialIndirect75 {
inline bool Valid(ID3D11Buffer* buffer,ID3D11Device* device,unsigned offset){
 if(!buffer || !device || (offset&3u))return false;
 Microsoft::WRL::ComPtr<ID3D11Device> owner;buffer->GetDevice(&owner);if(owner.Get()!=device)return false;
 D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
 return (desc.MiscFlags&D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) && offset<=desc.ByteWidth && sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS)<=desc.ByteWidth-offset;
}
}
