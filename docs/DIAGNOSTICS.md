# Diagnostic keys

For normal play, use F9 to recenter and Pause to compare first/third person.
F8 writes large diagnostic captures as well as both-eye screenshots. The initial
development baseline also makes startup observations. These files are local and
are not part of the release archive.

The remaining keys are retained for investigating problems. Accidentally
changing one can disable an accepted correction for the current session.
Restarting restores the INI-selected defaults.

| Key | Comparison/tool |
| --- | --- |
| F1 | AA-off lighting grid correction |
| F2 | Native NPC camera/culling comparison |
| F3 | GPU profiling |
| F4 | Foveation comparison |
| F5 | Source-view GPU skipping |
| F6 | Sky/cloud projection correction |
| F7 | GI ray correction |
| F11 | NPC edge guard |
| F12 | HUD/menu projection correction |
| Insert | AA-off lighting tile-ray correction |
| NumPad1 | Distant tree billboard removal |
| NumPad2 | Cycle particle-family isolation |
| NumPad3 | Small-smoke isolation |
| NumPad4 | Combined chimney-smoke removal |
| NumPad5 | CPU lookup fast paths |
| NumPad6 | Extended lighting foveation |
| NumPad8 | Volumetric fog removal |
| NumPad9 | Group/individual visibility-camera comparison |

Num Lock must be ON for numpad keys. The accepted central gameplay-camera
correction is controlled by `Culling.GameplayHeadCamera`; NumPad9 alone does not
disable or replace that central fix. NumPad0 and Scroll Lock have no new shadow
experiment in this baseline. F10 does not stop VR.

## Shader hunter

End enables the hunter. PageUp/PageDown select and temporarily disable a shader.
Delete marks the selection for later investigation. NumPad7 cycles shader
stages (PS, CS, the other stages, then ALL). Home restores disabled shaders and
disables the tool. Marking a shader is diagnostic; it does not install a fix.

When reporting an issue, include the game executable hash, GPU/driver, OpenXR
runtime, location/action that triggers it, AA/shadow settings and whether it
occurs in normal non-VR play. Logs and captures may contain local paths and
screenshots; review them before sharing. Do not upload crash dumps or game
shader files by default.
