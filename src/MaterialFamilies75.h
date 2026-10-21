#pragma once
#include <cstdint>
namespace MaterialFamilies75 {
constexpr unsigned Groups=7,EyeGpuLimit=24,TargetGpuReserve=9;
struct Shape {unsigned kind{},indices{},instances{},eye{};bool replay{};};
inline bool Eligible(const Shape& s){return s.eye>=1 && s.eye<=2 && !s.replay;}
inline unsigned Group(const Shape& s,unsigned pair){
 if(!Eligible(s))return 0;
 if(s.kind==20 && s.instances){if(pair==1 && s.indices==6)return 1;if(pair>=2 && pair<=5 && s.indices==384)return pair;}
 if(s.kind==39 && !s.instances && !(s.indices&3u) && pair>=6 && pair<=7)return pair;
 return 0;
}
inline bool Hide(unsigned group,bool trees,unsigned particles,bool smallSmoke=false,bool chimneySmoke=false){
 if(chimneySmoke && (group==2 || group==5))return true;
 if(group>=6 && group<=7)return trees;
 if(group==5)return smallSmoke;
 return group>=2 && group<=4 && particles==group-1;
}
inline unsigned TargetLimit(unsigned group){return group==2 || group==5?2u:1u;}
inline bool GpuEligible(unsigned total,unsigned ordinary,unsigned group,unsigned groupCopies){
 if(total>=EyeGpuLimit || group>Groups)return false;
 return group?groupCopies<TargetLimit(group):ordinary<EyeGpuLimit-TargetGpuReserve;
}
}
