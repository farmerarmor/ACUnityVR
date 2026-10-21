#pragma once
#include <cstdint>
#include <cstdio>
namespace SourceRendering {
bool Initialize(void* image,void* source,FILE* log);
void SetEyes(void* left,void* right);
void SetTracked(bool value);
void BeginFrame();
void Stop();
// Read-only identity for consumers of this exact private backend table.
bool OwnsBackend(void* object) noexcept;
uint64_t Skipped();
}
