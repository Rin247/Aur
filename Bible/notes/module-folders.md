# Plan: Nested Module Helper Folders (`module-folders`)

Status: Phase 2 in progress (`[module-folders-p2.1]`).

## Goal

Ensure every module in `studio/` keeps its helper files in a nested module subfolder (`studio/theme/`, `studio/playtest/`, `studio/map/`, `studio/pixel/`, `studio/tileset/`, `studio/world/`, `studio/forge/`, `studio/bestiary/`, `studio/events/`, `studio/music/`), leaving only the top-level entry point (`main.cpp`, `MainWindow.{hpp,cpp}`), the 9 tab panel headers/implementations (`GameView`, `MapEditorPanel`, `TilesetEditorPanel`, `WorldPanel`, `SpriteEditorPanel`, `CreatureForgePanel`, `DatabasePanel`, `EventsPanel`, `MusicPanel`), and shared cross-module primitives (`AurPaths`, `CommandStack`, `UndoCommands`, `RhiBackend`) at `studio/` root.

## Contracts

- All existing class names, namespaces (`aur`, `aur::editor`, `aur::world`, `aur::records`, `aur::theme`), CLI flags, and CTest targets (`aur_tests`, `synth_fm_tests`, `synth_spc_tests`, `synth_fx_tests`, `fx_effect_tests`) remain unchanged.
- `target_include_directories` for `aur_studio`, `aur_game`, `aur_editor_common`, and `aur_tests` keeps `studio` on the include path so nested headers can be included explicitly by module folder (`"map/MapCanvas.hpp"`, `"pixel/PixelSurface.hpp"`, `"playtest/QtRenderer.hpp"`, `"forge/CharacterSheetWidget.hpp"`, `"tileset/TilePngIo.hpp"`, `"events/EventCommandOps.hpp"`, etc.).
- No file exceeds the 750-line soft limit or 100-line function soft limit, and no comment block exceeds 2 lines (`QUIRKS.md` §43).

## Current shape (verified)

- `studio/bestiary/`, `studio/music/`, `studio/theme/`, and `studio/world/` exist, but several helpers for those modules still sit flat at `studio/` root (`StudioChrome.*`, `FramelessWindow.*`, `MusicPanelSession.cpp`, `MidiDeviceIo.*`, `SongHistory.*`, `SharedAudio.*`, `WorldModel.*`, `WorldSettingsPanel.*`, `WorldPanelActions.cpp`).
- Map Editor (`MapCanvas.*`, `MapEditUndo.*`, `TilePaletteWidget.*`, `TileTexture.hpp`, `EditorDefs.hpp`) and Pixel/Sprite/Tileset (`PixelSurface.*`, `PixelEditSession.*`, `PixelFrameOps.*`, `SpriteCanvas.*`) have all their helpers flat at `studio/` root.
- Playtest (`GameView.{hpp,cpp}`), Creature Forge (`CreatureForgePanel.{hpp,cpp}`), Tileset Editor (`TilesetEditorPanel.{hpp,cpp}`), and Events (`EventsPanel.{hpp,cpp}`) embed their helper classes and operations inline instead of in nested module folders (`studio/playtest/`, `studio/forge/`, `studio/tileset/`, `studio/events/`).

## Hazards found

- `AUTOMOC` scans headers included by target sources; when moving `Q_OBJECT` headers (`MapCanvas.hpp`, `TilePaletteWidget.hpp`, `PixelSurface.hpp`, `SpriteCanvas.hpp`, `StudioChrome.hpp`, `WorldSettingsPanel.hpp`, `CharacterSheetWidget.hpp`) into nested folders, list the `.hpp` files in `CMakeLists.txt` under `aur_studio` so CMake's `AUTOMOC` always tracks them.
- `aur_game` links `GameView.cpp`, `SharedAudio.cpp`, `QtRenderer.cpp`, and `PlaytestDebugDialog.cpp`; `CMakeLists.txt` must update `aur_game` alongside `aur_studio`.

## Target layout

