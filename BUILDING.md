# Building ACUnityVR

Use Windows x64, Visual Studio 2022's C++ desktop tools (including MASM and a
Windows SDK), CMake 3.24 or newer, Git and Python 3 for the OpenXR build scripts.

Obtain the official dependencies outside this repository:

```powershell
git clone --branch release-1.1.60 --recurse-submodules https://github.com/KhronosGroup/OpenXR-SDK-Source.git dependencies/OpenXR-SDK-Source
git clone https://github.com/NVIDIA/nvapi.git dependencies/nvapi
```

Open an **x64 Native Tools Command Prompt for VS 2022** and run:

```bat
build.cmd -DOPENXR_SDK_SOURCE="C:/path/to/dependencies/OpenXR-SDK-Source" -DACUNITY_NVAPI_ROOT="C:/path/to/dependencies/nvapi"
```

The NVAPI directory must contain its headers and `amd64/nvapi64.lib`.
Outputs are placed in `build/bin`:

- `ACUnityVRNative.dll`
- `ACUnityShaderRegistry.dll`
- `ACUnityVRHost.exe`

Transport ABI 9 / experiment 76 and the portable-startup definitions must match
across the native DLL, registry and companion. F10 stop is compiled out of the
portable native/host path; internal lifecycle/error handling remains.

The repository contains the source corresponding to the restored r29 baseline.
The production CMake file omits historical experiment targets while retaining
the baseline's source, compile definitions and correction paths. Runtime shader
identification is in memory; no captured game shader bytecode is needed to build
or distribute it. Some native code-byte checks are exact-build identity guards.

## Loader and packaging

The release uses the existing, tested `ACUnityProbeLoader.exe`, SHA-256:
`96be4c3c6561279dcf82f72a12a2d44cd659fa4cee0383769aed59940e445a21`.
It is not rebuilt by the default build. Its complete source is in
`tools/ProbeLoader.cpp`; an explicit `ACUnityProbeLoader` CMake target is provided
for developers who need to build their own loader. Rebuilding produces a new
binary identity and requires separate testing.

To package developer-built native/registry/host files with a chosen loader:

```powershell
.\package.ps1 -BinaryDirectory .\build\bin -LoaderExecutable C:\path\to\ACUnityProbeLoader.exe
```

This produces a ZIP and SHA-256 file in `dist`. It creates a fresh
`payload-manifest.json` for the supplied binaries. The launcher validates those
four binary hashes before starting. Never combine a manifest with unrelated
binaries or silently replace an existing user's INI.

The v0.1.0 release archive reuses the headset-used r29 binaries rather than
substituting the fresh source-validation build. The loader is the verified
binary above. Source-build success validates compilation; it is not a new
headset/stability test.
