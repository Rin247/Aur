# Aur roadmap

The order of Aur's work: the editor defect sweep (`Bible/notes/editsweep.md`, mostly landed),
then the owner's queue in `WANTED.md` — world-map tile expansion, the map seed generator, the
DAW panel split, and (staged only) the Qt desktop foundation — plus the open defects in
`PROJECT.md`. A stage is done when `cmd /c "AIO.bat build windows Release"` is green,
`ctest --test-dir build\Release` is 7 of 7 suites (67 checks), the render PNGs in
`build/renders/` are opened and read (root `AGENTS.md` §22), and
`python tools\conformance.py --project Aur` is all 9 checks clean. This file holds the current
state and is rewritten, not appended (§52, §44).

## The standard: what "done" means for a stage

1. **Build green.** `AIO.bat build windows Release` builds `build\Release\Aur.exe` and
   deploys the Qt runtime beside it — without it the exe exits 0xC0000135 and every runtime
   check fails on a build that passed (`PROJECT.md` §"Build targets").
2. **Suite green.** 7 of 7 suites pass (67 checks).
3. **Render evidence opened.** `tests\render_check.py` captures (Map Editor, World, Sprite
   Editor, Tileset Editor, DAW, Playtest, Creature Forge, Database at both estate sizes, plus
   the zoom sweep) are opened — an assertion passing is not the finding (§22).
4. **Conformance clean.** `python tools\conformance.py --project Aur` — all 9 checks clean.

## Stage 1 — Editor defect sweep (in flight, mostly landed)

`Bible/notes/editsweep.md`: D1 (13 audited bugs), D2.S1 (tile palette), D3.S1 (entity
selection), D4.S1b (QUndoStack rewrite), D4.S2 (owner's two visual reports), D5.S1 (sprite
studio), D6.S1 (tile pixel editing), D7.S1–D7.S3 (chrome) and D9 (studio tools) all landed;
ctest 7/7.
Exit gate: no open editsweep defect remains without an owner decision.

## Stage 2 — World Map expansion (owner queue)

Expand a world map tile into a map — pick a tile, send it to the map editor, grow it into a
map that keeps the direction (a beach on the west stays on the west); `pick_terrain` names
the cell's terrain and the side its land is on, and the map generator keeps that side. Plan
at `Bible/notes/globe-expand.md`. World Map Agent Work Packages WP0–WP3 landed; WP4 (WorldSim
hookup: temperature seeding, sim persistence — owner decisions) and WP6 cleanup are next; WP5
(globe view) needs WP4.
Exit gate: tile→map expansion works with direction preserved; WP4 accepted or owner-gated.

## Stage 3 — Map seed generator (owner queue)

The estate can seed a character, a creature, an effect, clothing, a prop and a tile, but not
a map — every field map was made by hand. Plan at `Bible/notes/map-generator.md`.
Exit gate: a generator produces a field map; build and suite green.

## Stage 4 — DAW panel split (owner queue)

`studio/MusicPanel.cpp` is over the ~1,250-line hard ceiling (1,311 lines per `PROJECT.md`,
1,349 per `HANDOVER.md`) and grew with the transport; the MIDI device strip (combos, connect
buttons, refresh, record arm, count-in, status) is the piece to move first; the FX strip is
already its own widget. DAW plan `Bible/notes/daw/plan.md`: P5 remainder closed; P7a (GM/GS
instrument) parked by owner decision 2026-11-11 (resume point in `Bible/notes/daw/gm-gs.md`);
P2 close awaits the owner's GUI soak.
Exit gate: `MusicPanel.cpp` under the ceiling with the MIDI device strip as its own widget;
parked phases remain owner-gated.

## Stage 5 — Platformer engine (owner queue; needs its own plan file)

P1–P3 landed (movement modes, tile physics, one-way platform, cameras); P4 waits on a solid
floor tile — the owner picks the tile, then the field's walkable floors become the
platformer's floor (`studio/PixelSurface` is the one pixel surface for graphic frame and
tile).
Exit gate: a plan file exists for the platformer engine and P4 is owner-accepted.

## Stage 6 — Remaining open defects (`PROJECT.md`)

- Races tab still a bespoke editor — twelve lore fields as text boxes where the other tabs
  use the generic editor (`PROJECT.md` open defect 2).
- `bandit`, `bandit_archer` and `bandit_boss` name graphics the project does not have, so
  the Bestiary's portrait says so (`PROJECT.md` open defect 3).
- `studio/world/WorldMapCanvas.cpp` (934) and `studio/WorldPanel.cpp` (917 per
  `HANDOVER.md`, 928 per `PROJECT.md`) over the 750-line soft limit (`PROJECT.md` open defect
  4; pre-existing).
Exit gate: each defect resolved or its baseline recorded in `Bible/`.

## Stage 7 — Qt desktop foundation (staged only)

Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/
keybindings, canonical `Aur` identity (no Qt/desktop role suffixes). Plan at
`C:\Projects\Bible\notes\qt-desktop-foundation.md`; implementation waits until the existing
queue allows it (`HANDOVER.md`: stage only, no foundation code before the queue allows it).
Exit gate: audit findings recorded with `file:line` citations; no foundation code lands before
the queue allows it.
