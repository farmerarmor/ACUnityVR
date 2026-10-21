#pragma once
#include "CameraMath.h"
namespace UiProjectionScope65 {
using Task=void(__fastcall*)(void*,void*,void*);
// Only the serial, owned-eye UI callback enters this scope. Its native tail
// restores world view/projection; restore this separate 64-byte UI camera even
// on an exception. Never overwrite an unexpected native replacement.
inline bool Invoke(float* field,const CameraMath::Matrix& before,const CameraMath::Matrix& changed,Task native,void* task,void* backend,void* context){
 bool restored=false;memcpy(field,changed.m,64);
 __try{native(task,backend,context);}
 __finally{if(!memcmp(field,changed.m,64)){memcpy(field,before.m,64);restored=true;}}
 return restored;
}
}
