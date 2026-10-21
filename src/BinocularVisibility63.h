#pragma once
#include "UnityCamera.h"
#include <algorithm>
// Row-vector D3D clip inequalities from the exact renderer poses/projection.
namespace BinocularVisibility63 {
struct Frustums {float planes[2][6][4]{};};
inline bool Build(const CameraMath::Matrix& depth,const CameraMath::Matrix poses[2],const Transport::Eye* optics,Frustums& result) noexcept {
 if(!poses || !optics)return false;Frustums candidate{};
 for(unsigned eye=0;eye<2;++eye){
  if(!CameraMath::Rigid(poses[eye]))return false;
  float angles[]={optics[eye].left,optics[eye].right,optics[eye].up,optics[eye].down};
  for(float a:angles)if(!std::isfinite(a)||std::abs(a)>=1.5f)return false;
  if(!(angles[0]<0 && angles[1]>0 && angles[2]>0 && angles[3]<0))return false;
  CameraMath::Matrix projection{};if(!UnityCamera::Projection(depth,optics[eye],projection))return false;
  auto vp=CameraMath::Multiply(CameraMath::InverseRigid(UnityCamera::ToGraphics(poses[eye])),projection);
  for(float x:vp.m)if(!std::isfinite(x))return false;
  for(unsigned p=0;p<6;++p){auto& n=candidate.planes[eye][p];for(unsigned j=0;j<4;++j){auto row=vp.m+j*4;n[j]=p==4?row[2]:p==5?row[3]-row[2]:row[3]+((p&1)?-1.f:1.f)*row[p/2];}
   float length=std::hypot(n[0],n[1],n[2]);if(!std::isfinite(length))return false;
   if(length<1e-8f){if(p<4 || n[3]<=0)return false;n[0]=n[1]=n[2]=0;n[3]=1;}else for(float& x:n)x/=length;
   for(float x:n)if(!std::isfinite(x))return false;
  }
 }
 result=candidate;return true;
}
inline unsigned EyeMask(const Frustums& views,const float lo[3],const float hi[3]) noexcept {
 for(unsigned j=0;j<3;++j)if(!std::isfinite(lo[j])||!std::isfinite(hi[j])||lo[j]>hi[j]||hi[j]-lo[j]>100)return 0;
 unsigned mask=0;for(unsigned eye=0;eye<2;++eye){bool inside=true;for(auto& n:views.planes[eye]){
   for(float x:n)if(!std::isfinite(x))return 0;
   float maximum=n[3];for(unsigned j=0;j<3;++j)maximum+=n[j]*(n[j]>=0?hi[j]:lo[j]);
   if(!std::isfinite(maximum))return 0;
   if(maximum<0){inside=false;break;}
  }if(inside)mask|=1u<<eye;
 }return mask;
}
}
