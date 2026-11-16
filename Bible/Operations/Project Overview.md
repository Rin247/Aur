# PROJECT.md — Aur

> **Aur** (`Aur.exe`, `~/.Aur`)
> A desktop RPG studio and runtime for making 16-bit / 2D-HD RPGs (real-time action, tactical grid, and platformer/field) in an explorable world with time travel, day/night cycles, social sim, and adaptive audio.

> See the [Bible index](../README.md) for organized design documents and source-of-truth precedence.

## Visual Direction & Display Targets

- **Visual direction:** Crisp, high-detail 2D-HD pixel art matching the supplied reference: richly clustered terrain, clear detailed silhouettes, layered environments, and atmospheric effects. Chrono Trigger and SNES-era Final Fantasy inform the overworld feel, not a low-resolution visual target. The world map is a scaled diorama with a proportionally smaller hero; entering a zone transitions to a larger, full-scale field presentation. Both views share world-cell coordinates and authored locations. Tactical play uses XCOM / Wasteland 3 / Jagged Alliance 3 / Desperados 3-style grid tactics and Baldur’s Gate 3-style shared-world staging, not Fire Emblem's visual treatment.
- **Game codenames:** The three planned games are **Dawn** (turn-based), **Day** (tactical/grid), and **Dusk** (real-time action). **Night** is a possible fourth game, currently unplanned and outside the active scope. These are working codenames, not final titles.
- **Franchise naming:** Aur is the setting/planet name and the studio product name (`Aur`, `Aur.exe`, data root `~/.Aur`). Game titles may use names such as *Tales of Aur*, *Legends of Aur*, and *Aur Online*.
- **Studio workspace:** Starts maximized to the available display area with a frameless custom title/command bar (`StudioChrome` + `FramelessWindow`) instead of OS window chrome; remains usable down to the estate floor (`1280×690` at 16:9 and `1280×770` at 16:10, `C:\Projects\DESIGN.md` §12).
- **Game output:** Defaults to a 1280×720 (16:9) render target. This is a presentation target, not a limit on map size or authored asset dimensions.
- **Shared world coordinates and 2D-HD diorama presentation (owner clarification 2026-11-05):** World Map and Map Editor share integer world-cell coordinates: one world cell resolves to one playable field-map tile/zone anchor. In-game world selection is a crisp, detailed scaled-down diorama with a proportionally smaller hero; entering a zone transitions to the full-scale, high-detail 2D-HD field view of that same coordinate space. Current runtime baseline is a 32×32 logical field cell presented 2×, while authored source art may be more detailed than the logical cell (Sylvi world/terra tiles are 32×32; legacy 16×16 art is upscaled). This numeric baseline must not cap final art density or imply SNES-resolution visuals. World overview/fit zoom changes screen scale only, never cell identity. Map Editor edits one cell at a time today; future 2×2/chunked viewport editing preserves the same grid. The diorama-to-zone presentation is confirmed direction, not implemented.
- **Art-density target:**  The playable view uses a 640×360 logical camera presented at 1280×720 (2×), with 32×32 logical field tiles and roughly 20×11 tiles visible. Sylvi's world and terra tilesets use 32×32 indexed source tiles, and the hero generator produces 48×64 indexed source sprites with a bottom-center world anchor. Sylvi is the first explicit elf character pass, with child, teen, and adult sheets sharing a palette and outfit for proportion comparison; cave/legacy tiles and authored production-quality assets remain outstanding. Sylvi's separate world-select map is 256×128 tiles, not a single traversable region.
- **Character art direction:** use compact, costume-readable silhouettes with richer pixel clusters, material shading, and atmospheric lighting. Sea of Stars and Ragnarok Online are style references, not source artwork; all production sprites must be original.
- **Current art format:** Graphics and tiles use indexed pixels with 16-color palettes. This reflects the current C++ project format, not a final ceiling for the art direction; preserve a path to richer palettes as the asset pipeline evolves.

