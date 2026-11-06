# HANDOVER.md â€” Aur

> Current session status and implementation state for agent handover. See the [Bible index](../README.md) for document navigation and source precedence.

> Active direction: continue the existing C++20 + Qt 6.12 Studio/engine. Godot mentions in older design bibles are legacy context, not a planned port or current implementation target.
> Game codenames: **Dawn** (turn-based), **Day** (tactical/grid), and **Dusk** (real-time action) are the three planned games. **Night** is an unplanned possible fourth game; do not treat it as current scope. Codenames are provisional.

## Current status
Aur is the C++20 / Qt 6.12 product: `aur_engine`, `aur_studio`, `aur_game`, `aur_mcp`, and `aur_tests`. Release links all targets; the old `MapEditorPanel.cpp` compile errors are fixed. Verified 2026-11-06: Release `EngineTests` and `TopologyTests` passed 1/1 each; `AIO.bat compile windows Release` selected `build\\Release`, deployed Qt, and exited 0. P3.0 live seam evidence and remaining acceptance are in [World Map Topology Handover](World%20Map%20Topology%20Handover.md).
- **Engine:** `engine/aur/` builds the Qt-free `aur_engine`; active modules include field movement, tactics skeleton, music state, asset generators, and spherical world generation. See `Project Overview.md` for feature inventory.

- **Runtime scale:** 640Ã—360 logical view at 2Ã— output; 32Ã—32 field cells. The 2D-HD diorama-to-zone coordinate contract is documented in `Project Overview.md`.
- **Studio UI**: Starts maximized to the screen work area (16:10 on the target display) with a frameless Qt title/menu bar, Qt minimize/maximize/close controls, and a 1280Ã—720 minimum. The World editor includes 12 editable JRPG biome presets, biome painting, seeded spherical world generation (continents, archipelago, supercontinent, fractured) up to 2560Ã—1280 tiles (new-map default, capped at 3,276,800 cells), and an elevation-shaded HD surface with distance-aware beaches, surf, underwater shelves, smooth deep-ocean shading, polar ice, connected drainage-fed rivers, inland lakes, forest, and clustered mountain detail plus terrain overrides. Generated worlds use elevation/climate/biome/hydrology layers rather than legacy tile grids; terrain and biome painting work directly on those layers. The filtered terrain palette can add an original 21-tile starter set: nine general terrain/elevation materials plus dedicated terrain starters for all 12 project biomes, then brush selected tiles over any elevation/depth. The map canvas previews temperature/equator/poles and illustrative wind/current overlays; its weather selector previews project climate profiles or forced clear, cloudy, rain, snow, storm, and fog, while local time updates terrain lighting direction, day/night color, and weather animation. Weather/time are editor previews, not map-persisted settings. Generated worlds start without placeholder spawn/location markers; add authored locations as needed. River, lake, beach, mountain-range, hill, and cave brushes author terrain directly. The renderer supports this bounded map size; larger worlds need compact layer storage and viewport/chunk rendering. The current JSON layer model is memory-intensive at the maximum size: Release `aur_tests.exe` peaked at 6,280 MiB working set (6,359 MiB private bytes) while exercising the 2560Ã—1280 generator case; the complete suite took 27.83 seconds, so that is not an isolated generation-time measurement. Actual Studio generation and D3D12 rendering at 2560Ã—1280 were verified on Intel UHD Graphics: the UI returned to idle in 97.57 seconds, peaking at 7,031 MiB working set (7,133 MiB private bytes). The map was visually checked at 19%, 100%, and 400% zoom. This wall time includes UI refresh and a five-second idle tail; GPU frame time and FPS remain unmeasured. New generated maps use automatic seed-based IDs/names (numbered for duplicates), with a random seed on dialog open and a Randomize control, so seed experiments do not require manual renaming. Tactical maps show terrain, units, and collision like Fire Emblem / Advance Wars. Pixel Editor includes frame selection, animation preview, 720p-minimum adaptive zoom, and up to 64Ã— manual zoom; Sprite Editor focuses on direction/pose/frame editing.
- **World Map:** source-tile-aware zoom, cursor-centered wheel zoom, middle-drag pan, culling, fit, and project-backed location/exit editing are implemented.

