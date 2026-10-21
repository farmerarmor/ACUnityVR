#pragma once
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace LightingTiles69 {
constexpr size_t Bytes=7008;
using Callback=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);
struct Binding {ID3D11Buffer* previous{};ID3D11Buffer* replacement{};UINT first{},count{};bool ranged{},active{};};
struct Cache {Microsoft::WRL::ComPtr<ID3D11Buffer> native;void* module{};bool accepted{};};
struct Pool {
 Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate;Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;Microsoft::WRL::ComPtr<ID3D11Buffer> corrected;
 std::array<Cache,3> cache;unsigned entries{};uint8_t* image{};uint8_t* backend{};void* eyes[2]{};UINT width{},height{},columns{},rows{};uint32_t mode{};uint64_t sequence{},applied[2]{},refused{},conflicts{},readbacks{};bool installed{},apply{},held{};FILE* report{};
 bool Initialize(uint8_t*,void*,ID3D11Device*,ID3D11DeviceContext*,UINT,UINT,const std::filesystem::path&,FILE*);
 bool NeedsScope()const{return installed && apply && mode==1;}
 void SetEyes(void* a,void* b){eyes[0]=a;eyes[1]=b;}
 void BeginFrame(uint64_t,uint32_t);
 bool Begin(ID3D11DeviceContext*,void* nativeContext,unsigned view,unsigned descriptor,unsigned shader,UINT x,UINT y,UINT z,Binding*,bool capture);
 void End(ID3D11DeviceContext*,Binding*);
 void Execute(ID3D11DeviceContext*,void*,unsigned,unsigned,unsigned,UINT,UINT,UINT,Callback,bool);
 void Stop();
};
bool CorrectRays(const float* projection,const uint8_t* input,UINT width,UINT height,uint8_t* output);
}
