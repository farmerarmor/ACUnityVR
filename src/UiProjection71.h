#pragma once
#include "UnityCamera.h"
namespace UiProjection71 {
struct Settings {bool enabled=true;float widthDegrees=60.f,distance=2.f;};
inline bool SettingsValid(const Settings& s){return std::isfinite(s.widthDegrees+s.distance) && s.widthDegrees>=30 && s.widthDegrees<=140 && s.distance>=.5f && s.distance<=10;}
inline bool NativeMatrix(const CameraMath::Matrix& m,unsigned width,unsigned height){
 if(!width || !height)return false;
 for(float f:m.m)if(!std::isfinite(f))return false;
 for(unsigned i=0;i<16;++i)if(i!=0 && i!=5 && i!=10 && i!=11 && i!=14 && std::abs(m.m[i])>1e-6f)return false;
 return std::abs(m.m[5]-2.41421356237f)<.001f && std::abs(m.m[0]-m.m[5]*float(height)/float(width))<.001f && std::abs(m.m[11]+1)<1e-6f && m.m[10]<-1 && m.m[10]>-1.01f && m.m[14]<-.01f && m.m[14]>-1;
}
inline bool Build(const CameraMath::Matrix& native,const Transport::Eye (&eyes)[2],unsigned eye,const Settings& s,CameraMath::Matrix& out){
 if(eye>1 || !SettingsValid(s))return false;
 for(auto& e:eyes){
  auto q=e.pose.orientation;auto p=e.pose.position;
  float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  if(!std::isfinite(norm+p.x+p.y+p.z) || std::abs(norm-1)>.01f)return false;
 }
 CameraMath::Matrix runtime{};if(!UnityCamera::Projection(native,eyes[eye],runtime))return false;
 // The native local UI queue uses a 45-degree vertical camera and a logical
 // 16:9 layout. Its earlier world-marker queues use the untouched world camera.
 constexpr float aspect=16.f/9.f;float halfWidth=std::tan(s.widthDegrees*.008726646259971648f);
 auto& a=eyes[0].pose.position;auto& b=eyes[1].pose.position;auto& p=eyes[eye].pose.position;
 auto offset=CameraMath::Rotate(CameraMath::Inverse(eyes[eye].pose.orientation),{p.x-(a.x+b.x)*.5f,p.y-(a.y+b.y)*.5f,p.z-(a.z+b.z)*.5f});
 if(!std::isfinite(offset.x+offset.y+offset.z) || std::abs(offset.x)>.08f || std::abs(offset.y)>.08f || std::abs(offset.z)>.08f)return false;
 out=native;out.m[0]=(native.m[5]/aspect)*runtime.m[0]*halfWidth;out.m[5]=native.m[5]*runtime.m[5]*(halfWidth/aspect);
 // Negative Z projection: centre ray is -eyeOffset/distance. These signs give
 // binocular convergence at the configurable panel distance, in head space.
 out.m[8]=runtime.m[8]+runtime.m[0]*offset.x/s.distance;out.m[9]=runtime.m[9]+runtime.m[5]*offset.y/s.distance;
 for(float f:out.m)if(!std::isfinite(f))return false;return true;
}
}
