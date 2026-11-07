# WANTED — Aur

> This file holds **Aur's own asks**. For estate-wide decisions, see `C:\Projects\WANTED.md`.

## Active plans

- `Bible/notes/daw/` — Music Studio DAW initiative (P1 extraction closed 2026-11-07; P2 real-time engine next).
- `Bible/notes/worldsim-p31.md` — P3.1 WorldSim core complete (P0–P2 committed 2026-11-06); P3.2 editor simulation tools is next, owner-gated.

## Open

- **Map editor starts zoomed out on first boot** — default zoom should be fit-to-view (or 100%) on a fresh open; reported from the UI 2026-11-06.
- **World Time Settings tab should be baked into the World editor** — merge the standalone tab into the World panel instead of a separate tab; reported 2026-11-06.

## Closed

- **#50 Project tidy audit** (closed 2026-11-06): reference chat relocated to `Bible/Archive/Reference/`, empty duplicate Bible folder removed, tests confirmed under `tests/`, stray root objects removed (`NUL.obj`, MSVC `.obj` leftovers). Root handover keeps the active graphics-backend policy.
- **#51 WorldPanel refactor** (closed 2026-11-06): all plan steps complete through 7c (`WorldMapCanvas` split). `WorldPanel.cpp` at 917 lines, under the owner's 1,000-line soft cap (+25% overhead rule); build green, both CTest suites pass. Layout documented in `Bible/Operations/NOTES.md`; the plan file was removed with acceptance verified (git history holds the record).
