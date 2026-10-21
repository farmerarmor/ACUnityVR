// Exact AA-off native grid overflows its 16-bit indices at large eye sizes.
// The native vertex grid/shaders/count stay intact; only an owned 32-bit IB is
// bound for the five classified lighting draws, then the original IB restores.
#include "LightingGrid67.h"
#include <windows.h>
#include <cmath>
#include <cstring>
#include <limits>
using Microsoft::WRL::ComPtr;
namespace LightingGrid67 {
bool BuildIndices(UINT columns,UINT rows,std::vector<uint32_t>* out){
 if(!out || !columns || !rows || columns>1024 || rows>1024 || uint64_t(columns+1)*(rows+1)>1048576)return false;
 out->clear();out->reserve(size_t(columns)*rows*6);const UINT stride=columns+1;
 for(UINT y=0;y<rows;++y)for(UINT x=0;x<columns;++x){UINT v=y*stride+x;for(UINT i:{v,v+1,v+stride+1,v+stride+1,v+stride,v})out->push_back(i);}
 return true;
}
bool ValidateIndices(const void* data,size_t bytes,const std::vector<uint32_t>& corrected){
 if(!data || bytes!=corrected.size()*sizeof(uint16_t))return false;auto p=static_cast<const uint16_t*>(data);
 for(size_t i=0;i<corrected.size();++i)if(p[i]!=uint16_t(corrected[i]))return false;return true;
}
bool ValidateVertices(const void* data,size_t bytes,UINT columns,UINT rows){
 if(!data || !columns || !rows || bytes!=size_t(columns+1)*(rows+1)*8)return false;auto p=static_cast<const float*>(data);
 for(UINT y=0;y<=rows;++y)for(UINT x=0;x<=columns;++x){auto i=size_t(y)*(columns+1)+x;float ex=2.f*float(x)/float(columns)-1.f,ey=1.f-2.f*float(y)/float(rows);
  if(!std::isfinite(p[i*2]) || !std::isfinite(p[i*2+1]) || std::abs(p[i*2]-ex)>0.0001f || std::abs(p[i*2+1]-ey)>0.0001f)return false;}
 return true;
}
bool ReadBuffer(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Buffer* source,std::vector<uint8_t>* bytes){
 D3D11_BUFFER_DESC d{};source->GetDesc(&d);if(!d.ByteWidth || d.ByteWidth>4*1024*1024)return false;
 auto desc=d;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;desc.StructureByteStride=0;
 ComPtr<ID3D11Buffer> staging;if(FAILED(device->CreateBuffer(&desc,nullptr,&staging)))return false;context->CopyResource(staging.Get(),source);
 D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
 bytes->assign(static_cast<uint8_t*>(mapped.pData),static_cast<uint8_t*>(mapped.pData)+d.ByteWidth);context->Unmap(staging.Get(),0);return true;
}
bool NativeGuards(uint8_t* image){
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={
 {0x22de47f,"\x48\x89\x85\x98\x1b\x00\x00\x4c\x8b\x05\x43\xa1\xf7\x02\x41\xff\xc4\xff\xc6\x41\x8b\xfc\x83\xca\xff\x0f\xaf\xfe\x44\x8d\x2c\xfd\x00\x00\x00\x00",36},
 {0x22de652,"\x8b\x85\x90\x1b\x00\x00\x83\xca\xff\x44\x8d\x24\x40\x45\x03\xe4\x0f\xaf\x85\x94\x1b\x00\x00\x8d\x04\x40\x8d\x0c\x00\x89\x8d\xb8\x1b\x00\x00\x4c\x8b\x05\x54\x9f\xf7\x02\x8d\x0c\x4d\x04\x00\x00\x00\xe8\xd8\x50\x41",53},
 {0x22de6bc,"\x44\x0f\xb7\xd6\x44\x8d\x5e\x01\x45\x8b\xce\x66\x44\x0f\xaf\xd3\x0f\x1f\x40\x00\x41\x8b\xd1\x43\x8d\x0c\x10\x41\xff\xc0\x8d\x41\x01\x66\x89\x0c\x57\x66\x89\x4c\x57\x0a\x66\x89\x44\x57\x02\x42\x8d\x04\x19\x41\x83\xc1\x06\x66\x89\x44\x57\x04\x66\x89\x44\x57\x06\x8d\x04\x31\x66\x89\x44\x57\x08\x44\x3b\x85\x90\x1b\x00\x00\x72\xc2\xff",83},
 {0x22de78b,"\x44\x8b\x85\xb8\x1b\x00\x00\x45\x33\xc9\x49\x8b\xd5\x48\x8b\xc8\x45\x03\xc0\x48\x89\x7c\x24\x28\x44\x89\x7c\x24\x20\xe8\x13\x0d\x01\x00",34},
 {0x22c00d5,"\x48\x8b\x8e\xa0\x1b\x00\x00\x33\xd2\xe8\x7d\xb4\xed\xff\x4c\x8b\x86\xa8\x1b\x00\x00\x0f\xb6\xc0",24},
 {0x22c010d,"\x48\x8b\x8e\xb0\x1b\x00\x00\x48\x85\xc9\x74\x2f\x48\x89\x5c\x24\x70\x48\x8b\x59\x28\xe8\x09\xf9\x02\x00\x48\x8b\xc8\xe8\x61\x81\xe6\xfd\x48\x8b\x4f\x28\x45\x33\xc9\x44\x8b\xc0\x48\x8b\xd3\xe8\x6f\x6b\x03\x00\x48\x8b\x5c\x24\x70\xeb\x11\x48\x8b\x4f\x28\x45",64},
 {0x22c0159,"\xba\x39\x00\x00\x00\x48\x8b\xcf\xe8\x1a\x70\x03\x00\x41\x8b\xd6\x48\x8b\xcf\xe8\x1f\x6f\x03\x00\x8b\x8e\xb8\x1b\x00\x00\x44\x89\x7c\x24\x40\xc7\x44\x24\x38\x01\x00\x00\x00\xb8\xab\xaa\xaa\xaa\x45\x33\xc9\x45\x33\xc0\xf7\xe1\xd1\xea\x89\x54\x24\x30\x44\x89\x7c\x24\x28\x89\x4c\x24\x20\x48\x8b\xcf\xb2\x03\xe8\xb6\x74\x03\x00",81},
 {0x33f0180,"\x44\x58\x42\x43\x41\x5c\x81\x86\xf0\x0b\xfe\xed\x73\x63\xe9\x85\x45\x6f\x09\x68\x01\x00\x00\x00\x18\x02\x00\x00\x05\x00\x00\x00",32},
 {0x31f5200,"\x44\x58\x42\x43\x93\xa2\x0a\x7d\x4a\x1e\x85\xf6\xeb\x34\x42\x2a\x16\x2e\x72\x99\x01\x00\x00\x00\xec\x68\x00\x00\x05\x00\x00\x00",32},
 {0x31ff300,"\x44\x58\x42\x43\x02\xa7\x91\x06\x56\x1e\xc5\x7e\x31\x3c\x80\xaf\x0f\x3b\x18\x43\x01\x00\x00\x00\xc4\x71\x00\x00\x05\x00\x00\x00",32},
 {0x32096b0,"\x44\x58\x42\x43\xa4\x35\xdc\xff\x54\xf4\x48\xae\xde\xc0\xac\x44\x3a\x4f\x49\x3c\x01\x00\x00\x00\xc8\x77\x00\x00\x05\x00\x00\x00",32},
 {0x3211f50,"\x44\x58\x42\x43\x7b\x42\x7f\x4c\x1e\xb4\xbd\x19\xb2\x13\x8e\xc1\xd3\x64\x48\xe8\x01\x00\x00\x00\xe8\x4a\x00\x00\x05\x00\x00\x00",32},
 {0x32184f0,"\x44\x58\x42\x43\x59\xd8\xce\xe0\x63\x4f\x38\x93\xfe\x30\xc2\x8b\x69\xe1\x08\xee\x01\x00\x00\x00\x88\xa1\x00\x00\x05\x00\x00\x00",32},
 };
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;return true;
}
bool Pool::Initialize(uint8_t* image,ID3D11Device* d,ID3D11DeviceContext* c,UINT w,UINT h,const std::filesystem::path& output,FILE* log){
 if(!image || !d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || !NativeGuards(image) || !w || !h || w>16384 || h>16384)return false;
 device=d;immediate=c;report=log;width=w;height=h;columns=(w+15)/16;rows=(h+15)/16;vertices=(columns+1)*(rows+1);indices=columns*rows*6;
 // Refuse non-integral cells rather than infer the game's padded grid layout.
 installed=true;apply=(w%16==0 && h%16==0 && columns<=1024 && rows<=1024 && vertices<=1048576);
 auto ini=output/L"ACUnityVR.ini";apply=apply && GetPrivateProfileIntW(L"Lighting",L"FixAAOffGrid",1,ini.c_str())!=0;
 if(report){fprintf(report,"LightingGrid67 installed size=%ux%u grid=%ux%u vertices=%u indices=%u enabled=%d; native16 generator and consumer guarded; F1 toggles; no native mesh mutation\n",w,h,columns,rows,vertices,indices,apply);fflush(report);}return true;
}
void Pool::BeginFrame(uint64_t id,uint32_t m){sequence=id;mode=m;bool down=(GetAsyncKeyState(VK_F1)&0x8000)!=0;if(down&&!f1Held){apply=!apply;if(report){fprintf(report,"LightingGrid67 correction %s sequence=%llu\n",apply?"ON":"OFF",sequence);fflush(report);}}f1Held=down;}
bool Pool::Begin(ID3D11DeviceContext* c,unsigned view,unsigned descriptor,unsigned vs,unsigned ps,UINT n,UINT count,UINT start,INT base,UINT first,Binding* binding){
 if(!binding || !NeedsScope() || c!=immediate.Get() || mode!=1 || view<1 || view>2 || descriptor!=0x504a0b0 || vs!=57 || ps<301 || ps>305 || n!=indices || count!=1 || start || base || first)return false;
 UINT vpCount=1;D3D11_VIEWPORT vp{};c->RSGetViewports(&vpCount,&vp);if(vpCount!=1 || vp.TopLeftX || vp.TopLeftY || vp.Width!=float(width) || vp.Height!=float(height))return false;
 D3D11_PRIMITIVE_TOPOLOGY topology{};c->IAGetPrimitiveTopology(&topology);if(topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)return false;
 ComPtr<ID3D11RenderTargetView> target;c->OMGetRenderTargets(1,&target,nullptr);if(!target)return false;ComPtr<ID3D11Resource> resource;target->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return false;D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);
 if(td.Width!=width || td.Height!=height || td.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || td.SampleDesc.Count!=1 || td.ArraySize!=1)return false;
 ComPtr<ID3D11Buffer> ib,vb;DXGI_FORMAT format{};UINT offset{},stride{},vbOffset{};c->IAGetIndexBuffer(&ib,&format,&offset);c->IAGetVertexBuffers(0,1,&vb,&stride,&vbOffset);
 if(!ib || !vb || format!=DXGI_FORMAT_R16_UINT || offset || stride!=8 || vbOffset)return false;
 D3D11_BUFFER_DESC ibd{},vbd{};ib->GetDesc(&ibd);vb->GetDesc(&vbd);if(ibd.ByteWidth!=indices*2 || vbd.ByteWidth!=vertices*8 || (ibd.Usage!=D3D11_USAGE_DEFAULT && ibd.Usage!=D3D11_USAGE_IMMUTABLE) || (vbd.Usage!=D3D11_USAGE_DEFAULT && vbd.Usage!=D3D11_USAGE_IMMUTABLE) || ibd.CPUAccessFlags || vbd.CPUAccessFlags || !(ibd.BindFlags&D3D11_BIND_INDEX_BUFFER) || !(vbd.BindFlags&D3D11_BIND_VERTEX_BUFFER))return false;
 Cache* found{};for(unsigned i=0;i<entries;++i)if(cache[i].nativeIndices.Get()==ib.Get() && cache[i].nativeVertices.Get()==vb.Get()){found=&cache[i];break;}
 if(!found){
  if(entries==cache.size()){++refused;return false;}found=&cache[entries++];found->nativeIndices=ib;found->nativeVertices=vb;
  std::vector<uint8_t> nativeI,nativeV;std::vector<uint32_t> fixed;
  bool validated=BuildIndices(columns,rows,&fixed) && ReadBuffer(device.Get(),c,ib.Get(),&nativeI) && ReadBuffer(device.Get(),c,vb.Get(),&nativeV) && ValidateIndices(nativeI.data(),nativeI.size(),fixed) && ValidateVertices(nativeV.data(),nativeV.size(),columns,rows);
  if(validated){D3D11_BUFFER_DESC bd{};bd.ByteWidth=indices*4;bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_INDEX_BUFFER;D3D11_SUBRESOURCE_DATA data{fixed.data(),0,0};found->accepted=SUCCEEDED(device->CreateBuffer(&bd,&data,&found->indices32));}
  if(report){fprintf(report,"LightingGrid67 native mesh validation cache=%u ib=%p vb=%p bytes=%u/%u verified=%d accepted=%d maxCorrectedIndex=%u; original16-bit values and every native vertex checked\n",entries-1,ib.Get(),vb.Get(),ibd.ByteWidth,vbd.ByteWidth,validated,found->accepted,vertices-1);fflush(report);}
 }
 if(!found->accepted){++refused;return false;}
 c->IAGetIndexBuffer(&binding->previous,&binding->format,&binding->offset);binding->replacement=found->indices32.Get();binding->active=true;c->IASetIndexBuffer(binding->replacement,DXGI_FORMAT_R32_UINT,0);++applied[view-1];return true;
}
void Pool::End(ID3D11DeviceContext* c,Binding* b){
 if(!b->active)return;ComPtr<ID3D11Buffer> now;DXGI_FORMAT format{};UINT offset{};c->IAGetIndexBuffer(&now,&format,&offset);
 if(now.Get()==b->replacement && format==DXGI_FORMAT_R32_UINT && !offset)c->IASetIndexBuffer(b->previous,b->format,b->offset);
 else{++conflicts;apply=false;if(report){fprintf(report,"LightingGrid67 unexpected IB replacement preserved; correction disabled\n");fflush(report);}}
 b->previous->Release();b->previous=nullptr;b->active=false;
}
void Pool::Execute(ID3D11DeviceContext* c,unsigned view,unsigned descriptor,unsigned vs,unsigned ps,UINT n,UINT count,UINT start,INT base,UINT first,Callback callback,bool capture){
 Binding binding{};bool changed=Begin(c,view,descriptor,vs,ps,n,count,start,base,first,&binding);
 if(changed && report && (applied[view-1]==1 || capture)){fprintf(report,"LightingGrid67 applied eye=%u sequence=%llu ps=%u count=%u indexFormat=R32_UINT capture=%d\n",view-1,sequence,ps,n,capture);fflush(report);}
 __try{callback(c,n,count,start,base,first);}__finally{End(c,&binding);}
}
void Pool::Stop(){installed=false;apply=false;if(report){fprintf(report,"LightingGrid67 stopped applied=%llu/%llu refused=%llu conflicts=%llu\n",applied[0],applied[1],refused,conflicts);fflush(report);}}
}
