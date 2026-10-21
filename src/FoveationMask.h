#pragma once
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
namespace FoveationMask {
constexpr unsigned Tile=16;
struct Mask {unsigned width{},height{},fullTiles{},coarseTiles{};float centreX{},centreY{};std::vector<uint8_t> pixels;};
inline bool Build(unsigned width,unsigned height,float left,float right,float up,float down,Mask& result,float radius=.35f){
 const float l=std::tan(left),r=std::tan(right),u=std::tan(up),d=std::tan(down);
 if(!std::isfinite(radius) || radius<.1f || radius>1.f)return false;
 if(width<16 || height<16 || width>16384 || height>16384 || !std::isfinite(l+r+u+d) || l>=0 || r<=0 || u<=0 || d>=0)return false;
 Mask out;out.width=(width+Tile-1)/Tile;out.height=(height+Tile-1)/Tile;out.centreX=-l/(r-l);out.centreY=u/(u-d);
 out.pixels.resize(size_t(out.width)*out.height);
 // Preserve every tile intersecting the central ellipse, not just its centre.

 for(unsigned y=0;y<out.height;++y)for(unsigned x=0;x<out.width;++x){
  float x0=float(x*Tile)/float(width),x1=float((x+1)*Tile)/float(width),y0=float(y*Tile)/float(height),y1=float((y+1)*Tile)/float(height);
  float dx=out.centreX<x0?x0-out.centreX:out.centreX>x1?out.centreX-x1:0;
  float dy=out.centreY<y0?y0-out.centreY:out.centreY>y1?out.centreY-y1:0;
  bool full=(dx*dx+dy*dy)<=radius*radius;out.pixels[size_t(y)*out.width+x]=full?0:1;
  if(full)++out.fullTiles;else ++out.coarseTiles;
 }
 result=std::move(out);return true;
}
inline bool ExtendedLightingShader(unsigned shader){return shader==49 || (shader>=301 && shader<=305);}
inline bool ShaderAllowed(unsigned pass,unsigned shader,bool extendedLighting=false){
 if(pass==0x504a0b0)return shader==276 || (extendedLighting && ExtendedLightingShader(shader));
 if(pass==0x504a150)return shader==391 || shader==387 || shader==30 || shader==4 || shader==461;
 return false;
}
}
