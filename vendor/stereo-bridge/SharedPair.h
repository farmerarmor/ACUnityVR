#pragma once
#include <windows.h>
#include <cstdint>
#include <cwchar>
#include <cstddef>
namespace Transport {
inline constexpr uint32_t Magic=0x50545356;
#ifdef ACUNITYVR_VISIBILITY_MASK
inline constexpr uint32_t Version=9;
inline constexpr unsigned VisibilityMaxVertices=512,VisibilityMaxIndices=1536;
struct VisibilityVertex {float x,y;};
struct VisibilityMesh {uint32_t vertexCount,indexCount;VisibilityVertex vertices[VisibilityMaxVertices];uint32_t indices[VisibilityMaxIndices];};
struct Visibility {uint32_t valid,reserved;VisibilityMesh eyes[2];};
#endif
#ifndef ACUNITYVR_VISIBILITY_MASK
inline constexpr uint32_t Version=8;
#endif
struct Quaternion {float x,y,z,w;};
struct Vector {float x,y,z;};
struct Pose {Quaternion orientation;Vector position;};
struct Eye {Pose pose;float left,right,up,down;};
struct Controller {Pose aim;uint32_t valid;};
struct Gamepad {
    uint32_t valid;
    uint16_t buttons;uint8_t leftTrigger,rightTrigger;
    int16_t leftX,leftY,rightX,rightY;
};
static_assert(sizeof(Gamepad)==16);
struct alignas(8) Tracking {
    uint64_t id,tick;
    Pose head;
    Eye eyes[2];
    uint32_t valid;
    Controller rightController;
    Gamepad gamepad;
};
struct RenderInfo {Tracking tracking;uint32_t mode,eyeMask;};
static_assert(sizeof(Tracking)==184 && sizeof(RenderInfo)==192);
// Identical ABI in the x64 game and x64 companion. GPU keyed mutex protects
// texture contents + frameId; generation publishes a new texture description.
struct alignas(8) Header {
    uint32_t magic,version,pid,reserved;
    volatile LONG generation;
    uint32_t width,height,format;
    uint64_t sharedHandle;
    LUID adapter;
    uint64_t frameId;
    uint32_t swapEyes,reserved2;
    volatile LONG trackingLock;
    uint32_t reserved3; // Experiment30 only: 1=compiled clean, 2=profile; 0=unspecified.
    Tracking tracking;
    RenderInfo rendered;
#ifdef ACUNITYVR_VISIBILITY_MASK
    volatile LONG visibilityLock,visibilityGeneration;
    Visibility visibility;
#endif
};
#ifdef ACUNITYVR_VISIBILITY_MASK
static_assert(offsetof(Header,visibilityLock)==448 && sizeof(Header)==20960);
inline bool ReadVisibility(Header* h,uint32_t& generation,Visibility& result){
 if(!h)return false;
 auto published=uint32_t(InterlockedCompareExchange(&h->visibilityGeneration,0,0));
 if(published==generation || InterlockedCompareExchange(&h->visibilityLock,1,0))return false;
 result=h->visibility;generation=uint32_t(h->visibilityGeneration);InterlockedExchange(&h->visibilityLock,0);return true;
}
inline bool WriteVisibility(Header* h,const Visibility& value){
 if(!h || InterlockedCompareExchange(&h->visibilityLock,1,0))return false;
 h->visibility=value;InterlockedIncrement(&h->visibilityGeneration);InterlockedExchange(&h->visibilityLock,0);return true;
}
#else
static_assert(sizeof(Header)==448);
#endif
inline bool ReadTracking(Header* h,Tracking& result) {
    if(!h || InterlockedCompareExchange(&h->trackingLock,1,0))return false;
    result=h->tracking;InterlockedExchange(&h->trackingLock,0);return true;
}
inline void WriteTracking(Header* h,const Tracking& value) {
    if(!h || InterlockedCompareExchange(&h->trackingLock,1,0))return;
    h->tracking=value;InterlockedExchange(&h->trackingLock,0);
}
// A contended nonblocking mailbox read is not loss of tracking. Preserve the
// last sample briefly, but honour newly received invalid data and sample age.
struct TrackingReader {
    Tracking latest{};
    uint64_t reused{},rejected{};
    bool Read(Header* h,Tracking& result,uint64_t now) {
        if(!h){latest={};++rejected;return false;}
        Tracking incoming{};
        if(ReadTracking(h,incoming))latest=incoming;
        else ++reused;
        if(!latest.valid || !latest.tick || now<latest.tick || now-latest.tick>=250){++rejected;return false;}
        result=latest;return true;
    }
};
// Distinct private live experiments can coexist as dormant modules without
// reusing a stopped mapping. ABI9 adds the bounded visibility mailbox; the
// tracking/rendered layout and original header offsets remain unchanged.
#ifndef ACUNITYVR_EXPERIMENT
#define ACUNITYVR_EXPERIMENT 0
#endif
inline void Name(wchar_t (&name)[96],DWORD pid) {
    if constexpr(ACUNITYVR_EXPERIMENT)swprintf_s(name,L"Local\\ACUnityVR-v%u-e%u-%lu",Version,ACUNITYVR_EXPERIMENT,pid);
    else swprintf_s(name,L"Local\\ACUnityVR-v%u-%lu",Version,pid);
}
inline void FrameEventName(wchar_t (&name)[96],DWORD pid) {
    if constexpr(ACUNITYVR_EXPERIMENT)swprintf_s(name,L"Local\\ACUnityVR-frame-v%u-e%u-%lu",Version,ACUNITYVR_EXPERIMENT,pid);
    else swprintf_s(name,L"Local\\ACUnityVR-frame-v%u-%lu",Version,pid);
}
}
