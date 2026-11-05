# HANDOVER.md — Aur

> Current session status and implementation state for agent handover. See the [Bible index](../README.md) for document navigation and source precedence.

> Active direction: continue the existing C++20 + Qt 6.12 Studio/engine. Godot mentions in older design bibles are legacy context, not a planned port or current implementation target.
> Game codenames: **Dawn** (turn-based), **Day** (tactical/grid), and **Dusk** (real-time action) are the three planned games. **Night** is an unplanned possible fourth game; do not treat it as current scope. Codenames are provisional.

## Current State (Part 1 PoC)

- **Build System**: `build.bat [release|debug] [build|run|game]` configures/builds through CMake presets with Ninja, `--parallel 8`, then deploys Qt runtime dependencies and `Qt6Multimedia.dll`. It can optionally launch Studio or the game from the selected output directory; no separate run script is needed.
- **Targets**: `aur_engine` (static lib, std-only), `aur_studio` (Qt 6.12 Widgets IDE), `aur_game` (runtime), `aur_mcp` (stdio MCP server), `aur_tests` (CTest).
- **Tests**: Release `EngineTests` passed on 2026-11-04 (1/1 CTest, 26.89 s) and the focused Release `TopologyTests` passed 1/1 CTest (5 functions) on the same day. The focused suite adds fully formed bounded-map rejection, generated-world Y rejection, climate sampling at both X edges, malformed biome-row fallback, and camera policy coverage. This still does not complete P3.0: the full `make` / `build.bat release` cannot link `Aur Studio` because of 23 pre-existing `studio/MapEditorPanel.cpp` errors, so live editor seam and GPU/fallback visuals remain unverified. See [World Map Topology Handover](World%20Map%20Topology%20Handover.md) for the phase-specific checklist.
- **Engine modules**: the public engine lives in `engine/aur/` under the `aur` namespace and builds as `aur_engine`. `field.hpp` includes normalized eight-way movement and directional-frame selection; `tactics.hpp` remains a grid/unit/selection skeleton; `music.hpp` is playback-state/queue. `forge.cpp` provides deterministic seeded NPC, creature/pet, effect, and tile generation plus native indexed-pixel clothing and prop drawing. `worldgen.cpp` creates seeded spherical worlds with multiple topology presets and saved elevation, moisture, and temperature fields.
- **Game launch**: `Game::update` loads the configured start map into a `FieldScene` on first tick. Named map spawn records now set the player's position and facing; explicit travel coordinates retain pixel positioning. `Game::travel()` switches maps.
- **Playtest rendering**: Runtime scenes receive update ticks; the Qt renderer draws actual project tileset pixels in a 640×360 logical view, 2× presented at 1280×720. Field tiles are 32×32 logical pixels, framing about 20×11 tiles. The Sylvi world and terra source tilesets now contain 32×32 indexed art; cave and any other legacy tilesets remain 16×16 and are upscaled at runtime.
- **Studio UI**: Starts maximized to the screen work area (16:10 on the target display) with a frameless Qt title/menu bar, Qt minimize/maximize/close controls, and a 1280×720 minimum. The World editor includes 12 editable JRPG biome presets, biome painting, seeded spherical world generation (continents, archipelago, supercontinent, fractured) up to 2560×1280 tiles (new-map default, capped at 3,276,800 cells), and an elevation-shaded HD surface with distance-aware beaches, surf, underwater shelves, smooth deep-ocean shading, polar ice, connected drainage-fed rivers, inland lakes, forest, and clustered mountain detail plus terrain overrides. Generated worlds use elevation/climate/biome/hydrology layers rather than legacy tile grids; terrain and biome painting work directly on those layers. The filtered terrain palette can add an original 21-tile starter set: nine general terrain/elevation materials plus dedicated terrain starters for all 12 project biomes, then brush selected tiles over any elevation/depth. The map canvas previews temperature/equator/poles and illustrative wind/current overlays; its weather selector previews project climate profiles or forced clear, cloudy, rain, snow, storm, and fog, while local time updates terrain lighting direction, day/night color, and weather animation. Weather/time are editor previews, not map-persisted settings. Generated worlds start without placeholder spawn/location markers; add authored locations as needed. River, lake, beach, mountain-range, hill, and cave brushes author terrain directly. The renderer supports this bounded map size; larger worlds need compact layer storage and viewport/chunk rendering. The current JSON layer model is memory-intensive at the maximum size: Release `aur_tests.exe` peaked at 6,280 MiB working set (6,359 MiB private bytes) while exercising the 2560×1280 generator case; the complete suite took 27.83 seconds, so that is not an isolated generation-time measurement. Actual Studio generation and D3D12 rendering at 2560×1280 were verified on Intel UHD Graphics: the UI returned to idle in 97.57 seconds, peaking at 7,031 MiB working set (7,133 MiB private bytes). The map was visually checked at 19%, 100%, and 400% zoom. This wall time includes UI refresh and a five-second idle tail; GPU frame time and FPS remain unmeasured. New generated maps use automatic seed-based IDs/names (numbered for duplicates), with a random seed on dialog open and a Randomize control, so seed experiments do not require manual renaming. Tactical maps show terrain, units, and collision like Fire Emblem / Advance Wars. Pixel Editor includes frame selection, animation preview, 720p-minimum adaptive zoom, and up to 64× manual zoom; Sprite Editor focuses on direction/pose/frame editing.
- **Studio visual system**: A shared dark, warm-metal, teal-accent theme now applies consistently across tabs. World Map uses source-tile-aware zoom (100% default), terrain thumbnail previews, cursor-centered wheel zoom, middle-drag pan, off-screen tile culling, Fit, and project-backed location/exit placement, editing, deletion, and undo/redo.
- **World surface polish**: World-map weather is clipped to map bounds; clouds and animated rain/snow use deterministic spatial variation instead of extending into the canvas or repeating in a rigid grid. Forests use procedural, hashed canopy shading in the GPU renderer and fewer, larger translucent clusters in the fallback renderer. The final canopy pass was visually checked at 400% on D3D12; Rain, Snow, and Fog were visually checked at 400%, with clear-weather lighting checked at 12:00 and 20:00. Release build and `EngineTests` pass. GPU frame time and FPS remain unmeasured.
- **Music Studio**: Songs, tracks, notes, scenes, and clips are project-backed. Session Grid provides track rows, scene columns, clip creation/removal, scene launch preview, and scene management; the Piano Roll remains a separate tab for note editing. A searchable, named 128-patch GM program selector writes standard program numbers, with a dedicated channel-10 drum-track workflow. Standard MIDI Type 0/1 import/export retains channel, bank, program, common controllers, and SysEx. On Windows, WinMM MIDI input/output can connect hardware devices, record incoming notes into the selected track, and route preview playback to external GM/GS devices. Built-in GM-family, six-operator FM-style, and retro PSG previews are original oscillator synthesis—not sampled SoundFont playback or a GS soundbank player. SoundFont/sample playback, VST hosting, and BRR/SPC playback are not implemented.
- **Editor basics**: File menu supports Open Project (`Ctrl+O`), Save (`Ctrl+S`), and Reload from Disk. Opening another project prompts to save only when the project data differs from its last saved/opened snapshot. Map paint strokes and Sprite/Pixel frame edits have bounded local undo/redo; Sprite/Pixel support same-dimension full-frame copy/cut/paste. Transparent sprite previews now use a light checkerboard. This is a first pass, not project-wide history yet.
- **Character generation**: The Character Forge supports the complete race and supernatural category roster, seeded race-aware appearances, gender identity (`female`, `male`, `non-binary`, `unspecified`), visual presentation (`feminine`, `masculine`, `androgynous`), and main-character/NPC/follower roles. Creature and pet generation is in the same workflow. Alignment is authored separately from creed/morality and corruption. See [Character Creator and Species Visuals](../Production/Character%20Creator%20and%20Species%20Visuals.md).
- **Asset Forge**: Studio draws deterministic editable 32×32 indexed-pixel clothing (pants, shorts, skirts, dresses, shirts, tunics) and props natively; this is procedural pixel drawing, not image generation. Character generation produces clothing/accessories only—weapon art is a separate prop workflow.
- **Race authoring**: Database > Races now edits project-backed race records (create, rename/ID change, update, delete) with editable lore and gameplay fields. Sylvi's sample includes the six mortal peoples and supernatural groups from the legacy World Bible taxonomy; Sylvi's actor record links to `sylvani`. Edits apply in memory and persist through the normal Save Project action.
- **World/time authoring**: World & Time Settings edits calendar length, seasons, phase boundaries, the five era definitions, and weighted weather profiles by climate. The game clock reads those project settings; weather rolls use the active map's climate profile, and the renderer displays rain, snow, fog, and cloud cover.
- **In-game cheats**: Press **F2** during gameplay or Studio playtest to open the in-game debug menu (available in Release builds). It pauses movement/clock while open and can set era, year/season/day/time, clock rate and pause state, climate (or return to the map climate), weather lock, and arbitrary runtime story/world switches. An optional HUD shows the active values. These are runtime overrides, not saved game state; project settings persist through Save Project.
- **Era scope**: Era definitions and live era selection are editable, but era-specific map-layer variants, timegate transitions, and cross-era world consequences are not implemented yet.
- **Sylvi starter content**: `maps/earthlike_world.json` is a separate 256×128 HD Earth-like world (`seed=20250308`, continents, water 50%, vegetation 65%, decay 15%) with elevation, moisture, temperature, editable terrain overrides, biome assignments, and four map markers. Its unused legacy tile grids are removed. Aurelia remains a traversable field map with its existing links, and the project's start map is unchanged.
- **Playtest protagonist**: Sylvi's project includes a project-backed eight-direction `hero` graphic with 48×64 source frames, rendered at native size and bottom-center anchored for the 32-pixel field grid. The first deliberate race pass is Sylvi as a female elf traveler, with pointed ears, long copper hair, circlet, layered green-and-gold clothing, and a sword. Separate child and teen graphics use the same palette and outfit as adult `hero` to compare proportions. Production-authored character art is still needed.
- **Project layout**: `projects/sylvi/` is the canonical sample used by C++ launchers. The Python implementation was only a scratchpad/testbed and has been retired rather than kept as a porting target; its historical README and visual screenshots remain under `reference/`.
- **Game output**: `aur_game` presents a 1280×720 (16:9) target; 32×32 logical field tiles render as 64×64 screen pixels at 2×. Runtime day/night ambient lighting and biome-based climate selection are wired; World Map wind/current/weather marks remain preview overlays rather than a unified persisted simulation. Both Studio and game open with the Sylvi project; the named-frame startup crash is fixed.
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

