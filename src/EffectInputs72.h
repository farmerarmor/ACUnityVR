#pragma once
// Read-only worker draw identity, using independently verified native layouts.
#include <array>
#include <cstdint>
#include <cstring>
namespace EffectInputs72 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
template<class T>T Get(const void* p,size_t at){T v{};memcpy(&v,static_cast<const uint8_t*>(p)+at,sizeof(v));return v;}
struct Worker{uint64_t context{},wrapper{},cache{},d3d{};};
struct Snapshot {
 uint64_t context{},wrapper{},cache{},d3d{},view{},state{},batch{};
 uint64_t shaders[3]{},buffers[3][14]{};
 uint32_t eye{},frame{},pass{},subview{},shaderDirty{},bufferDirty[3]{},optionalErrors{};
 uint8_t camera[0x440]{},gpu[0x660]{},bindings[0x910]{},batchData[0x680]{};
};
inline bool Read(Reader read,const Worker& w,uint64_t surface,const std::array<uint64_t,2>& eyes,Snapshot& out){
 out={};out.context=w.context;out.wrapper=w.wrapper;out.cache=w.cache;out.d3d=w.d3d;
 if(!w.context || !w.wrapper || !w.cache || !w.d3d || !read(out.gpu,w.context,sizeof(out.gpu)) || !read(out.bindings,w.cache,sizeof(out.bindings)))return false;
 if(Get<uint64_t>(out.gpu,0)!=w.wrapper || Get<uint64_t>(out.gpu,0x20)!=surface || Get<uint64_t>(out.bindings,0x30)!=w.wrapper || Get<uint64_t>(out.bindings,0x38)!=w.d3d)return false;
 uint64_t link{};if(!read(&link,w.wrapper+0x28,8) || link!=w.cache || !read(&link,w.wrapper+0x38,8) || link!=w.d3d)return false;
 out.view=Get<uint64_t>(out.gpu,0x30);out.state=Get<uint64_t>(out.gpu,0x28);
 out.eye=out.view==eyes[0]?1:out.view==eyes[1]?2:0;
 if(!out.eye || !out.view || !out.state || !read(out.camera,out.view,sizeof(out.camera)) || Get<uint64_t>(out.camera,0x430)!=out.state || !read(&link,out.state+0x8208,8) || link!=out.view)return false;
 out.frame=Get<uint32_t>(out.camera,0x294);out.pass=Get<uint32_t>(out.gpu,0x3c);out.subview=Get<uint32_t>(out.gpu,0x370);
 const unsigned offsets[]{0x4e8,0x4e0,0x508};out.shaderDirty=Get<uint32_t>(out.bindings,0x510);
 for(unsigned stage=0;stage<3;++stage){out.shaders[stage]=Get<uint64_t>(out.bindings,offsets[stage]);out.bufferDirty[stage]=Get<uint32_t>(out.bindings,0x860+stage*12);for(unsigned slot=0;slot<14;++slot)out.buffers[stage][slot]=Get<uint64_t>(out.bindings,0x5b8+stage*0x70+slot*8);}
 if(!read(&out.batch,w.context+0x1a68,8))out.optionalErrors|=1;
 if(out.batch && !read(out.batchData,out.batch,sizeof(out.batchData)))out.optionalErrors|=2;
 // Repeat all owner links after the bounded read; optional batch failure is recorded.
 if(!read(&link,w.context,8) || link!=w.wrapper || !read(&link,w.context+0x20,8) || link!=surface || !read(&link,w.context+0x28,8) || link!=out.state || !read(&link,w.context+0x30,8) || link!=out.view || !read(&link,w.wrapper+0x28,8) || link!=w.cache || !read(&link,w.wrapper+0x38,8) || link!=w.d3d || !read(&link,w.cache+0x30,8) || link!=w.wrapper || !read(&link,w.cache+0x38,8) || link!=w.d3d || !read(&link,out.view+0x430,8) || link!=out.state || !read(&link,out.state+0x8208,8) || link!=out.view)return false;
 return true;
}
}
