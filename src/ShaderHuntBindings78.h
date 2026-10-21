#pragma once
#include "MaterialDraw75.h"
// Read-only current native worker shader bindings. No deferred Get-state APIs.
namespace ShaderHuntBindings78 {
struct Snapshot {uint64_t shaders[6]{},context{},cache{},viewPointer{};unsigned view{};};
inline bool Read(EffectInputs72::Reader read,const EffectInputs72::Worker& w,uint64_t surface,const std::array<uint64_t,2>& eyes,Snapshot& out){
 out={};uint64_t observedView{};if(!read || !read(&observedView,w.context+0x30,8) || !observedView)return false;
 // Hunting includes native shadow/reflection/source subviews, still on an exact
 // registered worker/current surface with both native camera/state owner links.
 std::array<uint64_t,2> checkEyes{observedView,0};MaterialDraw75::Identity before,after;
 if(!MaterialDraw75::Read(read,w,surface,checkEyes,before))return false;
 const unsigned offsets[]{0x4e8,0x4e0,0x508,0x4f8,0x500,0x4f0};uint64_t values[6]{},again[6]{};
 for(unsigned i=0;i<6;++i)if(!read(values+i,w.cache+offsets[i],8))return false;
 if(!MaterialDraw75::Read(read,w,surface,checkEyes,after) || after.view!=before.view || after.state!=before.state || after.shaders[0]!=before.shaders[0] || after.shaders[1]!=before.shaders[1])return false;
 for(unsigned i=0;i<6;++i)if(!read(again+i,w.cache+offsets[i],8) || again[i]!=values[i])return false;
 if(values[0]!=before.shaders[0] || values[1]!=before.shaders[1])return false;
 memcpy(out.shaders,values,sizeof(values));out.context=w.context;out.cache=w.cache;out.viewPointer=before.view;out.view=before.view==eyes[0]?1:before.view==eyes[1]?2:3;return true;
}
}
