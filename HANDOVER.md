# HANDOVER

Graphics policy (USAGE BAN) handover: a temporary DX11/D3D11 solution is not acceptable.
Revert any active D3D11 backend use; rendering must run on Vulkan or D3D12 only.
Route backend selection through one shim; startup assert backend in {Vulkan, D3D12};
headless Null only under RHI_POLICY_HEADLESS=1. See C:\Projects\POLICY.md.

## Where things stand (2026-11-19)

Read in this order: `AGENTS.md` (rules), this file (current handoff), `WANTED.md` (ordered asks and active plans), `PROJECT.md` (verified product state and decisions), then the active plans linked in `WANTED.md`. `ROADMAP.md` is strategic sequencing; `IMPLEMENTATION.md` is only the open gate ledger. `Bible/Operations/NOTES.md` is history, and the similarly named Operations snapshots are archival. `Bible/README.md` indexes design sources and document roles.

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
- **The world is a GPU globe with 3-tier LOD and D3D12/Vulkan parity** (`studio/shaders/globe.frag`,
  `studio/world/TerrainRhiWidget.{hpp,cpp}`, `studio/world/TerrainLod.{hpp,cpp}`,
  `studio/world/HdSurface.cpp`; plan `Bible/notes/globe-view.md` P1–P3, W-04 closed 2026-11-21).
  Both `world_sculpt.frag` and `globe.frag` use `rhi()->isYUpInNDC()` for right-side-up Vulkan and
  D3D12 parity and share the `#171b20` canvas margin; `globeRevision_` tracks `WorldModel::revision()`
  so live sculpts update the globe immediately; `build_hd_surface` is genuinely region-scoped with a
  2-tile detail border and X-wrapping; `LodTier` switches between `Coarse` (`1 px/tile`), `Hd`
  (`<= 32 px/tile`), and `CloseRegion` (`64`/`128 px/tile` cached via `CloseRegionKey` and composited
  via `globeDetailTexture_`) as globe curvature smoothly flattens at high zoom up to 3200%. Proved
  by `aur_tests` 76/76, CTest 7/7, and opened D3D12 + Vulkan flat, orbit globe, and 3200% close globe
  renders (`world_1280x690_z1.0.png`, `world_globe_1280x690_z1.0.png`, `world_globe_close_1280x690_z10.0.png`).
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
  "Generate map…", and `Aur --map seeded_platformer` plays the shipped room. W-02 and
  W-01 are closed (`apply_coastal_beach` and `generate_field_beach_tiles(0)` render
  directional west water, golden sand, and field grass).
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



## Active plans and next action (see WANTED for order)
`WANTED.md` is the only ordered execution queue. All four W-01..W-04 items are now closed
(`W-01` in `f2c102b`; `W-04` P3 in `a894fbc`/`96097fa` + P3.3 closeout on 2026-11-21),
and **P-01** (`Bible/notes/platformer.md` P4) is parked on the side per owner direction
until the other core tracks reach MVP readiness. Next action awaits the owner's next-track
choice (WorldSim WP4/P3.2, DAW P2 soak, or Stage 3 RPG engine skeleton from
`aur_master_expansion_hd_upgrade_plan.md`).

The root `aur_master_expansion_hd_upgrade_plan.md` is the latest owner-added broad proposal, not an obsolete file. Its tracker and detailed stages are draft proposal material; `WANTED.md` remains execution order and focused plans hold verified acceptance. The older divergent `Bible/notes/` copy is retained as provenance. Conformance reports the root file as stray; its long-term home is a separate owner decision, and both copies remain preserved.

- `Bible/notes/globe-view.md` — W-04 complete and visually verified on D3D12 + Vulkan (2026-11-21); no open steps remain.
- `Bible/notes/globe-expand.md` — W-01 complete and verified (2026-11-21, `f2c102b`); no open steps remain.
- `Bible/notes/platformer.md` — P1–P3 landed; parked on the side per owner direction (2026-11-21) until the other core tracks are MVP-ready.
- `Bible/notes/Aur World Map Agent Work Packages.md` — WP0–WP3 landed; WP4 temperature seeding/persistence are gated, WP6 later. Its WP5 globe dependency is historical; the focused globe plan owns W-04.
- `Bible/notes/worldsim-p31.md` — core landed; P3.2 editor integration owner-gated.
- `Bible/notes/daw/plan.md` — DAW plan; P7a parked by owner, resume in `Bible/notes/daw/gm-gs.md`; P2 close awaits the owner GUI soak.
- Estate audit plans `C:\Projects\Bible\notes\project-handoff-prep.md` and `qt-desktop-foundation.md` are read-only/staged, behind Aur's owner queue.
- `Bible/notes/doc-reconciliation.md` — S-01.5b–S-01.7 and P2 proposal validation are complete; older RPG/HD scope maps to root Stages 3–6, and source-backed design is distinguished from proposal-only elaboration. Aur docs policy updated to 2,000-line soft / 4,000-line hard. Remaining: estate-policy handoff and separate root-proposal placement/conformance; preserve WANTED order, owner gates, both proposal copies, and unrelated worktree paths.

- Maintained notes/docs are living documents: 2,000-line soft cap and 4,000-line hard cap; split earlier for navigation or topic cohesion, and at hard cap split into focused docs with a short index before further growth. Vendored/reference material and immutable source archives are exempt. Aur's canonical AGENTS.md, PROJECT.md, Bible index, and this handover state the policy. `Bible/notes/forge-brief.md` was last measured at 1,900 lines; review its shape when next maintained.

## Handoff for the next session
- **Next owner choice:** All W-01..W-04 world/studio items are closed. Confirm the next active track: WorldSim WP4/P3.2 (`Bible/notes/worldsim-p31.md`), DAW P2 soak (`Bible/notes/daw/plan.md`), or Stage 3 RPG engine skeleton (`aur_master_expansion_hd_upgrade_plan.md`).
- **W-04 closed & verified (2026-11-21):** region-scoped `build_hd_surface`, 3-tier `TerrainLod` (`CloseRegionKey`), Vulkan/D3D12 `isYUpInNDC()` parity, `#171b20` margins, `globeRevision_` live sculpts, smooth curvature-to-flat zoom, `globeDetailTexture_` at 64/128 px/tile, `aur_tests` 76/76, CTest 7/7, and opened D3D12 + Vulkan captures.
- **P-01 (`Bible/notes/platformer.md` P4) parked on the side (2026-11-21):** owner directed keeping the platformer part on the side until the other core tracks feel MVP-ready.
- Preserve unrelated worktree modifications (`AIO-Win.bat`, `cmake/`, doc-reconciliation edits, and both untracked `aur_master_expansion_hd_upgrade_plan.md` copies).
- Estate handoff: `C:\Projects\AGENTS.md` / `ESTATE-LENGTH.md` / `DESIGN.md` §14 live above this workspace and remain for the estate agent to reconcile to the 2,000-soft / 4,000-hard standard.
- Startup reading order: `AGENTS.md`, `HANDOVER.md`, `WANTED.md`, `PROJECT.md`, then every active plan listed in `WANTED.md`.

## Verification and build references

- `PROJECT.md` is the verified product state and decisions; `WANTED.md` is the ordered owner queue; `ROADMAP.md` is strategic stages; `IMPLEMENTATION.md` is the open gate ledger. `Bible/Operations/NOTES.md` is historical record, not live status.
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
