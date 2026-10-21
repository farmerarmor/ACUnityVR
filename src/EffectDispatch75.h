#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace EffectDispatch75 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
struct Snapshot {uint64_t object{},table{};std::array<uint64_t,41> entries{};};
inline bool Read(Reader read,uint64_t object,Snapshot& out){
 out={};Snapshot s;s.object=object;uint64_t after{};
 if(!read || !object || !read(&s.table,object,8) || !s.table || !read(s.entries.data(),s.table,sizeof(s.entries)) || !read(&after,object,8) || after!=s.table)return false;
 out=s;return true;
}
inline bool Unchanged(Reader read,const Snapshot& s){
 Snapshot after;return s.object && Read(read,s.object,after) && after.table==s.table && after.entries==s.entries;
}
inline bool SharedDrawMethods(const Snapshot& worker,const Snapshot& immediate){
 if(!worker.object || !immediate.object)return false;
 for(unsigned slot:{12u,13u,20u,21u,38u})if(!worker.entries[slot] || worker.entries[slot]!=immediate.entries[slot])return false;
 return true;
}
}