## Open Work (see [MVP roadmap](Roadmap.md))

- **World-map wrap gate:** P3.0 cylindrical topology remains in progress, not accepted. Release `EngineTests` passed on 2026-11-03, including all-four-topology predicate regressions, but runtime/editor seam visuals, shader repeat/clamp behavior, and GPU/fallback parity are not yet verified. See [World Map Topology Handover](World%20Map%20Topology%20Handover.md). Do not start WorldSim/hydrology until the P3.0 acceptance evidence is complete and the next phase is approved.
- Add automatic external-change notifications; current MCP integration refreshes through the guarded **Reload Agent Changes** action.
- Add project-backed authoring workflows to the remaining Studio editors; world-map locations and exits now support placement, selection, editing, deletion, and undo/redo.
- Implement era-specific map layers and timegate transition behavior; the current era authoring/cheat controls prepare and test state data but do not yet alter map art or geometry.
- Grow generator templates and add region-level vegetation/water/decay editing for hand-authored maps.
- Extend the active C++ project's 32×32 world/terra art approach to remaining tilesets, then replace procedural foundations with authored higher-detail world, terrain, and character art. The retired Python game remains reference-only; its tiles are not reused.
- Keep playable overworlds as distinct 80×80–128×128-tile regions instead of treating the 256×128 world-select map as one traversable field; introduce chunking if region sizes grow substantially.
- Grow Music Studio with licensed SoundFont/sample playback, VST hosting, BRR/SPC playback, and production audio controls.
- Extend undo/redo beyond canvas edits; add map/world selection and region clipboard, full entity authoring/inspection, pan/zoom, and project dirty-state/autosave recovery.
- Improve the procedural asset pipeline further; current seeded pixel sprites are more legible but are not a substitute for production-quality authored HD-2D assets.
- No maintained golden-image baseline is currently available for headless rendering regression tests.
- Externalize UI strings into an en-US resource (no hardcoding).
- Extend the current 16-color indexed art format if the asset workflow needs richer palettes; 1280×720 is only the game presentation target.

