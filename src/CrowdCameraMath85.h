#pragma once
#include "UnityCamera.h"
namespace CrowdCameraMath85 {
inline bool Build(const CameraMath::Matrix& pose,float values[3][4]){
 if(!CameraMath::Rigid(pose))return false;
 for(unsigned i=0;i<3;++i){values[0][i]=pose.m[12+i];values[2][i]=pose.m[4+i];}values[0][3]=values[2][3]=0;
 // Matrix stores rotated native basis vectors as rows. Quaternion extraction
 // uses their transpose, matching CameraMath::Rotation and native camera maths.
 const auto& m=pose.m;float q[4]{},trace=m[0]+m[5]+m[10];
 if(trace>0){float v=std::sqrt(trace+1)*2;q[3]=v*.25f;q[0]=(m[6]-m[9])/v;q[1]=(m[8]-m[2])/v;q[2]=(m[1]-m[4])/v;}
 else if(m[0]>m[5] && m[0]>m[10]){float v=std::sqrt(1+m[0]-m[5]-m[10])*2;q[3]=(m[6]-m[9])/v;q[0]=v*.25f;q[1]=(m[4]+m[1])/v;q[2]=(m[8]+m[2])/v;}
 else if(m[5]>m[10]){float v=std::sqrt(1+m[5]-m[0]-m[10])*2;q[3]=(m[8]-m[2])/v;q[0]=(m[4]+m[1])/v;q[1]=v*.25f;q[2]=(m[9]+m[6])/v;}
 else{float v=std::sqrt(1+m[10]-m[0]-m[5])*2;q[3]=(m[1]-m[4])/v;q[0]=(m[8]+m[2])/v;q[1]=(m[9]+m[6])/v;q[2]=v*.25f;}
 float norm=0;for(float v:q){if(!std::isfinite(v))return false;norm+=v*v;}if(norm<.99f || norm>1.01f)return false;
 norm=1/std::sqrt(norm);for(unsigned i=0;i<4;++i)values[1][i]=q[i]*norm;
 auto rebuilt=CameraMath::Rotation({values[1][0],values[1][1],values[1][2],values[1][3]});
 for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)if(std::abs(rebuilt.m[row*4+col]-pose.m[row*4+col])>.002f)return false;
 return true;
}
}
