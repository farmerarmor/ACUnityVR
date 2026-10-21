#pragma once
#include "SharedPair.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace HiddenAreaMask {
struct Point {double x,y;};
struct Triangle {Point p[3];double x0,y0,x1,y1;};
// Separating-axis rectangle/triangle intersection. Touching counts as visible.
inline bool Intersects(const Triangle& t,double x0,double y0,double x1,double y1){
 constexpr double eps=1e-7;
 if(t.x1<x0-eps || t.x0>x1+eps || t.y1<y0-eps || t.y0>y1+eps)return false;
 for(unsigned i=0;i<3;++i){auto a=t.p[i],b=t.p[(i+1)%3];double nx=-(b.y-a.y),ny=b.x-a.x;
  double lo=1e300,hi=-1e300;for(auto p:t.p){double v=nx*p.x+ny*p.y;lo=std::min(lo,v);hi=std::max(hi,v);}
  double rlo=nx*(nx>=0?x0:x1)+ny*(ny>=0?y0:y1),rhi=nx*(nx>=0?x1:x0)+ny*(ny>=0?y1:y0);
  if(hi<rlo-eps || lo>rhi+eps)return false;
 }return true;
}
inline bool Build(const Transport::VisibilityMesh& mesh,const Transport::Eye& fov,unsigned width,unsigned height,unsigned margin,std::vector<uint8_t>& hidden,unsigned& count){
 hidden.clear();count=0;
 if(!width || !height || width>16384 || height>16384 || margin>256 || mesh.vertexCount<3 || mesh.vertexCount>Transport::VisibilityMaxVertices || !mesh.indexCount || mesh.indexCount>Transport::VisibilityMaxIndices || mesh.indexCount%3)return false;
 double l=std::tan(fov.left),r=std::tan(fov.right),u=std::tan(fov.up),d=std::tan(fov.down);
 if(!std::isfinite(l+r+u+d) || l>=0 || r<=0 || d>=0 || u<=0)return false;
 std::vector<Point> vertices;vertices.reserve(mesh.vertexCount);
 for(unsigned i=0;i<mesh.vertexCount;++i){auto p=mesh.vertices[i];Point v{(p.x-l)/(r-l)*width,(u-p.y)/(u-d)*height};if(!std::isfinite(v.x) || !std::isfinite(v.y) || std::abs(v.x)>width*16. || std::abs(v.y)>height*16.)return false;vertices.push_back(v);}
 std::vector<Triangle> triangles;bool centreVisible=false;
 const double cx=-l/(r-l)*width,cy=u/(u-d)*height;
 for(unsigned i=0;i<mesh.indexCount;i+=3){Triangle t{};t.x0=t.y0=1e300;t.x1=t.y1=-1e300;
  for(unsigned j=0;j<3;++j){auto index=mesh.indices[i+j];if(index>=vertices.size())return false;t.p[j]=vertices[index];t.x0=std::min(t.x0,t.p[j].x);t.y0=std::min(t.y0,t.p[j].y);t.x1=std::max(t.x1,t.p[j].x);t.y1=std::max(t.y1,t.p[j].y);}
  auto a=t.p[0],b=t.p[1],c=t.p[2];if(std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))<1e-10)continue;
  centreVisible|=Intersects(t,cx,cy,cx,cy);triangles.push_back(t);
 }
 if(triangles.empty() || !centreVisible)return false;
 unsigned tw=(width+15)/16,th=(height+15)/16;hidden.resize(size_t(tw)*th);
 for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){double x0=double(x*16)-margin,y0=double(y*16)-margin,x1=double(std::min((x+1)*16,width))+margin,y1=double(std::min((y+1)*16,height))+margin;bool visible=false;
  for(auto& t:triangles)if(Intersects(t,x0,y0,x1,y1)){visible=true;break;}
  hidden[size_t(y)*tw+x]=visible?0:1;count+=!visible;
 }return true;
}
}