## What to verify next

Latest verification (2026-11-05): the 23 pre-existing `studio/MapEditorPanel.cpp` errors are fixed and `Aur Studio` links in Release. Changes: `mapBounds()` moved to a public section in `studio/MapEditorPanel.hpp`; entity find/push in `studio/MapEditorPanel.cpp` now uses `entities.as_array()` and `operator[]` instead of the nonexistent range/push_back/at API. Release full build (via `vcvars64` in the same command) produced all four exes; `TopologyTests` passed 1/1 (0.07 s) and full `EngineTests` passed 1/1 (31.89 s). Committed as `1ed95c3` along with the pre-existing uncommitted AGENTS.md estate rules and AIO-win.bat windeployqt line. Remaining P3.0 item: the live editor/GPU seam visual verification (GPU/fallback parity screenshots), now buildable.

## Build entry point split (2026-11-04)

`AIO.bat` is now the command and menu, and it calls two standalone parts:
`AIO-win.bat` and `AIO-linux.bat`. There is no Android part: Aur is
Windows/Linux only. Generated by `tools\split_aio.py`; `build.bat` still works
unchanged.

**Verified 2026-11-04:**

- `AIO.bat compile` — reached ninja and reported `no work to do` against an
  up-to-date `build\Release` tree.
- `AIO.bat test` — 1/1 `EngineTests` passed (6.74 s, Debug tree).
- `AIO.bat run` — launched `build\Release\Aur Studio.exe`, PID 8304, responding,
with a main window; killed by PID.
- `AIO.bat run` on Release — the only binary is `build\Debug\Aur Studio.exe`
  (2 Nov), so a Release `run` does not exist until the source defect below is
  fixed. The Release link did not complete.
