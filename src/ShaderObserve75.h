#pragma once
#include <windows.h>
#include <d3d11.h>
#include <filesystem>
#include <cstdint>
namespace ShaderObserve75 {
struct Summary { uint64_t factories{}, shaderCalls{}, captured{}, vertex{}, pixel{}, bytes{}, rejected{}, budgetRefused{}, failures{}, hookTargets{}, activeCalls{}; bool accepting{}; };
bool Start(const std::filesystem::path& directory) noexcept;
bool Stop() noexcept;
Summary Stats() noexcept;
bool SaveStatus(const char* state) noexcept;
bool ValidDXBC(const void* data, size_t size, unsigned& stage) noexcept;
}
