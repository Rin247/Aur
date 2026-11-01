# HANDOVER.md — Aur

> Current session status and implementation state for agent handover. See the [Bible index](../README.md) for document navigation and source precedence.

> Active direction: continue the existing C++20 + Qt 6.12 Studio/engine. Godot mentions in older design bibles are legacy context, not a planned port or current implementation target.
> Game codenames: **Dawn** (turn-based), **Day** (tactical/grid), and **Dusk** (real-time action) are the three planned games. **Night** is an unplanned possible fourth game; do not treat it as current scope. Codenames are provisional.

## Current State (Part 1 PoC)

- **Build System**: `build.bat [release|debug] [build|run|game]` configures/builds through CMake presets with Ninja, `--parallel 8`, then deploys Qt runtime dependencies and `Qt6Multimedia.dll`. It can optionally launch Studio or the game from the selected output directory; no separate run script is needed.
- **Targets**: `mana_engine` (static lib, std-only), `aur_studio` (Qt 6.12 Widgets IDE), `mana_game` (runtime), `aur_mcp` (stdio MCP server), `mana_tests` (CTest).
- **Tests**: Release build and CTest pass, including MCP protocol and project-edit coverage.
- **Engine modules**: `field.hpp` includes normalized eight-way movement and directional-frame selection; `tactics.hpp` remains a grid/unit/selection skeleton; `music.hpp` is playback-state/queue. `forge.cpp` now provides deterministic seeded NPC, creature/pet, effect, and tile generation plus native indexed-pixel clothing and prop drawing. `worldgen.cpp` creates adjustable Earth-like maps with an axial/equator angle and per-tile temperature data.
- **Game launch**: `Game::update` loads the configured start map into a `FieldScene` on first tick. Named map spawn records now set the player's position and facing; explicit travel coordinates retain pixel positioning. `Game::travel()` switches maps.
- **Playtest rendering**: Runtime scenes receive update ticks; the Qt renderer draws actual project tileset pixels in a 640×360 logical view, 2× presented at 1280×720. Field tiles are 32×32 logical pixels, framing about 20×11 tiles. The Sylvi world and terra source tilesets now contain 32×32 indexed art; cave and any other legacy tilesets remain 16×16 and are upscaled at runtime.
- **Studio UI**: Starts maximized to the screen work area (16:10 on the target display) with a frameless Qt title/menu bar, Qt minimize/maximize/close controls, and a 1280×720 minimum. The World editor includes 12 editable JRPG biome presets, biome painting, and seeded Earth-like map creation with water/vegetation/decay and axial/equator-angle controls. The map canvas previews temperature/equator/poles, illustrative wind/current/weather overlays, and local time-of-day lighting; river, lake, beach, mountain-range, hill, and cave brushes author terrain directly. Ocean autotile resolution is shared with runtime rendering. New generated maps use automatic seed-based IDs/names (numbered for duplicates), with a random seed on dialog open and a Randomize control, so seed experiments do not require manual renaming. Tactical maps show terrain, units, and collision like Fire Emblem / Advance Wars. Pixel Editor includes frame selection, animation preview, 720p-minimum adaptive zoom, and up to 64× manual zoom; Sprite Editor focuses on direction/pose/frame editing.
- **Studio visual system**: A shared dark, warm-metal, teal-accent theme now applies consistently across tabs. World Map uses source-tile-aware zoom (100% default), terrain thumbnail previews, cursor-centered wheel zoom, middle-drag pan, off-screen tile culling, Fit, and project-backed location/exit placement, editing, deletion, and undo/redo.
- **Music Studio**: Songs, tracks, notes, scenes, and clips are project-backed. Session Grid provides track rows, scene columns, clip creation/removal, scene launch preview, and scene management; the Piano Roll remains a separate tab for note editing. Create/delete songs and tracks, rename tracks, edit tempo/instrument/volume/pan, and add/move/resize/delete notes. Preview synthesizes looping stereo audio; Save Project persists edits. This is a functional foundation, not a production sampler or MIDI workstation.
- **Editor basics**: File menu supports Open Project (`Ctrl+O`), Save (`Ctrl+S`), and Reload from Disk. Opening another project prompts to save only when the project data differs from its last saved/opened snapshot. Map paint strokes and Sprite/Pixel frame edits have bounded local undo/redo; Sprite/Pixel support same-dimension full-frame copy/cut/paste. Transparent sprite previews now use a light checkerboard. This is a first pass, not project-wide history yet.
- **Character generation**: The Character Forge supports the complete race and supernatural category roster, seeded race-aware appearances, gender identity (`female`, `male`, `non-binary`, `unspecified`), visual presentation (`feminine`, `masculine`, `androgynous`), and main-character/NPC/follower roles. Creature and pet generation is in the same workflow. Alignment is authored separately from creed/morality and corruption. See [Character Creator and Species Visuals](../Production/Character%20Creator%20and%20Species%20Visuals.md).
- **Asset Forge**: Studio draws deterministic editable 32×32 indexed-pixel clothing (pants, shorts, skirts, dresses, shirts, tunics) and props natively; this is procedural pixel drawing, not image generation. Character generation produces clothing/accessories only—weapon art is a separate prop workflow.
- **Race authoring**: Database > Races now edits project-backed race records (create, rename/ID change, update, delete) with editable lore and gameplay fields. Sylvi's sample includes the six mortal peoples and supernatural groups from the legacy World Bible taxonomy; Sylvi's actor record links to `sylvani`. Edits apply in memory and persist through the normal Save Project action.
- **World/time authoring**: World & Time Settings edits calendar length, seasons, phase boundaries, the five era definitions, and weighted weather profiles by climate. The game clock reads those project settings; weather rolls use the active map's climate profile, and the renderer displays rain, snow, fog, and cloud cover.
- **In-game cheats**: Press **F2** during gameplay or Studio playtest to open the in-game debug menu (available in Release builds). It pauses movement/clock while open and can set era, year/season/day/time, clock rate and pause state, climate (or return to the map climate), weather lock, and arbitrary runtime story/world switches. An optional HUD shows the active values. These are runtime overrides, not saved game state; project settings persist through Save Project.
- **Era scope**: Era definitions and live era selection are editable, but era-specific map-layer variants, timegate transitions, and cross-era world consequences are not implemented yet.
- **Sylvi starter content**: `maps/earthlike_world.json` is a separate 256×128 Earth-like world (`seed=20250308`, water 50%, vegetation 65%, decay 15%) with biome assignments and four map markers. It does not replace Aurelia or change the project's start map.
- **Playtest protagonist**: Sylvi's project includes a project-backed eight-direction `hero` graphic with 48×64 source frames, rendered at native size and bottom-center anchored for the 32-pixel field grid. The first deliberate race pass is Sylvi as a female elf traveler, with pointed ears, long copper hair, circlet, layered green-and-gold clothing, and a sword. Separate child and teen graphics use the same palette and outfit as adult `hero` to compare proportions. Production-authored character art is still needed.
- **Project layout**: `projects/sylvi/` is the canonical sample used by C++ launchers. The Python implementation was only a scratchpad/testbed and has been retired rather than kept as a porting target; its historical README and visual screenshots remain under `reference/`.
- **Game output**: `mana_game` presents a 1280×720 (16:9) target; 32×32 logical field tiles render as 64×64 screen pixels at 2×. Runtime day/night ambient lighting and biome-based climate selection are wired; World Map wind/current/weather marks remain preview overlays rather than a unified persisted simulation. Both Studio and game open with the Sylvi project; the named-frame startup crash is fixed.
- **Default project launch**: Studio resolves its default Sylvi project relative to the working directory or executable; the AIO build script launches from the selected output directory. This prevents a blank project (and empty World tab) when launched from the build directory or another working directory.
- **MCP**: `aur_mcp <project-directory>` exposes project/map/biome inspection, biome painting, seeded world generation, seeded character/creature/pet/effect/tile generation, and tile/palette/graphic edits. Mutations validate input and save inside the supplied project directory. Use Studio's **Reload Agent Changes** button to refresh the open project without silently losing unsaved edits. See the [MCP server guide](MCP%20Server.md).
- **Git**: initialized (`git init`); `.gitignore` present.