- `AIO.bat build` — reaches the compiler and **fails on pre-existing source**:
  23 errors in `studio/MapEditorPanel.cpp` against `aur::Json`
  (`l-value specifies const object`, `no member named push_back/at`,
  `mapBounds` is private). Not an AIO defect and not touched.

`build.bat` remains the dispatcher's fallback for this project.


## Gated Track A — Asset Pipeline (audit gate A0; 2026-11-03)

**Scope:** SNES-authentic improvements across the five existing asset tabs (Tileset, Sprite, Pixel, Map, Forge), separate from P3 topology and M Music. This entry records an audit only; no implementation behavior was changed. Constraints/design language: 8×8 tiles, 16-color indexed/sub-palettes, tile-map attributes (index + palette + priority + H/V flip), sprite H-flip, palette-swap recolors, and seeded generation. Keep features bounded and expose/automate these conventions rather than expanding into unlimited modern layers.

### One-page asset graph
```text
Project (shared in-memory object in MainWindow)
├─ tilesets.json -> Project::tilesets
│  └─ tileset: tile size, palettes[], tiles[{name, rows, pal, flags, optional anim/fps}]
│     ├─ Tileset Editor: tileset selector + currently unpopulated QListWidget + disconnected flag checkboxes
│     ├─ Map Editor: map.layers ground/objects/above store integer tile indices only;
│     │              tile preview resolves index -> selected map tileset pixels/palette;
│     │              Collision Overlay asks Project::tile_flags for ground/object indices
│     └─ Forge: seeded generated tile appended into selected tileset; displays a temporary graphic preview
├─ graphics.json -> Project::graphics
│  └─ graphic: w/h, frames{frame-id: indexed pixel rows}, palette[<=16], origin, anims{frames,fps,loop}
│     ├─ Pixel Editor: selects any graphic; edits frame pixels and locally owns palette/frame/animation selections
│     ├─ Sprite Editor: selects character/enemy/animated graphics; derives candidate frames from anims,
│     │                 direction, pose; locally owns selections and a separate local pixel undo stack
│     └─ Forge: seeded generation produces character/creature/effect/clothing/prop graphics in this store;
│              character/enemy/item records link to graphics; generated tiles instead live in tilesets
├─ events.json -> Project::events -> Event Scripting lists event IDs (editor is currently a sample/mock UI)
└─ maps/*.json -> Project::maps -> entities may contain event command data, but Map Inspector's Event ID
                    line edit is a hard-coded, unbound sample value (not persisted/validated)
```

