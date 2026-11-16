# HANDOVER

Graphics policy (USAGE BAN) handover: a temporary DX11/D3D11 solution is not acceptable.
Revert any active D3D11 backend use; rendering must run on Vulkan or D3D12 only.
Route backend selection through one shim; startup assert backend in {Vulkan, D3D12};
headless Null only under RHI_POLICY_HEADLESS=1. See C:\Projects\POLICY.md.

## Where things stand (2026-11-16)

- **The studio runs on the estate design system and Hydra theme stack (`studio/theme/`, `C:\Projects\DESIGN.md` §1, §2, §6.3, §13; `C:\Projects\Hydra\`).**
  `ThemeManager`, `ThemeContract`, and `AurStyle` (`QProxyStyle` over Fusion) drive every surface,
  control, tab, scrollbar, splitter and selected-row wash (`accentDim`) from `QPalette` and token
  roles, with zero stylesheets (`rg setStyleSheet studio` is 0). `ThemeIcons` paints vector glyphs in
  live token colours, and `AppearanceDialog` (reachable from the hamburger menu and the title bar)
  switches between all 13 Hydra modes (`system`, `brahmamuhurta` / `firstlight`, `dawn`, `sunrise`,
  `light`, `noon`, `sunset`, `bluehour`, `dusk`, `dark`, `midnight`, `amoled`, and accent-tinted
  `custom`), picks the user accent from the 24×24 HSV + 23-grey + escape-hatch matrix or a validated
  `#rrggbb` hex field, and configures interface scale (`75–200%`), dyslexia-friendly font, and
  colour-vision simulation (`protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia`).
- **Every `studio/` file is under the 750-line soft limit and every module keeps its helpers in a nested folder** (`studio/core/`, `studio/theme/`, `studio/playtest/`, `studio/map/`, `studio/pixel/`, `studio/tileset/`, `studio/world/`, `studio/forge/`, `studio/bestiary/`, `studio/events/`, `studio/music/`):
  - `studio/` root holds only `main.cpp`, `MainWindow.*`, and the nine tab panels; shared studio primitives (`AurPaths`, `CommandStack`, `UndoCommands`, `RhiBackend`) live under `studio/core/`.
  - `studio/MainWindow.cpp` (411 lines): `theme/StudioChrome.{hpp,cpp}` and `theme/FramelessWindow.{hpp,cpp}` moved out (`P3.1` / `P6.5`).
  - `studio/GameView.cpp` (186 lines): `playtest/QtRenderer.{hpp,cpp}` and `playtest/PlaytestDebugDialog.{hpp,cpp}` moved out.
  - `studio/MapEditorPanel.cpp` (427 lines): `map/MapCanvas.{hpp,cpp}`, `map/MapEditUndo.{hpp,cpp}`, `map/TilePaletteWidget.{hpp,cpp}`, `map/TileTexture.hpp`, and `map/EditorDefs.hpp` live under `studio/map/`.
  - `studio/SpriteEditorPanel.cpp` (618 lines) and `studio/TilesetEditorPanel.cpp` (387 lines): `pixel/PixelSurface.{hpp,cpp}`, `pixel/PixelEditSession.{hpp,cpp}`, `pixel/PixelFrameOps.{hpp,cpp}`, and `pixel/SpriteCanvas.{hpp,cpp}` live under `studio/pixel/`, and `tileset/TilePngIo.{hpp,cpp}` lives under `studio/tileset/`.
  - `studio/WorldPanel.cpp` (686 lines): `world/WorldPanelActions.cpp` (395 lines), `world/WorldModel.{hpp,cpp}`, `world/WorldSettingsPanel.{hpp,cpp}`, and `world/WorldMapCanvasInput.cpp` (403 lines) live under `studio/world/`.
  - `studio/CreatureForgePanel.cpp` (355 lines): `forge/CharacterSheetWidget.{hpp,cpp}` and `forge/ForgeAssetOps.{hpp,cpp}` live under `studio/forge/`.
  - `studio/DatabasePanel.cpp` (47 lines) and `studio/EventsPanel.cpp` (78 lines): `bestiary/*` and `events/EventCommandOps.{hpp,cpp}` live under `studio/bestiary/` and `studio/events/`.
  - `studio/MusicPanel.cpp` (661 lines): `music/MidiStrip.{hpp,cpp}`, `music/GmPatchList.hpp`, `music/MusicPanelSession.cpp` (598 lines), `music/PianoRollInput.cpp` (289 lines), `music/MidiDeviceIo.{hpp,cpp}`, `music/SongHistory.{hpp,cpp}`, and `music/SharedAudio.{hpp,cpp}` live under `studio/music/`.
- **The editors undo over `QUndoStack`** (`studio/core/UndoCommands.{hpp,cpp}`),
  and the map editor records per-cell diffs (`studio/map/MapEditUndo.{hpp,cpp}`).
  `tests/test_undo_commands.cpp`, `tests/test_theme.cpp`, and `tests/test_settings.cpp` pin undo,
  the Hydra theme contract, and schema-validated `AurSettings`; ctest 7/7 (75 checks).
- **The studio has one master volume and one audio device** (`studio/music/SharedAudio`,
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
