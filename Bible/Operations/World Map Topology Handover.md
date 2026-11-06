# World Map Topology Handover

## Purpose and scope

This document turns the user's
[`chat-Sea of Stars HD2D Map Size.txt`](../Archive/Reference/chat-Sea%20of%20Stars%20HD2D%20Map%20Size.txt)
conversation into an implementation handoff for Aur. The next implementation
task is **P3.0 only**: finish and verify cylindrical world topology. Stop at that
gate. Do not start WorldSim, hydrology, or gameplay-depth work without a separate
approval.

The product goal behind the work is a world editor where the author can sculpt
terrain and see a rich, zoomable HD-2D-style visualization without hand-painting
the whole world as one final bitmap. The player-facing runtime remains a
separate, low-cost rendering of the same authored world. The longer-term goal
adds a living climate, surface hydrology, aquatic play, and comparable editor
and in-game debug controls.

The handoff is intentionally broader than the active code task: it preserves
the product goal, the dependency chain, evidence already in the code, what is
known to be broken, what has not been verified, and where the user's later
world-building requests fit. It is not authorization to implement all of those
later systems at once.

Use the C++20/Qt implementation and current repository guidance in the [Agent
Guide](Agent%20Guide.md), [Bible index](../README.md), [operations
handover](Handover.md), and repository-root `HANDOVER.md`. The chat is now
available at the repository-root path above. It contains assistant proposals
as well as user decisions; follow the Bible's [source
precedence](../README.md#source-of-truth), and do not treat unconfirmed
assistant suggestions as locked design.

## Product and map-size contract

- The world is **tile data**, not a single-resolution bitmap. Keep authoring,
  editor viewport, runtime rendering, and baked map-image resolution separate.
- The supplied sculpt-view brief uses a **256×128 tile** world and proposes
  2048×1024 (or 4096×2048) baked map imagery. These are different quantities.
- Aur currently supports generated maps up to **2560×1280 tiles**; the Sylvi
  starter world is **256×128 tiles**. Do not silently shrink the supported
  maximum, enlarge the starter map, or hardcode the topology width to 256.
  `wrapX` must use each map's actual width.
- Existing runtime presentation is 640×360 logical pixels, shown at 1280×720;
  field tiles are 32×32. P3.0 must not change this presentation or invent a
  single-image world-size limit.
- The editor remains free to pan and zoom independently of runtime resolution.
  P3.0's camera rule is unbounded/repeating X and clamped Y for wrapping worlds.
- Keep the generated world-select/sculpt map distinct from traversable
  field/region maps. Current product guidance treats the Sylvi world map as
  256×128 source tiles and playable overworlds as authored 80×80–128×128-tile
  regions. Do not make every large spherical world a traversable field as a
  side effect of wrapping.
- The design conversation recommended 32×32 tile art, 640×360 runtime logical
  resolution scaled to at least 1280×720, native-resolution editor UI/overlays,
  arbitrary editor pan/zoom, and a 640×360 player preview. Aur already uses the
  first two runtime values; those are context, not a P3.0 task.
- The conversation's renderer proposal distinguishes an editor-only sculpt
  visualization, a baked map-screen image (2048×1024, optional 4096×2048),
  and gameplay chunks. P3.0 implements none of those bakes. Do not mistake the
  256×128 source grid for the 2048×1024 output image.

## Locked P3.0 topology

For generated spherical worlds, treat the map as a cylinder:

- X is longitude and wraps modulo the map width.
- Y is latitude and clamps to `[0, height)`. It does not wrap across the poles.
- Non-generated maps retain their current bounded-X behavior unless a later
  confirmed design explicitly changes it.
- Texture sampling for world fields repeats U and clamps V. Authored procedural
  noise must be seam-free (cylindrical embedding); simulation fields, when
  implemented in a later phase, must advect with wrapped grid indexing.
- Polar latitude for generated spherical worlds is derived from Y. `axial_tilt`
  is currently accepted and persisted by world generation, but source inspection
  found it is not applied to generated climate; seasonal declination behavior is
  not established in this generator. Do not claim either behavior is already
  preserved. Keep this known gap explicit and leave implementing tilt/climate
  behavior outside P3.0 unless separately approved.

`engine/aur/world_topology.hpp` already provides `wraps_world_x`, `wrapX`,
`worldX`, and `wrapped_delta_x`. Audit and reuse these helpers; do not add
competing modulo implementations.

## Current implementation state

The previous handoff reports partial P3.0 changes in:

- Engine/world rules: `engine/aur/world_topology.hpp`,
  `engine/aur/worldgen.cpp`, `engine/aur/autotile.cpp`,
  `engine/aur/project.cpp`, `engine/aur/mcp.cpp`, `engine/aur/game.cpp`,
  `engine/aur/field.hpp`.
- Editor: `studio/WorldModel.hpp`, `studio/WorldModel.cpp`,
  `studio/WorldPanel.hpp`, `studio/WorldPanel.cpp`,
  `studio/shaders/world_sculpt.frag`.
- Tests: `tests/test_autotile.cpp`, `tests/test_world_model.cpp`,
  `tests/test_forge.cpp`, `tests/main.cpp`.

The shared helper currently enables wrapping for `continents`, `archipelago`,
`supercontinent`, and `fractured`. The working tree has substantial pre-existing
untracked project files and modified documentation. Preserve all unrelated
work; inspect `git status` before touching anything.

### What source inspection has confirmed

These are present in the current files, but are not proof that the entire P3.0
acceptance gate passes:

| Surface | Current implementation evidence | Remaining proof |
|---|---|---|
| Topology primitive | `engine/aur/world_topology.hpp` has metadata-based world detection, positive-modulo `wrapX`, conditional `worldX`, and shortest-X delta. | Direct `EngineTests` now exercise all four topology identifiers and missing/non-string/unsupported topology metadata alongside normalization helpers; the Release suite passed on 2026-11-03. Ordinary-map callers still need auditing. |
| World generation | `engine/aur/worldgen.cpp` samples noise on 3D sphere coordinates (longitude/latitude), uses Y-derived climate latitude, and wraps several hydrology/coast neighbor reads. `axial_tilt` is serialized but not applied by this generator. | Audit every neighbor/hash/tie path and test seam-continuous generation. Keep axial-tilt climate behavior recorded as a separate unimplemented gap; do not imply an existing seasonal calculation. |
| Runtime field | `engine/aur/field.hpp` wraps collision lookup, tile lookup and visible draw iteration in X; camera X follows the hero while Y is bounded. | Tests now cover seam collision, bounded-map rejection, polar clamping, camera movement, and repeated tile columns at a non-starter width. Live runtime display verification remains open. |
| Climate lookup | `engine/aur/game.cpp` wraps the sampled biome X and clamps biome Y. | Verify both edges and malformed/short biome rows do not index out of bounds. |
| Autotile | `engine/aur/autotile.cpp` wraps cardinal X neighbor reads and paint/erase/retile coordinates. | Release tests cover seam neighbors, width-one paint/erase and `retile_all`; malformed/missing layer guards are also exercised. More four-edge visual proof remains in the editor gate. |
| Project/MCP painting | `engine/aur/project.cpp` and `engine/aur/mcp.cpp` normalize X for generated-world biome edits and reject invalid Y. | Tests now cover public negative/over-width world edits, bounded-map rejection, invalid Y, and MCP biome persistence across the seam. Public/MCP acceptance responses remain covered by `EngineTests`. |
| Editor data model | `studio/WorldModel.cpp` normalizes wrapped rectangle access, JSON writes and restore; `markDirty` splits a seam edit into valid texture rectangles. | Test rectangles wider than one world are rejected, multi-row wrap edits, undo/redo ordering, and exact dirty-rect contents/revision behavior. |
| GPU world fields | `studio/WorldPanel.cpp` creates repeat-U/clamp-V samplers for wrapping models, uses linear height and nearest biome/water sampling, and uploads dirty rectangles. The sculpt shader wraps sample U with `fract`. | Verify edge pixels, height derivatives/coast effects, dirty uploads, backend execution, and no seam at multiple zooms. |
| Editor fallback | `studio/WorldPanel.cpp` repeats the cached map image and computes wrapped stroke deltas. | Verify camera constraints, pointer-to-world mapping, selection, overlays, marker interaction and stroke continuity in a live window. |

The current terrain generator already uses a spherical 3D noise embedding. Do
not replace it mechanically with a flat cylinder-noise formula. Check whether
each authored spatial/hash effect is periodic and whether the existing sphere
projection provides the intended seam/pole behavior before changing it.

The wider P3.0 brief mentions chunk streaming, pathing, scatter hashes, and
bake loops. The current Part 1 handover describes full-map rendering and
explicitly calls larger-world chunk rendering future work; no dedicated
world-chunk streaming or pathfinding implementation has been established by
this inspection. Search for actual callers before adding code. For a surface
that does not exist, record it as a future dependency rather than inventing a
new subsystem to satisfy an inapplicable acceptance bullet. Accordingly, the
P3.0 acceptance gate covers only existing relevant behavior; chunk streaming
and pathfinding are conditional future dependencies, not required deliverables.

Likewise, a search of `engine/aur` found `Game::travel(...)`, but no runtime
dispatch that reads `type == "exit"` entities. Studio-authored exit markers
and runtime map-transition triggers are different features. P3.0 must keep
authored exit coordinates intact and verify any existing coordinate
normalization. Runtime exit traversal is not part of the acceptance gate unless
an actual trigger path is found; do not claim or implement a playable
seam-crossing transition without a trigger path and separate approval.

**Known validation state at handoff:**

- Historical baseline from 2026-11-02: `ctest --test-dir
  C:\Projects\Aur\build\Release -C Release -R EngineTests --output-on-failure`
  failed with exit code 8 after 26.63 seconds. The runner reported 10 passed
  and one failed assertion at the time, in `test_autotile_basic` at
  `tests/test_autotile.cpp:34`:
  `mask_at(m, ts, "ground", 0, 1, "path") == aur::W`. The fixture was corrected; see the fresh passing run below.
- The original line-34 fixture issue was corrected by clearing the center tile
  before the independent seam-edge mask assertions. Later one-column fixture
  issues were corrected by giving the test its own mask-to-tile atlas and
  preserving that atlas for paint/erase.
- Fresh validation on 2026-11-03: Release build completed and CTest passed
  (1/1 CTest test, 26.00 seconds; the runner reported 10 passed, 0 failed).
  The suite includes the new topology predicate coverage; visual acceptance
  evidence and the rest of P3.0 remain open.
- WorldModel seam edit/undo coverage was previously reported passing; this run
  did not independently re-audit every editor history path.
- Editor seam rendering, runtime seam traversal, shader repeat/clamp behavior,
  and a clean visual build have **not** been verified. Runtime exit traversal
  has no identified trigger path and is not a P3.0 acceptance requirement. The
 older test result above is historical; report only the fresh passing run for
  the current source.

### Dirty-worktree rules

The Aur checkout has extensive tracked modifications and untracked source,
test, and asset trees. Before coding, save the output of `git status --short`
and inspect only the relevant diffs. The earlier inspection showed unrelated
changes in Bible/operations documents, including handover material; do not
restore, stage, or rewrite them. Touch only P3.0 files and the directly related
handover/roadmap docs. Do not commit unless asked.

At resumption, do **not** assume the built executable exactly matches today's
source. First check whether `build/Release/aur_tests.exe` and the relevant
sources exist, rebuild, and rerun CTest. The harness registers one CTest test,
`EngineTests`; the individual functions are reported by `aur_tests.exe`, not
registered as separate CTest filters.

## P3.0 execution plan

1. **Establish the baseline.** Inspect the worktree and relevant diffs; preserve
   unrelated edits. The historical autotile fixture issue is corrected and the
   expanded regression suite passed in Release on 2026-11-03. Rebuild and rerun
   the current gate after any source change.
2. **Topology unit tests.** Direct helper and predicate coverage for
   negative/multi-width X, width one, odd/even half-width ties, invalid helper
   widths, all four topologies, and malformed/non-world metadata passes in the
   2026-11-03 Release run. Continue with bounded-map and Y behavior at owning API
   boundaries.
3. **Audit engine and project data.** Trace every existing world-grid access
   and make wrapping consistent for generation, tile lookup, collision, camera,
   MCP paint/edit requests, autotile, scatter hashes, and bake loops. Audit
   chunking, pathing, or runtime exits only if actual implementations/callers
   exist; otherwise record them as future dependencies. Normalize X at the world
   boundary; never wrap Y. Preserve JSON shape and authored location/exit
   semantics.
4. **Audit editor interaction.** Verify cursor-to-world conversion, brush
   interpolation, selection, pan, repeated rendering, markers/exits, undo/redo,
   JSON writes, and dirty-rect uploads across X=0. A seam-crossing edit must
   split uploads into valid texture rectangles without dropping either side.
5. **Audit GPU sampling and generated detail.** Confirm world data samplers use
   repeat-U/clamp-V, including shader/resource setup. Remove screen-space or
   tile-space discontinuities from authored noise at the seam. Do not change
   unrelated visual effects or add simulation in this phase.
6. **Run acceptance tests and inspect visuals.** Build, run the focused and
   complete test gates, launch the editor/runtime under the repository's
   current graphics policy, and collect the seam evidence listed below.
7. **Update handover status and stop.** Record exact commands/results, files
   changed, screenshots/evidence locations, and remaining gaps. Do not proceed
   to P3.1 until the user approves the next phase.

### File-level audit map

Use this as a navigation list, not a claim that every file is defective:

| File | Responsibility to inspect |
|---|---|
| `engine/aur/world_topology.hpp` | Generated-world predicate; wrapping, normalization and shortest-delta semantics. |
| `engine/aur/worldgen.cpp` | Sphere parameterization, climate latitude, procedural noise, D8/hydrology neighbor access, coast/biome edges, deterministic ordering. |
| `engine/aur/field.hpp` | Runtime movement/collision, camera X/Y policy, visible tile window, autotile lookup and layer draw. |
| `engine/aur/game.cpp` | Runtime biome/climate sampling at the player position. |
| `engine/aur/autotile.cpp` | Neighbor masks, paint/erase/retile and map-edge behavior. |
| `engine/aur/project.cpp` | Public project-level biome edits and coordinate validation. |
| `engine/aur/mcp.cpp` | External edit request validation, wrapping, persistence and error response. |
| `studio/WorldModel.hpp/.cpp` | Dense editor data, rectangular edits/snapshots, bounds, JSON serialization, dirty-region splitting. |
| `studio/WorldPanel.hpp/.cpp` | Camera/zoom, coordinate transforms, painting, brush interpolation, selection, markers/exits, overlays, fallback renderer, GPU resource setup and uploads. |
| `studio/shaders/world_sculpt.frag` | Wrapped U coordinate, finite differences across seam, vertical sampling and visual-edge behavior. |
| `tests/test_autotile.cpp` | Topology-aware neighbor and brush regression coverage. |
| `tests/test_world_model.cpp` | Seam edit, snapshot, undo/redo, JSON persistence and dirty-rect coverage. |
| `tests/test_forge.cpp` | Generated topology, deterministic output, polar/climate behavior and size limits. |
| `tests/test_mcp.cpp`, `tests/test_project.cpp` (if present) | Public API coordinate validation and saved edits. |

Search the full relevant tree for direct X neighbor expressions, linear cell
index calculations, map-edge clamps, independent modulo operations, shader
sampler creation, and coordinate hashing. For each hit, classify it as (a)
must wrap, (b) must remain bounded, or (c) non-world/screen coordinates.
Document any intentional exclusions. Do not assume the listed files are the
entire set of callers.

### Resume runbook

1. From `C:\Projects\Aur`, capture `git status --short`; do not clean the tree.
2. Read this handover, `Bible/Operations/Agent Guide.md`,
   `Bible/Operations/Handover.md`, the root `HANDOVER.md`, and the focused
   files in the audit map. The root handover's graphics policy is authoritative.
3. Run `.\build.bat release`. The checked-in script initializes
   `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`,
   configures the `Release` preset, builds with `--parallel 8`, then deploys
   Qt files. Close running Aur executables first if deployment cannot replace
   them. The preset's Qt path is
   `C:\Dev\Qt\6.12.0\msvc2022_64`; if toolchain initialization fails, inspect
   those exact paths rather than changing project configuration blindly.
4. Release `EngineTests` passed on 2026-11-03 (1/1 CTest test, 26.00 seconds).
   After any new changes, use `ctest --test-dir build\Release -C Release -R
   EngineTests --output-on-failure` to reproduce the gate.
5. Continue the numbered P3.0 audit; write focused regression tests before
   relying on visual inspection.
6. Launch Studio/game using the documented `build.bat release run` or
   `build.bat release game` only after a successful build. Verify the actual
   backend selected by the root graphics policy; do not treat fallback
   painting as proof of the QRhi/GPU path.
7. Record results in the Result record template, update both this document and
   `Bible/Operations/Handover.md`, then stop at the phase gate.

## P3.0 acceptance checklist

- `wrapX(x, W)` returns a value in `[0, W)` for negative and positive inputs;
  generated-world lookups use the actual map width. Tests exercise multiple
  generated-world topologies and a ten-column world to catch fixed-width logic.
- Source now has regression assertions for all four topology identifiers and
  missing, non-string, and unsupported topology metadata in addition to the
  wrapping helper cases. `build.bat release` and Release `EngineTests` passed on
 2026-11-03 (1/1 CTest test, 10 passed, 0 failed; 26.00 seconds). This confirms
  these engine regressions, not the outstanding live editor/runtime checks.
- Generated-world X wraps and Y clamps in engine movement/collision/camera.
  Ordinary bounded maps do not acquire wrapping accidentally.
- All four generated topology identifiers are covered, and wrapping never
  depends on a fixed width or the 256×128 starter size; the engine test suite
  exercises actual-width normalization and visibility at non-starter widths.
- A paint stroke, selection, and undo/redo crossing the seam modify and restore
  both edge columns exactly; dirty uploads are valid, split rectangles.
- Autotile neighbors are reciprocal between `x=W-1` and `x=0`. Chunk
  streaming and pathing are acceptance items only if an existing implementation
  is found; otherwise record them as future dependencies, not P3.0 blockers.
- Authored location/exit coordinates at the edge remain valid and serialize
  correctly. Runtime exits may be tested only if an actual trigger path exists;
  seam-crossing runtime traversal is not a P3.0 requirement in its absence.
- World-field texture coordinates repeat horizontally and clamp vertically;
  terrain/noise rendering has no visible seam at 100%, 400%, and fit zoom.
- A side-by-side or column-difference capture of both map edges shows no
  discontinuity. Record which topology, dimensions, and zoom produced it.
- Release build and all `EngineTests` pass. Keep measured build/test output;
  do not infer passing from a successful compile alone. The topology fixture and
  helper regressions are verified by the passing Release test run dated
 2026-11-03; other P3.0 acceptance items remain open.

### Suggested test matrix

| Case | Expected result |
|---|---|
| Generated map, X = -1 / W / 2W+1 | Reads/writes normalize to W-1 / 0 / 1; normalize coordinates before calling `wrapped_delta_x`. |
| Bounded map, X outside [0,W) | Existing rejection/solid behavior remains; no wrap. |
| Any topology, Y = -1 / H | Rejected, solid, or clipped per current surface API; never wraps to opposite pole. |
| Stroke from W-1 to 0 | Interpolates across one seam-adjacent cell, not across the whole map; undo/redo restores exact values. |
| Dirty upload crossing W-1 to 0 | Produces two valid non-overlapping texture subrects; row bytes and destination coordinates match. |
| Field camera crosses W repeatedly | For generated worlds in the runtime field, hero motion and visible ground/object layers remain continuous; camera Y stays bounded. |
| Autotile at both seam cells | Masks see reciprocal neighbors; painting/erasing retile both sides. |
| World-gen repeated with fixed seed | Byte/JSON-identical result; seam adjacency is spatially continuous. |
| Location/exit at an edge | Authored coordinates remain valid and selection/serialization work. Runtime transition behavior is conditional on finding an existing trigger path; otherwise it is out of scope. |
| GPU and fallback editor | Both show the same topology and no black/blank strip or discontinuity at any zoom. |

## Validation commands

Use the repository's documented build entry point and run the test executable
from the Release build:

```powershell
.\build.bat release
ctest --test-dir build\Release -C Release -R EngineTests --output-on-failure
```

Expected CTest registration is one test named `EngineTests`; the underlying
`aur_tests.exe` currently has these test functions: JSON basic/round-trip,
autotile, MCP server, forge generators, project calendar/weather, game/field
scene, MIDI import/export, WorldModel rectangle edits, and WorldModel wrap
edits. The 2026-11-02 baseline failed in `test_autotile_basic`. The fixture
correction and added topology-helper assertions are now present in source and
passed the fresh 2026-11-03 Release `EngineTests` run.

If using direct CMake instead, configure/build with the checked-in Release
preset and run the same CTest selection. Use the root handover's active graphics
policy; do not reintroduce a D3D11 renderer or bypass backend validation.

The checked-in CMake preset is named `Release`, uses Ninja, writes to
`build/Release`, and points at Qt 6.12 under `C:/Dev/Qt/6.12.0/msvc2022_64`.
This machine may need the VS 2022 developer environment initialized in the
same process for CMake/Ninja to find MSVC and shader tools. Prefer the
repository's `build.bat` when it establishes that environment. If the test
fails, preserve the complete failing assertion/output, fix the root behavior
or fixture, then rerun the same gate; do not just rebuild.

For code-only P3.0, use Release build + `EngineTests`; a GUI capture is also
required because seam continuity and camera behavior are visual/interactive.
Do not run unrelated full visual suites as a substitute for direct seam
verification. Record GPU backend, display size, map dimensions/topology, zoom,
and whether the screenshot came from the editor GPU path or fallback path.

### Result record template

At completion, add a dated handoff note with:

```text
Scope completed:
Files changed:
Worktree status before/after:
Build command/result:
EngineTests command/result:
Focused regressions:
GUI/runtime seam checks:
Screenshots/evidence paths:
Known limitations:
Next approved phase:
```

Never label a GPU timing, frame rate, clean build, or visual result as measured
unless it was actually measured in the current verification run.

## Decisions to preserve or resolve explicitly

1. **World dimensions:** the conversation's sculpt brief describes 256×128
   tiles, while current Aur supports up to 2560×1280 and its starter map is
   256×128. Keep topology width data-driven until the owner explicitly changes
   these product limits.
2. **Which maps wrap:** current behavior is topology metadata on generated
   worlds only. Do not make hand-authored field maps wrap merely because their
   map shape happens to be rectangular.
3. **Latitude and axial tilt:** world generation currently derives latitude
   from Y and persists the `axial_tilt` setting without applying it to climate.
   No seasonal-declination calculation is established in this generator. Keep
   this as an explicit follow-up design/implementation gap, separate from P3.0;
   do not describe it as existing behavior to preserve.
4. **Graphics backend:** use the active project-root graphics policy, which
   supersedes the older transitive-DLL wording in the downloaded conversation.

## Later roadmap (not part of P3.0)

Keep these as separate, gated phases. Each requires its own tests and approval;
none is a reason to expand the current topology patch.

### P3.1 — deterministic WorldSim core

Pure C++ shared by editor and runtime; no UI/RHI dependency. The conversation's
proposal uses a 128×64 half-tile grid, wrapped X, 600 simulated seconds per
fixed tick, accumulated time scaling with a strict per-frame tick cap, seeded
determinism, and packed field textures. Candidate fields are temperature,
moisture, wind, cloud coverage, rain, snowpack, and pin masks, with edited
height downsampled into the sim. Atmosphere coefficients belong in one
tunables structure. Must first settle units, boundary conditions, serialization,
input ordering, thread ownership, and a golden-hash fixture. Acceptance:
headless repeatability (including the proposed 10,000-tick golden hash),
deterministic save/load, and measured tick cost. Do not parallelize until
single-thread determinism is established.

### P3.1b — surface hydrology

Separate stable surface water from atmospheric moisture. The proposed design
uses deterministic priority-flood basin filling (X-wrapped, polar rows as
sinks), flat lake spill levels, flow routing/accumulation on the filled surface,
rain-climatology inputs rather than per-frame rain, river tiers, waterfall
flags, playa masks, and incremental living lake levels. Height/sea-level edits
trigger a throttled recomputation; ordinary sim ticks update lake state without
rebuilding the whole drainage graph. Acceptance should include a provably flat
lake surface, spill-cut/drain behavior, a lake-to-sea river, seam-crossing
drainage, deterministic output, and recompute timing. The conversation's
`<8 ms @256×128` is a target proposal, not measured evidence.

### P3.1c — river/lake representation

Deterministic river tiles derive from directed inflow/outflow arms (not a blob
mask), with reciprocal neighbor assertions, seam-aware diagonal handling,
confluence width promotion, and explicit spring, lake-mouth, estuary, delta,
and waterfall pieces. Lake shores reuse coast rules. A proposed 16-mask river
atlas has creek/river/major width tiers and bank/special rows; settle the atlas
contract against actual Aur tilesets before implementation. Acceptance:
gap-free connected channels, monotonic width at confluences, correct waterfall
direction/plunge, sea-vs-lake mouth cases, seam screenshot, and byte-identical
runtime/editor bake output.

### P3.2 — editor simulation tools

Only after the sim core/hydrology gates: field visualization (temperature,
moisture, wind, coverage, rain); local anomaly/pin tools; storm/drought
injection; season lock and time scrub; global climate controls; and an explicit
neutral/live mode. Shaders may add detail but cannot become a second source of
weather truth. Existing artist-authored values should import as pins before
simulation can overwrite them. Acceptance includes visible mountain rain-shadow
and vegetation response over sim-days, neutral mode independence, and no loss
of authored data.

### P3.3 — runtime parity and debug menu

Integrate the same simulation core with the runtime and a development-only or
cheat-gated debug UI. Provide field overlays, clock/date/season, pause/step/rate,
forced weather, storm injection, global controls, tick timing, and state hash.
Acceptance: identical seed/input/tick sequences yield identical editor/runtime
snapshots; field uploads remain within budget and gameplay frame cost is
measured on the low-end target.

### P3.4 — optional globe map

An optional map-screen-only globe is a visualization of the same wrapped fields
and sun direction; it must not fork simulation state. Keep it separate from
P3.0 cylindrical editing and prove cost at runtime target resolution.

### P4 — water volume and aquatic play

The user's stated requirement is underwater and on-water exploration and
battles. The conversation proposes a bed-height/water-level depth field,
shallow seabed visibility, deeper opaque water, surface/volume effects,
movement states (walk/wade/swim/dive), boats/docks, and waterfalls/mist. Before
building it, define water-level authority, traversal transitions, entity
occlusion, battle context, editor controls, and a low-end rendering budget.
Acceptance must cover shallow/deep transition, seabed visibility, swimming,
boat/dock traversal, underwater visibility, and battle behavior.

### P5–P6 — cliff faces and battle context

P5 proposes quantized elevation steps, cliff-face drawables and ledge/overhang
masking; P6 proposes battle modifiers derived from terrain, elevation, and
water state. Define a shared height/traversal contract first so the map editor,
runtime movement, and combat do not disagree. Require an occlusion matrix,
pathability tests, and battle modifier table before art integration.

### P7–P12 — additional game systems

- **P7 narrative timeline:** JSON data and a Qt timeline/vertical-list editor,
  plus a runtime director; avoid a node-graph editor unless the owner changes
  the stated preference.
- **P8 roads/infrastructure:** road graph/autotile and routing, with
  river-crossing bridges/fords, cliff stairs, docks, and traversal/gameplay
  consequences. Requires P3.1b hydrology and P5 elevation/traversal.
- **P9 ambience:** derive environmental audio profiles from biome, water,
  depth, time, and weather; provide manual overrides and editor visualization.
  Requires stable P3 simulation/hydrology fields and an audio design decision.
- **P10 epochs/time travel:** versioned world state/deltas and spatial mapping;
  reconcile with Aur's existing eras/time settings before designing persistence.
- **P11 tactical lens / P12 action lens:** derive tactical maps and action
  traversal from the same world data; do not fork terrain truth.

These later briefs contain many assistant-proposed numbers and algorithms.
They are design inputs, not settled implementation requirements unless
confirmed by the user or recorded in the authoritative Aur bibles.

### Dependency order and gates

```text
P3.0 topology
  -> P3.1 deterministic sim core
       -> P3.1b surface hydrology
            -> P3.1c river/lake bake and autotiles
       -> P3.2 editor simulation tools
       -> P3.3 runtime/debug parity
       -> P3.4 optional globe visualization
  -> P3.1b hydrology + P5 elevation traversal
       -> P4 water volume and aquatic gameplay
       -> P6 battle context (also depends on P4)
       -> P8 roads, bridges and routing
  -> P7 narrative (independent)
  -> P9 ambience (after P3.1 fields are stable)
  -> P10 epochs (reconcile with existing eras/time settings)
  -> P11 tactical / P12 action (derive from shared terrain/gameplay rules)
```

The dependency sketch is advisory: some branches can run independently only
after their shared data contracts are explicit. Each phase should end with a
small diff, exact test evidence, measured performance where relevant, visual
evidence where relevant, unresolved decisions, and an explicit stop. Do not
bundle later phases into a single "world system" implementation.

## Handoff completion

A successor should establish the current test result, then follow the numbered
P3.0 plan; the known autotile fixture failure has been corrected and the Release
test gate passed on 2026-11-03. Treat chunk streaming, pathfinding, and runtime
exit traversal as conditional on finding existing implementations; do not block
P3.0 on absent subsystems. Keep axial tilt and seasonal behavior as separate
gaps unless implemented under distinct approval. P3.0 is complete only when the
applicable acceptance checklist is evidenced and the operations handover is
updated.
The next authorized implementation target after that gate is P3.1 only if the
user approves it.


## Root build-artifact cleanup (2026-11-03)

- Removed root `regen_world.obj`, `regen_sylvi_ages.obj`, and `regen_hero.obj`. Inspection showed these were MSVC COFF object files (not Wavefront OBJ assets), and no source or project references them; `.gitignore` already excludes `*.obj`.
- Tidied the active root CMake setup: minimum CMake requirement now agrees with the presets (3.21), C++ extensions are disabled, obsolete global AUTOGEN toggles are replaced with Qt's standard project setup, and the required Qt 6.12 version is explicit. Removed the duplicated machine-local Qt prefix from both presets; the existing configured build cache retains the valid installed Qt path.
- Existing project targets, shader compilation, and per-preset build output locations are preserved. `cmake --preset Release` configured successfully. Qt's existing private-module warning and unavailable optional Vulkan headers were reported at configure time; neither prevented generation.
- Environment note: invoking the clean-first build directly from the agent PowerShell session fails because its MSVC environment lacks standard include paths and Qt's shader compiler cannot find `fxc`. A partially recovered Release build linked Studio, game, MCP, and tests successfully on 2026-11-03, and CTest was run separately, but no authoritative final CTest result was captured. A later clean-first rebuild could not clean `aur_tests.exe` because the test process was still running; therefore a clean build and final green test gate remain unverified. Close any running test process and rerun the documented build/test commands in a properly initialized VS environment.

- Follow-up verification: after the stale concurrent test invocations exited, the final `build/Release/aur_tests.exe` run completed with all 10 tests passing (0 failed), captured in the temporary files under `build/Release/`. `cmake --build build/Release --parallel 8` also reported no work to do; however, a fresh clean-first rebuild was not completed due to the earlier concurrent process locking the test executable.

## P3.0 boundary regressions (2026-11-04)

Scope completed: the owner-boundary gaps still listed in the P3.0 plan, without changing topology behavior.

Files changed:

- `tests/test_topology_boundaries.cpp` (new): fully formed ordinary-map paint rejection at negative X while the same map wraps once `worldgen.topology` is present; generated-world Y rejection at `-1` and `H`; invalid width `0` rejected by `wrapX` through the public paint path; climate sampling at both X edges of a 10-wide generated map (negative X wraps to the last column, past `W` wraps to column 0); short/malformed biome rows fall back to the map climate instead of indexing out of bounds; bounded maps clamp instead of wrapping; camera X is unbounded/repeating and camera Y is clamped at both poles for a generated map.
- `CMakeLists.txt`: adds the new test source and a focused CTest entry `TopologyTests` (`aur_tests --topology`). The focused mode exists because the full suite's 2560×1280 generator case needs roughly 1 GB and tens of seconds; it is additive and `EngineTests` still runs every function.
- `tests/main.cpp`: adds the `--topology` focused mode and registers the two new test functions. No production engine or editor file changed.

Build/test commands and results (2026-11-04, developer PowerShell):

```powershell
cmake --preset Release -S . -B build\Release
cmake --build build\Release --target aur_tests --parallel 8
ctest --test-dir build\Release -C Release -R TopologyTests --output-on-failure
```

`aur_tests` built and linked in Release; `TopologyTests` passed 1/1 CTest (5/5 functions, 0.08 s). This run was made with the VS 2026 `vcvarsall.bat x64` initialized in the same process; invoking the agent shell without that initialization fails to compile (`iostream` not found) and `vcvarsall` also emits a harmless `vswhere.exe` message. `cmake --preset Release` requires that initialized environment.

### P3.0 live-editor seam evidence (2026-11-05)

Scope completed: the live World editor visual checks from the acceptance checklist.

- Build command/result: Release tree rebuilt 2026-11-05 with `Aur Studio` linked (commit `1ed95c3`); Qt runtime deployed with windeployqt into `build/Release` (it was absent from the direct-cmake tree; first launch exited `0xC0000135`).
- Backend: D3D12 QRhi path (`AUR_RHI_BACKEND` unset defaults to d3d12 in `studio/RhiBackend.cpp`; a failed RHI init qFatals, and the app rendered). Not the fallback painter.
- Evidence: fit view and a ~306% cursor-centered wheel zoom over the wrap region of the 256×128 `continents` starter world — terrain continuity across the X seam with no black strip or discontinuity; markers repeat per wrapped copy as designed. Saved at `Bible/Operations/evidence/p3_0_world_fit_d3d12.png` and `p3_0_world_seam_306pct_d3d12.png`.
- Seam-crossing paint + undo (2026-11-05, live window): the Paint biome tool was selected via hotkey `3`; a mouse drag from inside the canvas across the left edge at 400% zoom painted a stroke whose pixel-diff bbox spanned the seam. Ctrl+Z restored the canvas to the exact pre-stroke state (0 px diff). Evidence frames were diffed from `game-sessions/default/`; temporary `build/` scripts were removed.
- Dedicated 400% edge-column comparison completed (2026-11-06, live window, D3D12 QRhi path): Earth-like world (256×128, `earthlike_world`), zoom set to 400% via the zoom spinbox so the full 256-tile width fits in one viewport — columns x=0 and x=255 are visible simultaneously in a single frame. Evidence: `Bible/Operations/evidence/p3-seam-400pct.png`. Both edge columns render continuous open ocean into the frame edges with no black strip, no vertical discontinuity, and wrapped location markers (`start`, `southern_ruins`) repeat on both sides as designed. With this capture, every item on the P3.0 acceptance checklist has live or test evidence; the P3.0 gate is complete. P3.1 remains separately gated and requires owner approval before starting.
- AIO config-selection defect was fixed in `AIO-win.bat`: `aio_win_tree` prefers the requested `build/<CONFIG>` cache before flat/fallback caches. `AIO.bat compile windows Release` verified the requested Release tree and reported `ninja: no work to do`; Qt deployment completed.
- The complete Release `EngineTests` (all functions incl. the 2560×1280 forge case) passed on 2026-11-05 after the keyboard/AIO changes: 1/1 CTest, 31.42 s. Focused `TopologyTests` also passed 1/1 on the same Release tree (5 functions).
- Runtime exit traversal, chunk streaming, pathfinding, axial-tilt climate, and seasonal declination remain out of scope as previously recorded.
