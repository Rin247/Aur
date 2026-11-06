# WorldSim P3.1: plan

Status: P0 (golden-hash safety net) — starting.

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

```
engine/aur/worldsim.hpp        params, WorldSim class, field enum
engine/aur/worldsim.cpp        kernels, hash, JSON round-trip
tests/test_worldsim.cpp        determinism, wrap, pins, save/load, golden hash
tests/main.cpp                 registration + --worldsim focused mode
CMakeLists.txt                 sources + test wiring
```

## Phases

### P0 Safety net
- [ ] `test_worldsim_determinism_stubs`: compile the empty `WorldSim` shell
      (params, configure, advance as no-op, hash over seed+tick) and pin the
      hash contract test so the kernel work starts from a passing green gate.
Acceptance: build green, EngineTests + TopologyTests pass, commit
`worldsim-p0`.

### P1 Core fields
- [ ] Configure: downsample height from a map JSON (half-tile mean), seed
      per-field from hash(seed, field, cell).
- [ ] Kernels: diffusion (temperature, moisture), advect+decay (wind u/v,
      cloud coverage), precipitation rule (coverage+moisture→rain), snowpack
      accumulate/melt by temperature; double-buffered, wrapped X reads.
- [ ] Pins honored in every kernel write.
Acceptance: unit tests — wrap continuity (edge cells read across the seam),
pin immutability, double-buffer (advance(2) == advance(1)+advance(1)), bounded
values; commit `worldsim-p1`.

### P2 Determinism + persistence
- [ ] Golden hash: 32×16 world, fixed seed, 10,000 ticks; hash recorded as a
      test constant. Repeat-run equality and save/load round-trip equality.
- [ ] `--worldsim` focused CTest mode.
Acceptance: golden-hash test green twice in a row (run-to-run), save/load
round-trip byte-equal hash; commit `worldsim-p2`.

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

- Half-tile grid as proposed in the handover brief; 600 s/tick kept.
- Seven candidate fields from the brief all included; wind stored as two
  component fields (u/v) so advection is stable and simple.
- Golden-hash fixture uses a 32×16 world (brief's example was 256×128 at
  128×64 half-tiles; 32×16 keeps the suite fast while proving the same
  determinism contract).
- P3.0 remains closed; no topology behavior changes here.
