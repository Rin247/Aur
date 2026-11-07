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

## Open

- **MD/SNES synth cores pending** — a frontier model is producing `engine/aur/audio/YmFmCore.{hpp,cpp}` (6-op FM, 6-channel hard limit, DAC mode) and `SnesPcmCore`/`BrrCodec`/`SnesEcho` (BRR decode/encode, Gaussian interpolation, 8-tap FIR echo) per a spec held with the owner; on delivery, integrate per DAW P7b/P7c contracts (portable C++20, parameter table, deterministic tests).
- **Map editor starts zoomed out on first boot** — default zoom should be fit-to-view (or 100%) on a fresh open; reported from the UI 2026-11-06.
- **World Time Settings tab should be baked into the World editor** — merge the standalone tab into the World panel instead of a separate tab; reported 2026-11-06.
- **Implement SNES-style platformer games and metroidvanias** — build SNES-style platformer and metroidvania game support into the engine.
- **Refactor and merge the pixel and sprite editors** — they are almost identical in scope; consolidate into one editor.

## Closed

- **#52 Review Bible/Operations/** (closed 2026-11-07): all six docs + evidence audited; Agent Guide title fixed, Handover.md mojibake/stale-DAW-status/empty-A1/duplicate open-work lists fixed, Roadmap Music Studio bullets refreshed to the landed DAW scope, World Map Topology Handover got a current-status header (P3.0+P3.1 complete, P3.2 owner-gated). Plan at `Bible/notes/ops-review.md`.
- **#50 Project tidy audit** (closed 2026-11-06): reference chat relocated to `Bible/Archive/Reference/`, empty duplicate Bible folder removed, tests confirmed under `tests/`, stray root objects removed (`NUL.obj`, MSVC `.obj` leftovers). Root handover keeps the active graphics-backend policy.
- **#51 WorldPanel refactor** (closed 2026-11-06): all plan steps complete through 7c (`WorldMapCanvas` split). `WorldPanel.cpp` at 917 lines, under the owner's 1,000-line soft cap (+25% overhead rule); build green, both CTest suites pass. Layout documented in `Bible/Operations/NOTES.md`; the plan file was removed with acceptance verified (git history holds the record).
