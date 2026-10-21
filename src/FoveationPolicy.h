#pragma once
#include <d3d11.h>
namespace FoveationPolicy {
inline bool ColourFormat(DXGI_FORMAT f){return f==DXGI_FORMAT_R16G16B16A16_FLOAT || f==DXGI_FORMAT_R8G8B8A8_UNORM || f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;}
inline bool SingleFullView(const D3D11_RENDER_TARGET_VIEW_DESC& v){
 if(!ColourFormat(v.Format))return false;
 switch(v.ViewDimension){
 case D3D11_RTV_DIMENSION_TEXTURE2D:return v.Texture2D.MipSlice==0;
 case D3D11_RTV_DIMENSION_TEXTURE2DMS:return true;
 case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:return v.Texture2DArray.MipSlice==0 && v.Texture2DArray.FirstArraySlice==0 && v.Texture2DArray.ArraySize==1;
 case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:return v.Texture2DMSArray.FirstArraySlice==0 && v.Texture2DMSArray.ArraySize==1;
 default:return false;
 }
}
inline unsigned EffectiveWrites(bool bound,unsigned flags,const D3D11_DEPTH_STENCIL_DESC& s){
 if(!bound)return 0;
 unsigned result=0;
 if(!(flags&D3D11_DSV_READ_ONLY_DEPTH) && s.DepthEnable && s.DepthWriteMask!=D3D11_DEPTH_WRITE_MASK_ZERO)result|=16;
 auto writes=[](const D3D11_DEPTH_STENCILOP_DESC& op){return op.StencilFailOp!=D3D11_STENCIL_OP_KEEP || op.StencilDepthFailOp!=D3D11_STENCIL_OP_KEEP || op.StencilPassOp!=D3D11_STENCIL_OP_KEEP;};
 if(!(flags&D3D11_DSV_READ_ONLY_STENCIL) && s.StencilEnable && s.StencilWriteMask!=0 && (writes(s.FrontFace) || writes(s.BackFace)))result|=32;
 return result;
}
}
