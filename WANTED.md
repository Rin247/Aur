# WANTED — Aur

> This file holds **Aur's own asks**. For estate-wide decisions, see `C:\Projects\WANTED.md`.

## Active plans
- `C:\Projects\Bible\notes\project-handoff-prep.md` — audit-only reconciliation. This plan is not an Aur implementation priority; keep the open owner queue below first.
- `C:\Projects\Bible\notes\qt-desktop-foundation.md` — P0 audit only; AIO/Vulkan, source/header map, naming, and toolbar/settings/keybindings. Preserve Aur's higher-priority map/DAW/editor requests.

- `Bible/notes/daw/` — Music Studio DAW initiative (P1, P3, P4.1–P4.3 MIDI
  device layer + count-in, P5 piano roll through P5.14 ids — P5 remainder
  closed 2026-11-11 — P6 through P6.7 (clips, launcher, scene
  record, arrangement placements/canvas/playback + arrangement MIDI export)
  all landed; P2 close awaits owner GUI soak; P8 effects framework landed;
  P9 step sequencer + arpeggiator landed 2026-11-11 via the groovebox
  initiative. Next: P7a GM/GS instrument — plan at `Bible/notes/daw/gm-gs.md`
  (P0 landed `daw-p7a.1`; **P1 deferred by owner 2026-11-11** — parked
  mid-wiring, exact resume point recorded in the plan).
- `Bible/notes/platformer.md` — the platformer engine: P1–P3 (movement modes,
  tile physics, one-way platform, cameras) landed and tested; P4 waits on a
  solid floor tile chosen by the owner.
- `Bible/notes/Aur World Map Agent Work Packages.md` — World Map editor
  work packages WP0–WP6; **WP0 landed** `1d9094a`, **WP1 single-copy view
  landed** `1be483e`, **WP2.S1 starter-UI removal landed** `a9d2726`,
  **WP3 weather renderer v2 landed** `6ea8eb0` (ctest 7/7; D3D12 visual
  captures and timing budget still owner-verified).
  Next: WP4 WorldSim hookup (owner decisions: temperature seeding, sim
  persistence), WP6 cleanup. WP5 globe view needs WP4.
- `Bible/notes/worldsim-p31.md` — P3.1 WorldSim core complete (P0–P2
  committed 2026-11-06); P3.2 editor simulation tools is next, owner-gated.
  NOTE: WP4 in the Work Packages brief covers the P3.2 weather slice and is
  owner-approved for that slice only.
- `Bible/notes/sc-cleanroom.md` — SC (Sound Canvas-style) clean-room instrument, planning only; sequenced behind DAW P7a/P7b.

- **`ROADMAP.md`** — the stages of this project's work and the standard a stage meets before the next starts (root `AGENTS.md` §"Project shape").
- **`IMPLEMENTATION.md`** — every open item, by area, with its evidence and its gate: W-01, W-02, W-03, P-01, D-01, Q-01.

## Open

- **Expand a world map tile into a map** (owner, 2026-11-12): pick a
  tile on the world map, send it to the map editor, and grow it into a map
  that keeps the direction -- a beach on the west side stays on the west
  side. Plan at `Bible/notes/globe-expand.md`.
- **A map seed generator** - the estate can seed a character, a creature,
  an effect, clothing, a prop and a tile, but not a map: every field map
  was made by hand. Plan at `Bible/notes/map-generator.md`.
- **Qt desktop foundation** — Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/keybindings, and canonical `Aur` artifact identity; do not suffix its app name with Qt/desktop roles. Preserve the higher-priority active Aur queue.
- **Implement SNES-style platformer games and metroidvanias** (plan
  `Bible/notes/platformer.md`; P1–P3 landed; P4 awaits owner floor tile selection):
  the engine splits movement into modes, gives the platformer gravity, a
  jump, a body, a one-way platform and a sideways camera, and leaves the
  shipped field byte for byte as it is.

## Recently closed (verified 2026-11-16)

- **Whole-app repaint and refactor** (`Bible/Operations/NOTES.md`, 2026-11-16):
  adopted the estate design system (`studio/theme/`, `DESIGN.md` §1, §2, §6.3, §13:
  `ThemeManager`, `AurStyle` `QProxyStyle`, `ThemeIcons`, `AppearanceDialog`,
  11 modes + `system`, `#3b82f6` default accent, zero `setStyleSheet` calls);
  extracted `StudioChrome`, `FramelessWindow`, `MidiStrip`, `MusicPanelSession`,
  `WorldPanelActions`, `WorldMapCanvasInput`, and `PianoRollInput` so every
  file in `studio/` sits below the 750-line soft limit; moved the `Races` tab
  onto the generic `records::RecordEditor` (`E-01`, `E-02`, `L-01` closed).
