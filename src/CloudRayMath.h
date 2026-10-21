#pragma once
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <algorithm>
#include <limits>
namespace CloudRayMath {
// Native GPU preparation inverts the camera matrix, introducing small float
// rounding in its basis/origin. Captured09/10 matrices measure that rounding.
inline bool SameCamera(const float* native,const float* gpu){
 if(!native || !gpu)return false;float originMagnitude=1;
 for(unsigned i=0;i<16;++i)if(!std::isfinite(native[i]) || !std::isfinite(gpu[i]))return false;
 for(unsigned i=12;i<15;++i)originMagnitude=(std::max)(originMagnitude,(std::max)(std::abs(native[i]),std::abs(gpu[i])));
 for(unsigned i=0;i<16;++i){float tolerance=i>=12 && i<15?8*std::numeric_limits<float>::epsilon()*originMagnitude:2.e-6f;if(std::abs(native[i]-gpu[i])>tolerance)return false;}
 return true;
}
// CS146 uses Dir + NDC.x * Right + NDC.y * Up. The sky composite uses
// the same perspective projection inverse, including its asymmetric offsets.
inline bool Correct(const float* p,const float* world,const float* native,unsigned width,unsigned height,float* result){
 if(!p || !world || !native || !result || !width || !height || width>8192 || height>8192)return false;
 for(unsigned i=0;i<16;++i)if(!std::isfinite(p[i]) || !std::isfinite(world[i]) || !std::isfinite(native[i]))return false;
 if(p[0]<=.01f || p[5]<=.01f || std::abs(p[11]+1)>.001f || std::abs(p[15])>.001f)return false;
 for(auto i:{1,2,3,4,6,7,12,13})if(std::abs(p[i])>.0001f)return false;
 const float expected[]{1.f/width,1.f/height,.5f/width,.5f/height};
 for(unsigned i=0;i<4;++i)if(std::abs(native[i]-expected[i])>1.e-7f)return false;
 // Original basis must come from this exact camera with positive FOV factors.
 // Preserve native thread scale/bias and all W components.
 for(unsigned i=0;i<3;++i)if(std::abs(native[4+i]+world[8+i])>2.e-4f)return false;
 for(unsigned row=0;row<2;++row){float dot{},norm{};
  for(unsigned i=0;i<3;++i){dot+=native[8+4*row+i]*world[4*row+i];norm+=world[4*row+i]*world[4*row+i];}
  if(std::abs(norm-1)>.002f || dot<.01f || dot>10)return false;
  for(unsigned i=0;i<3;++i)if(std::abs(native[8+4*row+i]-dot*world[4*row+i])>2.e-4f)return false;
 }
 if(std::abs(native[7])+std::abs(native[11])+std::abs(native[15])>.0001f)return false;
 std::memcpy(result,native,64);
 for(unsigned i=0;i<3;++i){result[8+i]=world[i]/p[0];result[12+i]=world[4+i]/p[5];result[4+i]=-world[8+i]+p[8]*result[8+i]+p[9]*result[12+i];}
 return true;
}
}
