#pragma once
#include <cmath>
namespace GIRayMath {
// Native GI uses (pixel+.5)*scale+bias and then flips Y. A row-vector
// projection maps view X/Y to NDC as X*P00/depth-P20, Y*P11/depth-P21.
inline bool Correct(const float* projection,const float* high,const float* pixel,unsigned width,unsigned height,float* highOut,float* pixelOut){
 if(!width || !height || width>16384 || height>16384)return false;
 for(unsigned i=0;i<16;++i)if(!std::isfinite(projection[i]))return false;
 for(unsigned i=0;i<4;++i)if(!std::isfinite(high[i]) || !std::isfinite(pixel[i]))return false;
 float x=projection[0],y=projection[5];if(x<=.01f || y<=.01f || std::abs(projection[11]+1)>.0001f)return false;
 auto near=[](float a,float b){return std::abs(a-b)<.00001f;};
 if(!near(high[0],-1/x) || !near(pixel[0],-1/x) || !near(high[1],-1/y) || !near(pixel[1],-1/y) ||
    !near(pixel[2],2/(float(width)*x)) || !near(pixel[3],2/(float(height)*y)) ||
    !near(high[2],16*pixel[2]) || !near(high[3],16*pixel[3]))return false;
 float dx=projection[8]/x,dy=-projection[9]/y;
 for(unsigned i=0;i<4;++i){highOut[i]=high[i];pixelOut[i]=pixel[i];}
 highOut[0]+=dx;pixelOut[0]+=dx;highOut[1]+=dy;pixelOut[1]+=dy;return true;
}
}
