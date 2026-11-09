# Historical Python Scratchpad

> This document describes the retired Python testbed, not the active product. The Python implementation has been removed; Aur's supported Studio and engine are the C++/Qt implementation in this repository. The shared sample project lives at `projects/sylvi/`.

A "GB Studio for the 16-bit era": a Qt 6.12 game studio and a Qt-free engine for making
**real-time action RPGs**, **Fire-Emblem-style tactics RPGs** and **2D-HD (HD-2D) RPGs** with SNES-style
graphics, a Chrono Trigger / Golden Sun style explorable world, day-night + weather, an event scripting
layer and a music studio (GM / Roland GS MIDI). Ships with a complete sample game,
**"Sylvi and the Moon Shard"** (tomboyish elf princess, Adol-style adventurer).

```
pip install -r requirements.txt        # PySide6>=6.12, numpy, Pillow
python run_studio.py                   # open the Studio on the sample project
python run_game.py ../../projects/sylvi  # run the shared sample without the editor
python -m unittest discover -s tests   # 47 tests (engine fully headless)
python tools/screenshots.py            # in-engine screenshots without Qt (Pillow reference renderer)
```

## Honest status

| Part | State |
|---|---|
| Engine (`mana/`): scenes, world transitions, day/night, weather, scripting, action combat, tactics, forge, autotiling, music, project IO | **Tested headless**: 47 tests incl. full playthroughs (quest flow, arena fight, tactics battle won/lost, time-gated doors, schedules, save/load). |
| Studio (`studio/`, PySide6 6.12) | **Written against the Qt 6 API but not yet run on a real Qt install** (the build sandbox had no PySide6). All panel *logic*, the renderer's projection/caching code and the playtest loop are smoke-tested against a fake Qt layer (`tests/fake_qt.py`), which cannot catch Qt-API misuse or layout problems. Expect a few first-run fixes; the first launch is the real test. |
| Audio | Songs render to WAV with a built-in chip-style synth for preview/playtest; export `.mid` (GM + GS SysEx) for real soundfont/hardware playback. |

## Architecture

```
mana/            engine, no Qt import (can run headless, in tests, or under another frontend)
  gfx.py         indexed 4bpp graphics, BGR555 snapping, PNG import/export
  forge.py       Character Forge: parts + colours -> full 3-direction walk sheets
  texgen.py      procedural tile textures
  clock.py       calendar, day/night light curve, seeded seasonal weather
  script.py      generator-based event interpreter + validator
  game.py        state, scene stack, transitions, dialog, menu, save/load
  field.py       exploration + real-time action scene (overworld, towns, dungeons, arenas)
  tactics.py     Fire-Emblem-style grid battles
  music.py       song model, mini-notation, MIDI export/import, chip synth, SFX
  render.py      the Renderer interface (engine -> backend)
studio/          PySide6 frontend: Qt renderer (SNES + HD-2D), playtest, editors
../../projects/sylvi/ the shared sample game (plain JSON, git-friendly)
tools/           scripts that generated the sample art/maps, PNG previews
```

The engine never draws pixels: it calls a `Renderer` (tile, sprite, light, ambient, weather, ui_*).
`studio/qt_renderer.py` implements it with QPainter; `NullRenderer` records calls for tests. Writing another
backend (pygame, Godot, web) means implementing about 15 methods.

