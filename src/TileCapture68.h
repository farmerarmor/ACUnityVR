#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <filesystem>
#include <array>
#include <cstdint>
#include <vector>
namespace TileCapture68 {
using Microsoft::WRL::ComPtr;
struct Item {ComPtr<ID3D11Texture2D> staging;unsigned view{},draw{};uintptr_t resource{};D3D11_TEXTURE2D_DESC desc{};};
struct Pool {
 std::vector<Item> items;bool copied[3]{};unsigned errors{};
 bool Input(ID3D11DeviceContext* c,unsigned view,unsigned draw,unsigned columns,unsigned rows){
  if(!c || view>=3 || copied[view])return false;ComPtr<ID3D11ShaderResourceView> srv;c->GSGetShaderResources(0,1,&srv);if(!srv)return false;
  D3D11_SHADER_RESOURCE_VIEW_DESC sd{};srv->GetDesc(&sd);if(sd.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || sd.Format!=DXGI_FORMAT_R32_UINT || sd.Texture2D.MostDetailedMip!=0)return false;
  ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return false;D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
  if(desc.Width!=columns || desc.Height!=rows || desc.Format!=DXGI_FORMAT_R32_UINT || desc.ArraySize!=1 || desc.MipLevels!=1 || desc.SampleDesc.Count!=1 || size_t(columns)*rows*4>4*1024*1024)return false;
  auto staging=desc;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.MiscFlags=0;ComPtr<ID3D11Device> device;c->GetDevice(&device);
  Item item{};item.view=view;item.draw=draw;item.resource=reinterpret_cast<uintptr_t>(resource.Get());item.desc=desc;
  if(FAILED(device->CreateTexture2D(&staging,nullptr,&item.staging))){++errors;return false;}c->CopyResource(item.staging.Get(),texture.Get());items.push_back(std::move(item));copied[view]=true;return true;
 }
 void Reset(){items.clear();for(auto& x:copied)x=false;errors=0;}
 void Dump(ID3D11DeviceContext* c,const std::filesystem::path& dir,FILE* events){
  for(auto& x:items){D3D11_MAPPED_SUBRESOURCE m{};bool written=false;auto name=L"classification68-view-"+std::to_wstring(x.view)+L"-draw-"+std::to_wstring(x.draw)+L".r32ui";
   if(SUCCEEDED(c->Map(x.staging.Get(),0,D3D11_MAP_READ,0,&m))){FILE* f{};if(m.RowPitch>=x.desc.Width*4 && _wfopen_s(&f,(dir/name).c_str(),L"wb")==0 && f){written=true;for(UINT y=0;y<x.desc.Height;++y)if(fwrite(static_cast<uint8_t*>(m.pData)+size_t(y)*m.RowPitch,4,x.desc.Width,f)!=x.desc.Width){written=false;break;}fclose(f);}c->Unmap(x.staging.Get(),0);}if(!written)++errors;
   if(events)fprintf(events,"{\"event\":\"classification68Pixels\",\"view\":%u,\"draw\":%u,\"resource\":\"%llx\",\"width\":%u,\"height\":%u,\"format\":%u,\"written\":%s,\"file\":\"%ls\"}\n",x.view,x.draw,static_cast<unsigned long long>(x.resource),x.desc.Width,x.desc.Height,unsigned(x.desc.Format),written?"true":"false",name.c_str());
  }items.clear();
 }
};
}
