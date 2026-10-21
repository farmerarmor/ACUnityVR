#pragma once
#include "EffectContexts74.h"
namespace EffectBackend74 {
using Owner=bool(*)(void*) noexcept;
inline bool Serial(EffectInputs72::Reader read,Owner owns,uint64_t backend,uint64_t originalTable,uint64_t cacheVtable,uint64_t immediate,uint64_t surface,EffectInputs72::Worker& out){
 out={};uint64_t table{},after{};if(!read(&table,backend,8) || (table!=originalTable && (!owns || !owns(reinterpret_cast<void*>(backend)))))return false;
 // SourceRendering validates its exact private table through OwnsBackend.
 // Keep every serial wrapper/cache/surface check with that approved table.
 if(!EffectContexts74::Serial(read,backend,table,cacheVtable,immediate,surface,out) || !read(&after,backend,8) || after!=table || (table!=originalTable && !owns(reinterpret_cast<void*>(backend)))){out={};return false;}
 return true;
}
}