### Per-tab A0 findings
| Tab | (a) State centralization | (b) Existing SNES/data-model support | A0 assessment |
|---|---|---|---|
| Tileset Editor | Tileset records live in shared `Project::tilesets`, but current selection is widget-local. Its tile list, flag checkboxes, and rendered tile attributes are not connected to project data. | Tiles include indexed `rows`, per-tile `pal`, gameplay `flags`, optional animation references/FPS; tileset has palettes and tile size. No map-cell palette/priority/H/V-flip record or editable per-tile render attributes found. | **Blank left pane is a UI wiring/feature gap, not empty project data.** `refreshTilesets()` clears `tileList` then only fills the tileset combo. Sylvi contains populated world/terra/cave tile arrays. Existing flag label incorrectly calls gameplay flags “SNES Tile Attributes.” |
| Sprite Editor | Sprite graphic/frame/palette/direction/pose selection is local to Sprite widgets; graphic rows and palette itself reside in shared graphics JSON. PixelCanvas owns its own current frame, color, and undo/redo state. | Frames, palette, origin, named animations with frame lists/FPS/loop exist. Character generation has eight authored directions; Forge has a mirror helper for derivation. No palette-variant/recolor set, explicit base+H-flip linkage, pivot separate from origin, sort offset, or collision rect contract found. | Indexed frame data is present; selection/history duplicated across Sprite and Pixel rather than a shared edit session. |
| Pixel Editor | Graphic/frame/animation/palette selection and frame undo history are local. Graphics and frame pixels are shared via the same `Project::graphics` object. | 16-entry palette convention and indexed pixel rows; frame/animation records and FPS. No explicit named sub-palettes, shared palette library, palette swap variants, per-frame delay/strip data, or serialized edit history. Pixel tools already expose 8×8 tile brush, pixel, 16×16 stamp, picker, eraser, frame copy/paste. | 0-index is used as transparent by draw conversion, but UI calls it an indexed palette with no explicit transparency indication. |
| Map Editor | Map selection/layer/tile and local undo history are widget-local; map tile IDs, layers, entities live in shared `Project::maps`. Tileset gameplay flags are read from shared tilesets through `Project::tile_flags`, so current Collision Overlay is already data-backed (but only flags tiles that exist). | Map cells are plain integers; flags are per tileset tile, not cell attributes. No palette/priority/H/V flips stored per cell. The map rendering path fetches tile pixels and palette from tileset schema. | Has working tile draw path and SOLID/WATER/HURT/SLOW/ENCOUNTER overlay; no event-ID validation/completer. Hard-coded Inspector fields are not wired to entity data. |
| Character Forge | Generator spec/preview and GUI controls local; generated graphic and linked character/enemy/item records enter the shared project stores. Seed is an explicit control; some saved records include seed. | Seeded character/creature/effect/clothing/prop/tile generators; graphics use indexed rows/palette and animations. Generator includes directional generation and mirror helper. No common serialized recipe/version/hash stamp, pinned gallery, palette library registration, or procedural-vs-hand-authored layer stack/provenance. | Forge output paths are split correctly by kind across graphics/characters/database/tilesets, but no uniform provenance contract. Clothing/props are direct generated editable frames, not separate non-destructive stacks. |

### Shared-store and schema conclusions
- The main in-memory asset/project store is already one `Project` instance passed to each panel and maps JSON-backed stores to `tilesets.json`, `graphics.json`, `characters.json`, `database.json`, `events.json`, maps, and music. This is shared data, not a dedicated asset bus: selection, transient palette, per-panel undo, and inspector state are duplicated/local.
- Gameplay tile flags are already centralized per tile in `Project::tilesets[*].tiles[*].flags`; Map overlay reads them directly. However, Tileset checkboxes are unbound, so there is currently no cross-tab editing/reflection pathway. Do not mistake the existing overlay for a missing flag store.
- Palette arrays currently live inline per tileset (`palettes[]`) or per graphic (`palette[]`). They are indexed color data, not a centralized palette library; named sub-palettes/palette variants/recolor references are absent.
- Tile-map attributes are incomplete: tileset tile records contain a palette selection (`pal`) and gameplay flags, but map layers hold only integer tile IDs. Priority and H/V flip per map cell are absent. Keep gameplay flags distinct from SNES render attributes.
- Sprite `origin` is the only observed placement metadata. Frame/animation data supports timelines, while editor playback derives duration from animation FPS; independent per-frame delays were not found. Eight directions may be explicitly authored, but derivable base+flip metadata is absent.
- Field and world maps both reference project tilesets and use `make_tileset_tile` against the same tileset `tiles[].rows`, `pal`, and `palettes[]` layout for preview. This suggests shared source-tile schema, but the field renderer and world renderer have different consumers/overrides; schema unity should be confirmed by A2 round-trip tests, not asserted as fully proven from this audit.
- Event registry data exists in `Project::events`; Event Scripting currently lists IDs but is a sample editor with mock commands/validator. Map’s “Event ID” is a hard-coded `QLineEdit`, not bound to map entity properties or validated against the registry.

