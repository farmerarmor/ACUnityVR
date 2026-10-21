#pragma once
#include <cstdint>
#include <cstring>
namespace UiMode71 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
struct State {uint64_t manager{},stack{},pages{};uint16_t pageCount{};bool menu{};};
template<class T>bool Read(Reader read,uint64_t p,T& value){return p>=0x10000 && read(&value,p,sizeof(value));}
inline bool Capture(Reader read,uint64_t image,State& out){
 State s{};uint64_t vt{},back{},again{};uint16_t count{};
 if(!Read(read,image+0x51eace8,s.manager) || !Read(read,s.manager,vt) || vt!=image+0x2fd2b80 ||
 !Read(read,s.manager+0x30,s.stack) || !Read(read,s.stack,vt) || vt!=image+0x2ed3888 || !Read(read,s.stack+0x28,back) || back!=s.manager ||
 !Read(read,s.stack+0x10,s.pages) || !Read(read,s.stack+0x1a,s.pageCount) || s.pageCount>32 || (s.pageCount && !s.pages))return false;
 for(unsigned i=0;i<s.pageCount;++i){uint64_t entry{},handle{},page{};uint32_t lifetime{};
  if(!Read(read,s.pages+i*8,entry) || !Read(read,entry+8,handle) || !Read(read,handle,page) || !Read(read,handle+12,lifetime) || int32_t(lifetime)>=0 || !page || !Read(read,page,vt) || vt<image+0x2c00000 || vt>image+0x3800000)return false;
 }
 if(!Read(read,image+0x51eace8,again) || again!=s.manager || !Read(read,s.manager+0x30,again) || again!=s.stack ||
 !Read(read,s.stack+0x10,again) || again!=s.pages || !Read(read,s.stack+0x1a,count) || count!=s.pageCount)return false;
 s.menu=s.pageCount!=0;out=s;return true;
}
}
