#pragma once
#include "SharedPair.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace NativeOptics {
// A native centered perspective covering both runtime eye frusta. Each eye's
// original native image is cropped in angle-tangent space for XR submission.
// No eye substitution, depth reprojection, temporal interleaving or toe-in.
struct Camera{float halfX{},halfY{},fovY{};};
struct Rect{int32_t x{},y{},width{},height{};float left{},right{},up{},down{};};
inline bool Build(const Transport::Tracking& t,float aspect,Camera& c){
 if(!t.valid || !std::isfinite(aspect) || aspect<=0)return false;
 float x{},y{};
 for(auto& e:t.eyes){
  float l=std::tan(e.left),r=std::tan(e.right),u=std::tan(e.up),d=std::tan(e.down);
  if(!std::isfinite(l+r+u+d) || l>=0 || r<=0 || d>=0 || u<=0)return false;
  x=(std::max)(x,(std::max)(-l,r));y=(std::max)(y,(std::max)(u,-d));
 }
 c.halfY=(std::max)(y,x/aspect);c.halfX=c.halfY*aspect;c.fovY=2*std::atan(c.halfY);
 return std::isfinite(c.fovY) && c.halfX>0 && c.halfY>0 && c.fovY<3.0f;
}
inline bool Crop(const Camera& c,const Transport::Eye& e,int32_t width,int32_t height,Rect& r){
 if(width<=0 || height<=0 || c.halfX<=0 || c.halfY<=0)return false;
 float x0=(std::tan(e.left)+c.halfX)/(2*c.halfX)*float(width);
 float x1=(std::tan(e.right)+c.halfX)/(2*c.halfX)*float(width);
 float y0=(c.halfY-std::tan(e.up))/(2*c.halfY)*float(height);
 float y1=(c.halfY-std::tan(e.down))/(2*c.halfY)*float(height);
 if(!std::isfinite(x0+x1+y0+y1) || x0<-.01f || y0<-.01f || x1>float(width)+.01f || y1>float(height)+.01f)return false;
 r.x=(std::max)(0,int32_t(std::floor(x0)));r.y=(std::max)(0,int32_t(std::floor(y0)));
 auto right=(std::min)(width,int32_t(std::ceil(x1)));auto bottom=(std::min)(height,int32_t(std::ceil(y1)));
 r.width=right-r.x;r.height=bottom-r.y;if(r.width<=0 || r.height<=0)return false;
 // Submit the exact rounded pixel-edge rays, keeping geometry consistent with
 // the rect even when requested runtime rays fall between source texels.
 r.left=std::atan(2*c.halfX*float(r.x)/float(width)-c.halfX);
 r.right=std::atan(2*c.halfX*float(right)/float(width)-c.halfX);
 r.up=std::atan(c.halfY-2*c.halfY*float(r.y)/float(height));
 r.down=std::atan(c.halfY-2*c.halfY*float(bottom)/float(height));return true;
}
}