- **Music Studio**: Songs, tracks, notes, scenes, and clips are project-backed. Session Grid provides track rows, scene columns, clip creation/removal, scene launch preview, and scene management; the Piano Roll remains a separate tab for note editing. A searchable, named 128-patch GM program selector writes standard program numbers, with a dedicated channel-10 drum-track workflow. Standard MIDI Type 0/1 import/export retains channel, bank, program, common controllers, and SysEx. On Windows, WinMM MIDI input/output can connect hardware devices, record incoming notes into the selected track, and route preview playback to external GM/GS devices. Built-in GM-family, six-operator FM-style, and retro PSG previews are original oscillator synthesisâ€”not sampled SoundFont playback or a GS soundbank player. SoundFont/sample playback, VST hosting, and BRR/SPC playback are not implemented.
- **Editor basics**: File menu supports Open Project (`Ctrl+O`), Save (`Ctrl+S`), and Reload from Disk. Opening another project prompts to save only when the project data differs from its last saved/opened snapshot. Map paint strokes and Sprite/Pixel frame edits have bounded local undo/redo; Sprite/Pixel support same-dimension full-frame copy/cut/paste. Transparent sprite previews now use a light checkerboard. This is a first pass, not project-wide history yet.
- **Character generation**: The Character Forge supports the complete race and supernatural category roster, seeded race-aware appearances, gender identity (`female`, `male`, `non-binary`, `unspecified`), visual presentation (`feminine`, `masculine`, `androgynous`), and main-character/NPC/follower roles. Creature and pet generation is in the same workflow. Alignment is authored separately from creed/morality and corruption. See [Character Creator and Species Visuals](../Production/Character%20Creator%20and%20Species%20Visuals.md).
- **Asset Forge**: Studio draws deterministic editable 32Ã—32 indexed-pixel clothing (pants, shorts, skirts, dresses, shirts, tunics) and props natively; this is procedural pixel drawing, not image generation. Character generation produces clothing/accessories onlyâ€”weapon art is a separate prop workflow.
- **Race authoring**: Database > Races now edits project-backed race records (create, rename/ID change, update, delete) with editable lore and gameplay fields. Sylvi's sample includes the six mortal peoples and supernatural groups from the legacy World Bible taxonomy; Sylvi's actor record links to `sylvani`. Edits apply in memory and persist through the normal Save Project action.
- **World/time authoring**: World & Time Settings edits calendar length, seasons, phase boundaries, the five era definitions, and weighted weather profiles by climate. The game clock reads those project settings; weather rolls use the active map's climate profile, and the renderer displays rain, snow, fog, and cloud cover.
- **In-game cheats**: Press **F2** during gameplay or Studio playtest to open the in-game debug menu (available in Release builds). It pauses movement/clock while open and can set era, year/season/day/time, clock rate and pause state, climate (or return to the map climate), weather lock, and arbitrary runtime story/world switches. An optional HUD shows the active values. These are runtime overrides, not saved game state; project settings persist through Save Project.
- **Era scope**: Era definitions and live era selection are editable, but era-specific map-layer variants, timegate transitions, and cross-era world consequences are not implemented yet.
- **Sylvi starter content**: `maps/earthlike_world.json` is a separate 256Ã—128 HD Earth-like world (`seed=20250308`, continents, water 50%, vegetation 65%, decay 15%) with elevation, moisture, temperature, editable terrain overrides, biome assignments, and four map markers. Its unused legacy tile grids are removed. Aurelia remains a traversable field map with its existing links, and the project's start map is unchanged.
- **Playtest protagonist**: Sylvi's project includes a project-backed eight-direction `hero` graphic with 48Ã—64 source frames, rendered at native size and bottom-center anchored for the 32-pixel field grid. The first deliberate race pass is Sylvi as a female elf traveler, with pointed ears, long copper hair, circlet, layered green-and-gold clothing, and a sword. Separate child and teen graphics use the same palette and outfit as adult `hero` to compare proportions. Production-authored character art is still needed.
- **Project layout**: `projects/sylvi/` is the canonical sample used by C++ launchers. The Python implementation was only a scratchpad/testbed and has been retired rather than kept as a porting target; its historical README and visual screenshots remain under `reference/`.
- **Game output**: `aur_game` presents a 1280Ã—720 (16:9) target; 32Ã—32 logical field tiles render as 64Ã—64 screen pixels at 2Ã—. Runtime day/night ambient lighting and biome-based climate selection are wired; World Map wind/current/weather marks remain preview overlays rather than a unified persisted simulation. Both Studio and game open with the Sylvi project; the named-frame startup crash is fixed.
- **Default project launch**: Studio resolves its default Sylvi project relative to the working directory or executable; the AIO build script launches from the selected output directory. This prevents a blank project (and empty World tab) when launched from the build directory or another working directory.
- **MCP**: `aur_mcp <project-directory>` exposes project/map/biome inspection, biome painting, seeded world generation, seeded character/creature/pet/effect/tile generation, and tile/palette/graphic edits. Mutations validate input and save inside the supplied project directory. Use Studio's **Reload Agent Changes** button to refresh the open project without silently losing unsaved edits. See the [MCP server guide](MCP%20Server.md).
- **Git**: initialized (`git init`); `.gitignore` present.