### A0 result / gate
Audit complete; no behavior changes made. A0 exit evidence: the three left-pane clues are directly accounted for by source inspection; both populated tilesets and project store mappings were examined; no test/build run was needed because implementation code/data were not altered. **Stop here. A1 is not authorized by this audit completion unless separately approved.** Track A remains separate from P3 topology and M Music. Modern unlimited layers/features are design-change proposals, not implementation scope.

**Open defects logged for future phase planning:** Tileset grid is empty despite project tiles; flag controls are decorative; gameplay flags are mislabeled as render attributes; Map Inspector event field is sample-only and accepts arbitrary text; palette state/library absent; map cells lack SNES render attributes; Sprite/Pixel palette/editor state and undo stacks are separate; Forge lacks versioned provenance/hash/gallery and layered outputs.


**Additional build-entry note:** verified that root `AIO.bat` also handles configure/build/run (`build`, `debug`, `test`, `run`). Its test action runs CTest against Debug; this audit did not execute either build path.


## Consolidated Track Brief Update (2026-11-03)

Owner clarification supersedes prior draft assumptions. P3 topology, M music, and A art pipeline are three independent gates; never bundle deliverables. This session followed STEP 0: `git status --short` was captured first; all named source files were read in full; prior A0 findings were rechecked rather than assumed. No implementation files or project data were modified.

### STEP 0 / revalidation
- `TilesetEditorPanel.cpp`: no longer missing/empty (55 lines). `refreshTilesets()` clears `tileList` and only fills the tileset combo; the blank list is confirmed to be a UI wiring gap. It contains unconnected SOLID/WATER/SLOW/HURT/ENCOUNTER/BRIDGE checkboxes under the misleading “SNES Tile Attributes” label. Data-model confirmation comes from project code/schema: gameplay `flags` are per tile, tiles have indexed rows + `pal`, and tilesets have palettes. Map cells have no palette/priority/H/V flip struct in the inspected model/editor path. The earlier “empty file” premise was stale; still, A2 implementation needs broader schema round-trip evidence.
- `PixelEditorPanel.cpp`: no longer missing/empty (841 lines). Brush numbering is confirmed: Pixel modes 0=8×8 tile, 1=pixel, 2=16×16 stamp, 3=picker, 4=eraser; Sprite UI options map to 1/0/4. Graphic frames are indexed row strings in `graphics[*].frames`; animations contain frame references, FPS and loop. UI reads inline graphic palettes; there is no explicit sub-palette view/swap or frame strip/per-frame delay. Pixel rows are `.`/hex indices; render path treats index 0 as transparent. Do not state the A3 “prior magic-int mapping” as unverified anymore; replace with a named enum and assert when implementing.
- `world_topology.hpp`: now read in full; no trailing whitespace inside any predicate literal is present. `wraps_world_x` compares exact topology IDs, rejecting missing/non-string/unsupported values. The claimed “sole defect is unseen header trailing spaces” does **not** reproduce in current file state. The current predicate implementation has six comparisons with four supported values; tests cover these supported topologies and missing/non-string/unsupported cases.
- `WorldModel.cpp`: now read in full; model uses `wraps_world_x(map)` and actual `width_` for X normalization, without a competing topology predicate.
- Git status showed the same broad pre-existing dirty/untracked project tree; no cleanup/reset/staging was performed. Source-only changes remained absent at the end of this audit.

### A0 asset graph corrections / map-runtime render search
- Preserve the already recorded one-page graph and per-tab audit, subject to these corrections: Map’s flag overlay already exists, renders SOLID/WATER/HURT/SLOW/ENCOUNTER using `Project::tile_flags`, and must not be rebuilt. The left paint palette has three tile entries plus four entity entries (not four total). The renderer switch recognizes eight entity kinds: spawn/exit/npc/chest/sign/trigger/light/enemy. They are duplicated, with no common registry, and the paint palette omits sign/trigger/light/enemy.
- Grep/read of `studio/GameView.cpp` found an index-to-color switch (`tile_color`: 0 grass, 1 path, 2 water, 3 bush) used only as fallback when `make_tileset_tile()` fails or returns a null image. The normal runtime path already uses `make_tileset_tile(*project, tileset, index)`, the same tile graphic helper as editor preview. Record the fallback table as a divergence risk/defect; A1 should establish whether any maps reach it and remove/route the fallback through authored tileset graphics only with tests.
- Map camera parity is not present in current `MapEditorPanel`: it fits entire map into `mapBounds()` with no wheel zoom/pan; WorldPanel has zoom, pan, fit-to-view, and camera constraints. Reuse/check WorldPanel interaction behavior before proposing an A1 implementation; do not alter WorldPanel in the P3 gate.
- A1 cost estimate remains **not finalized**: STEP 0 revalidated enough source to correct the one-page audit, but no implementation plan/breakdown was requested or performed. Existing `Project` store and `make_tileset_tile`/`invalidate`/Forge writes are additive integration points; do not rebuild a bus.

