#pragma once
// F8-only bounded GPU readback. The caller serializes access. The game device
// allocates this pool at initialization; recording only appends copies + events.
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <cstdint>
namespace GeometryConstants46 {
using Microsoft::WRL::ComPtr;
constexpr unsigned Limit=48,PassBytes=608,MiscBytes=560;
struct Item {
 ComPtr<ID3D11Buffer> pass,misc;
 ComPtr<ID3D11Query> ready;ComPtr<ID3D11Buffer> marker,markerStage;uint64_t token{};
 unsigned dispatch{},sourceBytes[2]{};bool requested{};
};
struct Pool {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;
 std::array<Item,Limit> items{};unsigned count{};uint64_t tokenSerial{};
 bool Initialize(ID3D11Device* d,ID3D11DeviceContext* c){
  if(!d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
  device=d;immediate=c;
  D3D11_BUFFER_DESC b{};b.Usage=D3D11_USAGE_STAGING;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};
  for(auto& i:items){b.ByteWidth=PassBytes;if(FAILED(d->CreateBuffer(&b,nullptr,&i.pass)))return false;
   b.ByteWidth=MiscBytes;if(FAILED(d->CreateBuffer(&b,nullptr,&i.misc)) || FAILED(d->CreateQuery(&q,&i.ready)))return false;
   D3D11_BUFFER_DESC marker{};marker.ByteWidth=16;marker.Usage=D3D11_USAGE_DEFAULT;
   const uint64_t zero[2]{};D3D11_SUBRESOURCE_DATA initial{zero,0,0};
   if(FAILED(d->CreateBuffer(&marker,&initial,&i.marker)))return false;
   marker.Usage=D3D11_USAGE_STAGING;marker.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   if(FAILED(d->CreateBuffer(&marker,&initial,&i.markerStage)))return false;}

  return true;
 }
 HRESULT Completed(Item& i){
  // Verify the GPU marker first: GetData on an event not yet executed can fail
  // INVALID_CALL. Staging buffers start at zero, distinct from all tokens.
  D3D11_MAPPED_SUBRESOURCE mapped{};auto hr=immediate->Map(i.markerStage.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
  if(hr!=S_OK)return hr;uint64_t token[2]{};memcpy(token,mapped.pData,sizeof(token));immediate->Unmap(i.markerStage.Get(),0);
  if(token[0]!=i.token || token[1]!=~i.token)return S_FALSE;
  BOOL done{};hr=immediate->GetData(i.ready.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  return hr==S_OK && !done?S_FALSE:hr;
 }
 bool Reset(){
  // The unique GPU marker proves this generation executed before polling its event.
  for(unsigned n=0;n<count;++n)if(items[n].requested && Completed(items[n])!=S_OK)return false;
  for(auto& i:items)i.requested=false;count=0;return true;
 }
 bool Valid(ID3D11Buffer* b,unsigned needed,D3D11_BUFFER_DESC* desc){
  if(!b)return false;b->GetDesc(desc);ComPtr<ID3D11Device> owner;b->GetDevice(&owner);
  return owner.Get()==device.Get() && (desc->BindFlags&D3D11_BIND_CONSTANT_BUFFER) &&
   desc->ByteWidth>=needed && desc->ByteWidth<=65536 && !(desc->ByteWidth&15) &&
   desc->Usage!=D3D11_USAGE_STAGING && !desc->MiscFlags;
 }
 int Capture(ID3D11DeviceContext* c,ID3D11Buffer* pass,ID3D11Buffer* misc,unsigned dispatch){
  if(!device || !c || count>=Limit)return -1;
  D3D11_BUFFER_DESC a{},b{};if(!Valid(pass,PassBytes,&a) || !Valid(misc,MiscBytes,&b))return -2;
  ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(owner.Get()!=device.Get())return -3;
  unsigned index=count++;auto& i=items[index];i.dispatch=dispatch;i.sourceBytes[0]=a.ByteWidth;i.sourceBytes[1]=b.ByteWidth;
  // Buffers use byte coordinates. No game resource/binding is changed.
  D3D11_BOX passBox{0,0,0,PassBytes,1,1},miscBox{0,0,0,MiscBytes,1,1};
  c->CopySubresourceRegion(i.pass.Get(),0,0,0,0,pass,0,&passBox);
  c->CopySubresourceRegion(i.misc.Get(),0,0,0,0,misc,0,&miscBox);
  i.token=++tokenSerial;const uint64_t marker[2]{i.token,~i.token};
  c->UpdateSubresource(i.marker.Get(),0,nullptr,marker,0,0);c->CopyResource(i.markerStage.Get(),i.marker.Get());
  c->End(i.ready.Get());i.requested=true;return int(index);
 }
 HRESULT Read(unsigned index,std::array<unsigned char,PassBytes>* pass,std::array<unsigned char,MiscBytes>* misc){
  if(index>=count || !items[index].requested)return E_INVALIDARG;auto& i=items[index];
  auto hr=Completed(i);if(hr!=S_OK)return hr;
  D3D11_MAPPED_SUBRESOURCE a{},b{};
  hr=immediate->Map(i.pass.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&a);if(hr!=S_OK)return hr;
  hr=immediate->Map(i.misc.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&b);
  if(hr==S_OK){memcpy(pass->data(),a.pData,PassBytes);memcpy(misc->data(),b.pData,MiscBytes);immediate->Unmap(i.misc.Get(),0);}
  immediate->Unmap(i.pass.Get(),0);return hr;
 }
};
}
