# HANDOVER

Graphics policy (USAGE BAN) handover: a temporary DX11/D3D11 solution is not acceptable.
Revert any active D3D11 backend use; rendering must run on Vulkan or D3D12 only.
Route backend selection through one shim; startup assert backend in {Vulkan, D3D12};
headless Null only under RHI_POLICY_HEADLESS=1. See C:\Projects\POLICY.md.

## Where things stand (2026-11-16)

- **The studio runs on the estate design system (`studio/theme/`, `C:\Projects\DESIGN.md` §1, §2, §6.3, §13).**
  `ThemeManager` and `AurStyle` (`QProxyStyle` over Fusion) drive every surface, control, tab,
  scrollbar, splitter and selected-row wash (`accentDim`) from `QPalette` and token roles, with
  zero stylesheets (`rg setStyleSheet studio` is 0). `ThemeIcons` paints vector glyphs in live
  token colours, and `AppearanceDialog` (reachable from the hamburger menu and the title bar)
  switches between `system` and the 11 modes (`firstlight`, `dawn`, `sunrise`, `light`, `noon`,
  `sunset`, `bluehour`, `dusk`, `dark`, `midnight`, `amoled`) and picks the user accent from the
  24×24 HSV + 23-grey + escape-hatch matrix or a validated `#rrggbb` hex field.
- **Every `studio/` file is under the 750-line soft limit:**
  - `studio/MainWindow.cpp` (377 lines): `StudioChrome.{hpp,cpp}` and `FramelessWindow.{hpp,cpp}`
    moved out (`P3.1` / `P6.5`).
  - `studio/MusicPanel.cpp` (628 lines): `music/MidiStrip.{hpp,cpp}`, `music/GmPatchList.hpp`, and
    `MusicPanelSession.cpp` (567 lines) moved out (`P6.1` / `E-02`), and the DAW header rows were
    compacted (`P5.1`) with `FxChainStrip` hiding its chain list when empty.
  - `studio/WorldPanel.cpp` (660 lines): `WorldPanelActions.cpp` (395 lines) moved out (`P6.2`).
  - `studio/world/WorldMapCanvas.cpp` (465 lines): `world/WorldMapCanvasInput.cpp` (412 lines)
    moved out (`P6.3`).
  - `studio/music/PianoRollCanvas.cpp` (536 lines): `music/PianoRollInput.cpp` (289 lines) moved
    out (`P6.4`).
  - `studio/DatabasePanel.cpp` (45 lines): the `Races` tab now uses the generic
    `records::RecordEditor` with `"races"` in `RecordFields.{hpp,cpp}` (`P6.6` / `E-01`).
- **The editors undo over `QUndoStack`** (`studio/UndoCommands.{hpp,cpp}`),
  and the map editor records per-cell diffs (`studio/MapEditUndo.{hpp,cpp}`).
  `tests/test_undo_commands.cpp` and `tests/test_theme.cpp` pin both; ctest 7/7 (73 checks).
- **The studio has one master volume and one audio device** (`studio/SharedAudio`,
  `studio/music/MasterVolumeStrip`).
- **A render harness exists** (`tests/render_check.py`). Run it and OPEN the
  PNGs in `build/renders/`:

```powershell
cd C:\Projects\Aur; py -3.15 tests\render_check.py
```

## Active plans

- `Bible/notes/globe-expand.md` — **W-01**: expand a world map tile into a map preserving coast direction.
- `Bible/notes/map-generator.md` — **W-02**: procedural seed generator for field, cave, and platformer maps.
- `Bible/notes/platformer.md` — **P-01**: P1–P3 landed and tested; P4 waits on a solid floor tile chosen by the owner.
- `Bible/notes/Aur World Map Agent Work Packages.md` — WP0–WP3 landed. Next: WP4 (WorldSim hookup: temperature seeding, sim persistence) and WP6 cleanup. WP5 (globe view) needs WP4.
- `Bible/notes/daw/plan.md` — the Music Studio DAW. P5 remainder closed; **P7a (the GM/GS instrument) is parked mid-wiring by owner decision 2026-11-11**, resume point in `Bible/notes/daw/gm-gs.md`. P2 close still needs the owner's GUI soak.
- `Bible/notes/worldsim-p31.md` — P3.1 landed, P3.2 owner-gated.

## Know before you go

- **The state, the decisions and the open defects are in `PROJECT.md`.** This file is the
  pointer. The older history is `Bible/Operations/NOTES.md`.
- The build entry point is `AIO.bat`:

```powershell
cd C:\Projects\Aur; cmd /c "AIO.bat build windows Release"
```

or from PowerShell with the compiler environment in the same command:

```powershell
cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cmake --build C:\Projects\Aur\build\Release --target aur_tests aur_studio --parallel 8'
```

- Tests: `C:\Dev\Qt\Tools\CMake_64\bin\ctest.exe --test-dir C:\Projects\Aur\build\Release --timeout 300`.
- Conformance: `py -3.15 C:\Projects\tools\conformance.py --project Aur`.
- An `Aur.exe` left running holds the link lock during a build.
