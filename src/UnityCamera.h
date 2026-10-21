#pragma once
#include "CameraMath.h"
namespace UnityCamera {
using CameraMath::Matrix;using Transport::Pose;using Transport::Eye;
// Confirmed Unity native axes: right, forward, up. D3D/OpenXR: right, up, back.
inline Matrix ToGraphics(const Matrix& n) {
    Matrix g=CameraMath::Identity();
    for(int j=0;j<3;++j){g.m[j]=n.m[j];g.m[4+j]=n.m[8+j];g.m[8+j]=-n.m[4+j];g.m[12+j]=n.m[12+j];}
    return g;
}
inline Matrix ToNative(const Matrix& g) {
    Matrix n=CameraMath::Identity();
    for(int j=0;j<3;++j){n.m[j]=g.m[j];n.m[4+j]=-g.m[8+j];n.m[8+j]=g.m[4+j];n.m[12+j]=g.m[12+j];}
    return n;
}
inline Matrix LevelBase(const Matrix& base) {
    auto n=base;
    const float length=std::hypot(base.m[0],base.m[1]);
    if(length<.001f)return base;
    n.m[0]=base.m[0]/length;n.m[1]=base.m[1]/length;n.m[2]=0;
    n.m[4]=-n.m[1];n.m[5]=n.m[0];n.m[6]=0;
    n.m[8]=n.m[9]=0;n.m[10]=1;
    return n;
}
inline Matrix EyeWorld(const Matrix& base,const Pose& reference,const Pose& eye) {
    auto inverse=CameraMath::Inverse(reference.orientation);
    auto q=CameraMath::Multiply(inverse,eye.orientation);
    auto p=CameraMath::Rotate(inverse,{eye.position.x-reference.position.x,eye.position.y-reference.position.y,eye.position.z-reference.position.z});
    auto relative=CameraMath::Rotation(q);relative.m[12]=p.x;relative.m[13]=p.y;relative.m[14]=p.z;
    return ToNative(CameraMath::Multiply(relative,ToGraphics(base)));
}
inline bool Projection(const Matrix& depth,const Eye& eye,Matrix& out) {
    const float l=std::tan(eye.left),r=std::tan(eye.right),u=std::tan(eye.up),d=std::tan(eye.down);
    if(!std::isfinite(l+r+u+d) || r-l<=0 || u-d<=0 || std::abs(depth.m[11]+1)>.001f || std::abs(depth.m[15])>.001f)return false;
    out={};out.m[0]=2/(r-l);out.m[5]=2/(u-d);out.m[8]=(r+l)/(r-l);out.m[9]=(u+d)/(u-d);
    out.m[10]=depth.m[10];out.m[11]=-1;out.m[14]=depth.m[14];return true;
}
}
