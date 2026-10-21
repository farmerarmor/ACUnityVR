# Third-party notices

- The D3D11/OpenXR bridge and camera utilities in `vendor/stereo-bridge`
  derive from the SteepVR/ThiefVR bridge and are licensed under LGPL 2.1.
  The adapted source and LGPL text are included.
- MinHook is included under its BSD/HDE component licenses in
  `vendor/minhook/LICENSE.txt` and `licenses/MinHook.txt`.
- The build links the official Khronos OpenXR loader from an external
  OpenXR-SDK-Source checkout. Its notice is included in `licenses/OpenXR.txt`.
- NVIDIA NVAPI is linked for driver shading-rate support. Its license notice
  is included in `licenses/NVAPI.txt`; SDK headers/libraries are obtained from
  the official NVIDIA dependency when building.

Corresponding source for the release's native DLL, shader registry, host,
loader and adapted bridge is supplied in this repository. Build and relinking
instructions are in `BUILDING.md`. The tested loader binary is reused unchanged
in the release package.

No Assassin's Creed Unity executable, game asset, mapped image, captured game
shader bytecode, private diagnostic output or personal machine configuration is
distributed. Native address/signature checks identify the supported game build.