### P3 predicate status and gate
- In the re-read current files, existing `tests/test_world_model.cpp` already asserts true for all four topology identifiers and false for absent worldgen/absent topology, non-string topology, and unsupported topology. It verifies `worldX` using width 4, not a hard-coded starter width. The empty-worldgen case covers missing topology; add an explicit ordinary bounded-map fixture only if the P3 gate wants to make that case more readable, rather than duplicating semantics.
- Because the current predicate contains no trailing-literal spaces, no topology change was made. Current working tree marks the listed engine/tests/WorldPanel/WorldModel paths untracked, so `git diff` cannot establish their provenance; preserve them.
- WorldPanel’s wrapping draw/camera/stroke path was only source-inspected; no rendering or camera changes made. A fresh Release build via `build.bat release`, the requested filtered `ctest -R EngineTests`, and fit/zoom backend parity screenshots were **not run** in this audit. P3 remains open; establish the VS build environment and capture evidence at its own gate. The owner has characterized endless X repetition as intended, not a defect.

### M track direction / initial audit pointers (not implementation authorization)

- Locked pivot: Bitwig is upstream composition/sound design. Aur is an adaptive stem arranger plus handwritten learning cores, not a VST host/Ableton clone. No SF2/GM-bank loader or plugin hosting. Imported MIDI uses Aur’s handwritten sketch voices; authentic GM/GS uses the existing hardware MIDI Out.
- Source grep/read confirms `engine/aur/midi.cpp` parses the file’s `division` and passes it to `midi_tick_to_step`, but a fixed `ticks_per_step = 480/4` constant is used by export. Thus the stated “import division-assignment site” was not found as an obvious error; exact MIDI roundtrip/import preservation and event coverage remain unverified. Do not change without tracing parser-to-grid semantics and fixtures.
- `MusicPanel::playPreview()` currently synthesizes the whole song into PCM, writes a temporary WAV, and plays it with the Qt media player; there is no streaming `QAudioSink` mixer. The imported song stores PPQN and an initial tempo/time-signature, but `parse_track` converts note-on/off to steps immediately; full per-event tempo/program/control automation maps are not evident in this representation. No plugin hosting/SF2 implementation was inspected or added.
- The requested authoritative tick timeline, CC64/CC1/pitch-bend fidelity, realtime mixer/tweening, bounded core curriculum, and ergonomic changes remain separate M-gated phases. Their detailed acceptance is not complete; no M implementation was performed.
- Use the Audio Bible’s confirmed adaptive mechanics as design authority. Do not invent a phase clock: project `current_era`, `phase_hours`, calendar/eras/weather are already authored/validated by World Settings. Any modernization outside bounded SNES and adaptive-audio conventions must be logged as an explicit design-change proposal, not silently implemented.

**Stop point:** no P3 code/evidence gate was completed; no M phase started; A0 is an audit only, and A1–A4 remain gated. Do not bundle them.


### Supplementary asset, topology, and audio audit notes (2026-11-03)
- Actual Sylvi world dimensions are 256×128 with worldgen topology `continents`; this is the live starter fixture, not a hard-coded width in topology helpers. The requested regression phrase “ordinary bounded maps” is broader than the existing test set: tests explicitly check no worldgen and empty worldgen metadata, but a fully formed ordinary map fixture without `worldgen` is not named as such. Add that explicit fixture assertion if needed at the P3 implementation gate.
- Correction to the A0 summary above: Map `paintEvent()` enters the collision overlay only when `current_layer == 3` and both ground and objects layers exist. It draws ground/object tile flags; the overlay is present and must not be rebuilt. Whether requiring both layers is a defect needs a focused regression against valid map schemas before changing it.
- Event UI gap is confirmed: EventPanel currently displays IDs from `project->events` alongside mock command rows and a hard-coded “valid” response. Map Editor has hard-coded `Facing Dir` and `Event ID` line edits and never binds them to entities.
- Animation records preserve frame order, FPS and loop in the observed editor/generator paths; no per-frame delay field surfaced. The sprite base+flip optimization proposed by the owner is explicitly a mirror-paint editing aid while retaining eight stored direction frames; do not migrate schema.
- Music import uses header division in tick-to-step conversion. It reads initial tempo and time signature, but converts note events to 16th steps; a fixed 120 ticks/step constant exists in the exporter. The exact import division bug described by the user is not confirmed at this parse site. `playPreview()` definitely bakes PCM to a temporary WAV, with no QAudioSink. Further M behavior awaits its own approved phase.

