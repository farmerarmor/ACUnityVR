#pragma once
#include "ComponentCameraInputs48.h"
namespace ComponentTaskInputs49 {
using Reader=ComponentCameraInputs48::Reader;
struct Layout{uint64_t surface{},state{},graph{},world{},scene{},context{},slots[2]{};};
inline bool Resolve(Reader read,uint64_t image,uint64_t source,Layout& out){
 Layout s;uint64_t q{},target{};auto ptr=[&](uint64_t p,uint64_t& value){return read(&value,p,8);};
 if(!ptr(source+0x420,s.surface) || !s.surface || !ptr(source+0x430,s.state) || !s.state || !ptr(source+0x440,s.graph) || !s.graph)return false;
 if(!ptr(s.graph,q) || q!=source || !ptr(s.graph+8,q) || q!=s.state || !ptr(s.graph+0x10,s.world) || !s.world || !ptr(s.world+0x1e8,s.scene) || !s.scene)return false;
 if(!ptr(s.scene,q) || q!=s.world || !ptr(s.scene+8,q) || q!=source || !ptr(s.scene+0xce0,target) || target!=image+0x219f480)return false;
 s.context=s.scene+0x1aa8;s.slots[0]=s.scene+0xeb0;s.slots[1]=s.scene+0x1210;
 for(auto slot:s.slots)if(!ptr(slot-0x1a8,q) || q!=image+0x30964e0 || !ptr(q+0x80,target) || target!=image+0x219f690 || !ptr(slot,target) || target!=image+0x218ceb8 || !ptr(slot+8,q) || q!=s.context)return false;
 if(!ptr(s.context,q) || q!=s.state || !ptr(s.context+8,q) || q!=source || !ptr(s.context+0x10,q) || q!=s.world || !ptr(s.state+0x890,q) || q!=source || !ptr(s.state+0x8208,q) || q!=source)return false;
 out=s;return true;
}
}
