#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <share.h>
namespace TerminalCapture64 {
using Microsoft::WRL::ComPtr;
enum class Status:unsigned {Copied=0,NoTexture=1,Shape=2,Format=3,Duplicate=4,CountLimit=5,Budget=6,CreateFailed=7};
inline unsigned BytesPerPixel(DXGI_FORMAT f){switch(f){case DXGI_FORMAT_R8G8B8A8_TYPELESS:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_TYPELESS:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return 4;case DXGI_FORMAT_R16G16B16A16_TYPELESS:case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_UNORM:return 8;default:return 0;}}
struct Item {ComPtr<ID3D11Texture2D> staging;D3D11_TEXTURE2D_DESC desc{};uintptr_t original{};unsigned view{},draw{},slot{},kind{},pixelBytes{};};
struct Pool {
 static constexpr size_t BudgetBytes=512ull*1024*1024;
 std::vector<Item> items;std::array<std::vector<uintptr_t>,3> inputs;bool targetSeen[3]{};size_t bytes{};unsigned copied{},skipped{},errors{};
 void Reset(){items.clear();for(auto& x:inputs)x.clear();for(auto& x:targetSeen)x=false;bytes=0;copied=skipped=errors=0;}
 Status Stage(ID3D11DeviceContext* context,ID3D11Resource* resource,unsigned view,unsigned draw,unsigned slot,unsigned kind){
  if(!context || !resource || view>=3)return Status::NoTexture;
  ComPtr<ID3D11Texture2D> texture;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture))))return Status::NoTexture;
  D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
  if(d.Width<512 || d.Height<512 || d.Width>8192 || d.Height>8192 || d.MipLevels!=1 || d.ArraySize!=1 || d.SampleDesc.Count!=1)return Status::Shape;
  auto pixelBytes=BytesPerPixel(d.Format);if(!pixelBytes)return Status::Format;
  auto identity=reinterpret_cast<uintptr_t>(texture.Get());auto& seen=inputs[view];
  if(kind==0){for(auto x:seen)if(x==identity)return Status::Duplicate;if(seen.size()>=2)return Status::CountLimit;}else if(targetSeen[view])return Status::Duplicate;
  size_t size=size_t(d.Width)*d.Height*pixelBytes;if(size>BudgetBytes || bytes>BudgetBytes-size)return Status::Budget;
  auto staging=d;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.MiscFlags=0;
  Item item{};ComPtr<ID3D11Device> device;context->GetDevice(&device);if(!device || FAILED(device->CreateTexture2D(&staging,nullptr,&item.staging))){++errors;return Status::CreateFailed;}
  item.desc=d;item.original=identity;item.view=view;item.draw=draw;item.slot=slot;item.kind=kind;item.pixelBytes=pixelBytes;
  context->CopyResource(item.staging.Get(),texture.Get());items.push_back(std::move(item));bytes+=size;++copied;if(kind==0)seen.push_back(identity);else targetSeen[view]=true;return Status::Copied;
 }
 Status Input(ID3D11DeviceContext* context,unsigned view,unsigned draw,unsigned slot){
  if(!context)return Status::NoTexture;ComPtr<ID3D11ShaderResourceView> srv;context->PSGetShaderResources(slot,1,&srv);if(!srv)return Status::NoTexture;
  D3D11_SHADER_RESOURCE_VIEW_DESC sd{};srv->GetDesc(&sd);if(sd.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || sd.Texture2D.MostDetailedMip!=0)return Status::Shape;
  ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);return Stage(context,resource.Get(),view,draw,slot,0);
 }
 Status Target(ID3D11DeviceContext* context,unsigned view,unsigned draw){
  if(!context)return Status::NoTexture;ComPtr<ID3D11RenderTargetView> rtv;context->OMGetRenderTargets(1,&rtv,nullptr);if(!rtv)return Status::NoTexture;ComPtr<ID3D11Resource> resource;rtv->GetResource(&resource);return Stage(context,resource.Get(),view,draw,0,1);
 }
 void Dump(ID3D11DeviceContext* context,const std::filesystem::path& directory,FILE* events){
  for(auto& x:items){D3D11_MAPPED_SUBRESOURCE mapped{};HRESULT hr=context->Map(x.staging.Get(),0,D3D11_MAP_READ,0,&mapped);bool valid=SUCCEEDED(hr) && mapped.pData && mapped.RowPitch>=x.desc.Width*x.pixelBytes;bool written=false;
   auto name=L"terminal64-view-"+std::to_wstring(x.view)+L"-draw-"+std::to_wstring(x.draw)+(x.kind?L"-before-ui":L"-input-"+std::to_wstring(x.slot))+L".bin";
   if(valid){FILE* f=_wfsopen((directory/name).c_str(),L"wb",_SH_DENYNO);if(f){written=true;for(unsigned y=0;y<x.desc.Height;++y)if(fwrite(static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,x.pixelBytes,x.desc.Width,f)!=x.desc.Width){written=false;break;}fclose(f);}}
   if(SUCCEEDED(hr))context->Unmap(x.staging.Get(),0);if(!written)++errors;
   if(events)fprintf(events,"{\"event\":\"terminal64Pixels\",\"view\":%u,\"draw\":%u,\"slot\":%u,\"kind\":%u,\"resource\":\"%llx\",\"width\":%u,\"height\":%u,\"format\":%u,\"pixelBytes\":%u,\"packedRows\":true,\"written\":%s,\"file\":\"%ls\"}\n",x.view,x.draw,x.slot,x.kind,static_cast<unsigned long long>(x.original),x.desc.Width,x.desc.Height,unsigned(x.desc.Format),x.pixelBytes,written?"true":"false",name.c_str());
  }items.clear();
 }
};
}