- `studio/theme/`: `StudioChrome.{hpp,cpp}`, `FramelessWindow.{hpp,cpp}`, plus existing theme files.
- `studio/playtest/`: `QtRenderer.{hpp,cpp}`, `PlaytestDebugDialog.{hpp,cpp}` (extracted from `GameView.{hpp,cpp}`).
- `studio/map/`: `MapCanvas.{hpp,cpp}`, `MapEditUndo.{hpp,cpp}`, `TilePaletteWidget.{hpp,cpp}`, `TileTexture.hpp`, `EditorDefs.hpp`.
- `studio/pixel/`: `PixelSurface.{hpp,cpp}`, `PixelEditSession.{hpp,cpp}`, `PixelFrameOps.{hpp,cpp}`, `SpriteCanvas.{hpp,cpp}`.
- `studio/tileset/`: `TilePngIo.{hpp,cpp}` (extracted from `TilesetEditorPanel.{hpp,cpp}`).
- `studio/world/`: `WorldModel.{hpp,cpp}`, `WorldSettingsPanel.{hpp,cpp}`, `WorldPanelActions.cpp`, plus existing `studio/world/` files.
- `studio/forge/`: `CharacterSheetWidget.{hpp,cpp}`, `ForgeAssetOps.{hpp,cpp}` (extracted from `CreatureForgePanel.{hpp,cpp}`).
- `studio/bestiary/`: existing 10 files (`BestiaryData.*`, `BestiaryEditor.*`, `CreaturePortrait.*`, `RecordEditor.*`, `RecordFields.*`).
- `studio/events/`: `EventCommandOps.{hpp,cpp}` (extracted from `EventsPanel.{hpp,cpp}`).
- `studio/music/`: `MusicPanelSession.cpp`, `MidiDeviceIo.{hpp,cpp}`, `SongHistory.{hpp,cpp}`, `SharedAudio.{hpp,cpp}`, plus existing `studio/music/` files.

## Phases

### Phase 1 — Consolidate existing module folders (`theme/`, `world/`, `music/`, `map/`, `pixel/`) (`module-folders-p1`)

- [x] `[module-folders-p1.1]` Move `StudioChrome.{hpp,cpp}` and `FramelessWindow.{hpp,cpp}` into `studio/theme/`; move `WorldModel.{hpp,cpp}`, `WorldSettingsPanel.{hpp,cpp}`, and `WorldPanelActions.cpp` into `studio/world/`; move `MusicPanelSession.cpp`, `MidiDeviceIo.{hpp,cpp}`, `SongHistory.{hpp,cpp}`, and `SharedAudio.{hpp,cpp}` into `studio/music/`; move `MapCanvas.{hpp,cpp}`, `MapEditUndo.{hpp,cpp}`, `TilePaletteWidget.{hpp,cpp}`, `TileTexture.hpp`, and `EditorDefs.hpp` into `studio/map/`; move `PixelSurface.{hpp,cpp}`, `PixelEditSession.{hpp,cpp}`, `PixelFrameOps.{hpp,cpp}`, and `SpriteCanvas.{hpp,cpp}` into `studio/pixel/`. Update includes and `CMakeLists.txt`.

Acceptance: `aur_studio`, `aur_game`, and `aur_tests` build cleanly and `ctest` passes 7/7. Tag: `module-folders-p1`.

### Phase 2 — Extract nested helper folders for Playtest, Forge, Tileset, and Events (`playtest/`, `forge/`, `tileset/`, `events/`) (`module-folders-p2`)

- [ ] `[module-folders-p2.1]` Extract `studio/playtest/QtRenderer.{hpp,cpp}` and `studio/playtest/PlaytestDebugDialog.{hpp,cpp}` from `GameView.{hpp,cpp}`; extract `studio/forge/CharacterSheetWidget.{hpp,cpp}` and `studio/forge/ForgeAssetOps.{hpp,cpp}` from `CreatureForgePanel.{hpp,cpp}`; extract `studio/tileset/TilePngIo.{hpp,cpp}` from `TilesetEditorPanel.{hpp,cpp}`; extract `studio/events/EventCommandOps.{hpp,cpp}` from `EventsPanel.{hpp,cpp}`. Update `CMakeLists.txt`, `Bible/Operations/NOTES.md`, and `PROJECT.md`.

Acceptance: `aur_studio`, `aur_game`, and `aur_tests` build cleanly, `ctest` passes 7/7, and `conformance.py --project Aur` passes 9/9. Tag: `module-folders-p2`.

## Acceptance

- Every module in `studio/` has its helpers organized in a nested subfolder (`studio/theme/`, `studio/playtest/`, `studio/map/`, `studio/pixel/`, `studio/tileset/`, `studio/world/`, `studio/forge/`, `studio/bestiary/`, `studio/events/`, `studio/music/`).
- `aur_studio`, `aur_game`, and `aur_tests` build cleanly; `ctest` 7/7 and `conformance.py --project Aur` 9/9 pass.

## Rollback

`git reset --hard <last-phase-tag>` and fix the plan before retrying.

## Decisions and deviations

- Keep the 9 top-level panel classes (`GameView`, `MapEditorPanel`, `TilesetEditorPanel`, `WorldPanel`, `SpriteEditorPanel`, `CreatureForgePanel`, `DatabasePanel`, `EventsPanel`, `MusicPanel`) at `studio/` root as the 1-to-1 entry points for the 9 studio tabs, while placing all module helpers in their nested folders.
