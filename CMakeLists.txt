cmake_minimum_required(VERSION 3.24)
project(ACUnityVR VERSION 0.1.0 LANGUAGES C CXX ASM_MASM)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "ACUnityVR requires Windows x64 and MSVC")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENXR_SDK_SOURCE "" CACHE PATH "Official Khronos OpenXR-SDK-Source checkout (tested release-1.1.60)")
set(ACUNITY_NVAPI_ROOT "" CACHE PATH "Official NVIDIA NVAPI SDK directory containing amd64/nvapi64.lib")
if(NOT OPENXR_SDK_SOURCE OR NOT ACUNITY_NVAPI_ROOT)
  message(FATAL_ERROR "Set OPENXR_SDK_SOURCE and ACUNITY_NVAPI_ROOT; see BUILDING.md")
endif()
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_ALL_EXTENSIONS OFF CACHE BOOL "" FORCE)
set(BUILD_LOADER ON CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
add_subdirectory("${OPENXR_SDK_SOURCE}" openxr-sdk EXCLUDE_FROM_ALL)
add_library(minhook STATIC vendor/minhook/src/buffer.c vendor/minhook/src/hook.c vendor/minhook/src/trampoline.c vendor/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC vendor/minhook/include)
add_library(stereo_bridge STATIC vendor/stereo-bridge/NativeBridge.cpp vendor/stereo-bridge/HostCopyTiming.cpp)
target_include_directories(stereo_bridge PUBLIC vendor/stereo-bridge)
target_compile_definitions(stereo_bridge PUBLIC ACUNITYVR_EXPERIMENT=76 ACUNITYVR_VISIBILITY_MASK=1 ACUNITYVR_HOST_COPY_TIMING=1 XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D11)
target_link_libraries(stereo_bridge PUBLIC openxr_loader d3d11 dxgi d3dcompiler advapi32)
add_library(pair_producer STATIC src/PairProducer.cpp)
target_compile_definitions(pair_producer PUBLIC ACUNITYVR_EXPERIMENT=76 ACUNITYVR_VISIBILITY_MASK=1)
target_include_directories(pair_producer PUBLIC src vendor/stereo-bridge)
target_link_libraries(pair_producer PUBLIC d3d11 dxgi)
add_executable(ACUnityVRHost tools/ACUnityVRHost.cpp)
target_compile_definitions(ACUnityVRHost PRIVATE ACUNITYVR_PORTABLE92=1)
target_link_libraries(ACUnityVRHost PRIVATE stereo_bridge)
add_library(ACUnityVRNative SHARED
  src/NativeTracked74.cpp
  src/EarlyCamera91.cpp
  src/StationSubmission90.cpp
  src/DenseVisibility87.cpp
  src/NativeGroup88.cpp
  src/FirstPerson70.cpp
  src/NpcCamera63.cpp
  src/NpcCallback63.cpp
  src/NpcDecision63.cpp
  src/NpcCone63.cpp
  src/NpcPriority63.asm
  src/ComponentGateProbe63.cpp
  src/PrimaryCamera52.cpp
  src/NativeFault48.cpp
  src/SelectionBoundary46.cpp
  src/EntityVisibility41.cpp
  src/NativeFoveation36.cpp
  src/OcclusionProbe46.cpp
  src/CullingProbe63.cpp
  src/SourceRendering.cpp
  src/NativeGpuTiming.cpp
  src/GameDisplay.cpp
  src/ProjectedShadowGuard37.cpp
  src/DrawDiagnostics74.cpp
  src/EffectDrawCapture74.cpp
  src/MaterialPrograms75.cpp
  src/VolumetricFog79.cpp
  src/CrowdDraw82.cpp
  src/BulkDraw83.cpp
  src/LightingGrid67.cpp
  src/LightingTiles69.cpp
  src/UiCamera71.cpp
  src/ShaderHunt78.cpp
  src/GIProjectionTask.cpp
  src/CloudProjection.cpp
)
target_include_directories(ACUnityVRNative PRIVATE "${ACUNITY_NVAPI_ROOT}")
target_compile_definitions(ACUnityVRNative PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX ACUNITYVR_FULL_PASS_PROFILE=1 ACUNITYVR_SHADER_PROFILE=1 ACUNITYVR_DEPTH_PROFILE=1 ACUNITYVR_BATCH_PROFILE=1 ACUNITYVR_SUN_CAPTURE=1 ACUNITYVR_OCCLUSION_CAPTURE=1 ACUNITYVR_LEAF_PIPELINE=1 ACUNITYVR_NATIVE_FOVEATION=1 ACUNITYVR_GEOMETRY_FOVEATION=1 ACUNITYVR_GPU_CONSTANTS40=1 ACUNITYVR_LATE_MATERIAL_REGISTRATION=1 ACUNITYVR_SHADER_HUNT78=1 ACUNITYVR_FOG79=1 ACUNITYVR_STATION90=1 ACUNITYVR_EARLY91=1 ACUNITYVR_PORTABLE92=1)
target_compile_options(ACUnityVRNative PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/EHs;/utf-8>")
target_link_libraries(ACUnityVRNative PRIVATE pair_producer minhook bcrypt d3d11 dxgi d3dcompiler user32 "${ACUNITY_NVAPI_ROOT}/amd64/nvapi64.lib")
add_library(ACUnityShaderRegistry SHARED src/ShaderObserve75.cpp src/AutomaticStartup92.cpp)
target_compile_definitions(ACUnityShaderRegistry PRIVATE ACUNITY_SHADER_REGISTRY92=1 ACUNITY_SHADER_OBSERVE_FIXTURE=1)
target_compile_options(ACUnityShaderRegistry PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(ACUnityShaderRegistry PRIVATE minhook bcrypt d3d11 dxgi user32)
# Source is supplied for the tested loader. Release packaging reuses its verified
# binary. This optional target is deliberately excluded from the default build.
add_executable(ACUnityProbeLoader EXCLUDE_FROM_ALL tools/ProbeLoader.cpp)
target_compile_options(ACUnityProbeLoader PRIVATE /W4 /EHsc /utf-8)
foreach(t minhook stereo_bridge pair_producer ACUnityVRNative ACUnityVRHost ACUnityShaderRegistry ACUnityProbeLoader)
  target_compile_definitions(${t} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_target_properties(${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()
target_compile_options(ACUnityVRHost PRIVATE /W4 /EHsc /utf-8)
foreach(t ACUnityVRNative ACUnityVRHost ACUnityShaderRegistry)
  target_link_options(${t} PRIVATE "/PDBALTPATH:%_PDB%")
endforeach()
