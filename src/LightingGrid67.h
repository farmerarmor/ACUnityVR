#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <filesystem>
namespace LightingGrid67 {
using Callback=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
struct Binding { ID3D11Buffer* previous{};ID3D11Buffer* replacement{};DXGI_FORMAT format{};UINT offset{};bool active{}; };
struct Cache { Microsoft::WRL::ComPtr<ID3D11Buffer> nativeIndices,nativeVertices,indices32;bool accepted{}; };
struct Pool {
 Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate;
 std::array<Cache,3> cache;UINT width{},height{},columns{},rows{},vertices{},indices{};unsigned entries{};uint32_t mode{};uint64_t sequence{},applied[2]{},refused{},conflicts{};bool installed{},apply{},f1Held{};FILE* report{};
 bool Initialize(uint8_t* image,ID3D11Device*,ID3D11DeviceContext*,UINT,UINT,const std::filesystem::path&,FILE*);
 bool NeedsScope()const {return installed && apply && vertices>65536;}
 void BeginFrame(uint64_t id,uint32_t frameMode);
 bool Begin(ID3D11DeviceContext*,unsigned view,unsigned descriptor,unsigned vs,unsigned ps,UINT count,UINT instances,UINT start,INT base,UINT firstInstance,Binding*);
 void End(ID3D11DeviceContext*,Binding*);
 void Execute(ID3D11DeviceContext*,unsigned view,unsigned descriptor,unsigned vs,unsigned ps,UINT count,UINT instances,UINT start,INT base,UINT firstInstance,Callback,bool capture);
 void Stop();
};
bool BuildIndices(UINT columns,UINT rows,std::vector<uint32_t>* output);
bool ValidateIndices(const void*,size_t,const std::vector<uint32_t>&);
bool ValidateVertices(const void*,size_t,UINT columns,UINT rows);
}
