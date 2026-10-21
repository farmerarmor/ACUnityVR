#pragma once
#include "EffectInputs72.h"
namespace MaterialDraw75 {
struct Identity {uint64_t view{},state{},shaders[2]{};unsigned eye{},dirty{};};
inline bool Read(EffectInputs72::Reader read,const EffectInputs72::Worker& w,uint64_t surface,const std::array<uint64_t,2>& eyes,Identity& out){
 out={};Identity s;uint64_t link{},viewAfter{},stateAfter{},vsAfter{},psAfter{};unsigned dirtyAfter{};
 if(!read || !w.context || !w.wrapper || !w.cache || !w.d3d || !surface)return false;
 if(!read(&link,w.context,8) || link!=w.wrapper || !read(&link,w.context+0x20,8) || link!=surface || !read(&s.view,w.context+0x30,8) || !read(&s.state,w.context+0x28,8))return false;
 s.eye=s.view==eyes[0]?1:s.view==eyes[1]?2:0;
 if(!s.eye || !s.view || !s.state || !read(&link,s.view+0x430,8) || link!=s.state || !read(&link,s.state+0x8208,8) || link!=s.view)return false;
 if(!read(&link,w.wrapper+0x28,8) || link!=w.cache || !read(&link,w.wrapper+0x38,8) || link!=w.d3d || !read(&link,w.cache+0x30,8) || link!=w.wrapper || !read(&link,w.cache+0x38,8) || link!=w.d3d)return false;
 if(!read(&s.shaders[0],w.cache+0x4e8,8) || !read(&s.shaders[1],w.cache+0x4e0,8) || !read(&s.dirty,w.cache+0x510,4) || s.dirty)return false;
 if(!read(&link,w.context,8) || link!=w.wrapper || !read(&link,w.context+0x20,8) || link!=surface || !read(&viewAfter,w.context+0x30,8) || viewAfter!=s.view || !read(&stateAfter,w.context+0x28,8) || stateAfter!=s.state || !read(&link,s.view+0x430,8) || link!=s.state || !read(&link,s.state+0x8208,8) || link!=s.view)return false;
 if(!read(&link,w.wrapper+0x28,8) || link!=w.cache || !read(&link,w.wrapper+0x38,8) || link!=w.d3d || !read(&link,w.cache+0x30,8) || link!=w.wrapper || !read(&link,w.cache+0x38,8) || link!=w.d3d || !read(&vsAfter,w.cache+0x4e8,8) || vsAfter!=s.shaders[0] || !read(&psAfter,w.cache+0x4e0,8) || psAfter!=s.shaders[1] || !read(&dirtyAfter,w.cache+0x510,4) || dirtyAfter)return false;
 out=s;return true;
}
}
