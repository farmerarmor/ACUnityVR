#pragma once
// F8-only copy of the game's existing single-sample quarter-depth hierarchy.
// Uses the game device/immediate context. No shaders, bindings or game data change.
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace GpuSceneDepth46 {
using Microsoft::WRL::ComPtr;
struct Item {
 ComPtr<ID3D11Texture2D> staging;
 ComPtr<ID3D11Buffer> marker,markerStage;ComPtr<ID3D11Query> event;
 D3D11_TEXTURE2D_DESC desc{};uint64_t token{},resource{};bool requested{},saved{};
};
struct Pool {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;std::array<Item,2> items;uint64_t serial{};
 bool Initialize(ID3D11Device* d,ID3D11DeviceContext* c){
  if(!d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
  ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(owner.Get()!=d)return false;
  device=d;immediate=c;D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};
  for(auto& i:items){D3D11_BUFFER_DESC b{};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;
   const uint64_t zero[2]{};D3D11_SUBRESOURCE_DATA data{zero,0,0};
   if(FAILED(d->CreateBuffer(&b,&data,&i.marker)))return false;
   b.Usage=D3D11_USAGE_STAGING;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   if(FAILED(d->CreateBuffer(&b,&data,&i.markerStage)) || FAILED(d->CreateQuery(&q,&i.event)))return false;}
  return true;
 }
 HRESULT Completed(Item& i){
  D3D11_MAPPED_SUBRESOURCE map{};auto hr=immediate->Map(i.markerStage.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
  if(hr!=S_OK)return hr;uint64_t words[2]{};memcpy(words,map.pData,16);immediate->Unmap(i.markerStage.Get(),0);
  if(words[0]!=i.token || words[1]!=~i.token)return S_FALSE;
  BOOL done{};hr=immediate->GetData(i.event.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  return hr==S_OK && !done?S_FALSE:hr;
 }
 bool Reset(){
  if(!device)return false;
  for(auto& i:items)if(i.requested && Completed(i)!=S_OK)return false;
  for(auto& i:items){i.requested=false;i.saved=false;}return true;
 }
 HRESULT Capture(ID3D11DeviceContext* c,ID3D11Texture2D* source,unsigned eye,unsigned fullWidth,unsigned fullHeight){
  if(!device || c!=immediate.Get() || !source || eye>=2)return E_INVALIDARG;
  auto& i=items[eye];if(i.requested)return S_FALSE;
  D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);ComPtr<ID3D11Device> owner;source->GetDevice(&owner);
  if(owner.Get()!=device.Get() || !fullWidth || !fullHeight || fullWidth>16384 || fullHeight>8192 ||
   d.Format!=DXGI_FORMAT_R32G32_FLOAT || d.SampleDesc.Count!=1 || d.ArraySize!=1 || d.MipLevels!=1 ||
   d.Width!=(fullWidth+3)/4 || d.Height!=(fullHeight+3)/4 || uint64_t(d.Width)*d.Height*8>16*1024*1024 ||
   !(d.BindFlags&D3D11_BIND_SHADER_RESOURCE) || d.MiscFlags)return E_INVALIDARG;
  auto stage=d;stage.Usage=D3D11_USAGE_STAGING;stage.BindFlags=0;stage.CPUAccessFlags=D3D11_CPU_ACCESS_READ;stage.MiscFlags=0;
  ComPtr<ID3D11Texture2D> texture;auto hr=device->CreateTexture2D(&stage,nullptr,&texture);if(FAILED(hr))return hr;
  i.staging=texture;i.desc=d;i.resource=reinterpret_cast<uint64_t>(source);i.token=++serial;
  c->CopyResource(i.staging.Get(),source);
  const uint64_t marker[2]{i.token,~i.token};c->UpdateSubresource(i.marker.Get(),0,nullptr,marker,0,0);
  c->CopyResource(i.markerStage.Get(),i.marker.Get());c->End(i.event.Get());i.requested=true;return S_OK;
 }
 HRESULT Map(unsigned eye,D3D11_MAPPED_SUBRESOURCE* map){
  if(eye>=2 || !map || !items[eye].requested)return E_INVALIDARG;auto& i=items[eye];auto hr=Completed(i);if(hr!=S_OK)return hr;
  return immediate->Map(i.staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,map);
 }
 void Unmap(unsigned eye){immediate->Unmap(items[eye].staging.Get(),0);}
};
}
