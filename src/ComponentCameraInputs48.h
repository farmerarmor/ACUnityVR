#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace ComponentCameraInputs48 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
template<class T>T Get(const void* p,size_t offset){T v{};memcpy(&v,static_cast<const uint8_t*>(p)+offset,sizeof(v));return v;}
struct Child {uint64_t address{},geometry{};uint32_t flags{},count{},errors{},reserved{};};
constexpr unsigned ChildLimit=64;
struct Snapshot {
 uint64_t component{},context{},owner{},primary{},frustum{},member{},childrenPointer{};
 uint32_t sourceFrame{},primaryFrame{},primaryIndex=3,childCount{},childrenCaptured{},errors{};
 bool identity{},cameraRead{},childrenStable{};
 uint8_t componentRaw[0xc0]{},memberRaw[0xd0]{},viewRaw[0x440]{},planes[0xc0]{};
 std::array<Child,ChildLimit> children{};
};
inline bool Ptr(Reader read,uint64_t at,uint64_t& q){return read(&q,at,8);}
inline void CaptureInto(Snapshot& s,Reader read,uint64_t image,uint64_t surface,uint64_t source,uint64_t left,uint64_t right,uint64_t component,uint64_t context,uint64_t expectedCallback=0){
 memset(&s,0,sizeof(s));s.primaryIndex=3;s.component=component;s.context=context;uint64_t q{};
 if(!read(s.componentRaw,component,sizeof(s.componentRaw)) || Get<uint64_t>(s.componentRaw,0)!=image+0x30a1370 || !Ptr(read,image+0x30a1370+0x138,q) || q!=(expectedCallback?expectedCallback:image+0x22425a0)){s.errors|=1;return;}
 s.member=Get<uint64_t>(s.componentRaw,0x40);
 if(!s.member || !read(s.memberRaw,s.member,sizeof(s.memberRaw)) || Get<uint64_t>(s.memberRaw,0)!=image+0x30a1240 || Get<uint64_t>(s.memberRaw,0x18)!=component || !Ptr(read,image+0x30a1240+0xf0,q) || q!=image+0x2242740){s.errors|=2;return;}
 s.identity=true;
 if(!Ptr(read,context,s.owner) || !s.owner || !Ptr(read,s.owner+0x890,s.primary) || !s.primary){s.errors|=4;return;}
 s.primaryIndex=s.primary==source?0:s.primary==left?1:s.primary==right?2:3;
 uint64_t back{},graph{};
 if(!read(s.viewRaw,s.primary,sizeof(s.viewRaw)) || Get<uint64_t>(s.viewRaw,0x420)!=surface || !Get<uint64_t>(s.viewRaw,0x430) || !Ptr(read,Get<uint64_t>(s.viewRaw,0x430)+0x8208,back) || back!=s.primary || !Ptr(read,s.primary+0x440,graph) || !graph || !Ptr(read,graph,back) || back!=s.primary || !Ptr(read,graph+8,back) || back!=Get<uint64_t>(s.viewRaw,0x430)){s.errors|=8;return;}
 s.frustum=Get<uint64_t>(s.viewRaw,0x428);
 if(!s.frustum || !read(s.planes,s.frustum,sizeof(s.planes)) || !read(&s.sourceFrame,source+0x294,4)){s.errors|=16;return;}
 s.primaryFrame=Get<uint32_t>(s.viewRaw,0x294);s.cameraRead=true;
 s.childrenPointer=Get<uint64_t>(s.memberRaw,0xa0);s.childCount=Get<uint16_t>(s.memberRaw,0xaa);
 if(s.childCount>ChildLimit){s.errors|=32;return;}
 std::array<uint64_t,ChildLimit> pointers{},after{};
 if(s.childCount && (!s.childrenPointer || !read(pointers.data(),s.childrenPointer,size_t(s.childCount)*8))){s.errors|=64;return;}
 for(unsigned i=0;i<s.childCount;++i){
  auto& c=s.children[i];c.address=pointers[i];uint8_t raw[0xa0]{},verify[0xa0]{};
  if(!c.address || !read(raw,c.address,sizeof(raw))){c.errors=1;continue;}
  if(Get<uint64_t>(raw,0)!=image+0x30a1120 || !Ptr(read,image+0x30a1120+0xf8,q) || q!=image+0x21bd520){c.errors=2;continue;}
  c.geometry=Get<uint64_t>(raw,8);c.flags=Get<uint32_t>(raw,0x10);c.count=Get<uint32_t>(raw,0x94);
  if(!read(verify,c.address,sizeof(verify)) || memcmp(raw,verify,sizeof(raw)))c.errors=4;
  ++s.childrenCaptured;
 }
 uint64_t pointer{},owner{},primary{},member{};uint16_t count{};
 s.childrenStable=Ptr(read,s.member+0xa0,pointer) && read(&count,s.member+0xaa,2) && pointer==s.childrenPointer && count==s.childCount && (!count || (read(after.data(),pointer,size_t(count)*8) && !memcmp(after.data(),pointers.data(),size_t(count)*8)));
 if(!s.childrenStable)s.errors|=128;
 if(!Ptr(read,context,owner) || owner!=s.owner || !Ptr(read,owner+0x890,primary) || primary!=s.primary || !Ptr(read,component+0x40,member) || member!=s.member)s.errors|=256;
 return;
}
// Convenience wrapper for offline readers; native callbacks capture in place.
inline Snapshot Capture(Reader read,uint64_t image,uint64_t surface,uint64_t source,uint64_t left,uint64_t right,uint64_t component,uint64_t context,uint64_t expectedCallback=0){
 Snapshot s;CaptureInto(s,read,image,surface,source,left,right,component,context,expectedCallback);return s;
}
}
