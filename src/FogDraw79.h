#pragma once
#include "MaterialDraw75.h"
#include "VolumetricFog79.h"
namespace FogDraw79 {
inline bool Read(EffectInputs72::Reader read,const EffectInputs72::Worker& w,uint64_t surface,const std::array<uint64_t,2>& eyes,unsigned kind,unsigned vertices,unsigned start,bool replay,MaterialDraw75::Identity& out){
 out={};if(replay || kind!=13 || vertices!=6 || start)return false;MaterialDraw75::Identity before,after;
 if(!MaterialDraw75::Read(read,w,surface,eyes,before))return false;unsigned pass{},subview{},afterPass{},afterSubview{};
 if(!read(&pass,w.context+0x3c,4) || !read(&subview,w.context+0x370,4) || !read(&afterPass,w.context+0x3c,4) || !read(&afterSubview,w.context+0x370,4) || pass!=afterPass || subview!=afterSubview || !VolumetricFog79::Shape(kind,vertices,start,before.eye,pass,subview,replay))return false;
 if(!MaterialDraw75::Read(read,w,surface,eyes,after) || after.view!=before.view || after.state!=before.state || after.shaders[0]!=before.shaders[0] || after.shaders[1]!=before.shaders[1])return false;out=after;return true;
}
}
