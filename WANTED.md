# WANTED — Aur

> This file holds **Aur's own asks**. For estate-wide decisions, see `C:\Projects\WANTED.md`.

## Active plans
- `C:\Projects\Bible\notes\project-handoff-prep.md` — audit-only reconciliation. This plan is not an Aur implementation priority; keep the open owner queue below first.
- `C:\Projects\Bible\notes\qt-desktop-foundation.md` — P0 audit only; AIO/Vulkan, source/header map, naming, and toolbar/settings/keybindings. Preserve Aur's higher-priority map/DAW/editor requests.

- `Bible/notes/daw/` — Music Studio DAW initiative (P1, P3, P4.1–P4.3 MIDI
  device layer + count-in, P5 piano roll through P5.14 ids — P5 remainder
  closed 2026-11-11, see below — P6 through P6.7 (clips, launcher, scene
  record, arrangement placements/canvas/playback + arrangement MIDI export)
  all landed; P2 close awaits owner GUI soak; P8 effects framework landed
  (see Closed); P9 step sequencer + arpeggiator landed 2026-11-11 via the
  groovebox initiative (see below). Next: P7a GM/GS instrument — plan at
  `Bible/notes/daw/gm-gs.md` (P0 landed `daw-p7a.1`; **P1 deferred by owner
  2026-11-11** — parked mid-wiring, exact resume point recorded in the plan).
- `Bible/notes/database-editors.md` — the Database page's editors: six
  of its seven tabs are a list and a form of empty fields. P0 (the audit)
  done; P1 (the field tables) is next.
- `Bible/notes/platformer.md` — the platformer engine: the movement
  modes, the tile physics and the cameras. P0 (the audit) done; P1 (the
  modes) is next.
- `Bible/notes/bestiary.md` — the Bestiary: the Database page's
  creatures tab, with the record layer, the editor and the portrait.
  P0 (the audit) done; P1 (the Qt-free record layer) is next.
- `Bible/notes/editsweep.md` — editor defect sweep; D1 (all 13 audited bugs)
  landed `1e5fce9`, D2.S1 tile palette widget landed `07fd68f`, D3.S1 entity
  selection/multiples/inspector landed `218ba11`; ctest 7/7. D4.S1b (the
  QUndoStack rewrite) landed with its base corrected against the real document
  shapes and the map editor converted to per-cell diffs; D4.S2 landed the two
  visual defects the owner reported — the map editor's first-boot zoom and the
  world's black bars — with the render harness that proves them. **D5.S1 (the
  sprite studio) landed**: onion skinning, a live preview of the sprite's own
  sequence, the mirror, and the frame tools (add, duplicate, delete, reorder).
  **D6.S1 (tile pixel editing) landed**: one pixel surface for two editors,
  the tileset editor's canvas, and PNG in and out. **D7.S1 landed**: the file
  menu is a hamburger and the toolbar moved up a row. **D9 landed**: the
  arrangement timeline scrolls, the studio has one master volume, and the
  playtest plays the map's song through the one device the DAW previews
  with. Next: the owner queue below — the world map's time and weather
  parity, and the platformer engine (which needs its own plan).
- `Bible/notes/groovebox-sequencer.md` — 128-step sequencer + groovebox
  initiative. P0–P7 landed 2026-11-11: engine model (`StepSequencer`,
  `Arpeggiator`), realtime + offline playback, groove globals (swing/
  humanise/groove), Studio Step Sequencer tab (grid + strip), and the three
  oscillator synths (`VoiceCore` Gm/Fm/RetroPsg) retired in favour of the
  hardware cores. Closed 2026-11-11: P8 close-out done (DAW plan updated,
  WANTED/HANDOVER rewritten, ctest 7/7), and `test_note_index.cpp` added to
  close the P5 id-stable-selection remainder with test coverage.
- `Bible/notes/Aur World Map Agent Work Packages.md` — World Map editor
  work packages WP0–WP6; **WP0 landed** `1d9094a`, **WP1 single-copy view
  landed** `1be483e`, **WP2.S1 starter-UI removal landed** `a9d2726`,
  **WP3 weather renderer v2 landed** `6ea8eb0` (ctest 7/7, 53 checks;
  D3D12 visual captures and timing budget still owner-verified).
  Next: WP4 WorldSim hookup (owner decisions: temperature seeding, sim
  persistence), WP6 cleanup. WP5 globe view needs WP4.
- `Bible/notes/worldsim-p31.md` — P3.1 WorldSim core complete (P0–P2
  committed 2026-11-06); P3.2 editor simulation tools is next, owner-gated.
  NOTE: WP4 in the Work Packages brief covers the P3.2 weather slice and is
  owner-approved for that slice only.
- `Bible/notes/sc-cleanroom.md` — SC (Sound Canvas-style) clean-room instrument, planning only; sequenced behind DAW P7a/P7b.

- **`ROADMAP.md`** — the stages of this project's work and the standard a stage meets before the next starts (root `AGENTS.md` §"Project shape").
- **`IMPLEMENTATION.md`** — every open item, by area, with its evidence and its gate: W-01, W-02, W-03, E-01, E-02, P-01, D-01, Q-01, L-01.


## Open

- **Expand a world map tile into a map** (owner, 2026-11-12): pick a
  tile on the world map, send it to the map editor, and grow it into a map
  that keeps the direction -- a beach on the west side stays on the west
  side. Plan at `Bible/notes/globe-expand.md`.
- **A map seed generator** - the estate can seed a character, a creature,
  an effect, clothing, a prop and a tile, but not a map: every field map
  was made by hand. Plan at `Bible/notes/map-generator.md`.
- **Split the DAW's panel** - `studio/MusicPanel.cpp` is 1,349 lines,
  over the owner's hard ceiling of ~1,250, and it grew with the transport.
  The MIDI device strip (the combos, the connect buttons, the refresh, the
  record arm, the count-in and the status) is the piece to move first; the
  FX strip is already its own widget.
- **Qt desktop foundation** — Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/keybindings, and canonical `Aur` artifact identity; do not suffix its app name with Qt/desktop roles. Preserve the higher-priority active Aur queue.
- **Implement SNES-style platformer games and metroidvanias** (plan
  `Bible/notes/platformer.md`; P0 done, the audit): the engine moves a
  hero on a top-down field -- four directions, one speed, a centre
  probe, no gravity and no jump. The plan splits the movement into
  modes, gives the platformer gravity, a jump, a body and a sideways
  camera, and leaves the shipped field byte for byte as it is. The maps
  are already the rooms and the exits are already the doors, so what is
  missing is the rules and the camera, not a room model.