### Three genres x two looks
Every map has `mode` (`action` | `tactics`) and `style` (`snes` | `hd2d`). They are independent, so you get:
action RPG (SNES), tactics (SNES), HD-2D exploration (the sample's *Whisperwood Glade*), HD-2D tactics, and so on.
HD-2D = tilted ground plane, upright billboard sprites with perspective scale, 2x internal resolution,
tilt-shift depth of field, bloom, vignette and additive light glow.

### The world (Chrono Trigger x Golden Sun)
- **Overworld** maps (`kind: "world"`): chibi-scale hero, encounter-flag tiles, random encounters that drop you into an arena map
  (or a tactics map) and return you to the exact tile afterwards.
- **Exits** link any map to any spawn point (`target: {map, spawn}`) with effects (`fade`, `white`, `none`); `@return` goes back to the
  remembered overworld position. Exits can be gated by conditions (`require`, e.g. a shop door open 8:00-18:00).
- The **World tab** draws every map and exit as a graph and flags unreachable maps, one-way doors and missing spawns.
- `project.validate()` catches dangling exits/spawns/graphics before you playtest.

### Time and weather
`GameClock` (configurable minutes-per-second), 4 seasons x 28 days, a day/night light curve (dawn, dusk, night tint + darkness)
and a seeded `WeatherSystem` (clear/cloudy/rain/storm/snow/fog) driven by per-map climate tables (`temperate`, `arid`, `arctic`, `none`).
Everything is queryable from scripts and data:
- conditions: `{"time":[22,6]}`, `{"night":true}`, `{"weather":["rain","storm"]}`, `{"season":"winter"}`, `{"gold":20}`, `{"item":"moon_shard"}`, `{"switch":"x"}`, `{"var":"n",">=":3}`
- NPC **schedules** (`from`/`to` hours, optional `cond`; no matching entry = NPC is away), night-only lights and enemies (`cond` on any entity),
  weather-aware dialogue (`{weather}`, `{period}`, `{season}`, `{time}` tokens), inn `skip_to` morning.
- Studio playtest has live controls for hour, weather, clock speed and map teleport.

### The 16-bit look (what makes it SNES, not NES)
The first pass was technically 15-colour but read as late-NES: one flat palette per tileset, hard black outlines, sparse speckle, square tile edges.
The art pipeline and data model now follow how real SNES games got their depth:
- **Per-tile palette banks**: a tileset holds up to 16 palette banks of 15 colours + transparent and every tile picks one (like SNES BG tile attributes), so grass, water, foliage, buildings, interiors and cobbles each get their own 4-5 step ramp.
- **Hue-shifted ramps**: shadows drift cool/purple, highlights drift warm/yellow (`tools/artkit.ramp`, `forge.build_palette`).
- **Dithering and tileable noise** instead of random single-pixel speckle; calm base textures with tufts, pebbles, strata and glints.
- **Translucent palette entries** (`#rrggbbaa`): soft cast shadows under trees, barrels, mountains and buildings, the equivalent of SNES colour-math shadows.
- **Autotiling** (`mana/autotile.py`): paths, rivers, ponds, ocean shores and cave pools pick wobbly, periodic edge variants (rounded corners, foam, bank shadows) from their neighbours. The Studio has a Terrain brush and "Re-tile layer".
- **Big 2x3 trees**, tiled roofs with eave shadows, chimneys, timber corners, per-building cast shadows, a cobbled plaza, plank floors and woven rugs.
- **Characters**: three-tone skin and hair ramps, top-left rim light, shaded undersides, hair gloss, and *selective outlines* (deep outline on the shadow side, the material's own dark tone on the lit side).
- **Lighting**: day/night tint + lamp/window light pools (see the `shot_town_*` images), weather overlays, and in HD-2D tilt-shift, bloom and vignette.

`python tools/screenshots.py` renders real in-engine frames (day, dusk, night, rain, cave, overworld, interior, dialogue, tactics) through the Pillow reference renderer.

### Content tools (the anti-tedium part)
- **Character Forge**: choose ears/hair/top/bottom/accessories/weapon + 9 colours. Auto-shading, auto-outline, derived shadow colours and a
  procedural walk cycle produce 3 directions x 3 frames (left is mirrored) in one SNES-legal 15-colour palette. Outfit presets, random NPCs, hue-shifted
  colour variants, "bake to editable graphic" for hand polish, PNG sheet export. Add your own parts via `parts.json` (ASCII templates, see `forge.register_parts`).
- **Pixel editor**: pencil/line/rect/fill/picker, mirror drawing, onion skin, undo, palette editing snapped to 15-bit colour, frame + animation management, live preview, PNG import (auto-quantised to 15 colours) and sheet export.
- **Tileset editor**: per-tile palette banks, flags (solid, water, slow, hurt, encounter, bridge), animated tiles, texture generator (grass, stone, brick, planks, water, ...). Autotile groups live in the tileset JSON: `"autotiles": {"path": {"tiles": [16 indices by N/E/S/W neighbour mask], "joins": ["path"]}}`.
- **Level designer**: 3 tile layers, collision overlay, paint/terrain/erase/rect/fill/pick tools, autotile terrain brush, entity placement (spawn, npc, enemy, chest, sign, exit, trigger, light, tactics unit), JSON entity inspector, map properties (style, mode, climate, music, ambient override), resize, "test play this map" (F6).
- **Database**: actors, classes, enemies, items, weapons, skills (schema-driven forms).
- **Events**: structured command lists for common events *and* every NPC/trigger event, with a validator. Commands: `say choice if set_switch set_var give_item take_item give_gold heal join wait teleport set_time advance_time skip_to set_weather face move shake flash music sfx battle call toast end`. Add your own with `script.register(name, fn)`.
- **Music studio**: piano roll, per-track GM program / channel / volume / pan / reverb / chorus, GS reverb macro, loop point, quick-compose mini-notation
  (`"C5 - E5 . Am7"`, drum strings like `"k . h . s . h h"`, chord arpeggios), MIDI import, GM/GS MIDI export (GS reset SysEx + CC111 loop), WAV preview.

## Data formats (all JSON, designed to diff in git)
- Graphic: `{w,h,palette[16],frames{name:[rows]},anims{name:{frames,fps,flip?}},origin}`, rows use `.` = transparent and hex digits = palette index; palette entries may be `#rrggbbaa` for translucent shadows.
- Tileset: `{name,tile:16,palettes:[bank,...],tiles:[{name,rows,flags,pal,anim?,fps?}],autotiles{}}` (v1 single-`palette` tilesets upgrade automatically).
- Map: `{id,name,kind,mode,style,tileset,w,h,outdoor,climate,music,layers{ground,objects,above},entities[...]}`.
- Song: `{name,tempo,bars,beats,loop_bar,gs,reverb,tracks[{name,channel,program,volume,pan,reverb,chorus,drum,notes[[step,pitch,len,vel]]}]}`.

## Controls
Arrows/WASD move, **Z/J** attack (hold to charge, 3 levels) / confirm, **Space/Enter** confirm, **X/K** special / cancel, **Tab** swap leader, **E** menu.
Tactics: cursor with arrows, Z confirm, X cancel, E end-turn menu; put the cursor on an enemy and press Z to toggle its threat range.

## Known gaps / roadmap
Shops UI, equipment screen, status effects, vehicles (ship/airship), cutscene camera/path commands, multi-tile stamps, 47-tile blob autotiles with inner corners (the current 16-variant set can show small notches at inner corners), Mode-7 style overworld renderer, per-character multi-palette sprites,
undo for entity edits, a skill/ability editor beyond `nova|bolt|heal`, controller input, export to a standalone executable (PyInstaller recipe), and a
side-by-side visual regression tests for the Qt renderer against the Pillow one.
