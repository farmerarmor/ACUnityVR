#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>
#include "RenderSize.h"
namespace acu {
template<class T> T& Field(void* object,size_t offset){return *reinterpret_cast<T*>(static_cast<uint8_t*>(object)+offset);}
inline IUnknown* OutputInterface(void* surface){
    __try{
        auto offset=Field<void*>(surface,0x6f0)?0x700:0x8d0;
        auto object=Field<uint8_t*>(surface,offset+0x10);if(!object)return nullptr;
        auto unknown=Field<IUnknown*>(object,0x28);if(!unknown)return nullptr;
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(*reinterpret_cast<void**>(unknown),&m,sizeof(m)) || m.AllocationBase!=GetModuleHandleW(L"d3d11.dll"))return nullptr;
        return unknown;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
// Reset only the confirmed final destination between native eyes.
inline bool ClearOutput(void* surface,FILE* log){
    using Microsoft::WRL::ComPtr;
    auto unknown=OutputInterface(surface);if(!unknown)return false;
    ComPtr<ID3D11View> view;ComPtr<ID3D11Resource> resource;ComPtr<ID3D11Texture2D> texture;
    if(FAILED(unknown->QueryInterface(IID_PPV_ARGS(&view))))return false;
    view->GetResource(&resource);if(FAILED(resource.As(&texture)))return false;
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
    if(!RenderSize::Matches(d.Width,d.Height) || d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || d.SampleDesc.Count!=1 || d.ArraySize!=1 || !(d.BindFlags&D3D11_BIND_RENDER_TARGET))return false;
    ComPtr<ID3D11Device> device;texture->GetDevice(&device);
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    ComPtr<ID3D11RenderTargetView> rtv;
    if(FAILED(device->CreateRenderTargetView(texture.Get(),nullptr,&rtv)))return false;
    const float black[4]={0,0,0,0};context->ClearRenderTargetView(rtv.Get(),black);
    if(log){fprintf(log,"Cleared final destination between eyes texture=%p\n",texture.Get());fflush(log);}return true;
}
inline bool CaptureOutput(void* surface,const std::filesystem::path& path,FILE* log){
    using Microsoft::WRL::ComPtr;
    auto unknown=OutputInterface(surface);if(!unknown){fprintf(log,"No verified D3D11 output interface\n");return false;}
    ComPtr<ID3D11View> view;ComPtr<ID3D11Resource> resource;ComPtr<ID3D11Texture2D> texture;
    if(FAILED(unknown->QueryInterface(IID_PPV_ARGS(&view))))return false;
    view->GetResource(&resource);if(FAILED(resource.As(&texture)))return false;
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
    fprintf(log,"Output %ux%u format=%u samples=%u texture=%p thread=%lu\n",d.Width,d.Height,d.Format,d.SampleDesc.Count,texture.Get(),GetCurrentThreadId());
    bool rgba=d.Format==DXGI_FORMAT_R8G8B8A8_UNORM || d.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    bool bgra=d.Format==DXGI_FORMAT_B8G8R8A8_UNORM || d.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    if((!rgba && !bgra) || !RenderSize::Matches(d.Width,d.Height) || d.ArraySize!=1 || d.SampleDesc.Count!=1 || d.MipLevels!=1)return false;
    ComPtr<ID3D11Device> device;texture->GetDevice(&device);
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;if(FAILED(device->CreateTexture2D(&d,nullptr,&staging)))return false;
    context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)))return false;
    FILE* file{};if(_wfopen_s(&file,path.c_str(),L"wb")){context->Unmap(staging.Get(),0);return false;}
    BITMAPFILEHEADER fh{};BITMAPINFOHEADER bi{};
    fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(bi);fh.bfSize=fh.bfOffBits+d.Width*d.Height*4;
    bi.biSize=sizeof(bi);bi.biWidth=d.Width;bi.biHeight=-static_cast<LONG>(d.Height);bi.biPlanes=1;bi.biBitCount=32;
    fwrite(&fh,sizeof(fh),1,file);fwrite(&bi,sizeof(bi),1,file);std::vector<uint8_t> row(d.Width*4);
    for(UINT y=0;y<d.Height;++y){
        memcpy(row.data(),static_cast<uint8_t*>(map.pData)+y*map.RowPitch,row.size());
        for(UINT x=0;x<d.Width;++x){if(rgba)std::swap(row[x*4],row[x*4+2]);row[x*4+3]=255;}
        fwrite(row.data(),row.size(),1,file);
    }
    bool ok=!ferror(file);fclose(file);context->Unmap(staging.Get(),0);return ok;
}
}
