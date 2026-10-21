// The exact native CS31 classifier uses symmetric rays for asymmetric VR eyes.
// Bind owned CS5 constants only around that dispatch, retaining native shaders,
// planes, depth input and integer tile output. Native memory stays unchanged.
#include "LightingTiles69.h"
#include "GpuCapture.h"
#include <cmath>
#include <cstring>
#include <vector>
namespace LightingTiles69 {
using acu::Field;using Microsoft::WRL::ComPtr;
bool CorrectRays(const float* p,const uint8_t* input,UINT w,UINT h,uint8_t* out){
 if(!p || !input || !out || !w || !h || w>16384 || h>16384 || w%16 || h%16)return false;
 for(unsigned i=0;i<16;++i)if(!std::isfinite(p[i]))return false;
 UINT shape[4]{};float rays[4]{};memcpy(shape,input,16);memcpy(rays,input+16,16);
 if(shape[0]!=w/16 || shape[1]!=h/16 || shape[2]!=16 || shape[3]!=16 || p[0]<=.01f || p[5]<=.01f || std::abs(p[11]+1.f)>.0001f)return false;
 auto nearFloat=[](float a,float b){return std::isfinite(a) && std::abs(a-b)<.00001f;};
 if(!nearFloat(rays[0],-1/p[0]) || !nearFloat(rays[1],-1/p[5]) || !nearFloat(rays[2],32/(float(w)*p[0])) || !nearFloat(rays[3],32/(float(h)*p[5])))return false;
 // Native CS31 flips Y after constructing tile rays. Preserve that convention.
 rays[0]+=p[8]/p[0];rays[1]-=p[9]/p[5];memcpy(out,input,Bytes);memcpy(out+16,rays,16);return true;
}
static bool ReadNative(Pool* p,void* context,unsigned view,ID3D11Buffer* bound,uint8_t* data,float* projection,void** identity){__try{
 if(context!=Field<void*>(p->backend,0x20) || Field<void*>(context,8)!=p->backend)return false;
 auto v=Field<uint8_t*>(context,0x30),s=Field<uint8_t*>(context,0x28);
 if(v!=p->eyes[view-1] || !s || Field<void*>(v,0x430)!=s || Field<void*>(s,0x8208)!=v || memcmp(static_cast<uint8_t*>(context)+0x60,v+0x320,64))return false;
 if(Field<UINT>(p->backend,0x4d0)!=p->width || Field<UINT>(p->backend,0x4d4)!=p->height)return false;
 auto module=Field<uint8_t*>(p->backend,0x3540);if(!module || Field<void*>(module,0)!=p->image+0x35d5030)return false;
 auto wrapper=Field<uint8_t*>(module,0x20);if(!wrapper || Field<void*>(wrapper,0)!=p->image+0x30aa540 || Field<void*>(wrapper,0x28)!=bound)return false;
 if(Field<UINT>(module,0x1b90)!=p->columns || Field<UINT>(module,0x1b94)!=p->rows)return false;
 memcpy(data,module+0x30,Bytes);memcpy(projection,static_cast<uint8_t*>(context)+0x60,64);*identity=module;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool ReadBuffer(Pool* p,ID3D11Buffer* b,std::vector<uint8_t>* bytes){
 D3D11_BUFFER_DESC d{};b->GetDesc(&d);if(d.ByteWidth!=Bytes)return false;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;d.StructureByteStride=0;
 ComPtr<ID3D11Buffer> staging;if(FAILED(p->device->CreateBuffer(&d,nullptr,&staging)))return false;p->immediate->CopyResource(staging.Get(),b);D3D11_MAPPED_SUBRESOURCE m{};
 if(FAILED(p->immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)))return false;bytes->assign(static_cast<uint8_t*>(m.pData),static_cast<uint8_t*>(m.pData)+Bytes);p->immediate->Unmap(staging.Get(),0);++p->readbacks;return true;
}
static bool Texture(ID3D11View* v,UINT w,UINT h,DXGI_FORMAT format){
 if(!v)return false;ComPtr<ID3D11Resource> r;v->GetResource(&r);ComPtr<ID3D11Texture2D> t;if(FAILED(r.As(&t)))return false;D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);
 return d.Width==w && d.Height==h && d.Format==format && d.MipLevels==1 && d.ArraySize==1 && d.SampleDesc.Count==1;
}
static bool NativeGuards(uint8_t* image){
 const struct{size_t rva;const char* bytes;size_t size;} guards[]={
 {0x2290910,"\xe9\x1b\x37\x01\x00",5},
 {0x22a4030,"\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x30\x49\x8b\x48\x20\x49\x8b\xd8\x48\x8b\xfa\x48\x85\xc9\x74\x11\x48\x8b\x41\x50\x83",35},
 {0x22a40e5,"\x4c\x8b\x8f\xc8\x13\x00\x00\x48\x8b\x87\x40\x2f\x00\x00\x4c\x8b\x03\x4d\x8b\x49\x20\x48\x8b\x8f\x40\x35\x00\x00\x48\x8b\xd3\x48\x89\x44\x24\x20\xe8\xc2\x22\xfd\xff",41},
 {0x22763d0,"\x48\x8b\xc4\x4c\x89\x48\x20\x4c\x89\x40\x18\x48\x89\x50\x10\x48\x89\x48\x08\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8d\xa8\xc8\xfe",36},
 {0x2276561,"\x0f\x28\x63\x60\x41\x8b\xc6\xf3\x0f\x5e\x53\x74\xf3\x0f\x5e\x4b\x74\xf3\x44\x0f\x5e\xc0\xf3\x0f\x5e\xdc\x0f\x57\xc0\xf3\x41\x0f\x59\xca\xf3\x48\x0f\x2a\xc0\xf3\x0f\x5e\xf8\xf3\x44\x0f\x59\xe6\xf3\x44\x0f\x59\xde\x0f\x28\xc5\xf3\x0f\x5e\xc4\xf3\x45\x0f\x59\xe0\xf3\x44\x0f\x59\xdf\xf3\x44\x0f\x59\xe1\xf3\x41\x0f\x59\xc1\x41\x0f\x28\xcc\x0f\x14\xd1\xf3\x44\x0f\x59\xd8\x41\x0f\x28\xc8\xf3\x0f\x59\x0d\x93\xea\xbf\x00\x41\x0f\x28\xc3\x0f\x14\xd8\x0f\x14\xda\xf3\x44\x0f\x59\xc6\xf3\x41\x0f\x58\xcd\x0f\x28\xd7\x0f\x29\x5f\x10",131},
 {0x2276d3c,"\x4c\x8b\x46\x20\xbb\x05\x00\x00\x00\x49\x8b\xcf\x8b\xd3\xe8\xd1\xf8\xf0\xff\x49\x81\x8f\xf8\x00\x00\x00\x00\x00\x20\x00\x49\x89\xb7\x58",34},
 {0x2276dea,"\xba\x1f\x00\x00\x00\x49\x8b\xcf\xe8\xf9\x01\x08\x00\x45\x8b\x45\x34\x41\x8b\x55\x30\x41\x83\xc0\x07\x83\xc2\x07\x41\xb9\x01\x00\x00\x00\x41\xc1\xe8\x03\xc1\xea\x03\x49\x8b\xcf\xe8\x95\x07\x08\x00\x49\x8b\x57\x28\x4c",54},
 {0x2186620,"\x44\x8b\xca\x4c\x8b\xd1\x4d\x85\xc0\x74\x06\x49\x8b\x40\x28\xeb\x02\x33\xc0\x4c\x8b\x41\x28",23},
 {0x33f1060,"\x44\x58\x42\x43\x94\xd4\x46\xa3\x80\x51\x3e\x44\x37\x2d\x80\x1d\xc5\x9e\x4b\x42\x01\x00\x00\x00\xc8\x21\x00\x00\x05\x00\x00\x00",32},
 };
 for(auto& g:guards)if(memcmp(image+g.rva,g.bytes,g.size))return false;return true;
}
bool Pool::Initialize(uint8_t* base,void* b,ID3D11Device* d,ID3D11DeviceContext* c,UINT w,UINT h,const std::filesystem::path& dir,FILE* f){
 if(!base || !b || !d || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || !w || !h || w>16384 || h>16384 || w%16 || h%16 || !NativeGuards(base))return false;
 image=base;backend=static_cast<uint8_t*>(b);device=d;immediate=c;c->QueryInterface(IID_PPV_ARGS(&context1));width=w;height=h;columns=w/16;rows=h/16;report=f;
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(Bytes);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(d->CreateBuffer(&bd,nullptr,&corrected)))return false;
 installed=true;apply=GetPrivateProfileIntW(L"Lighting",L"FixAAOffTileRays",1,(dir/L"ACUnityVR.ini").c_str())!=0;
 if(report){fprintf(report,"LightingTiles69 installed enabled=%d; Insert compares owned CS5 ray bias, native classifier/planes/resources intact; first-use full GPU/CPU equality check, cached thereafter\n",apply);fflush(report);}return true;
}
void Pool::BeginFrame(uint64_t id,uint32_t m){sequence=id;mode=m;bool down=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;if(down&&!held){apply=!apply;if(report){fprintf(report,"LightingTiles69 correction %s sequence=%llu\n",apply?"ON":"OFF",sequence);fflush(report);}}held=down;}
bool Pool::Begin(ID3D11DeviceContext* c,void* context,unsigned v,unsigned descriptor,unsigned shader,UINT x,UINT y,UINT z,Binding* b,bool capture){
 if(!b || !NeedsScope() || c!=immediate.Get() || v<1 || v>2 || descriptor!=0x504a2e8 || shader!=31 || x!=(columns+7)/8 || y!=(rows+7)/8 || z!=1)return false;
 ComPtr<ID3D11Buffer> native;UINT first{},count{};if(context1)context1->CSGetConstantBuffers1(5,1,&native,&first,&count);else c->CSGetConstantBuffers(5,1,&native);
 if(!native || first || (context1 && count<Bytes/16)){++refused;return false;}D3D11_BUFFER_DESC bd{};native->GetDesc(&bd);if(bd.ByteWidth!=Bytes || bd.BindFlags!=D3D11_BIND_CONSTANT_BUFFER || bd.MiscFlags){++refused;return false;}
 alignas(16) uint8_t original[Bytes]{},updated[Bytes]{};float projection[16]{};void* module{};
 if(!ReadNative(this,context,v,native.Get(),original,projection,&module) || !CorrectRays(projection,original,width,height,updated)){++refused;if(report && refused<4){fprintf(report,"LightingTiles69 refused native ownership/rays eye=%u sequence=%llu\n",v-1,sequence);fflush(report);}return false;}
 ComPtr<ID3D11ShaderResourceView> depth;ComPtr<ID3D11UnorderedAccessView> output;c->CSGetShaderResources(0,1,&depth);c->CSGetUnorderedAccessViews(0,1,&output);
 if(!Texture(depth.Get(),columns,rows,DXGI_FORMAT_R32G32_FLOAT) || !Texture(output.Get(),columns,rows,DXGI_FORMAT_R32_UINT)){++refused;return false;}
 Cache* entry{};for(unsigned i=0;i<entries;++i)if(cache[i].native.Get()==native.Get() && cache[i].module==module){entry=&cache[i];break;}
 if(!entry){if(entries==cache.size()){++refused;return false;}entry=&cache[entries++];entry->native=native;entry->module=module;std::vector<uint8_t> gpu;entry->accepted=ReadBuffer(this,native.Get(),&gpu) && gpu.size()==Bytes && !memcmp(gpu.data(),original,Bytes);
  if(report){fprintf(report,"LightingTiles69 native constants validation cache=%u cb=%p module=%p bytes=%zu verified=%d readbacks=%llu\n",entries-1,native.Get(),module,Bytes,entry->accepted,readbacks);fflush(report);}}
 if(!entry->accepted){++refused;return false;}D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(c->Map(corrected.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m))){++refused;return false;}memcpy(m.pData,updated,Bytes);c->Unmap(corrected.Get(),0);
 b->previous=native.Detach();b->replacement=corrected.Get();b->first=first;b->count=count;b->ranged=bool(context1);b->active=true;c->CSSetConstantBuffers(5,1,&b->replacement);++applied[v-1];
 if(report && (applied[v-1]<=2 || capture)){float old[4]{},now[4]{};memcpy(old,original+16,16);memcpy(now,updated+16,16);fprintf(report,"LightingTiles69 applied eye=%u sequence=%llu before=(%.9g,%.9g) after=(%.9g,%.9g) step=(%.9g,%.9g) capture=%d\n",v-1,sequence,old[0],old[1],now[0],now[1],now[2],now[3],capture);fflush(report);}return true;
}
void Pool::End(ID3D11DeviceContext* c,Binding* b){
 if(!b->active)return;ComPtr<ID3D11Buffer> now;UINT first{},count{};if(context1)context1->CSGetConstantBuffers1(5,1,&now,&first,&count);else c->CSGetConstantBuffers(5,1,&now);
 if(now.Get()==b->replacement && first==0 && (!context1 || count==4096)){if(b->ranged)context1->CSSetConstantBuffers1(5,1,&b->previous,&b->first,&b->count);else c->CSSetConstantBuffers(5,1,&b->previous);}
 else{++conflicts;apply=false;if(report){fprintf(report,"LightingTiles69 unexpected CS5 binding preserved; correction disabled\n");fflush(report);}}
 b->previous->Release();b->previous=nullptr;b->active=false;
}
void Pool::Execute(ID3D11DeviceContext* c,void* context,unsigned v,unsigned desc,unsigned shader,UINT x,UINT y,UINT z,Callback callback,bool capture){Binding b{};Begin(c,context,v,desc,shader,x,y,z,&b,capture);__try{callback(c,x,y,z);}__finally{End(c,&b);}}
void Pool::Stop(){installed=false;apply=false;if(report){fprintf(report,"LightingTiles69 stopped applied=%llu/%llu refused=%llu conflicts=%llu readbacks=%llu\n",applied[0],applied[1],refused,conflicts,readbacks);fflush(report);}}
}