- **A0 audit corrections:** Map’s collision overlay checks for both ground and objects; do not change that condition absent schema evidence. No A1 cost estimate or implementation recommendation is finalized.


**A0 evidence addendum (2026-11-03):** Pixel Editor’s generic animation preview iterates `anims[animation_id].frames` in JSON order (PixelEditorPanel.cpp around 455–469, 492–507); no `std::set` reordering occurs there. Sprite Editor builds its candidate frame list in clip order and uses a set only to suppress duplicates (around 785–805). Withdraw the prior note claiming generic animation clip order is reordered. Per-frame delays remain absent in the inspected playback paths.


## A1 Map editor implementation status (2026-11-03)

- Started only A1, per the owner’s ordered-track instruction. Added Map canvas zoom controls/wheel zoom, fit-to-view, middle-button pan, viewport culling, and actual-tileset-size scaling. Extended the paint palette to list the eight renderer entity types and wired selecting/painting entity kinds; selection centers on an existing matching type. The collision overlay code remains the existing Map canvas path, with defensive handling for absent ground/objects layer arrays.
- This track is incomplete and unverified: entity edit undo/history integration is currently unsafe (entity snapshots use the layer-history stack), entity selection UI/edit semantics need follow-up, and no successful build/tests/UI check has been run yet. Do not begin M until these are corrected, tested, visually checked, and committed.
- `build.bat` has reappeared. It initializes `vcvarsall.bat` and performs CMake configure/build, but its deployment line uses `%QT_DIR%` without setting/defaulting it; test/run actions exist in `AIO.bat`. Use the repository entry point after fixing/confirming its, rather than assuming direct CMake uses a prepared MSVC environment.


## Estate update check (2026-11-03)

- Reviewed the other active repository, `C:\Projects\Aur`, before touching implementation. Its latest commits add MIDI PPQN/piano-roll range work and P3.0 cylindrical-topology groundwork. The current Aur handover explicitly gates P3.1 on the P3.0 acceptance evidence and user approval; do not import Aur files into Prometheus or bundle its work into this project.
- Aur's tracked edits and numerous untracked files are pre-existing working-tree state. No staging, cleanup, reset, build, or edits were performed there.
- Prometheus `_handoff/` is a cross-project safety bundle, not an instruction to merge every file: current `PrometheusImageGenBridge.cpp`, `ImageGenSurface.qml`, `MediaViewport.qml`, and bridge header compare byte-identical with active source; `PrometheusCMakeLists.txt` and `stage_media_aio-wip.py` do not match their active counterparts. The `*-wip` files are historical alternatives, not safe replacements. Preserve this bundle pending owner clarification; no source changes were made.
- The separate `_handoff/WANTED.md` still has open rows 27 (investigate bundle provenance/merge) and 21–37, including row 26's native syntax engine request. Do not mark the provenance ask closed based on identical current copies alone: the remaining differing artifacts need provenance and relevance established before any merge or deletion.
- The update includes broad unstaged migrations/deletions and staged syntax/grammar plus build-script changes. In particular the current `build.bat` is an 18-line wrapper that calls a Visual Studio 18 installation path; its fallback CMake path expects the retired `desktop/CMakePresets.json` and makes no tree-sitter, cache-clean, or deploy steps. `desktop/CMakeLists.txt` also drops explicit render-test timeout assignments. These are material build regressions/risk signals; they were not changed or tested during this read-only triage.
- `PrometheusSyntaxEngine` still has only its existing optional tree-sitter dynamic loading and regex fallback in the inspected source; the native tokenizer requested in the staged `WANTED.md` plan has not been implemented here. No build, test, launch, or image-generation run was performed.
