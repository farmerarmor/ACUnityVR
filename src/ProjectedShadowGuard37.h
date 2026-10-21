#pragma once
#include <cstdint>
namespace ProjectedShadowGuard {
bool Install(uint8_t* image,void* log);
void Enable(bool value);
void Restore();
uint64_t MissingTargets();
void Report(void* log);
}