## Build & Test Commands

```powershell
.\build.bat                      # Release build + deploy to build/Release
.\build.bat debug                # Debug build + deploy to build/Debug
.\build.bat release run          # Build, deploy, and launch Aur Studio
.\build.bat debug game           # Build, deploy, and launch aur_game
ctest --test-dir build\Release --output-on-failure
```

## Current open work
1. **Editors:** Map/Pixel shortcuts need a live smoke test; project-wide undo/redo, region selection/clipboard, complete entity authoring, and dirty-state/autosave recovery remain open.
2. **Roadmap:** MCP external-change notifications, era-specific layers/timegates, authored art, audio playback features, and headless golden-image tests. See `Roadmap.md`.
3. **P3.1 WorldSim** is next in the topology dependency chain but requires owner approval to start; the P3.0 gate is complete (see [World Map Topology Handover](World%20Map%20Topology%20Handover.md)).





## Build entry points
`AIO.bat` dispatches to `AIO-win.bat` and `AIO-linux.bat`; `build.bat` remains available. The Windows dispatcher selects the requested configuration tree and builds `aur_studio` for Qt deployment. Release tree selection was verified 2026-11-06; toolbar/context-menu changes rebuilt and passed Release CTest on 2026-11-06.


## Asset-pipeline audit (A0; implementation not authorized)
This source-inspection audit records unresolved editor gaps; recheck current code before acting. A0 is not authorization for implementation.

### One-page asset graph
```text
Project (shared in-memory object in MainWindow)
â”œâ”€ tilesets.json -> Project::tilesets
â”‚  â””â”€ tileset: tile size, palettes[], tiles[{name, rows, pal, flags, optional anim/fps}]
â”‚     â”œâ”€ Tileset Editor: tileset selector + currently unpopulated QListWidget + disconnected flag checkboxes
â”‚     â”œâ”€ Map Editor: map.layers ground/objects/above store integer tile indices only;
â”‚     â”‚              tile preview resolves index -> selected map tileset pixels/palette;
â”‚     â”‚              Collision Overlay asks Project::tile_flags for ground/object indices
â”‚     â””â”€ Forge: seeded generated tile appended into selected tileset; displays a temporary graphic preview
â”œâ”€ graphics.json -> Project::graphics
â”‚  â””â”€ graphic: w/h, frames{frame-id: indexed pixel rows}, palette[<=16], origin, anims{frames,fps,loop}
â”‚     â”œâ”€ Pixel Editor: selects any graphic; edits frame pixels and locally owns palette/frame/animation selections
â”‚     â”œâ”€ Sprite Editor: selects character/enemy/animated graphics; derives candidate frames from anims,
â”‚     â”‚                 direction, pose; locally owns selections and a separate local pixel undo stack
â”‚     â””â”€ Forge: seeded generation produces character/creature/effect/clothing/prop graphics in this store;
â”‚              character/enemy/item records link to graphics; generated tiles instead live in tilesets
â”œâ”€ events.json -> Project::events -> Event Scripting lists event IDs (editor is currently a sample/mock UI)
â””â”€ maps/*.json -> Project::maps -> entities may contain event command data, but Map Inspector's Event ID
                    line edit is a hard-coded, unbound sample value (not persisted/validated)
```

