# HANDOVER

Graphics policy (USAGE BAN) handover: a temporary DX11/D3D11 solution is not acceptable.
Revert any active D3D11 backend use; rendering must run on Vulkan or D3D12 only.
Route backend selection through one shim; startup assert backend in {Vulkan, D3D12};
headless Null only under RHI_POLICY_HEADLESS=1. See C:\Projects\POLICY.md.

## Where things stand (2026-11-18)

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
  - `studio/CreatureForgePanel.cpp` (310 lines): `forge/ForgePanelTabs.cpp` (221 lines, the four tab builders), `forge/ForgeColorSwatches.cpp` (64 lines), `forge/CharacterSheetWidget.{hpp,cpp}` and `forge/ForgeAssetOps.{hpp,cpp}` live under `studio/forge/`. The engine side is `engine/aur/forge/{SpeciesCatalog,CharacterRenderer,AssetGenerators}.cpp`, all three under 650 lines.
  - `studio/DatabasePanel.cpp` (47 lines) and `studio/EventsPanel.cpp` (78 lines): `bestiary/*` and `events/EventCommandOps.{hpp,cpp}` live under `studio/bestiary/` and `studio/events/`.
  - `studio/MusicPanel.cpp` (661 lines): `music/MidiStrip.{hpp,cpp}`, `music/GmPatchList.hpp`, `music/MusicPanelSession.cpp` (598 lines), `music/PianoRollInput.cpp` (289 lines), `music/MidiDeviceIo.{hpp,cpp}`, `music/SongHistory.{hpp,cpp}`, and `music/SharedAudio.{hpp,cpp}` live under `studio/music/`.
- **The world is a globe as well as a sheet.** `studio/world/GlobeProjection` is an
  orthographic projection with capped poles; the World tab's Globe checkbox (or
  `Aur --globe`) shows it and dragging spins it. `build/renders/world_globe_1280x690_z1.0.png`
  is the proof, and `tests/test_globe_projection.cpp` pins the wrap, the poles and the
  shading. **Next: W-04's third LOD level** (a denser picture for the visible region).
- **The world map has two levels of detail** (`studio/world/TerrainLod.{hpp,cpp}`,
  `studio/world/WorldTerrainPaint.{hpp,cpp}`; plan `Bible/notes/globe-view.md` P1).
  The crossover is a screen measurement -- the drawn width over the map's width --
  and `kHdSurfacePixelsPerTile = 3.0` picks the generated HD surface at or above it
  and, below it, the same terrain at one pixel per tile (`build_hd_surface(..., 1)`).
  Both are cached, so crossing over on a zoom change is a lookup. Proved by
  `build/renders/world_1280x690_z1.0.png` (HD) and `..._z0.5.png` (coarse).
  Open: the HD surface is capped at 32 px/tile (22 for a 256x128 map), so at the
  3200% ceiling it is upsampled; a denser picture for the visible region is the
  third level.
- **The window has a bottom context strip** (`studio/theme/StatusStrip.{hpp,cpp}`, 2026-11-18):
  page name, the last line posted by a panel, project + saved/unsaved, RHI backend, theme mode
  and interface scale. `theme::postStatus` is the one channel; the Forge posts its preview and
  creation lines through it, and a page change clears the line so nothing stale survives a switch.
- **Every creature has artwork.** The three bandits are generated by `generate_enemy_graphic`
  (24×24, four idle frames) and written by the MCP's `enemy_generate`; the Bestiary portrait
  now gets the project at construction, which fixes the "no artwork" message every creature
  showed on a fresh open. W-03 is closed.
- **A map is seeded and playable.** `engine/aur/mapforge.*` makes a town, a cave or a
  platformer room with its own tiles; the MCP has `map_generate`, the World tab has
  "Generate map…", and `Aur --map seeded_platformer` plays the shipped room. W-02 is
  closed; W-01's pick and expand landed and only the field's beach tile is open.
- **The Creature Forge was refactored to the owner's brief** (`Bible/notes/forge-brief.md`,
  moved into `Bible/notes/` from the project root, 2026-11-18): the 1,240-line `forge.cpp`
  became `engine/aur/forge/{SpeciesCatalog,CharacterRenderer,AssetGenerators}.cpp`, the
  renderer is native 48×64 with species/hair/role/accessory layers, the preview is a
  four-way turnaround, and the panel has Character / Creatures / Gear & FX / Tiles tabs with
  variant, affinity, alignment and record-role fields plus `setIcon` colour swatches.
  ctest 7/7 (75 checks), conformance 9/9, 24 render captures opened. **Next: the brief's
  next-track choice (W-01 + W-02, or Track B) is the owner's call.**
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

- `Bible/notes/globe-view.md` — the globe's levels of detail and the sphere. **P1 landed** (the two levels, `TerrainLod` + `WorldTerrainPaint`); **P2 is next** (the grid on a UV sphere, the poles capped), and the third LOD level — a denser surface for the 3200% ceiling — is recorded as open.
- `Bible/notes/globe-expand.md` — **W-01**: expand a world map tile into a map preserving coast direction.
- `Bible/notes/platformer.md` — **P-01**: P1–P3 landed and tested; P4 waits on a solid floor tile chosen by the owner.
- `Bible/notes/Aur World Map Agent Work Packages.md` — WP0–WP3 landed. Next: WP4 (WorldSim hookup: temperature seeding, sim persistence) and WP6 cleanup. WP5 (globe view) needs WP4.
- `Bible/notes/daw/plan.md` — the Music Studio DAW. P5 remainder closed; **P7a (the GM/GS instrument) is parked mid-wiring by owner decision 2026-11-11**, resume point in `Bible/notes/daw/gm-gs.md`. P2 close still needs the owner's GUI soak.
- `Bible/notes/worldsim-p31.md` — P3.1 landed, P3.2 owner-gated.

## Know before you go

- **The state, the decisions and the open defects are in `PROJECT.md`.** This file is the
  pointer. The older history is `Bible/Operations/NOTES.md`. The order of work is
  `ROADMAP.md` (the stages) and `IMPLEMENTATION.md` (every open item by area, with its gate);
  `WANTED.md` is the short owner queue pointing at both.
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
