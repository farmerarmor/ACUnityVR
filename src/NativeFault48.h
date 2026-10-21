#pragma once
#include <filesystem>
#include <cstdio>
namespace NativeFault48 {
bool Install(const std::filesystem::path& output,FILE* report);
void Stop();
}
