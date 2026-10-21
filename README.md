# ACUnityVR

An experimental native stereo OpenXR mod for **Assassin's Creed Unity** on Windows.
Both eyes render in the same game frame, with headset rotation and translation.
This uses the game's renderer rather than alternating-eye rendering.

[Download v0.1.0](https://github.com/farmerarmor/ACUnityVR/releases/tag/v0.1.0)

## Features

- Automatic startup after loading gameplay; no manual shader collector step.
- First-person view with Arno's body hidden, with a third-person toggle.
- Headset-aware native camera and visibility, including the packed stationary crowds.
- Runtime-recommended eye dimensions and refresh-rate pacing.
- Separate HUD and menu sizes, with stereo UI corrections.
- AA-off lighting grid/ray corrections, plus sky/cloud corrections.
- Configurable foveated shading and hidden-area mask.
- Optional removal of the identified chimney smoke, distant tree billboards and volumetric fog effects.

Normal mouse, keyboard and gamepad controls remain in use. Motion-controller
hands, aiming and collision-aware roomscale movement are not implemented.

## Requirements

- Windows x64, Assassin's Creed Unity using Direct3D 11, and an active PC OpenXR runtime.
- The supported `ACU.exe` must have SHA-256:
  `748abdc6456397fe7d8080f975f2a5ed3123bce548e7bf1c68b23f47e8b27e40`.
  Other executable builds are refused.
- Meta Quest Link is the tested connection/runtime. Other headset/runtime
  combinations have not been validated. NVIDIA foveation support depends on
  the GPU and driver; other GPU configurations are unverified.

## Installation and startup

1. Download the binary ZIP from **Releases**. GitHub's automatic source ZIP is
   for developers and does not contain the runtime binaries.
2. Extract it into the game directory so that `ACU.exe` and the `ACUnityVR`
   folder sit beside one another. Keep all four supplied binaries together.
3. Close Unity. Connect the headset and activate Quest Link or your PC OpenXR connection.
4. Run `ACUnityVR\Start-ACUnityVR.cmd`, then load your save normally. VR starts
   automatically when gameplay is ready.

The launcher backs up changed display settings in `ACUnityVR\display-backups`,
sets a windowed canvas to the headset's recommended eye dimensions and disables
desktop VSync. The tall desktop window is expected. Your chosen texture/AA
settings are not changed by the launcher.

If the game must start through its store launcher, run this first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Start-ACUnityVR.ps1 -WaitForGame
```

Then launch the game normally through Steam or Ubisoft Connect. If the mod
folder is elsewhere, pass `-GameExecutable` with the full path to `ACU.exe`.
Do not start the mod after Unity is already running. A failed or stopped
initialization requires a fresh game process.

## Controls and settings

| Key | Action |
| --- | --- |
| F9 | Recenter headset tracking |
| Pause | Toggle first/third person |
| F8 | Save both-eye screenshots and diagnostic captures |

There is **no F10 stop hotkey**. Close Unity normally to end VR.

Edit `ACUnityVR.ini` with the game closed. Restart after changing it.

- `[UI] HUDWidthDegrees` and `MenuWidthDegrees` size the HUD and menus separately.
  Their distance settings are independent too.
- `[FirstPerson] Enabled`, `HideBody`, `HeightOffset` and `ForwardOffset` control the camera.
- `[Foveation] Enabled`, `Strength`, `CentreRadius`, `HiddenAreaMask` and
  `HiddenAreaMargin` tune peripheral rendering. Keep the centre large enough for your headset.
- `[Effects] RemoveChimneySmoke`, `RemoveTreeBillboards` and
  `RemoveVolumetricFog` control the accepted effect-removal options.

The included configuration matches the tested baseline. Keep
`[Culling] GameplayHeadCamera=1`: it supplies the tracked head camera before
native visibility decisions. See [diagnostic controls](docs/DIAGNOSTICS.md)
before using the remaining function/numpad keys; many toggle corrections.

## Known issues

- Dark shading can slide across surfaces with head movement. It remains unresolved.
- Changing in-game graphics settings during VR has caused a game/PC freeze.
  Configure graphics in a normal non-VR session, close the game, then launch VR.
- Soft shadows have stereo problems; the tested setup uses a standard shadow mode.
- Performance is demanding at the runtime's recommended resolution. A 90 Hz
  headset/compositor does not guarantee 90 newly rendered stereo pairs per second.
- Some native NPC LOD changes also occur in the unmodified game.
- Full-game stability, every cutscene and all areas have not been validated.

This initial prerelease contains the restored, headset-used **r29** baseline:
automatic startup and F10 removal, without the later vignette/cloud-shadow experiments.
It is suitable for community testing, not a claim that the entire game is issue-free.

## Removing the mod

Close Unity and launch `ACU.exe` normally without the mod launcher. Nothing is
installed as a graphics proxy or a permanent game patch. To remove the files,
delete the `ACUnityVR` folder. To undo display changes, copy the desired backed-up
`ACU-*.ini` from `display-backups` over
`Documents\Assassin's Creed Unity\ACU.ini` while the game is closed; restoring
an older backup also restores its other saved settings.

## Development and license

[BUILDING.md](BUILDING.md) explains the Windows build and packaging process.
The release supplies the exact tested native DLL, host, registry and loader;
their hashes are recorded in `payload-manifest.json` and `checksums.sha256`.

ACUnityVR and its adapted stereo bridge are distributed under LGPL 2.1;
see [LICENSE](LICENSE). Third-party components retain their licenses;
see [THIRD_PARTY.md](THIRD_PARTY.md). Game executables, assets, captured game
shaders, research dumps and debug symbols are not included.
