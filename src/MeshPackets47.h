// F8-only read-only native mesh input/queue snapshots. No callbacks or writes.
#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
namespace MeshPackets47 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
constexpr unsigned Groups=31,Lanes=6,ListLimit=16384,RecordLimit=65536,PartLimit=16384;
template<class T>T Get(const void* p,size_t offset){T v{};memcpy(&v,static_cast<const uint8_t*>(p)+offset,sizeof(v));return v;}
struct QueueRecord {uint32_t group{},lane{},source{},reserved{};uint8_t raw[32]{};};
struct Proxy {uint64_t address{};uint8_t raw[0xd8]{};uint32_t errors{},stable{};};
struct Part {uint64_t proxy{},address{},callback{},definition{};uint32_t index{},errors{};uint8_t raw[0x80]{},model[0x100]{},children[4][0x20]{};uint64_t childAddresses[4]{};uint32_t childErrors{},stable{};};
static_assert(sizeof(QueueRecord)==48 && sizeof(Proxy)==232 && sizeof(Part)==592);
struct List {uint64_t pointer{},sorted{};uint32_t capacity{},count{},captured{},sortedCaptured{};bool read{},stable{},complete{};};
struct Snapshot {uint64_t view{},state{},graph{},manager{},selected{};uint32_t count{},capacity{},generation{};bool owner{},ownerStable{},selectedRead{},selectedStable{},queuesComplete{true},partsComplete{true};unsigned errors{},partErrors{};std::array<uint8_t,0x4020> header{};std::array<List,Groups*Lanes> lists{};std::vector<uint64_t> selectedPointers;std::vector<QueueRecord> records;std::vector<Proxy> proxies;std::vector<Part> parts;};
inline bool Ptr(Reader read,uint64_t at,uint64_t& value){return read(&value,at,8);}
inline bool Identity(Reader read,uint64_t image,uint64_t source,uint64_t view,uint64_t& state,uint64_t& graph,uint64_t& manager){
 uint64_t back{},value{};
 if(!view || view==source || !Ptr(read,view+0x430,state) || !state || !Ptr(read,view+0x440,graph) || !graph || !Ptr(read,state+0x8220,manager) || !manager || !Ptr(read,state+0x8208,back) || back!=view || !Ptr(read,manager+0x3ff8,back) || back!=state || !Ptr(read,graph,back) || back!=view || !Ptr(read,graph+8,back) || back!=state)return false;
 for(auto pair:std::array<std::array<uint64_t,2>,5>{{{0x2e20,0x21a6050},{0x2ff0,0x21be390},{0x3188,0x21a6020},{0x3330,0x21a6030},{0x34d8,0x21a6040}}})if(!Ptr(read,graph+pair[0],value) || value!=image+pair[1])return false;
 return true;
}
inline Snapshot Capture(Reader read,uint64_t image,uint64_t source,uint64_t view){
 Snapshot s;s.view=view;s.owner=Identity(read,image,source,view,s.state,s.graph,s.manager);
 if(!s.owner || !read(s.header.data(),s.manager,s.header.size())){++s.errors;s.queuesComplete=s.partsComplete=false;return s;}
 for(unsigned group=0;group<Groups;++group)for(unsigned lane=0;lane<Lanes;++lane){
  auto offset=(group*Lanes+lane)*0x58;auto& l=s.lists[group*Lanes+lane];l.pointer=Get<uint64_t>(s.header.data(),offset);l.capacity=Get<uint32_t>(s.header.data(),offset+8);l.count=Get<uint32_t>(s.header.data(),offset+0x40);l.sorted=Get<uint64_t>(s.header.data(),offset+0x48);
  l.complete=l.count<=l.capacity && l.count<=ListLimit;
  if(l.complete){
   auto copy=[&](uint64_t address,unsigned version,unsigned& captured){
    if(s.records.size()+l.count>RecordLimit)return false;
    std::vector<std::array<uint8_t,32>> rows(l.count);if(l.count){if(!read(rows.data(),address,size_t(l.count)*32))return false;std::vector<std::array<uint8_t,32>> verify(l.count);if(!read(verify.data(),address,size_t(l.count)*32) || memcmp(rows.data(),verify.data(),size_t(l.count)*32))return false;}
    for(const auto& row:rows){QueueRecord q;q.group=group;q.lane=lane;q.source=version;memcpy(q.raw,row.data(),32);s.records.push_back(q);}captured=l.count;return true;
   };
   l.read=copy(l.pointer,0,l.captured);if(l.read && l.sorted && l.sorted!=l.pointer)l.read=copy(l.sorted,1,l.sortedCaptured);
   l.complete=l.read;
  }
  std::array<uint8_t,0x58> after{};l.stable=read(after.data(),s.manager+offset,after.size()) && !memcmp(after.data(),s.header.data()+offset,after.size());
  if(!l.complete || !l.stable){s.queuesComplete=false;++s.errors;}
 }
 bool selectedHeader=Ptr(read,s.state+0x2230,s.selected) && read(&s.capacity,s.state+0x2238,4) && read(&s.count,s.state+0x2270,4) && read(&s.generation,s.state+0x864c,4);
 if(selectedHeader && s.count<=s.capacity && s.count<=ListLimit){s.selectedPointers.resize(s.count);s.selectedRead=!s.count || read(s.selectedPointers.data(),s.selected,size_t(s.count)*8);}
 if(s.selectedRead)for(auto address:s.selectedPointers){
  Proxy proxy;proxy.address=address;
  if(!read(proxy.raw,address,sizeof(proxy.raw))){proxy.errors=1;++s.partErrors;s.proxies.push_back(proxy);continue;}
  auto count=Get<uint32_t>(proxy.raw,0xa0)&63;std::array<uint64_t,63> pointers{};
  if(count && !read(pointers.data(),address+0xd8,size_t(count)*8)){proxy.errors=2;++s.partErrors;}else for(unsigned i=0;i<count;++i){
   if(s.parts.size()==PartLimit){s.partsComplete=false;break;}
   Part p;p.proxy=address;p.address=pointers[i];p.index=i;
   if(!p.address){p.errors=1;}else if(!read(p.raw,p.address,sizeof(p.raw)))p.errors=2;else{
    auto vt=Get<uint64_t>(p.raw,0);bool known{};
    for(auto pair:std::array<std::array<uint64_t,2>,5>{{{0x309b1f0,0x21bd520},{0x3092160,0x21bd520},{0x30991a0,0x21bd5c0},{0x309d6d0,0x21bd520},{0x30a1240,0x222d000}}})if(vt==image+pair[0]){known=Ptr(read,vt+0xf8,p.callback) && p.callback==image+pair[1];break;}
    if(!known)p.errors=4;else if(vt==image+0x30991a0){
     p.definition=Get<uint64_t>(p.raw,0x28);if(!p.definition || !read(p.model,p.definition,sizeof(p.model)))p.errors|=8;else {std::array<uint8_t,0x100> after{};if(!read(after.data(),p.definition,after.size()) || memcmp(after.data(),p.model,after.size()))p.errors|=16;}
     for(unsigned c=0;c<4;++c){p.childAddresses[c]=Get<uint64_t>(p.raw,0x30+c*8);if(p.childAddresses[c]){std::array<uint8_t,0x20> after{};if(!read(p.children[c],p.childAddresses[c],sizeof(p.children[c])))p.childErrors|=1u<<c;else if(!read(after.data(),p.childAddresses[c],after.size()) || memcmp(after.data(),p.children[c],after.size()))p.childErrors|=1u<<(c+4);}}
    }
    std::array<uint8_t,0x80> after{};p.stable=read(after.data(),p.address,after.size()) && !memcmp(after.data(),p.raw,after.size());
   }
   s.partErrors+=p.errors!=0 || p.childErrors!=0 || !p.stable;s.parts.push_back(p);
  }
  std::array<uint8_t,0xd8> after{};std::array<uint64_t,63> pointersAfter{};proxy.stable=read(after.data(),address,after.size()) && !memcmp(after.data(),proxy.raw,after.size()) && (!count || (read(pointersAfter.data(),address+0xd8,size_t(count)*8) && !memcmp(pointersAfter.data(),pointers.data(),size_t(count)*8)));
  s.partErrors+=!proxy.stable;s.proxies.push_back(proxy);
 }
 uint64_t list{};uint32_t count{},capacity{},gen{};s.selectedStable=s.selectedRead && Ptr(read,s.state+0x2230,list) && read(&capacity,s.state+0x2238,4) && read(&count,s.state+0x2270,4) && read(&gen,s.state+0x864c,4) && list==s.selected && count==s.count && capacity==s.capacity && gen==s.generation;
 if(s.selectedStable && s.count){std::vector<uint64_t> after(s.count);s.selectedStable=read(after.data(),s.selected,size_t(s.count)*8) && !memcmp(after.data(),s.selectedPointers.data(),size_t(s.count)*8);}
 uint64_t state{},graph{},manager{};s.ownerStable=Identity(read,image,source,view,state,graph,manager) && state==s.state && graph==s.graph && manager==s.manager;
 if(!s.ownerStable){s.queuesComplete=false;s.partsComplete=false;++s.errors;}return s;
}
}