### Per-tab A0 findings
| Tab | (a) State centralization | (b) Existing SNES/data-model support | A0 assessment |
|---|---|---|---|
| Tileset Editor | Tileset records live in shared `Project::tilesets`, but current selection is widget-local. Its tile list, flag checkboxes, and rendered tile attributes are not connected to project data. | Tiles include indexed `rows`, per-tile `pal`, gameplay `flags`, optional animation references/FPS; tileset has palettes and tile size. No map-cell palette/priority/H/V-flip record or editable per-tile render attributes found. | **Blank left pane is a UI wiring/feature gap, not empty project data.** `refreshTilesets()` clears `tileList` then only fills the tileset combo. Sylvi contains populated world/terra/cave tile arrays. Existing flag label incorrectly calls gameplay flags â€œSNES Tile Attributes.â€ |
| Sprite Editor | Sprite graphic/frame/palette/direction/pose selection is local to Sprite widgets; graphic rows and palette itself reside in shared graphics JSON. PixelCanvas owns its own current frame, color, and undo/redo state. | Frames, palette, origin, named animations with frame lists/FPS/loop exist. Character generation has eight authored directions; Forge has a mirror helper for derivation. No palette-variant/recolor set, explicit base+H-flip linkage, pivot separate from origin, sort offset, or collision rect contract found. | Indexed frame data is present; selection/history duplicated across Sprite and Pixel rather than a shared edit session. |
| Pixel Editor | Graphic/frame/animation/palette selection and frame undo history are local. Graphics and frame pixels are shared via the same `Project::graphics` object. | 16-entry palette convention and indexed pixel rows; frame/animation records and FPS. No explicit named sub-palettes, shared palette library, palette swap variants, per-frame delay/strip data, or serialized edit history. Pixel tools already expose 8Ã—8 tile brush, pixel, 16Ã—16 stamp, picker, eraser, frame copy/paste. | 0-index is used as transparent by draw conversion, but UI calls it an indexed palette with no explicit transparency indication. |
| Map Editor | Map selection/layer/tile and local undo history are widget-local; map tile IDs, layers, entities live in shared `Project::maps`. Tileset gameplay flags are read from shared tilesets through `Project::tile_flags`, so current Collision Overlay is already data-backed (but only flags tiles that exist). | Map cells are plain integers; flags are per tileset tile, not cell attributes. No palette/priority/H/V flips stored per cell. The map rendering path fetches tile pixels and palette from tileset schema. | Has working tile draw path and SOLID/WATER/HURT/SLOW/ENCOUNTER overlay; no event-ID validation/completer. Hard-coded Inspector fields are not wired to entity data. |
| Character Forge | Generator spec/preview and GUI controls local; generated graphic and linked character/enemy/item records enter the shared project stores. Seed is an explicit control; some saved records include seed. | Seeded character/creature/effect/clothing/prop/tile generators; graphics use indexed rows/palette and animations. Generator includes directional generation and mirror helper. No common serialized recipe/version/hash stamp, pinned gallery, palette library registration, or procedural-vs-hand-authored layer stack/provenance. | Forge output paths are split correctly by kind across graphics/characters/database/tilesets, but no uniform provenance contract. Clothing/props are direct generated editable frames, not separate non-destructive stacks. |

