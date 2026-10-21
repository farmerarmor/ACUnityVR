#pragma once
#include <cstdint>
namespace DispatchKnown77 {
// The context is held by the owning renderer. Only previously validated and
// installed code targets qualify; unknown/currently changing tables go slow.
template<class Read,class Known> bool ReadKnown(uintptr_t context,uintptr_t& table,uintptr_t& target,Read read,Known known){
 table=target=0;uintptr_t a{},b{},again{},repeat{};
 if(!context || !read(context,a) || !a || (a&7) || a>UINTPTR_MAX-42*sizeof(uintptr_t) || !read(a+41*sizeof(uintptr_t),b) || !b || !known(b))return false;
 if(!read(context,again) || again!=a || !read(a+41*sizeof(uintptr_t),repeat) || repeat!=b)return false;
 table=a;target=b;return true;
}
}
