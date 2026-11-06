# Aur MVP Roadmap

> Outstanding roadmap items and implementation tasks for agent execution. See the [Bible index](../README.md) and [handover](Handover.md).

> Planned game codenames: **Dawn** (turn-based), **Day** (tactical/grid), **Dusk** (real-time action). **Night** is unplanned and not part of this roadmap.

## Engine (`engine/aur/`)

- **`field.hpp`** — normalized eight-way hero movement + camera + directional animation-frame selection implemented (MVP); combat, NPC schedules, encounters, and interaction still stubbed.
- **`tactics.hpp`** — grid + unit placement + selection skeleton implemented; turn AI, attack, and win/loss logic not implemented.
- **`music.hpp`** — playback state + queue implemented; audio output not implemented.
- **`forge.cpp`** — seeded indexed generators now create eight-direction NPC sprite sheets, cat/dog and fantasy-creature sprites, animated effects, and terrain tiles. Expand the procedural art library and add editing/polish workflows.
- First-class project biome presets and painting are implemented; vegetation, decay, and water-level controls are recorded for biomes and used by seeded world generation.
- **2D-HD diorama direction (owner clarification 2026-11-05):** Keep the supplied reference's crisp, high-detail pixel-art density. Chrono Trigger/SNES Final Fantasy inform the overworld feel; tactical combat should feel like XCOM, Wasteland 3, Jagged Alliance 3, Desperados 3, or Baldur's Gate 3: the Chrono-Trigger-like shared overview world expressed as a readable tactical grid, not Fire Emblem's visual treatment or a low-resolution target. The in-game world map is a scaled diorama with a smaller hero; entering a zone transitions to full-scale 2D-HD field presentation. Both views share world-cell coordinates; one world cell maps to one field tile/zone anchor. Current 32×32 logical field cells at 2× output are a runtime baseline, not a ceiling on authored art density. Map Editor edits one cell at a time now; future 2×2/chunked view preserves the grid. Diorama-to-zone presentation is confirmed but unimplemented.
- The World editor can create seeded spherical worlds up to 2560×1280 tiles and 3,276,800 total cells in four topologies (continents, archipelago, supercontinent, fractured), defaulting to the maximum size for new maps. Generated maps persist elevation, moisture, temperature, biomes, drainage-fed river networks, seeded inland lakes, and editable terrain overrides without legacy tile grids; the editor renders distance-aware beaches and surf, shallow shelves, smooth deep-ocean shading, polar ice, connected rivers, lakes, forests, and clustered directional mountain ridges. Sylvi's starter Earth-like World ships with these HD layers while retaining its map identity and markers; the project start is unchanged. World and terra tiles use 32×32 indexed source art for authored terrain overrides. Larger worlds still need compact layer storage and viewport/chunk rendering beyond this cap; the JSON layer model is memory-intensive at maximum resolution.
- The former Python implementation was a scratchpad/testbed, not a feature-parity contract. Continue the agreed MVP directly in C++/Qt, guided by the current roadmap and design bibles.
- World & Time Settings now edits calendar, phase boundaries, era definitions, and climate-weighted weather profiles; the in-game F2 cheat menu can live-override the current era, time, climate, weather, and world switches in Release builds. Still needed: era-specific map-layer variants, timegate transitions, and consequences that propagate across eras.

## Studio (`studio/`)