### Shared-store and schema conclusions
- The main in-memory asset/project store is already one `Project` instance passed to each panel and maps JSON-backed stores to `tilesets.json`, `graphics.json`, `characters.json`, `database.json`, `events.json`, maps, and music. This is shared data, not a dedicated asset bus: selection, transient palette, per-panel undo, and inspector state are duplicated/local.
- Gameplay tile flags are already centralized per tile in `Project::tilesets[*].tiles[*].flags`; Map overlay reads them directly. However, Tileset checkboxes are unbound, so there is currently no cross-tab editing/reflection pathway. Do not mistake the existing overlay for a missing flag store.
- Palette arrays currently live inline per tileset (`palettes[]`) or per graphic (`palette[]`). They are indexed color data, not a centralized palette library; named sub-palettes/palette variants/recolor references are absent.
- Tile-map attributes are incomplete: tileset tile records contain a palette selection (`pal`) and gameplay flags, but map layers hold only integer tile IDs. Priority and H/V flip per map cell are absent. Keep gameplay flags distinct from SNES render attributes.
- Sprite `origin` is the only observed placement metadata. Frame/animation data supports timelines, while editor playback derives duration from animation FPS; independent per-frame delays were not found. Eight directions may be explicitly authored, but derivable base+flip metadata is absent.
- Field and world maps both reference project tilesets and use `make_tileset_tile` against the same tileset `tiles[].rows`, `pal`, and `palettes[]` layout for preview. This suggests shared source-tile schema, but the field renderer and world renderer have different consumers/overrides; schema unity should be confirmed by A2 round-trip tests, not asserted as fully proven from this audit.
- Event registry data exists in `Project::events`; Event Scripting currently lists IDs but is a sample editor with mock commands/validator. Mapâ€™s â€œEvent IDâ€ is a hard-coded `QLineEdit`, not bound to map entity properties or validated against the registry.

### A0 result / gate
Audit complete; no behavior changes made. A0 exit evidence: the three left-pane clues are directly accounted for by source inspection; both populated tilesets and project store mappings were examined; no test/build run was needed because implementation code/data were not altered. **Stop here. A1 is not authorized by this audit completion unless separately approved.** Track A remains separate from P3 topology and M Music. Modern unlimited layers/features are design-change proposals, not implementation scope.

**Open defects logged for future phase planning:** Tileset grid is empty despite project tiles; flag controls are decorative; gameplay flags are mislabeled as render attributes; Map Inspector event field is sample-only and accepts arbitrary text; palette state/library absent; map cells lack SNES render attributes; Sprite/Pixel palette/editor state and undo stacks are separate; Forge lacks versioned provenance/hash/gallery and layered outputs.


**Additional build-entry note:** verified that root `AIO.bat` also handles configure/build/run (`build`, `debug`, `test`, `run`). Its test action runs CTest against Debug; this audit did not execute either build path.


## Open work
- P3.0 topology gate is complete (final 400% edge-column evidence captured 2026-11-06, `Bible/Operations/evidence/p3-seam-400pct.png`). P3.1 WorldSim is the next phase and requires owner approval before starting. See [World Map Topology Handover](World%20Map%20Topology%20Handover.md).
- Map/Pixel keyboard shortcuts are implemented but not live-smoke-tested. Project-wide undo/redo, map region selection/clipboard, complete entity authoring, and dirty-state/autosave recovery remain open.
- Other roadmap work: MCP external-change notifications, era-specific layers/timegates, authored art, audio playback features, and headless golden-image tests. See `Roadmap.md`.

## Focused handovers
P3.0 evidence and remaining acceptance are in [World Map Topology Handover](World%20Map%20Topology%20Handover.md). A0 findings are source-inspection notes, not implementation authorization; recheck active code before acting. Audio work remains in the roadmap and focused design documents.
















## A1 Map Editor follow-up

