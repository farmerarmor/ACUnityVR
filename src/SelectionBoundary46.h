#pragma once
#include <windows.h>
#include <cstdio>
namespace SelectionBoundary46 {
using Observer=void(*)(unsigned,void*);
bool Install(unsigned char* image,void* left,void* right,Observer observer,FILE* log);
void Stop();
}
