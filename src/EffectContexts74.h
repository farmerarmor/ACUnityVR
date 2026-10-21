#pragma once
// Resolve only the native serial draw cache verified against the same D3D immediate context.
#include "EffectInputs72.h"
namespace EffectContexts74 {
inline bool Serial(EffectInputs72::Reader read,uint64_t backend,uint64_t backendVtable,uint64_t cacheVtable,uint64_t immediate,uint64_t surface,EffectInputs72::Worker& out){
 out={};if(!backend || !immediate || !surface)return false;uint64_t link{};EffectInputs72::Worker w;
 if(!read(&link,backend,8) || link!=backendVtable || !read(&w.context,backend+0x20,8) || !w.context || !read(&w.wrapper,w.context,8) || !w.wrapper || !read(&w.cache,w.wrapper+0x28,8) || !w.cache || !read(&w.d3d,w.wrapper+0x38,8) || w.d3d!=immediate)return false;
 if(!read(&link,w.wrapper+0x30,8) || link!=immediate || !read(&link,w.cache,8) || link!=cacheVtable || !read(&link,w.cache+0x30,8) || link!=w.wrapper || !read(&link,w.cache+0x38,8) || link!=immediate || !read(&link,w.context+0x20,8) || link!=surface)return false;
 // Reject a context/cache swap during the read; owned-eye/state identity is checked by EffectInputs72::Read.
 if(!read(&link,backend+0x20,8) || link!=w.context || !read(&link,w.context,8) || link!=w.wrapper || !read(&link,w.wrapper+0x28,8) || link!=w.cache || !read(&link,w.wrapper+0x30,8) || link!=immediate || !read(&link,w.wrapper+0x38,8) || link!=immediate || !read(&link,w.cache+0x30,8) || link!=w.wrapper || !read(&link,w.cache+0x38,8) || link!=immediate || !read(&link,w.context+0x20,8) || link!=surface)return false;
 out=w;return true;
}
}
