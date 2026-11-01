# Aur MVP Roadmap

> Outstanding roadmap items and implementation tasks for agent execution. See the [Bible index](../README.md) and [handover](Handover.md).

> Planned game codenames: **Dawn** (turn-based), **Day** (tactical/grid), **Dusk** (real-time action). **Night** is unplanned and not part of this roadmap.

## Engine (`engine/mana/`)

- **`field.hpp`** — normalized eight-way hero movement + camera + directional animation-frame selection implemented (MVP); combat, NPC schedules, encounters, and interaction still stubbed.
- **`tactics.hpp`** — grid + unit placement + selection skeleton implemented; turn AI, attack, and win/loss logic not implemented.
- **`music.hpp`** — playback state + queue implemented; audio output not implemented.
- **`forge.cpp`** — seeded indexed generators now create eight-direction NPC sprite sheets, cat/dog and fantasy-creature sprites, animated effects, and terrain tiles. Expand the procedural art library and add editing/polish workflows.
- First-class project biome presets and painting are implemented; vegetation, decay, and water-level controls are recorded for biomes and used by seeded world generation.
- The World editor can create a separate Earth-like 2:1 world map with seed, water, vegetation, decay, and axial/equator-angle controls; generated maps store temperature per tile and generated map IDs/names derive from their seed, with numbered duplicates. Sylvi includes a 256×128 starter at `maps/earthlike_world.json` (seed `20250308`), without replacing its existing Aurelia map or project start. World and terra tiles now use 32×32 indexed source art; canvas zoom tracks actual source-tile size, displays terrain thumbnails, culls off-screen tiles, and provides Fit.
- The former Python implementation was a scratchpad/testbed, not a feature-parity contract. Continue the agreed MVP directly in C++/Qt, guided by the current roadmap and design bibles.
- World & Time Settings now edits calendar, phase boundaries, era definitions, and climate-weighted weather profiles; the in-game F2 cheat menu can live-override the current era, time, climate, weather, and world switches in Release builds. Still needed: era-specific map-layer variants, timegate transitions, and consequences that propagate across eras.

## Studio (`studio/`)

- Map Editor has a working **Game Mode** selector (Real-Time Action / Turn-Based / Tactical → map `mode`) and tactical canvas overlay.
- WorldPanel displays the project's actual world-map tile art, terrain palette, and location/exit markers. Add Location/Exit tools place project-backed markers by clicking the map; markers can be selected from the canvas or list, edited, deleted, and undone/redone.
- WorldPanel can paint river, lake, beach, mountain-range, hill, and cave landforms with adjustable brushes, preview temperature/equator/poles plus wind/current/weather overlays, and scrub local time-of-day lighting. Wind/current/weather are visual previews, not yet one persisted simulation shared with the runtime; runtime day/night lighting and biome-based climate selection are wired.
- Pixel Editor supports frame selection, animation preview, 8×8 tile, 16×16 stamp, pixel, picker, and eraser brushes against project palettes. Its auto-fit honors a 720p minimum work area and scales with available space; manual zoom reaches 64×. The dedicated Sprite Editor selects direction, pose, and frame for project-backed sprites.
- Music Studio now has a project-backed Ableton-style Session Grid with track rows, scene columns, clips derived from piano-roll bars, clip removal, scene add/remove, and Play Scene preview. The Piano Roll is a separate tab and remains the detailed note editor. Quick Compose creates editable arrangements; sampled instruments and MIDI import/export remain outstanding.
- The runtime uses a project-backed eight-direction Sylvi hero graphic rendered at 48×64 logical pixels and bottom-center anchored. Sylvi is the first explicit elf design: a female copper-haired sword traveler with pointed ears, a circlet, and green-and-gold travel clothes. Child, teen, and adult sheets share the same design for side-by-side age comparison. This is a more intentional generator baseline, not a substitute for authored production-quality art.
- Asset Forge's native indexed-pixel clothing/prop drawing is separate from character generation; generated characters do not include weapons.
- **Character art direction:** combine the compact, immediately readable silhouettes and costume cues of classic MMO sprites with denser, carefully clustered pixel shading and strong environmental lighting. Sea of Stars and Ragnarok Online are high-level references only; create original characters, poses, palettes, and designs rather than reusing their sprites.
- Map and world-map paint strokes plus sprite-frame pixel edits now support bounded undo/redo; Pixel and Sprite Editors expose buttons plus Ctrl/Cmd+Z/Y and same-size frame copy/cut/paste. File > Open Project (Ctrl/Cmd+O), Save (Ctrl/Cmd+S), dirty-project prompts, and reload-from-disk are available.
- The sprite canvas and Forge preview use a light transparency checkerboard so dark indexed pixels remain visible. Generated NPCs now store gender identity separately from a selectable feminine/masculine/androgynous visual presentation, and include more distinct hair, outfit, face, shading, and movement details.
- Visual reference: *Chrono Trigger* / *Sea of Stars* overworld; *Fire Emblem* 3DS / *Advance Wars* tactical-map readability.
- Studio starts maximized to the screen work area (16:10 on the target display) without the OS title bar; its Qt chrome provides window controls and the application menu. Minimum size remains 1280×720. Game output uses a 640×360 logical camera scaled 2× to 1280×720; 32×32 field tiles frame about 20×11 tiles.
- A shared SNES-inspired dark/gold/teal Studio theme now applies across tabs. Character generation and the Sylvi hero use 48×64 source sprites, with elf ears, hair, outfit layers, and accessories as the first race-specific design pass. Current tiles/sprites are procedural pixel art; the remaining tilesets and production-authored HD-2D assets still need a full art pass. The retired Python game's tiles are reference-only and are not reused.
- Music Studio still needs sampled instruments, MIDI import/export, and production audio controls.
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

- Both `aur_studio` and `mana_game` stay running with the Sylvi project; the named-frame startup crash is fixed. Full gameplay interactions remain unverified.

## Golden Image & Regression Tests

- No QTest headless screen-rendering tests or comparison against `reference/visual-baseline/python-scratchpad/` reference screenshots yet.

## Tooling

- `build.bat` uses CMake presets (`build/Release`, `build/Debug`), `--no-translations`, `--parallel 8`, and deploys Qt runtime files including `Qt6Multimedia.dll`.
- Tests run with `ctest --test-dir build/Release --output-on-failure`.
- Git initialized (`git init`); `.gitignore` present.