- Map Editor has a working **Game Mode** selector (Real-Time Action / Turn-Based / Tactical → map `mode`) and tactical canvas overlay.
- WorldPanel displays the project's actual world-map tile art, terrain palette, and location/exit markers. Add Location/Exit tools place project-backed markers by clicking the map; markers can be selected from the canvas or list, edited, deleted, and undone/redone.
- WorldPanel can paint river, lake, beach, mountain-range, hill, and cave landforms with adjustable brushes, filter the tile palette by terrain category or biome starters, and add an original 21-tile starter set: nine general terrain/elevation materials plus distinct terrain tiles for all 12 project biomes. Selected tiles can be brushed over any elevation or depth as terrain overrides. It can also preview temperature/equator/poles plus wind/current/weather overlays and scrub local time-of-day lighting. Wind/current/weather are visual previews, not yet one persisted simulation shared with the runtime; runtime day/night lighting and biome-based climate selection are wired.
- Pixel Editor supports frame selection, animation preview, 8×8 tile, 16×16 stamp, pixel, picker, and eraser brushes against project palettes. Its auto-fit honors a 720p minimum work area and scales with available space; manual zoom reaches 64×. The dedicated Sprite Editor selects direction, pose, and frame for project-backed sprites.
- Music Studio now has a project-backed Session Grid, Piano Roll, Quick Compose, Standard MIDI Type 0/1 import/export, a searchable named 128-patch GM program selector, channel-10 drum tracks, and Windows WinMM hardware MIDI input/output with note recording and external GM/GS-device playback. Track bank/channel/program/controllers and SysEx are retained as MIDI metadata. Built-in GM-family, six-operator FM-style, and retro PSG previews are original oscillator synthesis—not sampled SoundFont playback or a GS soundbank player. SoundFont/sample playback, VST hosting, and BRR/SPC playback remain outstanding.
- The runtime uses a project-backed eight-direction Sylvi hero graphic rendered at 48×64 logical pixels and bottom-center anchored. Sylvi is the first explicit elf design: a female copper-haired sword traveler with pointed ears, a circlet, and green-and-gold travel clothes. Child, teen, and adult sheets share the same design for side-by-side age comparison. This is a more intentional generator baseline, not a substitute for authored production-quality art.
- Asset Forge's native indexed-pixel clothing/prop drawing is separate from character generation; generated characters do not include weapons.
- **Character art direction:** combine the compact, immediately readable silhouettes and costume cues of classic MMO sprites with denser, carefully clustered pixel shading and strong environmental lighting. Sea of Stars and Ragnarok Online are high-level references only; create original characters, poses, palettes, and designs rather than reusing their sprites.
- Map and world-map paint strokes plus sprite-frame pixel edits now support bounded undo/redo; Pixel and Sprite Editors expose buttons plus Ctrl/Cmd+Z/Y and same-size frame copy/cut/paste. File > Open Project (Ctrl/Cmd+O), Save (Ctrl/Cmd+S), dirty-project prompts, and reload-from-disk are available.
- The sprite canvas and Forge preview use a light transparency checkerboard so dark indexed pixels remain visible. Generated NPCs now store gender identity separately from a selectable feminine/masculine/androgynous visual presentation, and include more distinct hair, outfit, face, shading, and movement details.
- Visual reference: *Chrono Trigger* / *Sea of Stars* overworld feel with crisp 2D-HD art; tactical combat uses XCOM / Wasteland 3 / Jagged Alliance 3 / Desperados 3-style grid tactics and Baldur’s Gate 3-style shared-world staging, on the same physical grid. Fire Emblem is not the active tactical presentation reference.
- Studio starts maximized to the screen work area (16:10 on the target display) without the OS title bar; its Qt chrome provides window controls and the application menu. Minimum size remains 1280×720. Game output uses a 640×360 logical camera scaled 2× to 1280×720; 32×32 field tiles frame about 20×11 tiles.
- A shared SNES-inspired dark/gold/teal Studio theme now applies across tabs. Character generation and the Sylvi hero use 48×64 source sprites, with elf ears, hair, outfit layers, and accessories as the first race-specific design pass. Current tiles/sprites are procedural pixel art; the remaining tilesets and production-authored HD-2D assets still need a full art pass. The retired Python game's tiles are reference-only and are not reused.
- Music Studio still needs sampled instruments (including a licensed SoundFont path), VST hosting, BRR/SPC playback, and production audio controls.
- Still missing for editor-grade parity: project-wide undo/redo for database, events, biome/worldgen and other non-canvas edits; map/world rectangular selection and region copy/cut/paste; full field-map entity placement/inspection; consistent dirty-state reporting and autosave/recovery; usable item, creature, and animation editing flows. Current history and clipboard remain canvas/frame scoped.
- Keep the world-select map separate from traversable maps; build authored 80×80–128×128-tile regions with chunking as needed rather than making one huge playable map.
- Reference baseline: GB Studio documents tool modes, selection/multi-select, world view controls and contextual shortcuts ([project editor](https://www.gbstudio.dev/docs/project-editor/), [shortcuts](https://www.gbstudio.dev/docs/getting-started/keyboard-shortcuts/)). The SNES Studio reference separates editor state/history and sprite drawing sessions ([source](https://github.com/dmassiani/snes-studio)). Use these as workflow references, not as art or code to copy.
- `main.cpp` accepts `argv[1]` project path; defaults to `projects/sylvi`.

## Agent Integration

- `aur_mcp` is a local stdio MCP server. Tools cover project/maps/biomes, biome painting, seeded Earth-like map generation, tile generation, character generation by age group, pet/creature generation, animated effects, and tile/palette editing.
- Character generation accepts `gender` (`female`, `male`, `non-binary`, `unspecified`) and `presentation` (`feminine`, `masculine`, `androgynous`) as distinct fields.
- Remaining: add automatic external-change notifications, vegetation/water/decay controls for hand-authored regions, and richer generator templates. Studio already provides a guarded reload action and project-backed world-map location/exit authoring.
- The current asset data format uses 16-entry indexed palettes. This is a serialization constraint, not a final art-direction restriction.
- Studio provides a guarded **Reload Agent Changes** action to load MCP edits without restarting; automatic external-change notifications are still wanted.

## Gameplay Playtest

- Both `aur_studio` and `aur_game` stay running with the Sylvi project; the named-frame startup crash is fixed. Full gameplay interactions remain unverified.

## Golden Image & Regression Tests

- No QTest headless screen-rendering tests or maintained golden-image baseline yet.

## Tooling

- `build.bat` uses CMake presets (`build/Release`, `build/Debug`), `--no-translations`, `--parallel 8`, and deploys Qt runtime files including `Qt6Multimedia.dll`.
- Tests run with `ctest --test-dir build/Release --output-on-failure`.
- Git initialized (`git init`); `.gitignore` present.
