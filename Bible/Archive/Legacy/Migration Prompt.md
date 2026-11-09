# Task: port Mana Studio to native C++ with Qt 6.12

> Historical migration brief. The Python code described below was a scratchpad/testbed and has since been retired; it is not an active implementation or a feature-parity contract. Names and paths in this original brief reflect the migration's starting point. For current status, see [Project Overview](../Operations/Project%20Overview.md), [Handover](../Operations/Handover.md), and the [MVP roadmap](../Operations/Roadmap.md).

You are porting a working Python game studio ("Mana Studio") to **C++20 + Qt 6.12 (LTS)**. The Python version in
`reference/` is the executable specification: same behaviour, same data files, same test intent. Do not redesign it;
port it, then improve it only where this brief says so.

## 1. What the product is
A GB-Studio-style tool for making 16-bit RPGs: real-time action RPGs, Fire-Emblem-style tactics RPGs and 2D-HD (HD-2D)
RPGs, in one explorable world (overworld, towns, caves, interiors, Chrono Trigger / Golden Sun style transitions).
It has a Studio (editors) and an engine (the game runtime). The sample game is `projects/sylvi/` ("Sylvi and the Moon
Shard"). Features that must survive the port: SNES-style art pipeline (palette banks, translucent shadows, autotiles),
Character Forge, day/night + seasons + weather, NPC schedules, event scripting, music studio (GM/Roland GS MIDI),
playtest tools (hour/weather/teleport).

## 2. What already exists in this folder
```
reference/      the complete Python implementation (engine `mana/`, PySide6 Studio `studio/`, tools, 47 tests, docs images)
projects/sylvi/ the sample game as plain JSON. THE DATA FORMAT DOES NOT CHANGE. Both implementations must load it.
data/forge_parts.json   Character Forge part templates, exported from Python (kind -> name -> direction -> [[x0,y0,[rows]]])
engine/mana/json.hpp    finished, std-only JSON (ordered objects, compact numeric arrays); round-trips every sample file
tests/                  finished mini test framework (mini_test.hpp, main.cpp) + test_json.cpp (4 passing tests)
docs (reference/docs)   screenshots of the intended look: use them as visual targets
```
Toolchain notes from the sandbox that produced this: g++ 13 (C++20) works; no CMake/Qt were available there, so the
Qt layer has never been compiled by the author. Assume nothing in `reference/studio/` has ever run on real Qt, it was
smoke-tested only against a fake Qt layer (`reference/tests/fake_qt.py`). Treat it as a behavioural sketch, not a
proven design.

## 3. Target architecture
```
CMakeLists.txt                 qt_standard_project_setup(REQUIRES 6.12), C++20, -Wall -Wextra (-Werror in CI)
engine/  (static lib `mana_engine`, NO Qt dependency, std only)
  json.hpp gfx.* project.* clock.* autotile.* script.* forge.* render.hpp game.* field.* tactics.* music.*
studio/  (executable `mana_studio`, Qt 6.12 Widgets + Multimedia)
  QtRenderer, GameView, PlaytestWindow, AudioBank, MainWindow, MapEditor, PixelEditor, TilesetEditor, ForgePanel,
  DatabasePanel, EventPanel, MusicPanel (piano roll), WorldPanel
game/    (executable `mana_game`: runs a project without the editor)
tests/   (ctest) engine tests (no Qt), Qt tests (QTest, QT_QPA_PLATFORM=offscreen)
```
Rules:
* The engine never includes a Qt header. It draws only through the abstract `Renderer` (section 5). This is what keeps it
  testable headless (`NullRenderer`) and lets CI render screenshots without a GPU.
* Qt Widgets + QPainter for the Studio and the in-Studio game view. Do **not** introduce QML/Qt Quick in this port; keep
  the door open (the engine has no UI dependency).
* No third-party dependencies besides Qt. Keep `json.hpp` for the engine; the Studio may use it too.
* Use C++20 value semantics, `std::string`, `std::vector`, `std::map`; avoid raw owning pointers; scenes are
  `std::unique_ptr<Scene>`.

## 4. Port map (do in this order; each phase must build and pass tests before the next)
Line counts are the Python sizes, a C++ port is typically 1.5-2x.

| Phase | Python source | C++ target | Notes |
|---|---|---|---|
| 1 | `mana/gfx.py` (173) | `gfx.*` | 16-colour palettes, index 0 transparent, rows use `.` + hex digits; entries are `#rrggbb` **or `#rrggbbaa`** (alpha = translucent shadow). `snap555(v) = ((v>>3)<<3)\|(v>>5)`. `Graphic`, `Anim{frames,fps,flip}`, `animFrame()`, `frameRGBA()`, tileset v2 (`palettes[]`, per-tile `pal`, `autotiles`), v1 single-`palette` tilesets must auto-upgrade. |
| 1 | `mana/project.py` (183) | `project.*` | Load/save all JSON (project, tilesets, graphics, characters, database, events, parts, maps/*, music/*). Typed layers (`vector<int>` x3), entities stay `Json`. `collision(map)`: OR of ground+objects flags; BRIDGE cancels WATER and SOLID. `validate()`: dangling exits/spawns/graphics, tile `pal` bounds, autotile groups (16 valid indices), **entities placed inside solid/water tiles**. Output key order must match the input (round-trip test). |
| 1 | `mana/autotile.py` (77) | `autotile.*` | Masks N=1 E=2 S=4 W=8. `joins` is one-directional. `edge_joined` (default true) = off-map counts as joined. paint/erase/retile/retile_all. |
| 2 | `mana/clock.py` (188) | `clock.*` | 28-day seasons, light keyframes, `between(a,b)` wraps midnight, seeded 3-hour weather slots, climates (`temperate arid arctic none`), cross-fade ~6 s, darkness per weather. Do not try to reproduce Python's RNG stream; keep determinism *within* the C++ implementation. |
| 2 | `mana/forge.py` (323) | `forge.*` | Compile a character spec to a Graphic: 15 palette slots (outline, skin x3, hair x3, eye, top x2, trim, bottom, bottom-shade/boots, accent, metal), template letters -> slots (`SLOT` map), per-layer autoshade (shade south/east edges, highlight top edges + skin left edge + hair sheen/gloss), composite order per direction, **selective outline** (dark outline on shadow side, material's own dark tone on lit side), procedural 3-frame walk legs, hue-shifted palette (`build_palette`), left = flipped right, presets/variants/random NPC. Load parts from `data/forge_parts.json` (+ project `parts.json` overrides). Embed the JSON as a generated byte array, not a string literal (MSVC 16 KB literal limit). |
| 3 | `mana/script.py` (212) | `script.*` | Python uses generators; use an explicit frame stack `{const Json* cmds; size_t i;}` plus a `wait` predicate. Commands: say choice if set_switch set_var give_item take_item give_gold heal join wait teleport set_time advance_time skip_to set_weather face move shake flash music sfx battle call toast end. Nested blocks: `then/else`, `options[].do`, `on_win/on_lose`. Include `validate()` and a registration hook for custom commands. |
| 3 | `mana/game.py` (520) | `game.*` | State (switches, vars, inventory, gold, party, actors), scene stack, `InputState`, Dialog (typewriter, pages of 3 lines, choices, `{time} {season} {weather} {period} {name} {gold}` tokens), Menu, Transition (fade/white/none), world-position memory + `@return`, battles (`start_battle/end_battle/game_over`), conditions (`switch not_switch var item gold time night weather season party map`), save/load. |
| 4 | `mana/field.py` (653) | `field.*` | Exploration + real-time action: hero/followers trail, collision with 4-corner feet box, NPC schedules (time + weather cond; no matching entry = away), exits with `require`/`blocked_text`, triggers, chests, encounters on ENCOUNTER tiles, charge attack (3 levels), specials (`nova bolt heal`), enemy AIs (`hopper chaser flyer`), lights + shared `ambient_pass`, HUD, banner. |
| 4 | `mana/tactics.py` (484) | `tactics.*` | Grid battles: Dijkstra reach, weapon triangle, hit/dmg/doubling, terrain, phases, enemy AI, objectives (rout/seize/survive), step queue (pop-first, then run: see tests), forecast UI. |
| 5 | `mana/music.py` (394) | `music.*` | Mini-notation, chords, MIDI export (format 1, PPQ 96, GS reset SysEx `F0 41 10 42 12 40 00 7F 00 41 F7`, reverb macro, CC111 loop, `loopStart/loopEnd` markers), MIDI import, chip synth (square/pulse/saw/tri/noise by GM family, drums, SPC-style echo, seamless loop render), SFX. Output int16 stereo PCM + WAV writer. |
| 6 | `studio/qt_renderer.py` (295) | `studio/QtRenderer.*` | See section 5. |
| 6 | `studio/game_view.py`, `audio.py` | `GameView`, `PlaytestWindow`, `AudioBank` | Fixed-step-clamped timer loop, key map, integer scaling for SNES, smooth for HD-2D, debug toolbar (hour, weather, clock speed, teleport). `QSoundEffect` loops WAVs (`-2` / `QSoundEffect::Infinite`); no MIDI synth exists in Qt, keep the chip synth for preview and MIDI export for real playback. |
| 7 | `studio/*.py` editors (~3000) | `studio/*` | Map editor (3 layers, collision overlay, terrain brush, entities, JSON entity inspector), pixel + tileset editors (palette banks, flags, anim, texture generator), Forge panel, Database (schema-driven), Events (command list + validator), Music (piano roll + quick-compose), World graph. Reuse the Python files as UX reference only. |

## 5. The Renderer contract (engine -> backend)
```
begin(style) end() camera(x,y)
tile(tilesetId, index, x, y, layer)            // layer: ground | objects | above
sprite(graphic, frame, x, y, flip, scale, tint, alpha, shadow)   // x,y = feet position
ground_rect(x,y,w,h,color,alpha)  world_text  arc  ring
light(x,y,radius,color,intensity)  ambient(color,darkness)  weather(kind,intensity,t,flash)
ui_rect ui_text ui_graphic  fade(alpha,color)  flash(alpha)
```
World ops take world pixels (camera applied by the backend); `ui_*` take screen pixels of the logical size:
SNES 256x224, HD-2D 384x216 (rendered at 2x internally). Required look:
* **SNES**: 1:1 pixels, integer-scaled. Ambient = multiply overlay (white -> tint by `darkness*1.7`) with light pools added to the
  overlay, plus a warm additive glow; weather overlays (rain/snow/fog/cloudy/storm + lightning flash).
* **HD-2D**: tilted ground plane (`TILT 0.78`), perspective scale `s = 1/(1 - 0.30*dy/h)`, ground/objects tiles as scaled
  rects (overlap by 1-2 px to avoid seams), `above` tiles and sprites as upright billboards anchored at their feet, then
  tilt-shift blur (top/bottom 22%), bloom (additive blurred copy), vignette. UI is drawn on a separate layer so it is not blurred.
* Images: build from `frameRGBA` into `QImage::Format_RGBA8888` (non-premultiplied) and **copy** so Qt owns the memory; composite
  into `Format_ARGB32_Premultiplied`. Cache by (graphic id, frame, tint, flip, palette).
A reference implementation of the same contract exists in `reference/mana/pil_renderer.py` (SNES only) and generated the
screenshots in `reference/docs/shot_*.png`.

## 6. Data contracts (unchanged; read `reference/README.md` and the sample files)
Tile flags: SOLID=1 WATER=2 SLOW=4 HURT=8 ENCOUNTER=16 BRIDGE=32. Map entity types: spawn npc enemy chest sign exit trigger
light unit. Map keys: id name kind(world|town|dungeon|interior|field|battle) mode(action|tactics) style(snes|hd2d) tileset w h
outdoor climate music peaceful battle layers{ground,objects,above} entities, optional ambient{color,darkness}, encounters,
encounter_rate, objective, on_win. Database tables: actors classes enemies items weapons skills. Songs: tempo bars beats loop_bar
gs reverb tracks[{name channel program volume pan reverb chorus drum notes[[step,pitch,len,vel]]}].

## 7. Traps discovered while building the Python version (avoid repeating them)
1. Palette strings can be `#rrggbbaa`, but `QColor("#rrggbbaa")` reads 8 digits as `#aarrggbb`. Never hand palette strings to QColor; parse with the engine's `hexToRgba` and build `QColor(r,g,b,a)`.
2. Tactics `Item` action consumed the candy twice (once through `use_item`, once manually). Fix it in the port (single code path) and add a test.
3. Tactics step queue: pop the step **before** running its callback (callbacks may replace the queue, e.g. victory). The original ran callbacks
   at the wrong moment and the end-of-battle hook never fired.
4. Big canopy trees (2x3) block movement only through the `block` tile under the canopy; spawns/NPCs/enemies/chests placed in solid tiles are a
   data bug, `validate()` must flag them (it found a real one).
5. Dialog + script interplay: scripts freeze the field; `update()` must still advance transitions and weather while a script waits.
6. Autotile edges are periodic with period 16 so adjacent tiles line up; if you regenerate art, keep that property. 16-variant sets show small
   notches at inner corners (known limitation; a 47-tile blob set is a welcome follow-up, not part of this port).
7. Weather/clock are queried from scripts and schedules every hour tick; refreshing schedules must not teleport an NPC the player is talking to.
8. Float -> int rounding in tile/sprite placement must match (`round`, not truncate) or sprites jitter against tiles.

## 8. Tests = acceptance criteria
Port every Python test under `reference/tests/test_engine.py`, `test_autotile.py`, `test_music.py` to C++ (same names, same assertions;
tests that depended on Python's RNG sequence must be made seed-agnostic, e.g. force `encounter_rate = 1.0`). Required headless scenarios on the sample
project with `NullRenderer`: start + draw, walk town -> overworld -> cave -> back, `@return`, door closed at night / open at day, NPC schedules by time and
weather, night-only enemies, random encounter, conditions, inn rest (pay, heal, skip to 06:00), full quest flow into the tactics battle, chest once,
save/load round trip, arena fight to victory + level-up, tactics forecast/triangle, tactics win and defeat flows, every map loads and draws in both styles.
Plus: JSON/project round-trip, forge part combinations all produce valid 9-frame sheets, translucent palette alpha, v1 tileset upgrade, MIDI round trip + GS header,
seamless loop length, deterministic weather.

Qt tests (QTest, `QT_QPA_PLATFORM=offscreen`): `QtRenderer` renders every map in both styles without errors; a golden-image test renders 4 chosen frames
(town day, town night, interior, cave) and compares to `reference/docs/shot_*.png` with a generous tolerance (mean abs diff) because fonts differ; MainWindow
constructs and loads the sample project; map-editor terrain brush paints autotiles; pixel-editor undo/redo; piano-roll add/move/delete note.

Definition of done: `cmake --build` clean with `-Wall -Wextra -Werror`, `ctest` green, ASan+UBSan clean on engine tests, `mana_game projects/sylvi`
plays start-to-finish, `mana_studio projects/sylvi` opens every tab, playtest (F5) works with audio, and a short `README` documents build + layout.

## 9. How to work
* Work phase by phase (section 4). After each phase: build, run tests, commit. Never leave the tree non-compiling.
* Port tests first for each module (red), then the module (green). Keep each file focused; engine files < ~800 lines.
* When Python behaviour is ambiguous, run it (`python -m unittest discover -s reference/tests`, `python reference/tools/screenshots.py`) and match it.
* Record every intentional deviation in `DEVIATIONS.md` with the reason.
* If something can't be verified in your environment (e.g. no display, no audio device), say so in the commit message / README instead of guessing.

## 10. Explicitly out of scope
Qt Quick/QML, new gameplay systems (shops, equipment, status effects, vehicles), 47-tile autotiles, Mode-7 overworld, mobile builds. Do not change the JSON schema.