## Architecture

- **Active product:** Native C++20 engine (`aur_engine` static library, zero Qt dependencies) + Qt 6.12 Widgets studio (`aur_studio`) and game runtime (`aur_game`).
- Godot references in older design documents are legacy context only; Godot is not part of the current implementation plan. Continue the existing C++/Qt product.

## Key Components

- **Engine (`engine/aur/`, `aur` namespace)**: JSON parser, indexed graphics (BGR555, palettes, translucent shadows), project IO, seeded character/creature/effect/tile generators, spherical procedural world generation, biome data, autotiling, game clock, weather system, scripting interpreter, field exploration, and stdio MCP tools.
- **Studio (`studio/`)**: Qt 6.12 Widgets UI (`MainWindow`) themed via the estate design system (`studio/theme/`, `C:\Projects\DESIGN.md` §1, §2, §6.3, §13: `ThemeManager` + `AurStyle` `QProxyStyle` + `ThemeIcons` + `AppearanceDialog`, 11 modes + `system`, `#3b82f6` default accent, zero stylesheets), with Map Editor, Sprite Editor, Tileset Editor, Creature Forge, Database (generic `RecordEditor` across Actors, Classes, Races, Items, Weapons, Skills plus Bestiary), Events, DAW (`MusicPanel`, `MusicPanelSession`, `MidiStrip`, `FxChainStrip`, `GrooveBoxPanel`), and World (`WorldPanel`, `WorldPanelActions`, `WorldMapCanvas`, `WorldSettingsPanel`).
- **MCP (`aur_mcp`)**: Local stdio server for agent-driven map/biome editing, seeded asset generation, tile/palette editing, and indexed graphics/character/creature edits. See the [MCP server guide](MCP%20Server.md).
- **Former Python scratchpad**: Its experimental implementation has been retired; it was a testbed, not a required feature-parity target. The active Studio and engine are the C++/Qt rewrite. Historical notes and screenshots remain under `reference/`.
- **Tests (`tests/`)**: Headless CTest suite (`aur_tests`) covering JSON, autotiling, seeded generators, world-generation controls, biomes, audio/DAW, theme math/palettes/QPalette, and MCP protocol/project edits.
- **Design Bibles**: Focused documents are organized under `Bible/World/`, `Bible/Game Design/`, `Bible/Systems/`, `Bible/Characters/`, `Bible/Time and Puzzles/`, and `Bible/Production/`. The preserved conversation archive is the user's designated source of truth; historical documents are in `Bible/Archive/`.

## Verified

- `cmd /c "AIO.bat build windows Release"` builds cleanly; Qt runtime deployed; executables in `build/Release`.
- CTest passes 7 of 7 suites in Release.
- `main.cpp` accepts an optional project path and CLI flags (`--tab`, `--sub-tab`, `--size`, `--zoom`, `--theme`, `--sprite`, `--preview`, `--no-state`); `AIO.bat` builds and deploys `build/Release/Aur.exe`.
- Studio’s Pixel Editor auto-fits against a 720p minimum and uses larger available work areas; manual zoom reaches 64×.
- World & Time Settings edits calendar/phase boundaries, the five era definitions, and climate-weighted weather profiles; the in-game F2 debug menu can override era, date/time, clock rate, climate, weather, and world flags live, including in Release builds.
- Music Studio edits project-backed songs, tracks, and piano-roll notes, creates arrangements from Quick Compose presets, and plays looping stereo synthesized previews.
- Music Studio includes an Ableton-style Session Grid for track/scene clips and launch previews, with the Piano Roll retained as a distinct note-editing view.
- World Map location and exit markers can be placed by clicking the map, selected from the canvas or list, edited, deleted, and restored through canvas undo/redo.
- World generation offers seeded continent, archipelago, supercontinent, and fractured topologies; the World Map editor renders generated elevation and biome data as a shaded HD surface.
