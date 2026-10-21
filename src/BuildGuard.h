#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <string>
#include <vector>

namespace acu {
inline constexpr char SupportedSha256[] = "748abdc6456397fe7d8080f975f2a5ed3123bce548e7bf1c68b23f47e8b27e40";
inline bool VerifyImage() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, _countof(path))) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD bytes{}, objectSize{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &bytes, 0) >= 0;
    std::vector<UCHAR> object(objectSize), block(1024*1024);
    std::array<UCHAR, 32> digest{};
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) >= 0;
    while (ok) {
        if (!ReadFile(file, block.data(), static_cast<DWORD>(block.size()), &bytes, nullptr)) { ok = false; break; }
        if (!bytes) break;
        ok = BCryptHashData(hash, block.data(), bytes, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (!ok) return false;
    std::string hex;
    for (auto b : digest) { hex += "0123456789abcdef"[b >> 4]; hex += "0123456789abcdef"[b & 15]; }
    return hex == SupportedSha256;
}
}
