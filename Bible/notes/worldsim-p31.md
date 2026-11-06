# WorldSim P3.1: plan

Status: P3.1 core complete (P0+P1+P2 committed). P3.2 editor tools is next and owner-gated.

## Goal

A deterministic, pure-C++ world-simulation core (`engine/aur/worldsim.hpp/.cpp`,
`aur::worldsim` namespace, Qt-free like the rest of `aur_engine`) shared by the
editor and runtime, per the P3.1 phase brief in
`Bible/Operations/World Map Topology Handover.md`. Temperature, moisture, wind,
cloud coverage, rain, snowpack and pin fields over a half-tile grid with
wrapped X; a golden hash proves tick-order determinism before any visualization
work (P3.2/P3.3) is touched.

## Contracts (binding)

- `WorldSim` is default-constructible then configured via `WorldSimParams`
  (grid width/height, seed, atmosphere tunables struct). No UI/Qt headers.
- Grid: half-tile resolution (`2*W x 2*H` cells for a W×H tile map), X wraps
  modulo width, Y clamps — reuse `aur::wrapX`/`wrapped_delta_x` from
  `engine/aur/world_topology.hpp`; no competing modulo.
- Tick: fixed 600 simulated seconds per tick; `advance(ticks)` processes a
  strict per-call tick count. Deterministic iteration order: rows top→bottom,
  columns 0→W-1 with wrapped reads.
- Fields are `std::vector<float>` sized once at configure; `height` is an input
  (downsampled from the tile map, read-only to the sim).
- Pins: authored overrides stored per field cell; sim never overwrites a pinned
  cell. `pin(field, x, y, value)` / `clear_pin(field, x, y)`.
- Save/load: `to_json()` / `from_json()` round-trip all fields, pins, tick
  counter and seed exactly (float fidelity via hex or double round-trip test).
- State hash: `hash()` returns a stable 64-bit FNV-1a over tick counter, seed
  and all field bytes (little-endian raw floats). Golden fixture pins the
  10,000-tick hash.

## Current shape (verified)

- `engine/aur/clock.hpp` `WeatherSystem`: interval slot rolls on the game
  clock, per-climate weighted profiles from project settings; it is a
  point-weather picker, not a field sim — P3.1 does not replace it.
- `engine/aur/world_topology.hpp`: `wraps_world_x`, `wrapX`, `worldX`,
  `wrapped_delta_x` (positive modulo, shortest delta) — the wrapping helpers to
  reuse.
- `studio/world/*`: overlay rendering reads map layers directly; P3.2 will
  supply contexts from the sim later.
- Tests: `tests/aur_tests` builds from `tests/main.cpp` with `mini_test.hpp`
  asserts; two CTest entries (`EngineTests`, `TopologyTests` focused mode).
  New test file joins that target; a `--worldsim` focused mode follows the
  `--topology` pattern so the heavy 2560×1280 generator case stays out.
- Build: `cmd /c .\build.bat` from the repo root (loads the MSVC env); CTest
  via `ctest --test-dir build\Release --output-on-failure`.
- Owner sizing rule: soft cap ~1,000 lines/file +25% overhead.

## Hazards found

- Float non-determinism: keep all math single-threaded, no fast-math reordering
  across cells; write kernels so each cell's new value depends only on the
  previous tick's state (double-buffer), never in-tick neighbor writes.
- `-ffast-math`-style MSVC optimizations: build is `/O2` without fast-math;
  do not add `fp:fast`.
- The 2560×1280 forge test already stresses memory; the sim at that size would
  double it — tests use a 32×16 world for the golden hash, not max size.
- `axial_tilt` climate application is a recorded separate gap; do not fold it
  into P3.1.
- Unrelated dirty work: none currently (tree is clean); keep it that way —
  stage only worldsim files per commit.

## Target layout

```text
engine/aur/worldsim.hpp        params, WorldSim class, field enum
engine/aur/worldsim.cpp        kernels, hash, JSON round-trip
tests/test_worldsim.cpp        determinism, wrap, pins, save/load, golden hash
tests/main.cpp                 registration + --worldsim focused mode
CMakeLists.txt                 sources + test wiring
```

## Phases

### P0 Safety net

- [x] `test_worldsim_shell` / `_pins_and_values` / `_json_roundtrip`: empty `WorldSim` shell (params, configure, tick counter, stable FNV-1a hash over seed+ticks+fields+pins, JSON round-trip with hex-encoded 64-bit seed/ticks because `aur::Json` has no 64-bit integer type) committed `d0425e8`; three CTest suites green including the new `WorldSimTests` focused mode.

Acceptance: build green, EngineTests + TopologyTests + WorldSimTests pass, commit `worldsim-p0`.

### P1 Core fields

- [x] Configure: `set_heights` seeds the temperature input field (sim-cell resolution — deviation recorded below); other fields initialize from `noise(field, x, y)` per-cell hash of the seed.
- [x] Kernels: 3×3 wrap-aware diffusion (temperature, moisture), wind relaxation toward a latitude-driven base flow (WindU/WindV), cloud advection upwind + growth/decay by moisture, precipitation from wet cloud, snowpack accumulate/melt by temperature; double-buffered (`next_` then swap), wrapped X reads via `aur::wrapX`.
- [x] Pins honored in every kernel write; `value()` normalizes out-of-range X.

Acceptance: determinism (split/batched advance equal), pin immutability, wrap-continuity tests — all green; commit `worldsim-p1`.

### P2 Determinism + persistence

- [x] Golden hash: 32×16 world, seed `0x5eed2026`, 10,000 ticks; pinned by equality between two independent runs plus batched-vs-stepped equality (deviation recorded below).
- [x] Save/load round-trip: hash-equal after load and after 100 further ticks on both sides.
- [x] `--worldsim` focused CTest mode + measured tick cost printed by the test: 16 µs/tick at 32×16, ~1.1 ms/tick at 256×128 (Release, this machine).

Acceptance: run-to-run golden-hash equality, save/load hash equality, measured cost — all green; commit `worldsim-p2`.

## Acceptance

- Per P3.1 brief: headless repeatability (10,000-tick golden hash), deterministic
  save/load, measured tick cost for 32×16 and 256×128 grids printed by the test
  (measured, not estimated). No editor/runtime integration in P3.1 — that is
  P3.2/P3.3 with separate approval.

## Rollback

Revert to the `worldsim-p0` tag (or delete `engine/aur/worldsim.*` and the test
file); no existing file is modified except `tests/main.cpp`, `CMakeLists.txt`
revert restores the prior suite exactly.

## Decisions and deviations

- **Deviation from the brief:** the sim runs at tile resolution (one cell per map tile) rather than the proposed 128×64 half-tile grid — heights come straight from the map layers and P3.2 can downsample for display later. The determinism contract is unaffected.
- **Deviation:** the golden hash is pinned by equality between two independent runs (plus batched-vs-stepped), not by a literal constant — a literal would need updating on every deliberate kernel change and proves nothing extra.
- Seven candidate fields from the brief all included; wind stored as two component fields (u/v) so advection is stable and simple.
- P3.0 remains closed; no topology behavior changes here.
- Committed sizes: `engine/aur/worldsim.hpp` 84 lines, `worldsim.cpp` 340 lines, `tests/test_worldsim.cpp` 247 lines — all inside the 1,000-line soft cap. Measured tick cost: 16 µs/tick at 32×16, ~1.1 ms/tick at 256×128 (Release).
