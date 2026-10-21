#pragma once
#include "UnityCamera.h"
#include <array>
namespace BinocularComponentInput50 {
using CameraMath::Matrix;
struct Result {Matrix pose{},projection{},viewProjection{},inverseProjection{},inverseVP{};float tangents[4]{};};
inline bool Build(const Matrix& depth,const Matrix poses[2],const Transport::Eye optics[2],Result& output){
 Result r;for(auto& pose:std::array<Matrix,2>{poses[0],poses[1]})if(!CameraMath::Rigid(pose))return false;
 for(unsigned i=0;i<12;++i)if(std::abs(poses[0].m[i]-poses[1].m[i])>1e-4f)return false;
 r.pose=poses[0];for(unsigned i=12;i<15;++i)r.pose.m[i]=(poses[0].m[i]+poses[1].m[i])*.5f;
 auto graphics=UnityCamera::ToGraphics(r.pose);auto view=CameraMath::InverseRigid(graphics);
 float l=INFINITY,rr=-INFINITY,u=-INFINITY,d=INFINITY;
 for(unsigned eye=0;eye<2;++eye){
  Matrix projection{},inverse;if(!UnityCamera::Projection(depth,optics[eye],projection) || !CameraMath::Inverse(projection,inverse))return false;
  auto world=UnityCamera::ToGraphics(poses[eye]);
  float delta[3]{world.m[12]-graphics.m[12],world.m[13]-graphics.m[13],world.m[14]-graphics.m[14]};
  if(std::abs(delta[0]*graphics.m[8]+delta[1]*graphics.m[9]+delta[2]*graphics.m[10])>1e-4f)return false;
  auto inverseVP=CameraMath::Multiply(inverse,world);
  for(unsigned corner=0;corner<8;++corner){
   const float ndc[4]{corner&1?1.f:-1.f,corner&2?1.f:-1.f,corner&4?1.f:0.f,1.f};double h[4]{};
   for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)h[j]+=double(ndc[k])*inverseVP.m[k*4+j];
   if(!std::isfinite(h[3]) || std::abs(h[3])<1e-8)return false;
   double local[3]{};for(unsigned j=0;j<3;++j){local[j]=view.m[12+j];for(unsigned k=0;k<3;++k)local[j]+=(h[k]/h[3])*view.m[k*4+j];}
   if(!std::isfinite(local[0]+local[1]+local[2]) || -local[2]<.001)return false;
   float x=float(local[0]/-local[2]),y=float(local[1]/-local[2]);l=(std::min)(l,x);rr=(std::max)(rr,x);d=(std::min)(d,y);u=(std::max)(u,y);
  }
 }
 // Numerical padding only; extents come from both actual eye frustum corners.
 const float epsilon=1e-5f;l-=epsilon;rr+=epsilon;d-=epsilon;u+=epsilon;
 if(!std::isfinite(l+rr+d+u) || rr-l<.001f || u-d<.001f)return false;
 r.projection={};r.projection.m[0]=2/(rr-l);r.projection.m[5]=2/(u-d);r.projection.m[8]=(rr+l)/(rr-l);r.projection.m[9]=(u+d)/(u-d);r.projection.m[10]=depth.m[10];r.projection.m[11]=-1;r.projection.m[14]=depth.m[14];
 if(!CameraMath::Inverse(r.projection,r.inverseProjection))return false;
 r.viewProjection=CameraMath::Multiply(view,r.projection);r.inverseVP=CameraMath::Multiply(r.inverseProjection,graphics);
 r.tangents[0]=l;r.tangents[1]=rr;r.tangents[2]=u;r.tangents[3]=d;output=r;return true;
}
}
