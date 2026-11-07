# WANTED — Aur

> This file holds **Aur's own asks**. For estate-wide decisions, see `C:\Projects\WANTED.md`.

## Active plans

- `Bible/notes/daw/` — Music Studio DAW initiative (P1, P3, P4.1–P4.3 MIDI
  device layer + count-in, P5 piano roll through P5.14 ids, P6 through P6.7
  (clips, launcher, scene record, arrangement placements/canvas/playback +
  arrangement MIDI export) all landed; P2 close awaits owner GUI soak; next:
  P7a GM/GS instrument, P8 effects).
- `Bible/notes/sc-cleanroom.md` — SC (Sound Canvas-style) clean-room instrument, planning only; sequenced behind DAW P7a/P7b.
- `Bible/notes/worldsim-p31.md` — P3.1 WorldSim core complete (P0–P2 committed 2026-11-06); P3.2 editor simulation tools is next, owner-gated.
- `Bible/notes/pixmerge.md` — Pixel/Sprite editor merge; Phase 0 (behaviour-pinning tests) is next.

## Open

- **MD/SNES synth cores pending** — a frontier model is producing `engine/aur/audio/YmFmCore.{hpp,cpp}` (6-op FM, 6-channel hard limit, DAC mode) and `SnesPcmCore`/`BrrCodec`/`SnesEcho` (BRR decode/encode, Gaussian interpolation, 8-tap FIR echo) per a spec held with the owner; on delivery, integrate per DAW P7b/P7c contracts (portable C++20, parameter table, deterministic tests).
- **Time and weather functions for the world map** — the World Map editor needs time-of-day and weather preview/function parity with the rest of the editor (World & Time Settings defines calendar, seasons, eras, and climate profiles; the map canvas already previews temperature/equator/poles and forced weather). Close the gap so time/weather authoring on the map matches the project settings and the in-game debug menu's runtime overrides.
- **Implement SNES-style platformer games and metroidvanias** — build SNES-style platformer and metroidvania game support into the engine. Large: needs its own plan file (engine movement modes, tile physics, room/camera model) before any code.
- **Refactor and merge the pixel and sprite editors** — they are almost identical in scope; consolidate into one editor. Plan committed at `Bible/notes/pixmerge.md`.

## Closed

- **Map editor starts zoomed out on first boot** (closed 2026-11-07, `757b932`): the fix landed in the World editor — first map selection now calls `fitToView()`; the Map Editor itself already fitted on `refreshMaps()`.
- **World Time Settings tab baked into the World editor** (closed 2026-11-07, `a6a2b3a`): WorldSettingsPanel is now a "Time & World Settings" sub-tab inside the World tab; the standalone MainWindow tab is removed; `setProject` still reaches it via recursive `findChildren`.
- **A0 editor defects: Tileset Editor + Map Inspector** (closed 2026-11-07, `205bd9b` + `9f097f3`): tile list data-bound and rendered via `make_tileset_tile`, gameplay-flag checkboxes write `tiles[i].flags` (mislabeled "SNES Tile Attributes" retitled), Map Inspector's hardcoded Event ID replaced by a registry-validated combo with facing/event write-back. Operations/Handover.md A0 defect list should be annotated closed on next touch.

- **#52 Review Bible/Operations/** (closed 2026-11-07): all six docs + evidence audited; Agent Guide title fixed, Handover.md mojibake/stale-DAW-status/empty-A1/duplicate open-work lists fixed, Roadmap Music Studio bullets refreshed to the landed DAW scope, World Map Topology Handover got a current-status header (P3.0+P3.1 complete, P3.2 owner-gated). Plan at `Bible/notes/ops-review.md`.
- **#50 Project tidy audit** (closed 2026-11-06): reference chat relocated to `Bible/Archive/Reference/`, empty duplicate Bible folder removed, tests confirmed under `tests/`, stray root objects removed (`NUL.obj`, MSVC `.obj` leftovers). Root handover keeps the active graphics-backend policy.
- **#51 WorldPanel refactor** (closed 2026-11-06): all plan steps complete through 7c (`WorldMapCanvas` split). `WorldPanel.cpp` at 917 lines, under the owner's 1,000-line soft cap (+25% overhead rule); build green, both CTest suites pass. Layout documented in `Bible/Operations/NOTES.md`; the plan file was removed with acceptance verified (git history holds the record).
