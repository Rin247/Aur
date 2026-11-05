# PROJECT.md — Aur

> **Aur Studio & Engine** (formerly Mana Studio)
> A GB-Studio-style toolset and runtime for making 16-bit RPGs (real-time action, tactical grid, and 2.5D HD-2D) in an explorable world with time travel, day/night cycles, social sim, and adaptive audio.

> See the [Bible index](../README.md) for organized design documents and source-of-truth precedence.

## Visual Direction & Display Targets

- **Visual direction:** A cohesive, high-quality pixel-art RPG in the spirit of *Sea of Stars*. The overworld should feel like *Chrono Trigger* / *Sea of Stars*; tactical maps should read like *Fire Emblem* on 3DS or *Advance Wars*. Distinct combat systems can draw from different inspirations while sharing one visual identity.
- **Game codenames:** The three planned games are **Dawn** (turn-based), **Day** (tactical/grid), and **Dusk** (real-time action). **Night** is a possible fourth game, currently unplanned and outside the active scope. These are working codenames, not final titles.
- **Franchise naming:** Aur is the setting/planet name. Game titles may use names such as *Tales of Aur*, *Legends of Aur*, and *Aur Online*; the editor product is branded **Aur Studio**.
- **Studio workspace:** Starts maximized to the available display area (16:10 target) with a custom Qt title/menu bar instead of OS window chrome; remains usable down to 1280×720.
- **Game output:** Defaults to a 1280×720 (16:9) render target. This is a presentation target, not a limit on map size or authored asset dimensions.
- **Art-density target:** The playable view uses a 640×360 logical camera presented at 1280×720 (2×), with 32×32 logical field tiles and roughly 20×11 tiles visible. Sylvi's world and terra tilesets use 32×32 indexed source tiles, and the hero generator produces 48×64 indexed source sprites with a bottom-center world anchor. Sylvi is the first explicit elf character pass, with child, teen, and adult sheets sharing a palette and outfit for proportion comparison; cave/legacy tiles and authored production-quality assets remain outstanding. Sylvi's separate world-select map is 256×128 tiles, not a single traversable region.
- **Character art direction:** use compact, costume-readable silhouettes with richer pixel clusters, material shading, and atmospheric lighting. Sea of Stars and Ragnarok Online are style references, not source artwork; all production sprites must be original.
- **Current art format:** Graphics and tiles use indexed pixels with 16-color palettes. This reflects the current C++ project format, not a final ceiling for the art direction; preserve a path to richer palettes as the asset pipeline evolves.

## Architecture

- **Active product:** Native C++20 engine (`aur_engine` static library, zero Qt dependencies) + Qt 6.12 Widgets studio (`aur_studio`) and game runtime (`aur_game`).
- Godot references in older design documents are legacy context only; Godot is not part of the current implementation plan. Continue the existing C++/Qt product.

## Key Components

- **Engine (`engine/aur/`, `aur` namespace)**: JSON parser, indexed graphics (BGR555, palettes, translucent shadows), project IO, seeded character/creature/effect/tile generators, spherical procedural world generation, biome data, autotiling, game clock, weather system, scripting interpreter, field exploration, and stdio MCP tools.
- **Studio (`studio/`)**: Qt 6.12 Widgets UI (`MainWindow`) with a shared SNES-inspired theme, Map Editor, Pixel Editor with frame/animation preview and zoom, directional Sprite Editor, Tileset Editor, Seeded Asset Forge, Database Panel with editable race records, Event Panel, project-backed Music Studio, zoomable/pannable World Map editor with location/exit authoring, and JSON-backed World & Time Settings.
- **MCP (`aur_mcp`)**: Local stdio server for agent-driven map/biome editing, seeded asset generation, tile/palette editing, and indexed graphics/character/creature edits. See the [MCP server guide](MCP%20Server.md).
- **Former Python scratchpad**: Its experimental implementation has been retired; it was a testbed, not a required feature-parity target. The active Studio and engine are the C++/Qt rewrite. Historical notes and screenshots remain under `reference/`.
- **Tests (`tests/`)**: Headless CTest suite (`aur_tests`) covering JSON, autotiling, seeded generators, world-generation controls, biomes, and MCP protocol/project edits.
- **Design Bibles**: Focused documents are organized under `Bible/World/`, `Bible/Game Design/`, `Bible/Systems/`, `Bible/Characters/`, `Bible/Time and Puzzles/`, and `Bible/Production/`. The preserved conversation archive is the user's designated source of truth; historical documents are in `Bible/Archive/`.

## Verified

- `.\build.bat` builds cleanly; `Qt6Multimedia.dll` deployed; executables in `build/Release`.
- CTest `EngineTests` passes in Release.
- `main.cpp` accepts an optional project path; the AIO `build.bat` script can build, deploy, and launch Studio or the game.
- Studio’s Pixel Editor auto-fits against a 720p minimum and uses larger available work areas; manual zoom reaches 64×.
- World & Time Settings edits calendar/phase boundaries, the five era definitions, and climate-weighted weather profiles; the in-game F2 debug menu can override era, date/time, clock rate, climate, weather, and world flags live, including in Release builds.
- Music Studio edits project-backed songs, tracks, and piano-roll notes, creates arrangements from Quick Compose presets, and plays looping stereo synthesized previews.
- Music Studio includes an Ableton-style Session Grid for track/scene clips and launch previews, with the Piano Roll retained as a distinct note-editing view.
- World Map location and exit markers can be placed by clicking the map, selected from the canvas or list, edited, deleted, and restored through canvas undo/redo.
- World generation offers seeded continent, archipelago, supercontinent, and fractured topologies; the World Map editor renders generated elevation and biome data as a shaded HD surface.
