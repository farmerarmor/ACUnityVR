#pragma once
// F8-only copies of the PS8 resource explicitly set on known game contexts.
// Deferred contexts have no supported Get-state. This records the setter's
// declared resource, not proof against subsequent automatic hazard unbinding.
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
namespace SmokeDepth75 {
using Microsoft::WRL::ComPtr;
constexpr unsigned Limit=4,ContextLimit=65;constexpr uint64_t PixelLimit=32ull*1024*1024;
struct Binding {ID3D11DeviceContext* context{};ComPtr<ID3D11ShaderResourceView> view;bool known{};};
struct Mirror {
 std::array<Binding,ContextLimit> bindings;unsigned count{};
 bool Add(ID3D11DeviceContext* c){if(!c || count==ContextLimit)return false;for(unsigned i=0;i<count;++i)if(bindings[i].context==c)return true;bindings[count++].context=c;return true;}
 void Reset(){for(unsigned i=0;i<count;++i){bindings[i].known=false;bindings[i].view.Reset();}}
 bool Set(ID3D11DeviceContext* c,UINT start,UINT n,ID3D11ShaderResourceView*const* views){
  if(start>=128 || n>128-start || !n || !views || start>8 || n<=8-start)return false;
  for(unsigned i=0;i<count;++i)if(bindings[i].context==c){bindings[i].view=views[8-start];bindings[i].known=true;return true;}return false;
 }
 Binding* Find(ID3D11DeviceContext* c){for(unsigned i=0;i<count;++i)if(bindings[i].context==c)return &bindings[i];return nullptr;}
};
inline bool Describe(ID3D11ShaderResourceView* srv,ID3D11Device* expected,ComPtr<ID3D11Texture2D>& texture,D3D11_TEXTURE2D_DESC& desc){
 texture.Reset();desc={};if(!srv || !expected)return false;ComPtr<ID3D11Device> owner;srv->GetDevice(&owner);if(owner.Get()!=expected)return false;
 D3D11_SHADER_RESOURCE_VIEW_DESC view{};srv->GetDesc(&view);
 if(view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || view.Format!=DXGI_FORMAT_R32_FLOAT || view.Texture2D.MostDetailedMip || view.Texture2D.MipLevels!=1)return false;
 ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);if(FAILED(resource.As(&texture)))return false;owner.Reset();texture->GetDevice(&owner);if(owner.Get()!=expected)return false;texture->GetDesc(&desc);
 return desc.Width && desc.Height && desc.Width<=16384 && desc.Height<=16384 && uint64_t(desc.Width)*desc.Height<=PixelLimit && desc.MipLevels==1 && desc.ArraySize==1 && desc.SampleDesc.Count==1 && (desc.Format==DXGI_FORMAT_R32_FLOAT || desc.Format==DXGI_FORMAT_R32_TYPELESS) && desc.Usage!=D3D11_USAGE_STAGING;
}
struct Item {ComPtr<ID3D11Texture2D> staging;ComPtr<ID3D11Buffer> marker,markerStage;ComPtr<ID3D11Query> ready;D3D11_TEXTURE2D_DESC desc{};uint64_t token{},view{},source{};unsigned draw{},eye{},group{};bool requested{};};
struct Pool {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;std::array<Item,Limit> items;unsigned count{},selected[2][2]{},unknown{},nullBindings{},refused{},pending{};uint64_t serial{};
 bool Initialize(ID3D11Device* d,ID3D11DeviceContext* c){
  if(!d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;ComPtr<ID3D11Device> owner;c->GetDevice(&owner);if(owner.Get()!=d)return false;device=d;immediate=c;
  for(auto& item:items){D3D11_BUFFER_DESC b{};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;uint64_t zero[2]{};D3D11_SUBRESOURCE_DATA data{zero,0,0};
   D3D11_QUERY_DESC event{D3D11_QUERY_EVENT,0};if(FAILED(d->CreateBuffer(&b,&data,&item.marker)) || FAILED(d->CreateQuery(&event,&item.ready)))return false;b.Usage=D3D11_USAGE_STAGING;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(d->CreateBuffer(&b,&data,&item.markerStage)))return false;}
  return true;
 }
 HRESULT Complete(Item& item){
  D3D11_MAPPED_SUBRESOURCE m{};auto hr=immediate->Map(item.markerStage.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&m);if(hr!=S_OK)return hr;
  uint64_t token[2]{};memcpy(token,m.pData,sizeof(token));immediate->Unmap(item.markerStage.Get(),0);if(token[0]!=item.token || token[1]!=~item.token)return S_FALSE;
  BOOL done{};hr=immediate->GetData(item.ready.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);return hr==S_OK && !done?S_FALSE:hr;
 }
 bool Reset(){for(unsigned i=0;i<count;++i)if(items[i].requested && Complete(items[i])!=S_OK)return false;for(auto& i:items)i.requested=false;count=unknown=nullBindings=refused=pending=0;memset(selected,0,sizeof(selected));return true;}
 bool Wants(unsigned eye,unsigned group)const {return eye>=1 && eye<=2 && (group==2 || group==5) && !selected[eye-1][group==5?1:0] && count<Limit;}
 int Capture(ID3D11DeviceContext* c,const Binding* binding,unsigned draw,unsigned eye,unsigned group){
  if(!Wants(eye,group))return -1;
  if(!binding || binding->context!=c || !binding->known){++unknown;return -2;}if(!binding->view){++nullBindings;return -3;}
  ComPtr<ID3D11Device> owner;c->GetDevice(&owner);ComPtr<ID3D11Texture2D> texture;D3D11_TEXTURE2D_DESC source{};
  if(owner.Get()!=device.Get() || !Describe(binding->view.Get(),device.Get(),texture,source)){++refused;return -4;}
  auto& item=items[count];if(!item.staging || item.desc.Width!=source.Width || item.desc.Height!=source.Height || item.desc.Format!=source.Format){
   auto staging=source;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.MiscFlags=0;ComPtr<ID3D11Texture2D> made;
   if(FAILED(device->CreateTexture2D(&staging,nullptr,&made))){++refused;return -5;}item.staging=made;
  }
  item.desc=source;item.draw=draw;item.eye=eye;item.group=group;item.view=reinterpret_cast<uintptr_t>(binding->view.Get());item.source=reinterpret_cast<uintptr_t>(texture.Get());
  c->CopyResource(item.staging.Get(),texture.Get());item.token=++serial;uint64_t token[2]{item.token,~item.token};c->UpdateSubresource(item.marker.Get(),0,nullptr,token,0,0);c->CopyResource(item.markerStage.Get(),item.marker.Get());c->End(item.ready.Get());item.requested=true;
  selected[eye-1][group==5?1:0]=1;return int(count++);
 }
 HRESULT Read(unsigned index,std::vector<uint8_t>& out){
  out.clear();if(index>=count || !items[index].requested)return E_INVALIDARG;auto& item=items[index];auto hr=Complete(item);if(hr!=S_OK)return hr;
  D3D11_MAPPED_SUBRESOURCE m{};hr=immediate->Map(item.staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&m);if(hr!=S_OK)return hr;
  const auto pitch=item.desc.Width*4u;if(m.RowPitch<pitch){immediate->Unmap(item.staging.Get(),0);return E_FAIL;}
  out.resize(size_t(pitch)*item.desc.Height);for(unsigned row=0;row<item.desc.Height;++row)memcpy(out.data()+size_t(row)*pitch,static_cast<const uint8_t*>(m.pData)+size_t(row)*m.RowPitch,pitch);immediate->Unmap(item.staging.Get(),0);return S_OK;
 }
};
}
