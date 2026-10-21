#pragma once
// Copies only at F8 on the existing recording context. No bindings or game writes.
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace EffectConstants72 {
using Microsoft::WRL::ComPtr;
constexpr unsigned Limit=48,Slots=6,Prefix=1536,DefaultPrefix=1024;
struct Item {std::array<ComPtr<ID3D11Buffer>,Slots> buffers;ComPtr<ID3D11Buffer> marker,markerStage;ComPtr<ID3D11Query> ready;unsigned draw{},bytes[Slots]{};uint64_t token{};bool requested{};};
struct Pool {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;std::array<Item,Limit> items;unsigned count{};uint64_t serial{};
 bool Initialize(ID3D11Device* d,ID3D11DeviceContext* c){
  if(!d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;device=d;immediate=c;
  D3D11_BUFFER_DESC staging{};staging.ByteWidth=Prefix;staging.Usage=D3D11_USAGE_STAGING;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  D3D11_QUERY_DESC event{D3D11_QUERY_EVENT,0};
  for(auto& item:items){for(auto& buffer:item.buffers)if(FAILED(d->CreateBuffer(&staging,nullptr,&buffer)))return false;
   D3D11_BUFFER_DESC marker{};marker.ByteWidth=16;marker.Usage=D3D11_USAGE_DEFAULT;uint64_t zero[2]{};D3D11_SUBRESOURCE_DATA initial{zero,0,0};
   if(FAILED(d->CreateBuffer(&marker,&initial,&item.marker)) || FAILED(d->CreateQuery(&event,&item.ready)))return false;marker.Usage=D3D11_USAGE_STAGING;marker.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(d->CreateBuffer(&marker,&initial,&item.markerStage)))return false;}
  return true;
 }
 HRESULT Completed(Item& item){
  D3D11_MAPPED_SUBRESOURCE data{};auto hr=immediate->Map(item.markerStage.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&data);if(hr!=S_OK)return hr;
  uint64_t marker[2]{};memcpy(marker,data.pData,sizeof(marker));immediate->Unmap(item.markerStage.Get(),0);if(marker[0]!=item.token || marker[1]!=~item.token)return S_FALSE;
  BOOL ready{};hr=immediate->GetData(item.ready.Get(),&ready,sizeof(ready),D3D11_ASYNC_GETDATA_DONOTFLUSH);return hr==S_OK && !ready?S_FALSE:hr;
 }
 bool Reset(){for(unsigned i=0;i<count;++i)if(items[i].requested && Completed(items[i])!=S_OK)return false;for(auto& i:items){i.requested=false;memset(i.bytes,0,sizeof(i.bytes));}count=0;return true;}
 int Capture(ID3D11DeviceContext* c,const std::array<ID3D11Buffer*,Slots>& buffers,unsigned draw,unsigned prefix=DefaultPrefix){
  if(!prefix || prefix>Prefix || (prefix&15))return -5;if(!device || !c || count==Limit)return -1;ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(owner.Get()!=device.Get())return -2;
  std::array<unsigned,Slots> sizes{};bool any{};
  for(unsigned i=0;i<Slots;++i)if(buffers[i]){D3D11_BUFFER_DESC desc{};buffers[i]->GetDesc(&desc);owner.Reset();buffers[i]->GetDevice(&owner);
   if(owner.Get()!=device.Get() || !(desc.BindFlags&D3D11_BIND_CONSTANT_BUFFER) || !desc.ByteWidth || desc.ByteWidth>65536 || (desc.ByteWidth&15) || desc.Usage==D3D11_USAGE_STAGING || desc.MiscFlags)return -3;
   sizes[i]=(std::min)(desc.ByteWidth,prefix);any=true;}
  if(!any)return -4;auto index=count++;auto& item=items[index];item.draw=draw;
  for(unsigned i=0;i<Slots;++i)if(sizes[i]){D3D11_BOX box{0,0,0,sizes[i],1,1};c->CopySubresourceRegion(item.buffers[i].Get(),0,0,0,0,buffers[i],0,&box);item.bytes[i]=sizes[i];}
  item.token=++serial;uint64_t marker[2]{item.token,~item.token};c->UpdateSubresource(item.marker.Get(),0,nullptr,marker,0,0);c->CopyResource(item.markerStage.Get(),item.marker.Get());c->End(item.ready.Get());item.requested=true;return int(index);
 }
 HRESULT Read(unsigned index,std::array<std::array<uint8_t,Prefix>,Slots>& out){
  if(index>=count || !items[index].requested)return E_INVALIDARG;auto& item=items[index];auto hr=Completed(item);if(hr!=S_OK)return hr;
  for(unsigned i=0;i<Slots;++i)if(item.bytes[i]){D3D11_MAPPED_SUBRESOURCE data{};hr=immediate->Map(item.buffers[i].Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&data);if(hr!=S_OK)return hr;memcpy(out[i].data(),data.pData,item.bytes[i]);immediate->Unmap(item.buffers[i].Get(),0);}
  return S_OK;
 }
};
}
