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
  menu is a hamburger and the toolbar moved up a row. Next: the owner queue
  below — the world map's time and weather parity, and the platformer engine
  (which needs its own plan).
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


## Open

- **Qt desktop foundation** — Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/keybindings, and canonical `Aur` artifact identity; do not suffix its app name with Qt/desktop roles. Preserve the higher-priority active Aur queue.
- **Time and weather functions for the world map** — the World Map editor needs time-of-day and weather preview/function parity with the rest of the editor (World & Time Settings defines calendar, seasons, eras, and climate profiles; the map canvas already previews temperature/equator/poles and forced weather). Close the gap so time/weather authoring on the map matches the project settings and the in-game debug menu's runtime overrides.
- **Implement SNES-style platformer games and metroidvanias** — build SNES-style platformer and metroidvania game support into the engine. Large: needs its own plan file (engine movement modes, tile physics, room/camera model) before any code.


## Closed

- **DAW P8 effects framework** (closed 2026-11-08, plan `daw/effects.md`,
  all phases): `IEffect` + registry + 10 built-ins (EQ, SVF, compressor,
  limiter, distortion, bitcrusher, tremolo, widener, phaser, SNES echo FIR),
  master chain (bypass slots, meters, CPU budget), additive song `effects`
  key (absent = bit-identical old path), offline/realtime parity 5.8e-06,
  MusicPanel FX strip. ctest 7/7 under MSVC. Design notes in the plan record
  (git history holds `effects.md`).
- **MD/SNES synth cores delivered + integrated** (closed 2026-11-08, plan
  `synthint` P0–P3, commits `0f0c8fe`…`e779982`): frontier-model drop verified
  (MinGW g++ 16.1 + MSVC 18), cores moved to `engine/aur/audio/` (YmFmCore,
  SnesPcmCore/BrrCodec/SnesEcho, FxRack), suites ported to three CTest
  executables (6/6 green), preview ids `md_fm`/`spc` + realtime mixer path
  wired with parity < 1e-5, MusicPanel dropdown updated. PSG (SN76489) and
  P8 FX framework remain open in the DAW plan. Details in git history;
  verification notes in `Bible/notes/synth-cores-delivery.md`.
- **World Time Settings tab baked into the World editor** (closed 2026-11-07, `a6a2b3a`): WorldSettingsPanel is now a "Time & World Settings" sub-tab inside the World tab; the standalone MainWindow tab is removed; `setProject` still reaches it via recursive `findChildren`.
- **A0 editor defects: Tileset Editor + Map Inspector** (closed 2026-11-07, `205bd9b` + `9f097f3`): tile list data-bound and rendered via `make_tileset_tile`, gameplay-flag checkboxes write `tiles[i].flags` (mislabeled "SNES Tile Attributes" retitled), Map Inspector's hardcoded Event ID replaced by a registry-validated combo with facing/event write-back. Operations/Handover.md A0 defect disposition recorded 2026-11-07.
- **Refactor and merge the pixel and sprite editors** (closed 2026-11-07, `f958e55`, tag `pixmerge-p2`): one "Pixel & Sprite Editor" tab with Graphic/Sprite modes and shared canvas/palette/history; P0–P2 verified, suite 40/40. See Operations/NOTES.md and git history.

- **#52 Review Bible/Operations/** (closed 2026-11-07): all six docs + evidence audited; Agent Guide title fixed, Handover.md mojibake/stale-DAW-status/empty-A1/duplicate open-work lists fixed, Roadmap Music Studio bullets refreshed to the landed DAW scope, World Map Topology Handover got a current-status header (P3.0+P3.1 complete, P3.2 owner-gated). Plan at `Bible/notes/ops-review.md`.
- **#50 Project tidy audit** (closed 2026-11-06): reference chat relocated to `Bible/Archive/Reference/`, empty duplicate Bible folder removed, tests confirmed under `tests/`, stray root objects removed (`NUL.obj`, MSVC `.obj` leftovers). Root handover keeps the active graphics-backend policy.
- **#51 WorldPanel refactor** (closed 2026-11-06): all plan steps complete through 7c (`WorldMapCanvas` split). `WorldPanel.cpp` at 917 lines, under the owner's 1,000-line soft cap (+25% overhead rule); build green, both CTest suites pass. Layout documented in `Bible/Operations/NOTES.md`; the plan file was removed with acceptance verified (git history holds the record).
- **Qt desktop foundation:** Audit AIO/Vulkan/backend behavior, legacy `src/`/`include/` co-location, and toolbar/settings/keybindings after the higher-priority World Map, DAW, and editor asks. Product identity is `Aur`; do not suffix Qt/desktop roles. Stage only; no foundation implementation until the existing queue allows it.

## Conformance findings (2026-11-11)

- **No ~/.Aur data root** (15a): the studio has no QSettings/QStandardPaths; add the root and route settings/window state through it.
- **Identity**: the app is `Aur Studio` while the folder is Aur — owner decides which is canonical (29). This also makes the "deployed" row UNKNOWN, because the checker looks for `Aur.exe`.
- **App icon** (closed 2026-11-11): Aur's mark is a pixel globe from the
  estate's single generator (`AssetLibrary/Icons`), installed as
  `studio/resources/icons/` with a PE resource (`.rc`) so Explorer draws it.
- **Render harness** (closed 2026-11-11): `tests/render_check.py` renders the
  Map Editor, World, Pixel & Sprite Editor and Tileset Editor pages at both
  estate sizes and sweeps the zoom as a control; the PNGs land in
  `build/renders/` and AGENTS.md says to open them.
- **Frameless mechanism** (closed 2026-11-12): `WM_NCCALCSIZE` gives the
  client area the whole window, so the chrome reaches the screen edge and
  the edge resize in `WM_NCHITTEST` is the app's own.
