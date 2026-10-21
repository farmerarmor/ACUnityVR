#pragma once
#include <cstdint>
namespace PrimaryScene52 {
using Reader=bool(*)(void*,uint64_t,size_t) noexcept;
using Bind=void(__fastcall*)(void*,void*);using Task=void(__fastcall*)(void*);
using Sort=void(__fastcall*)(void*,void*,int64_t,uint8_t);using Prepared=void(*)(void*);
// Exact native219f480: source state Bind/QT, then source component-list sort.
// Insert the accepted renderer camera data between Bind and QT; each native call once.
enum class Result{RefusedBefore,Complete,RefusedAfter};
inline Result Execute(Reader read,void* scene,void* source,void* owner,void* world,Bind bind,Task qt,Sort sort,Prepared prepared,void* data){
 uint64_t q{};uint32_t count{};uint64_t vector{};auto at=reinterpret_cast<uint64_t>(scene),s=reinterpret_cast<uint64_t>(source),st=reinterpret_cast<uint64_t>(owner),w=reinterpret_cast<uint64_t>(world);
 if(!scene || !source || !owner || !world || !bind || !qt || !sort || !prepared ||
 !read(&q,at,8) || q!=w || !read(&q,at+8,8) || q!=s || !read(&q,w+0x1e8,8) || q!=at || !read(&q,s+0x430,8) || q!=st || !read(&q,st+0x8208,8) || q!=s)return Result::RefusedBefore;
 bind(owner,source);prepared(data);qt(owner);
 // Match the native body: read the list after Bind/QT, not before.
 if(!read(&vector,at+0x13d8,8) || !read(&count,at+0x13e0,4) || count>1048576 || (count && !vector))return Result::RefusedAfter;
 sort(reinterpret_cast<void*>(vector),reinterpret_cast<void*>(vector+uint64_t(count)*8),count,0);return Result::Complete;
}
}