## Build & Test Commands

```powershell
.\build.bat                      # Release build + deploy to build/Release
.\build.bat debug                # Debug build + deploy to build/Debug
.\build.bat release run          # Build, deploy, and launch Aur Studio
.\build.bat debug game           # Build, deploy, and launch mana_game
ctest --test-dir build\Release --output-on-failure
```

## Open Work (see [MVP roadmap](Roadmap.md))

- Add automatic external-change notifications; current MCP integration refreshes through the guarded **Reload Agent Changes** action.
- Add project-backed authoring workflows to the remaining Studio editors; world-map locations and exits now support placement, selection, editing, deletion, and undo/redo.
- Implement era-specific map layers and timegate transition behavior; the current era authoring/cheat controls prepare and test state data but do not yet alter map art or geometry.
- Grow generator templates and add region-level vegetation/water/decay editing for hand-authored maps.
- Extend the active C++ project's 32×32 world/terra art approach to remaining tilesets, then replace procedural foundations with authored higher-detail world, terrain, and character art. The retired Python game remains reference-only; its tiles are not reused.
- Keep playable overworlds as distinct 80×80–128×128-tile regions instead of treating the 256×128 world-select map as one traversable field; introduce chunking if region sizes grow substantially.
- Grow Music Studio with real instrument samples, soundfont/MIDI import/export, and production audio controls.
- Extend undo/redo beyond canvas edits; add map/world selection and region clipboard, full entity authoring/inspection, pan/zoom, and project dirty-state/autosave recovery.
- Improve the procedural asset pipeline further; current seeded pixel sprites are more legible but are not a substitute for production-quality authored HD-2D assets.
- Add golden-image/headless regression tests against the retained scratchpad screenshots in `reference/visual-baseline/python-scratchpad/`.
- Externalize UI strings into an en-US resource (no hardcoding).
- Extend the current 16-color indexed art format if the asset workflow needs richer palettes; 1280×720 is only the game presentation target.

## What to verify next

Latest verification: latest source compiled in an isolated Release build and CTest passed. The existing `build\Release\Aur Studio.exe` was open during the rebuild, preventing replacement of that binary; close Studio and run `.\build.bat release` to update it. Character generator tests cover weapon-free output and stripping legacy weapon fields from character graphics. `build.bat /?` documents configuration, build, deployment, and launch options.
