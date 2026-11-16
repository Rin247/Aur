# Aur roadmap

The order of Aur's work: the editor defect sweep and whole-app repaint/refactor (landed 2026-11-16),
then the owner's queue in `WANTED.md` — world-map tile expansion, the map seed generator, and
(staged only) the Qt desktop foundation — plus the remaining open defect in `PROJECT.md`. A stage
is done when `cmd /c "AIO.bat build windows Release"` is green, `ctest --test-dir build\Release` is
7 of 7 suites (73 checks), the render PNGs in `build/renders/` are opened and read (root
`AGENTS.md` §22), and `py -3.15 C:\Projects\tools\conformance.py --project Aur` is all 9 checks
clean. This file holds the current state and is rewritten, not appended (§52, §44).

## The standard: what "done" means for a stage

1. **Build green.** `AIO.bat build windows Release` builds `build\Release\Aur.exe` and
   deploys the Qt runtime beside it — without it the exe exits 0xC0000135 and every runtime
   check fails on a build that passed (`PROJECT.md` §"Build targets").
2. **Suite green.** 7 of 7 suites pass (73 checks).
3. **Render evidence opened.** `tests\render_check.py` captures (Map Editor, World, Sprite
   Editor, Tileset Editor, DAW, Playtest, Creature Forge, Database at both estate sizes, plus
   the zoom sweep) are opened — an assertion passing is not the finding (§22).
4. **Conformance clean.** `py -3.15 C:\Projects\tools\conformance.py --project Aur` — all 9 checks clean.

## Stage 1 — Editor defect sweep and design-system repaint/refactor (landed 2026-11-16)

`Bible/notes/editsweep.md` (D1–D15) and the 2026-11-16 whole-app repaint/refactor all landed:
`studio/theme/` adopts `C:\Projects\DESIGN.md` §1, §2, §6.3, §13 (`ThemeManager` + `AurStyle`
`QProxyStyle` + `ThemeIcons` + `AppearanceDialog`, 11 modes + `system`, zero stylesheets);
`StudioChrome`, `FramelessWindow`, `MidiStrip`, `MusicPanelSession`, `WorldPanelActions`,
`WorldMapCanvasInput`, and `PianoRollInput` split every `studio/` file below the 750-line soft
limit; and the `Races` tab moved onto the generic `RecordEditor`.

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

## Stage 4 — Platformer engine (owner queue)

P1–P3 landed (movement modes, tile physics, one-way platform, cameras) in
`Bible/notes/platformer.md`; P4 waits on a solid floor tile — the owner picks the tile, then the
field's walkable floors become the platformer's floor (`studio/PixelSurface` is the one pixel
surface for graphic frame and tile).
Exit gate: P4 is owner-accepted.

## Stage 5 — Remaining open defect (`PROJECT.md`)

- `bandit`, `bandit_archer` and `bandit_boss` name graphics the project does not have, so
  the Bestiary's portrait says so (`PROJECT.md` open defect 1; `IMPLEMENTATION.md` `W-03`).
Exit gate: artwork present, or the records name the missing graphic honestly.

## Stage 6 — Qt desktop foundation (staged only)

Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/
keybindings, canonical `Aur` identity (no Qt/desktop role suffixes). Plan at
`C:\Projects\Bible\notes\qt-desktop-foundation.md`; implementation waits until the existing
queue allows it (`HANDOVER.md`: stage only, no foundation code before the queue allows it).
Exit gate: audit findings recorded with `file:line` citations; no foundation code lands before
the queue allows it.
