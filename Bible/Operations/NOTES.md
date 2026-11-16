# Operations Notes
## Project tidy audit (2026-11-06)

- Moved the original Sea of Stars map-size conversation into `Bible/Archive/Reference/`; updated its topology-handover link. The Bible root now contains only its index.
- Removed the empty duplicate `Bible/Puzzle & Time/` directory; `Bible/Time and Puzzles/` is the populated topic directory.
- Confirmed tests are grouped under root `tests/`; no `work/` or `imagegen/` directories exist in the inspected project tree.
- Kept ignored `build/` output in place; it contains active build products and diagnostic images/logs and is not source content.
- Tidy ask #50 remains open: the broader Operations/root audit is incomplete. The root handover still carries the active graphics-backend policy and should not be dropped as stale.


## WorldPanel refactor audit (2026-11-06)

- `WorldPanel.cpp` starts at 3,100 lines; the header has a duplicate `<memory>` include and relied on transitive Qt widget includes. Header cleanup now keeps value-type Qt includes, forward-declares pointer/member-event types, and includes the standard containers/types it names.
- Baseline `cmake --build build/Release --parallel 8` and `ctest --test-dir build/Release --output-on-failure` passed before edits. Use `cmd /c ".\\AIO.bat build windows Release"` under this shell to load Visual Studio's compiler environment; direct cmake build fails because the shell lacks MSVC standard include paths.
- After the header cleanup, the project build exposed missing direct implementation includes (`QPainter`, widget definitions, and `QFutureWatcher`); adding those to `WorldPanel.cpp` restores a successful full build.
- Existing `studio/world/MapView.*` / `WorldMapHelpers.*` files were already untracked at task start, so keep them out of unrelated commits while integrating them intentionally in the refactor.
- Runtime `WeatherSystem` is not a direct replacement for the editor's deterministic `climate_weather(project, climate, roll)` preview selection; preserve the editor behavior unless a matching shared selector is added and tested.
- The persistent plan is `WorldPanel Refactor Plan.md (deleted on landing)`; the first implementation checkpoint is header/include hygiene. The required `buildHdSurface` checksum still needs a testable Qt image-builder seam before completing step 1.

- Added Aur request #51 to `WANTED.md` to keep the full WorldPanel refactor visible; its durable sequence and honest completion checklist are in `WorldPanel Refactor Plan.md (deleted on landing)`. Header cleanup is committed, but the checksum/surface extraction and steps 2–7 remain open; do not close the request until acceptance is tested.

- WorldPanel step 1 moved the complete HD surface pixel-generation body to `studio/world/HdSurface.cpp`; the public operation accepts map, project and region, returns a cropped image only for a non-null region, and preserves the prior full-map output for a null region. A 32×16 seeded generated world yields stable RGB32 FNV-1a checksum `7986464436153998528`. Both full Release build and both CTest targets passed.
- WorldPanel tool state and entity-placement dispatch now use `WorldTool` with the ordered `kWorldToolNames` table; pixel/map behavior was not changed. Typed-tool changes pass Release build and both CTest targets. Pending checkpoint commit before continuing.
- World tab now sits immediately after Map Editor (commit `8ed26ab`), matching the requested tab-view placement.
- Step 3 integrates existing helper implementations into the panel and adds defensive `MapView::elevationAt`/`biomeAt` accessors plus test coverage. Camera bounds/fit/tile hit testing now gets dimensions, tileset identity and wrapping from MapView. Surface rendering itself remains isolated behind its extracted API. Release build and CTest passed; this step's commit is still pending.
- Terrain renderer extraction is the current active checkpoint, not complete: an explicit `TerrainView`/provider API has been drafted in an untracked `studio/world/TerrainRhiWidget.hpp`, but the QRhi implementation and canvas provider/removal of friend access have not yet moved. Keep step 4 open; do not stage this header alone. Existing unrelated dirty-tree changes (`AGENTS.md`, topology handover and archive relocation) remain untouched.

- Step 4 complete (commit `636b69f`): `TerrainRhiWidget` now lives in `studio/world/TerrainRhiWidget.{hpp,cpp}` and consumes a `TerrainView` snapshot through a provider callback instead of friend access to `WorldMapCanvasWidget`. `paintCanvas` is temporarily public so `WorldMapOverlayWidget` can drive it without friendship; step 5 replaces that with overlay state. WorldPanel.cpp dropped to 2,266 lines. Build and both CTest targets green.

- Step 5 progress: overlay toggles unified under `WorldOverlay` + `setOverlay(WorldOverlay, bool)`; weather rendering now lives in `studio/world/WorldWeatherOverlay.{hpp,cpp}` taking a `WeatherOverlayContext` (temperature/biome rows, bounds, seed, tick, weather string, ocean predicate callback). Weather is the first extracted overlay; remaining overlays still render inline and are tracked in the plan. `WorldPanel.cpp` is 2,085 lines (from 3,100 at audit). Build and both CTest targets green; weather commit `78a14f8`.

- Step 5b (commit `2bf84bf`): temperature shading, climate axes, wind/current flow vectors, and the day/night CPU-fallback tint now render from `studio/world/WorldClimateOverlays.{hpp,cpp}` via context structs. Overlay composition order and the headless fallback condition are unchanged. WorldPanel.cpp down to 1,938 lines; build and both CTest targets green. Remaining overlays: entity markers, hover highlight.

- Step 5 complete (commit `fdb05f1`): all overlays now render through extracted modules — `WorldWeatherOverlay`, `WorldClimateOverlays`, and `WorldMarkerOverlays` (entity markers, hover highlight, zoom label). `paintCanvas` is an ordered overlay pipeline; draw order and the day/night CPU fallback are unchanged. WorldPanel.cpp is 1,877 lines (from 3,100). Build and both CTest targets green.
- Fast worldgen confirmation from the owner: new seeded worlds generate near-instantly — no regression from the seams introduced so far.

- Step 6a (commit `a3a2a12`): camera math (zoom clamp, bounds, wrap-aware pan constrain, tile hit-testing, fit) now lives in `studio/world/WorldCamera.hpp` as pure functions over a `WorldCameraSpec`; the canvas builds the spec from `MapView` plus widget viewport. Behavior unchanged; build and tests green. Next: project-level command history (step 6b), then the press/drag/release tool interface (6c).

- World editor text mojibake fixed (`0b7506b`): UTF-8 multibyte chars (×, ·, …, °, →) in `WorldPanel.cpp`/`WorldMarkerOverlays.cpp` string literals were corrupted in source; replaced with `\uXXXX` escapes inside `QStringLiteral`. Rule going forward: any non-ASCII character in Aur C++ source string literals must be written as a `\u` escape, never as a raw multibyte character.

- Step 6b complete (commit `63bd21d`): project-level command history moved to `studio/world/WorldCommandHistory.{hpp,cpp}` — `Action` (entities flag, Json state, model rect snapshots), 100-entry limit, redo cleared on new record (matches old vector semantics); `undo()`/`redo()` now take/put actions via `take_undo`/`push_redo`/`take_redo`/`push_undo`. Added `test_command_history_roundtrip` and `test_command_history_limit` to both CTest suites. WorldPanel.cpp holds at 1,857 lines (net: history state removed from the widget, apply logic stays until step 6c). Build and both CTest targets green. Note: `aur::Json` has no `operator==`; tests compare via `as_int`/`as_string`. Untracked world headers reformatted by clang-format in the tree (`HdSurface.hpp`, `MapView.hpp`, `WorldMarkerOverlays.hpp`, `test_map_view.cpp`) were restored to HEAD before committing so the commit stays scoped; their formatting change remains a pending cleanup, not part of this step.

- Step 6c complete (commit `0e88559`): press/drag/release stroke contract moved to `studio/world/WorldStrokeState.{hpp,cpp}`. `WorldStrokeState` owns begin/end bookkeeping (active, last tile, captured cell set, captured model rects); `stroke_line_tiles(from, to, wrap_width)` reproduces the drag interpolation byte-for-byte — legacy behavior passes raw x offsets and lets `paintTile` clamp/wrap, so the extracted helper must too (verified with a one-off MSVC probe before finalizing the test; expectation `(0,0)/(1,0)` was wrong, `(10,0)/(11,0)` is correct for from=9→to=1 on width 10). Added `test_stroke_line_tiles`. WorldPanel.cpp at 1,842 lines. Build + both CTest targets green. Probe scripts cleaned up after use.

- Step 7a complete (commit `14f6314`): `WorldGenerationJob` extracted to `studio/world/WorldGenerationJob.{hpp,cpp}`. It owns the `QProgressDialog`, the cancel flag and request/source-project staleness checks; the panel constructs it once in its constructor and consumes `generationFinished`/`generationFailed` signals, preserving the exact acceptance condition (newest request + same project + not cancelled). Panel keeps naming/uniqueness and dialog UI, passes a `WorldGenerationParams` value. QtConcurrent/`QFutureWatcher`/`atomic_bool` includes left `WorldPanel.cpp`; it is 1,816 lines. Build + both CTest targets green. Note: the class is `Q_OBJECT` and lives in `aur::world`, so its moc requires the QtConcurrent-linked aur_studio target (wired in CMakeLists).

- Step 7b complete (commit `fc1d8ab`): entity dialog construction extracted to `studio/world/WorldEntityDialogs.{hpp,cpp}`. `run_add_exit_dialog` keeps the "project has no target maps" warning inside the dialog helper (returns nullopt); edit dialogs return raw entered values (`WorldLocationEdits` / `WorldExitEdits`) and the panel retains ID normalization (`make_biome_id`), duplicate-ID rejection and `recordEntityEdit()` ordering so undo semantics are unchanged. `make_exit_entity` builds the identical Json payload (effect fade, h/w 1, target map/spawn). Note: `connect` in a non-QObject plain namespace needs explicit `QObject::connect` (WorldEntityDialogs functions are free functions). WorldPanel.cpp at 1,754 lines; build + both CTest targets green.

- Owner decision (2026-11-06): general file sizing rule set to a soft cap of ~1,000 lines with a 25% overhead allowance (hard ceiling ~1,250), replacing the 800-line target for this refactor's acceptance. At 1,754 lines `WorldPanel.cpp` still exceeds even the hard ceiling, so a canvas-widget split (step 7c) is still required; after it, remaining files should be trimmed toward the 1,000-line soft cap where practical.

- Step 7c complete: `WorldMapCanvasWidget` extracted to `studio/world/WorldMapCanvas.{hpp,cpp}` (128 + 868 lines). The overlay helper widget lives in the .cpp next to the canvas (it needs only the public `paintCanvas`, no friendship). `WorldPanel.hpp` now contains only the `WorldPanel` class (65 lines) and includes the canvas header; `MainWindow.cpp` needed no changes (`findChild<WorldMapCanvasWidget*>()` still resolves). Panel class implementation remains in `WorldPanel.cpp`, now 917 lines — inside the owner's 1,000-line soft cap. Build + both CTest targets green. MOC pitfall noted: moving a `Q_OBJECT` class between headers requires the target's AUTOMOC to pick up the new header path (automatic here since it's under a listed source's include graph).

- Ask #50 closed (2026-11-06): root audit completed — the Sea of Stars map-size reference relocation committed, stray root objects (`NUL.obj`, `WorldStrokeState.obj` MSVC leftovers) removed, `WANTED.md` trimmed to two closed entries with no open asks. The completed `WorldPanel Refactor Plan.md` was deleted per estate rule (git history retains it); both asks recorded under Closed with verified evidence.
- Sizing rule from the owner, noted for all future work: soft cap ~1,000 lines per file with a 25% overhead allowance (hard ceiling ~1,250), replacing the earlier 800-line target.
- Operations is now clear of completed/refactor material: the WorldPanel plan is gone, and the handover no longer references it. Remaining documents are active: Agent Guide, Handover, Roadmap, MCP Server, World Map Topology Handover, NOTES.

- P3.0 gate complete (2026-11-06): the last acceptance item — the dedicated 400% edge-column comparison — was captured live on the D3D12 QRhi path. At 400% zoom the full 256-tile Earth-like world fits in one viewport, so columns x=0 and x=255 render in the same frame (`Bible/Operations/evidence/p3-seam-400pct.png`): continuous ocean across both edges, no black strip, wrapped markers repeat correctly. Captured by launching Studio via the game-development skill (window activation + click into the World tab + zoom spinbox entry); evidence saved into the tracked `evidence/` folder. Handover updated; P3.1 remains owner-gated.

- P3.1 WorldSim core complete (2026-11-06), plan at `Bible/notes/worldsim-p31.md`: `engine/aur/worldsim.{hpp,cpp}` (84 + 340 lines, Qt-free, `aur::worldsim`). Seven fields (temperature, moisture, wind u/v, cloud, rain, snowpack) over a wrap-aware grid; 3×3 diffusion, upwind cloud advection, latitude-driven wind relaxation, precipitation and snowpack kernels; double-buffered with pins honored everywhere. Determinism proven by run-to-run and batched-vs-stepped 10,000-tick hash equality on a 32×16 seeded world; save/load round-trip hash-equal (seed/ticks stored as 16-hex-digit strings because `aur::Json` has no 64-bit integer type — do not switch back to numeric without widening). Measured cost: 16 µs/tick at 32×16, ~1.1 ms/tick at 256×128. `WorldSimTests` added as a third focused CTest entry. Commits: `d0425e8` (P0 shell), `9f207a6` (P1 kernels), `6d92e14` (P2 determinism). P3.2 (editor simulation tools) is next and owner-gated. Deviations recorded in the plan: tile-resolution grid instead of half-tile; golden hash by equality rather than a literal constant.

## DAW initiative (2026-11-07)

- P3.4 (`e92bfd5`): SongHistory wired into PianoRollCanvas/MusicPanel. Note edits group per drag stroke (begin_note_edit on press, commit on release); right-click/Delete, add, move, resize are all undoable; Ctrl+Z/Y handled on the canvas. `commit_track_change` was rewritten: it previously snapshotted `before` AFTER mutation and never set `after`, making undo a silent no-op — now `begin_track_edit()` snapshots pre-mutation and commit pairs post-mutation. `updateTrackProperty` pushes into project->music on every change (tests assert undo of a props edit restores both notes and missing keys).
- P3.5 (`2fbeb92`): `SongDocument::note_velocity/set_note_velocity/note_props/set_note_prop/clear_note_props` — note stays `[step,pitch,length,velocity]` with optional 5th object slot for extra per-note data; velocity clamped 0..127, default 90 for v1 short notes. Piano roll: Alt+drag vertically edits velocity, notes render velocity-shaded (darker = quieter). Tests in `test_song_history.cpp::test_note_props`.
- P3 remainder (automation lanes, arrangement clips) moved into P6 where their editors live — do not build them under P3.
- Build pitfall encountered: an earlier edit to `SongDocument.hpp` left a namespace imbalance (`namespace aur::audio` opened, two closers) that surfaced only as a cryptic MSVC syntax error at the file's last line. When a header edit reports success but the compiler points at `}` lines, diff the file against git before hunting semantics.
- Frontier-model synth cores (YmFmCore, SnesPcmCore/BrrCodec/SnesEcho) expected as portable C++20 drops under `engine/aur/audio/` per WANTED.md; on delivery integrate behind VoiceCore/RealtimeMixer seams with golden-hash determinism tests.
- P4.1 (`f1420e3`): MIDI fidelity pass. Import now records every tempo meta into `tempo_map` (was: first only) and every time-sig meta into `time_sig_map`; markers (meta 0x06) land in `markers`; CC controllers beyond bank/vol/pan, pitch bend, and channel/poly aftertouch are captured per channel into `track["automation"]` lanes (`{cc, points:[{step,value}]}`, bend=128, pressure=129 — poly AT folded into channel AT as a documented approximation). Export mirrors all of it back (tempo/T.S. maps with scalar fallbacks, marker meta, CC/bend/pressure lanes). `test_midi_fidelity` covers a full export→import round trip plus a hand-built synthetic file. Fixture gotchas recorded: a marker meta with a short length byte derails parsing into a running-status error, and bend msb 0x50 = value 2048 (not 192) — both cost a debug cycle; the synthetic-fixture builder in the test is the template for future device-layer tests.
- P6.1 (`7d0b8da`): first-class clips. Decision recorded in daw/decisions.md — flat `scenes[n].clips[track]` slot layout kept, additive fields (`name/start/length/muted/color`) inside each non-null clip, accessors supply v1 defaults so old songs read unchanged. Session-grid buttons now render clip name + bars + note count; newly created clips get the full field set. `SongDocument::clip_at/_mut/clip_info/set_clip_info` are the engine-side seam the mixer/arrangement will consume. `test_song_clips.cpp` pins defaults, clamping and notes-payload preservation. Suite 36/36.
- P6.2 note: automation-lane *data* landed with P4.1 (`track["automation"]`); lane edits must use `begin_track_edit`/`commit_track_change` (TrackProps) because a NoteEdit commit only snapshots the notes array. Lane editor UI deferred to the P5 piano-roll strip.
- P5.1 (`532de0b`): `SongDocument` automation-lane accessors over the P4.1 `track["automation"]` model — `lane_at/_mut` (find by cc id), `create_lane` (idempotent), `lane_points` (step-sorted), `set_lane_point` (upsert, CC lanes clamp 0..127; bend lanes clamped by callers to ±8192), `clear_lane_point`, `lane_value_at` (step-lookup with hold-last semantics). `test_automation_lanes.cpp` pins read/sort/edit/defaults; suite 37/37.
- P5.2 (`721e043`): `VelocityLaneStrip` docked under the piano scroll in MusicPanel. Velocity row edits every note starting at the clicked step (`set_note_velocity`); automation rows draw points + segments (bend centred, ±8192). All edits go through `begin_track_edit`/`commit_track_change` (TrackProps) per the P6.2 rule — a NoteEdit commit snapshots only notes. Strip mirrors `pianoRoll->setSong` at every call site. Known simplification: a velocity edit at a step retunes all simultaneous notes; per-note lane targeting is P5-remainder work.
- Pitfall: `JsonArray`/`JsonObject` aggregate-init from `parse_json` — reordered lane points written back through `lane_at_mut` must keep the `points` array object type; `set_lane_point` guards with an is_array check before mutating.
- P5.3+P5.4 (`70113b3`): piano roll multi-select. `selectedNotes` is a `std::set<int>`; drags capture a `dragOrigins` map (index → pre-drag step/pitch/length/velocity) so group move/resize/velocity-drag are all relative and one `commit_note_edit` covers the whole stroke. Marquee on empty-grid plain drag; Shift+click creates notes now (behaviour change from plain-click-create, called out in the hint label). Copy/paste keeps velocities, pastes at relative offsets from the copied span start; duplicate appends one selection-span right. Index-based selection is invalidated by undo/redo and setSong (cleared) — stable note ids remain P5 work (they gate cross-track clipboard fidelity).
- Editor-agent commits noticed 2026-11-07: `94c8406` (AGENTS.md vswhere absolute-path rule — already reflected here), `b95dda2` (.gitattributes eol=crlf — explains the LF→CRLF warnings on every add; harmless).
- P5.5 (`0927a76`): snap lives on the canvas, not MusicPanel (panel is over the soft-cap ratchet; don't grow it). Ctrl+G cycles off/1-16/1-8/1-4/triplet; `snapped()` is applied at note create and to move deltas (origin-relative, so a snapped move stays stable when the grid changes mid-drag is avoided — deltas snap from the origin, not the live position). Ctrl+Q quantise snaps selected note starts and commits only when something actually moved (no empty undo entries).
- P5.6 (`88410f8`): playhead = mixer atomics → step via `frames·bpm·4/(60·rate)`, 33 ms QTimer on the canvas, redraw only on step change. `RealtimeMixer::audition()` builds a Voice on the GUI thread under songMutex, resolved to absolute frames at the next render command boundary (`start = frameCounter + frames`), pushed into `upcoming`. No locks/allocation added on the audio thread beyond the existing boundary swap.
- P5.7 (`aead0a2`): zoom is six stepped levels {8,12,16,24,32,48} px/step via Ctrl+wheel (`zoom(int)`), pitch range widened to MIDI 12–107 with `pitchOffsetRows` vertical scroll on plain wheel. All row math goes through `pitchRow()/visiblePitch()/visiblePitchAt()`; horizontal scroll mapping in MusicPanel uses the `stepWidth()` accessor, not the old constant. Warning: the regex rename step→baseStepWidth touched MusicPanel.cpp too — verified by grep afterwards.
- P5.9 (`9e03b9d`): stable note ids live in the per-note props object (`note_props()["id"]`) — lazily assigned by `ensure_note_id(note, &nextId)`, monotonic per song, idempotent, v1 notes read as −1. Scale highlight: song-level `key_root` (0–11) + `key_scale` (index into a 13-mode table); masks are bit i = degree i above the tonic, rotated by root at read time (`scale_mask`). Painted as a green tint on in-scale piano keys. Test-data trap hit and fixed: the first mask table had inverted/nonsense bit orders — pinned by asserting D major = pcs {1,2,4,6,8(?),9,11}… actually D major = D,E,F#,A,B,C# = pcs {2,4,6,9,11,1}; pc 8 is NOT in it. Always derive expectations from note names, not by eye.
- P5.11 (`536cf54`): ScaleSelector is its own widget file — PianoRollCanvas.cpp was already over the 750-line ratchet, so the selector could not go there. Writes go through MusicPanel::currentSong() (returns a pointer into project->music) with `as_object().erase()` for clearing (the Json wrapper has no erase; the underlying std::map does). `syncScaleSelector` runs at every setSong site so a song switch resets the combos.
- P5.12 (`6c18073`): selection stability via ids only across undo/redo for now — the ids must already exist in note_props (P4.1-imported notes have them; freshly created notes do not until the create path calls ensure_note_id, which is the remaining P5 item). NoteIndex is header-only and shared by nothing else yet; drag-time selection stays index-based deliberately (ids would churn during a move anyway).
- P5.14 (`ce7274b`): ids on create — Shift+click, paste and duplicate all call `ensure_note_id(note, &nextNoteId)`. `nextNoteId` is seeded in setSong by running ensure_note_id over the loaded track (assigns to un-id'd imported notes too, and pushes the counter past the max), so no collisions with file-supplied ids. Deliberate: pasted/duplicated notes get NEW ids, not the source's — a duplicate is a new note; id reuse would break the "one id = one note" selection contract.
- P6.3: ClipLauncherStates state machine. moc traps hit and recorded: (1) moc cannot parse member declarations with commas inside templates (`std::map<std::int64_t, Slot>`) — dropped Q_OBJECT entirely, the class is a plain type with an `onStateChanged` std::function; (2) the member name `slots` collides with a Qt macro (silently expands → broken declaration → bogus C2208) — renamed to `slotStates`. Slot struct lives at namespace scope as `ClipSlotState` with a `value` field (avoids clashing with Qt's `State`-related parsing in the studio TU). These cost several rebuild cycles; any new Qt-adjacent plain class should start from this pattern.
- Launch quantise contract: launch while stopped = immediate; while running = queued to the next bar boundary (`targetBar = (frame + barFrames)/barFrames`, barFrames from mixer bpm atomics at 44100 Hz). Promotion only in `tick()` on the GUI timer — never from the audio thread.
- P6.3.4 (`18cd90e`): promotion → mixer swap. On tick promotion the timer handler scans the grid for any Playing slot, builds that scene with makeScenePreview, and calls loadSong — **without** resetPlayback, so the transport keeps running and the swap lands at the next command boundary (contract 4 holds: no mid-callback mutation). midiSession->startOutput re-arms hardware MIDI against the new scene; a stop was never queued (stopAll idles slots without a swap — notes-off on stop is currently implicit in the next loadSong, an acceptable simplification until scene-record work forces a proper queued stop).
- P6.4 (`fbcd0c3`): scene record mirrors each recorded note into the armed scene's clip for that track (step clipped to the 16-step window; empty clips created as `{"notes":[]}` on demand; clips resized to track count). The scene is captured at arm time (`recordingScene`), so switching the selected scene mid-record does not retarget — deliberate, matches DAW convention. Track notes and clip notes are written independently (no dedup between them).

## Operations doc review (2026-11-07)

- Closed the WANTED item "Review Bible/Operations/ per AGENTS.md". Findings
  and actions: Agent Guide.md title corrected (was mislabeled `AGENTS.md`);
  Operations `Handover.md` mojibake repaired (triple-encoded em dash in the
  title needed a byte-level replace — script under `build/fix_title.ps1`,
  deleted after use), stale "DAW P0 in progress" updated to the P6.7 state,
  empty `## A1 Map Editor follow-up` removed, duplicate open-work list
  merged, blank-line padding collapsed; `Roadmap.md` Music Studio bullets
  refreshed to the DAW plan's actual landed scope; `World Map Topology
  Handover.md` got a current-status header (P3.0 + P3.1 complete, P3.2
  owner-gated) with the historical body kept as gate-evidence record.
  `Project Overview.md`, `MCP Server.md`, `evidence/` audited accurate —
  unchanged. Review plan at `Bible/notes/ops-review.md`.


## Pixel/Sprite editor merge P2 (2026-11-07)

- P2 merged both top-level panels into one `PixelEditorPanel` with Graphic /
  Sprite mode toggle. `SpriteEditorPanel` class and MainWindow tab are gone.
- One graphic selector is filtered by mode: Graphic lists all graphics;
  Sprite lists graphics referenced by character/enemy records plus
  anim-backed graphics. Sprite labels use record names where available.
- Sprite mode exposes direction/pose and the shared frame selector; the
  existing `PixelEditSession::resolve_anim_frames` resolves frame ids. Both
  modes use the same canvas, palette, clipboard, and undo/redo path.
- Preserved the former Sprite controls: three brushes, Pixel brush as the
  default, 2x zoom on entry. Graphic mode retains its five brushes and
  animation preview. Mode changes retain the Graphic-mode brush/zoom choice.
- A P2 review caught and fixed two concrete defects before commit: Sprite
  mode initially displayed graphic ids instead of character/enemy names; the
  shared palette path created a throwaway `Project(".")` for null projects.
  Both corrected. `PixelEditorPanel.cpp` final size 742 lines; do not grow.
- Verification: Release `aur_studio` and `aur_tests` build succeeded; suite
 40/40 passed. Grep confirms no `SpriteEditorPanel` references in the
  implementation/header/MainWindow.
- Follow-up verification (2026-11-07): P2 and P3 were already landed in
  `f958e55` and `e68304f`; checked the tracked panel, MainWindow wiring, and
  retired-plan acceptance against the requested scope. No `SpriteEditorPanel`
  implementation references remain. Release build targets reported no work;
  CTest passed 3/3 (EngineTests, TopologyTests, WorldSimTests). P3 is the docs/
  acceptance/plan-retirement phase and is complete; no further code change was
  needed. The build emitted a non-fatal `vswhere.exe` PATH warning despite
  returning success; retain the documented absolute-path rule for future
  build invocations.

## Pixel/Sprite P2–P3 follow-up (2026-11-07)

- Decision: preserve the already-landed implementation and completed plan
  retirement rather than reapply either change. This continuation confirmed
  the single Graphic/Sprite mode selector, one MainWindow tab, no
  `SpriteEditorPanel` implementation references, and the P3 WANTED/plan cleanup.
- Verification in this pass: Release targets returned success with Ninja
  reporting no work; full CTest passed 3/3. Build output also contained a
  non-fatal `vswhere.exe` PATH warning, so future commands should use the
  absolute path noted in HANDOVER/AGENTS.

## Editor undo base, the two visual defects and the render harness (2026-11-11)

- **The undo base was written against fixtures the project does not have.**
  `RegionKey::MapLayer` resolved one ROW of a layer, so a map-layer undo
  wrote a whole layer into a single row; `RegionKey::TileFlags` resolved
  `tiles[i].flags` as an array, but the project stores an int bitmask, so the
  tileset undo resolved nothing and did nothing; `RegionKey::GraphicPixels`
  resolved `graphics[i].pixels`, which does not exist (a frame is an array of
  row strings). All three were invisible because the tests used the same
  wrong shapes. The base now resolves regions through one shared
  `resolveRegion()`, and `Json` grew an `operator==` so a recorder can tell a
  changed cell from an unchanged one.
- **Per-cell diffs for the map editor** (`studio/MapEditUndo.{hpp,cpp}`): a
  stroke records the cells it touched and pushes one command with a diff per
  cell. Entity strokes keep the whole-array snapshot, because an entity
  insert shifts every later index and an index-addressed diff cannot restore
  the array.
- **The map editor opened zoomed out** because the fit ran in the constructor,
  against a canvas that still had its minimum size, so the zoom was computed
  for a 512-wide widget and kept when the real canvas arrived. `fitToView()`
  now defers to the first show/resize that has a real size. The World
  editor's canvas had the same defect and got the same treatment.
- **The world map's black bars** were the terrain shader's own void colour,
  RGB (9,12,15), painted over the margin above and below the map, while the
  CPU path and the canvas paint that margin `#171b20`. The shader's margin
  is now `#171b20`, so the two paths agree. Measured in
  `build/renders/world_1280x770_z1.0.png`.
- **The map editor's tool combo did not match the tool indices the canvas
  used**: the combo said "Paint Tile" at index 0, which the canvas treats as
  Select/Move, so the paint tool painted nothing and the combo labelled the
  painting slot "Erase Tile". The canonical order is now 0 Select/Move,
  1 Paint Tile, 2 Place Entity, 3 Erase Tile.
- **The render harness** (`tests/render_check.py`) drives the shipped
  executable through `--tab`, `--zoom` and `--size` and writes one PNG per
  page and size into `build/renders/`. The app's own CLI is the control
  surface; the harness reaches nothing inside the app. The zoom is a factor
  of the canvas's own fit (0.75 / 1.0 / 1.5), which is what makes 1.0 the fit
  view. The harness sets per-monitor DPI awareness v2 before it launches
  anything, and the app inherits it: the capture is 1.5x the logical size on
  this machine, and a window rect read from a DPI-unaware process reports
  logical pixels, so the same window looks two different sizes depending on
  who asks.
- **The map editor was split** so neither half is over the size cap:
  `studio/MapCanvas.{hpp,cpp}` holds the canvas (664 lines) and
  `studio/MapEditorPanel.{hpp,cpp}` the panel (439 lines).
- **File-size baseline** (owner's rule: soft ~1,000, hard ~1,250):
  `studio/MusicPanel.cpp` is 1,348 lines — over the hard ceiling and
  pre-existing; it needs splitting and is not touched by this work.
  `studio/world/WorldMapCanvas.cpp` (934) and `studio/WorldPanel.cpp` (928)
  are under the soft cap.

## The sprite studio (2026-11-11, D5.S1)

- **The pixel editor's Graphic mode was complete and its Sprite mode was a
  frame list.** Sprite mode had a direction and a pose picker and nothing
  else: no onion skin, no preview, no mirror, no way to change the frames.
- **Onion skinning**: the canvas draws the previous frame warm and the next
  frame cool, at 0.32 alpha, behind the frame being drawn. The skin is on by
  default in sprite mode -- a pose is drawn against its neighbours, not in the
  dark.
- **The live preview was graphic-mode only**, so a sprite's frames were never
  seen moving. `PixelEditSession::resolve_anim` is now the one owner of the
  anim a direction/pose pair names (the pose-direction anim, then the bare
  pose, then none) and it carries the fps and the loop flag with it; the
  frame list, the preview and the frame tools read the same answer.
- **The sprite's controls had their own row.** They were in the graphic
  toolbar, which was clipping the "Direction:" and "Pose:" labels.
- **The frame tools are pure functions over the graphic JSON**
  (`studio/PixelFrameOps.{hpp,cpp}`): add a blank frame, copy a frame's
  pixels under a new name, take a frame out of the sequence (and out of the
  graphic when no anim names it any more), and swap a frame with its
  neighbour. A structural change is one undoable snapshot of the whole
  graphic, the way the entity edits take the whole array.
- **The mirror copies the right half onto the left, mirrored**, so the result
  is symmetric about the middle and the mirror of a symmetric frame is itself.
  The first version of the test expected a full flip ("dcab"); the
  implementation was right and the expectation was not.
- **The AIO build's Qt deploy named the project, not the executable.**
  `tools/AIO-win.template.bat` called windeployqt with `%AIO_APP%.exe`,
  which for Aur is "Aur.exe" while the executable is "Aur Studio.exe"; the
  deploy failed and the failure was unchecked, so the build reported OK over
  a tree that could not start. The template now deploys the stamped
  `%AIO_EXE%` and reports a failed deploy.

## One pixel surface, two editors, and PNG in and out (2026-11-12, D6.S1)

- **A tile is a graphic's frame.** A tile is `{name, pal, flags, rows}`
  where `rows` is an array of row strings of palette indices; a frame is the
  same shape. The pixel editor's canvas edited "a rows array with a
  palette", so the canvas became `studio/PixelSurface` (it owns the brush,
  the zoom, the undo and the painting) and `studio/PixelCanvas` is the
  binding that points it at a frame. The canvas lost 447 lines and the
  tileset editor got the same surface instead of a second copy.
- **The tileset editor's canvas**: the colour picker is the tile's own
  palette (`tile.pal` into the tileset's `palettes`), and the flag toggles
  moved onto the surface's stack, so one Ctrl+Z undoes the last edit whether
  it was a pixel or a flag.
- **A tile's rows live in a `std::vector`, so a pointer into one moves if
  the vector grows.** The surface's undo commands hold that pointer. The
  tileset editor never adds a tile, so it cannot happen today; a tile
  editor that does must re-bind the surface's target.
- **PNG in and out.** `snap_rgba_to_rows` snaps every pixel of an imported
  image to the nearest colour in the palette; a transparent pixel takes the
  palette's transparent entry. It is a pure function over the RGBA bytes
  and the palette, so it is tested without a widget.
- **The chrome**: the file commands live behind a hamburger in the title
  bar and the project toolbar slots under it, so every editor gets that row
  back. The hamburger is drawn (three lines) rather than shipped as an
  asset.

## The app is Aur, and it has a data root (2026-11-12)

- **The owner decided the identity: the app is `Aur`.** The executable is
  `Aur.exe`, the `project()` is `Aur`, the window title is `Aur`, and the
  AIO scripts name `Aur.exe`. The identity, exe-name and deployed rows
  pass now. The old "Aur Studio" strings were in `main.cpp`, `MainWindow.cpp`,
  `CMakeLists.txt`, the AIO scripts, the render harness and
  `.codegpt-game.json`.
- **The data root is `~/.Aur`**, with the leading dot, and the settings file
  lives in it (`studio/AurPaths.*`). The window's geometry and the page it
  was on are restored from there; a size or a page asked for on the command
  line wins over the saved one, which is how the render sweep holds a fixed
  size, and a geometry saved on another machine is not restored if it would
  put the window off-screen.
- **The pixel canvases fit the space they are given.** The fit was a 720p
  target, so a small image was drawn much larger than the pane that holds
  it and the canvas was cut off. The fit is now the viewport's size; the
  tile editor's canvas is in a scroll area like the pixel editor's.

## One control cluster, and canvases that hug their space (2026-11-12)

- **The project's buttons are icon buttons in the title row**, beside the
  hamburger, and the tab bar is in the same row: the pages are a
  `QStackedWidget` below it. The two rows the editors were losing are gone,
  and the chrome is one control cluster, which is the estate's rule.
- **The pixel surface learns its room from the scroll area's viewport**, and
  only from there: a viewport measured once is measured before the layout
  has run, so the fit collapsed to 1x and the sprite was drawn at a pixel
  per pixel in a small box in the middle of the pane. An event filter on the
  viewport is what tells the surface how much room it has.
- **Every canvas fits the space it is given**: the map's fit margin is 12,
  the world's padding is 8, and the pixel surface's margin is the 24 its
  readout needs.

## The studio's own tools: scrolling and volume (2026-11-12)

- **The arrangement timeline was a painted canvas with no scroll offset**,
  so a song wider than the window could not be reached. It is now sized to
  its content and sits in a scroll area.
- **The mixer had a master effect chain and no gain.** The master volume
  is now the studio's: one number in `~/.Aur`, read by the audio thread
  every callback and applied after the clip, so a quiet mix stays quiet
  instead of clipping.
- **One device for the app** (owner: "if you swap tabs the other stops").
  The DAW's preview and the playtest's music share one `QtAudioOutput`
  (`studio/SharedAudio`), so they cannot play over each other. The page
  on screen holds the device.
- **The playtest plays the map's song**, the one the map's `music` names,
  and the sound is opt-in: a studio that makes noise on its own is a
  studio nobody wants to open.
- **A hidden page's timer still ticks.** The playtest's timer kept
  restarting the music behind the other tabs, which is how "the audio
  plays when leaving the tab" happened: the page has to know it is
  visible before it touches the device.
- **The window's minimum height was 720, larger than the estate's floor**
  of 690 (`DESIGN.md` 12), so the app could not be shown at the floor and
  every capture was 45 px short at the bottom. The floor is now the
  minimum, and the captures hold the whole window.
- **A camera is the mode's.** The field's is the hero's place, clamped
  and wrap-aware; the platformer's scrolls sideways and holds its height
  until the hero leaves a dead zone. One camera for both would have broken
  both.
- **A one-way platform has to know which side the hero is on.** The
  rule "the feet were in a row above the platform's" is what lets a hero
  jump up through it and still land on it, and it never catches a hero
  who is already underneath.
- **The globe is the coarse picture and the map the fine one.** The
  owner's words: "we can think of the zoomed out worldmap as 8bit and the
  map becomes 16/32bit basically". An expansion is a magnification of the
  picked place, not an invention, so the land keeps the side it had.
- **A coastal tile on the globe is an autotile variant, not a tile
  kind.** The world tileset's ocean autotile is edge-joined, so a coastal
  cell holds the ocean's edge variant and the land is on the side the
  variant faces. That is where the direction already lives, so an
  expansion reads it rather than working it out a second way.
- **The MCP writes the project when it saves, and its startup seeds the
  starter lore into the database.** Running the tile tool on the shipped
  project added the "elementals" race and a walkable stone tile to it.
  The tools are for a project of your own, not the shipped one.
- **The tile's flags are per tile, so a platformer's floor cannot be
  the field's grass.** The forge's generator takes them now (Solid and
  One-way) and the tileset editor can mark any tile one-way, but a tile
  that is solid in one map is solid in all of them.
- **The Database's editors are one editor and a table.** The kinds
  differ only in their fields, so the fields are data: one row per field,
  with the kind (text, a number, a fraction, a nested group, a pair) and
  the range. Five copies of the same form would be five places to fix the
  same bug.
- **Six of the Database page's seven tabs were stubs.** A list and a
  form of empty fields, with no load, no save and no selection handling
  -- they look editable and are not. Only Races and (now) the Bestiary
  edit their records. The plan for the rest is
  Bible/notes/database-editors.md.
- **A blank capture is a page that has not painted.** The sweep
  captured the map editor blank once and the tileset editor another
  time, with the window answering WM_NULL: the page had not drawn. The
  sweep waits longer now and nudges the window's size when a capture
  comes back blank, so the page draws again.
- **A list's padding is dead space.** The list items carried 4px of
  vertical padding, which is the space the owner noticed: more items per
  list and fewer scrollbars. It is 2px now, and the horizontal padding
  stayed at 4px.
- **The engine's movement is now a mode.** The field's rule -- four
  directions, one speed, a centre probe -- is the top-down mode, byte for
  byte; a platformer map gets gravity, a jump with a variable height and
  a body swept per axis. The rules live in `engine/aur/movement.*` and
  the cameras in `engine/aur/camera.*`, and both are pure functions, so
  the tests need no widget.
- **The engine has one movement.** The field moves a hero on four
  directions at a fixed 148 px/s and probes the tile flags at the hero's
  centre: there is no velocity, no gravity, no jump and no body. The
  platformer's plan (Bible/notes/platformer.md) splits the movement into
  modes rather than into scenes, so the shipped field stays as it is.
- **The weather pick belongs to the engine.** The runtime rolled it
  inside `WeatherSystem::roll`, the studio had a second copy of the
  weighted table (with no callers), and the editor had neither: it drew a
  spatial field, which is an authoring view and not the game's answer.
  `weather_for_slot` is now the one pick, and the test pins that the
  editor's answer and the runtime's are the same.
- **A header with more controls than the floor can hold cuts its buttons
  in half.** The World header did at 1280: the map's own line moved to the
  map combo's tooltip, the page's title label went, and two buttons lost
  words. The time and weather controls moved into their own widget
  (`studio/world/TimeWeatherBar.*`) so the bar keeps its width.
- **A widget that is a tab's page has to fill it.** The Bestiary
  editor was added as a page with no layout, so it was sized to its hint
  and the form sat in a third of the window. The page needed a layout of
  its own.
- **The Database page's six non-race tabs are stubs**: a list and a form
  of empty fields, with no load, no save and no selection handling. The
  Bestiary (Bible/notes/bestiary.md) replaces one of them with a real
  editor; the rest stay stubs until they are built, and saying so is
  better than a form that looks editable and is not.
- **The Event Scripting page is Events**: the tab, the panel's label, and
  the class and its files (`studio/EventsPanel.*`) with it, so one name
  says one thing.
- **The world map's starter locations were stale markers**: western_harbor,
  eastern_capital and southern_ruins pointed at places on a seed that has
  been re-seeded since. `worldgen.cpp` gives a new world no entities at
  all, so the markers meant nothing. Nothing referenced them -- no exit
  targets them and no code names them -- so they went and the spawn stayed.
- **The Character Forge's preview had a scale cap of 8**, so a 48x64
  character sat in the middle of a large pane at a sixth of the space.
  Every other canvas had been fitted and this one had not.
- **The transport is play/pause and stop, as icons.** The style's media
  icons are drawn for a light theme, so a black play triangle on a dark
  button cannot be seen: the three glyphs are drawn in the app's own
  colour (`studio/music/TransportIcon.*`).
- **The MIDI file transfer moved out of the panel** (`studio/music/
  SongTransfer.*`): a pure move, so `MusicPanel.cpp` stopped growing at
  1,311 lines and the DAW's new controls could go in new files.

## The maximized window's client is the window (2026-11-14)

- **A maximized window kept its restored client rect.** `WM_NCCALCSIZE` set
  `rgrc[0] = rgrc[2]` -- the client rect from *before* the size change -- and
  returned 0, so on a maximize the client stayed at the old size while the
  window grew. Measured on the 2560x1600 display: window 2560x1540, client
  2400x1500. The chrome laid out 2400 wide inside a 2560 window, so the
  maximize and close buttons fell off the right edge and "New world" was
  clipped -- which read as "the toolbar breaks" and "manual maximizing does
  not work". It was one defect, not two.
- **The fix is to leave `rgrc[0]` alone.** On input it is the *new* window
  rect, and returning 0 tells Windows the client area is that rect, so the
  client grows with the window. `rgrc[0] = rgrc[2]` was the bug, not the
  frame removal: the frame removal is the `*result = 0`.
- **Measured after**: window 2560x1540, client 2560x1540, in the opened,
  restored and re-maximized states alike.

## The World header fits the floor (2026-11-15)

- **The World header elided its controls at 1280.** The render sweep at the
  estate floor showed `Day (`, `Game's ro` and `08:0` -- the day spin, the
  weather combo and the time edit were each narrower than their own text.
  The row is one `QHBoxLayout` of twelve controls; when the width runs out
  the combos absorb it by eliding, and a number elided mid-digit reads as
  broken rather than as a narrow control.
- **The "Weather:" label went.** The bar it introduced already leads with
  its own `Day` spin and its own `Time:` label, so the outer label named a
  group the bar was not in. Dropping it freed the 66 the row needed, and the
  map combo went back to 148 so `Earth-like World` is no longer cut.
- **Dead label removed.** An empty `QLabel` with a 15px style sat at the
  head of the row and was never given text.
- **Measured after**: at 1280x690 and 1280x770 every header control shows
  its full text, and at 2560 the row is unchanged in appearance.


## 2026-11-16 repaint P0: safety net and line baseline

- Plan: `Bible/notes/repaint.md`. Baseline before any change: ctest 7/7, `render_check.py` 24
  captures, `C:\Projects\tools\conformance.py --project Aur` 9/9 clean.
- The blank Map Editor capture was a cold-start flake, not a page defect: the same page painted
  4,790 colours on the next launch. The harness left the old PNG in place when a capture failed, so
  a stale white frame looked like a result. It now deletes the PNG before the shot and launches a
  failed shot once more. It also uses `get_flattened_data` where Pillow has it.
- Captures are 1920x1035 because this machine runs at 150% DPI (1280x690 logical). That is correct.
## 2026-11-16 repaint and whole-app refactor

- **Design-system theme (`studio/theme/`, `C:\Projects\DESIGN.md` §1, §2, §6.3, §13)**:
  - `ThemeMath.{hpp,cpp}` and `ThemePalettes.{hpp,cpp}` (Qt-free, tested in `tests/test_theme.cpp`):
    WCAG relative luminance, `is_dark_background(bg)` (`L < 0.18`), `on_accent(primary)` (`#ffffff` vs
    `#0f0f1a`), contrast ratios across all 11 modes + `system`, and validated `sanitize_choice`.
  - `ThemeQt.{hpp,cpp}` and `ThemeManager.{hpp,cpp}`: maps tokens onto `QPalette` per §6.3 (including
    `Mid`, `Dark`, `Shadow`, `PlaceholderText`, and disabled states), samples Windows
    `AppsUseLightTheme` once at startup, validates `theme/mode` and `theme/accent` from `~/.Aur`, and
    supports `--theme <id>` for transient CLI/capture runs.
  - `AurStyle.{hpp,cpp}` and `AurStylePaint.{hpp,cpp}` (`QProxyStyle` over Fusion): 28 px control
    height and hit-target floor, 4/6/8/14 px radii, flat buttons/edits/combos/spins/tabs/scrollbars/
    splitters/headers from palette roles, and `accentDim` wash (`PE_PanelItemViewItem` /
    `CE_ItemViewItem`) for selected item-view rows. All 21 `setStyleSheet` calls and the `main.cpp`
    stylesheet were removed (`rg setStyleSheet studio` is 0).
  - `ThemeIcons.{hpp,cpp}` (`QIconEngine`): draws Hamburger, Minimize, Maximize, Restore, Close, Save,
    Open, Reload, Play, Pause, Stop, and Appearance in live token colours; Close hovers in `bad`.
  - `AppearanceDialog.{hpp,cpp}`: 4x3 mode grid (`system` + 11 modes), 24x24 HSV + 23-grey +
    escape-hatch accent matrix (§4), and validated `#rrggbb` hex field. Accessible from the hamburger
    menu and the title bar; `F11` and `Alt+Enter` toggle maximized/windowed mode.
- **Canvases and page polish**:
  - `PixelSurface`, `MapCanvas`, `PianoRollCanvas`, `ArrangementCanvas`, `StepSequencerCanvas`,
    `VelocityLaneStrip`, `ClipButton`, `WorldMapCanvas`, `WorldMarkerOverlays`, `CreatureForgePanel`,
    `GameView` (`CardFrame`), and `bestiary/CreaturePortrait` read UI furniture from `theme::tokens()`
    while preserving data colours and the `#171b20` world terrain shader/CPU margin agreement.
  - `FxChainStrip` hides its 88 px chain list when empty, and `MusicPanel` places `MasterVolumeStrip`
    and `Quick Compose` on the top rows so the DAW track list and session grid fit at `1280x690`.
- **Refactor splits and line baseline (all `studio/` files now under the 750-line soft limit)**:
  - `MainWindow.cpp`: 575 -> 377 lines (`StudioChrome.{hpp,cpp}` and `FramelessWindow.{hpp,cpp}`).
  - `MusicPanel.cpp`: 1,350 -> 628 lines (`music/MidiStrip.{hpp,cpp}`, `music/GmPatchList.hpp`, and
    `MusicPanelSession.cpp` at 567 lines).
  - `WorldPanel.cpp`: 1,018 -> 660 lines (`WorldPanelActions.cpp` at 395 lines).
  - `world/WorldMapCanvas.cpp`: 895 -> 465 lines (`world/WorldMapCanvasInput.cpp` at 412 lines).
  - `music/PianoRollCanvas.cpp`: 847 -> 536 lines (`music/PianoRollInput.cpp` at 289 lines).
  - `DatabasePanel.cpp`: 256 -> 45 lines; the `Races` tab now uses the generic `records::RecordEditor`
    with `"races"` in `RecordFields.{hpp,cpp}` (tested in `tests/test_record_fields.cpp`).
  - `CMakeLists.txt`: removed duplicate `TransportIcon` and `BestiaryData` entries and trimmed the
    4-line icon comment block so the `QUIRKS.md` §43 pre-commit hook passes cleanly.
- **Responsive splitter and canvas scaling across all 9 panels**:
  - Added `studio/theme/ResponsiveSplitter.{hpp,cpp}` (`theme::bindResponsiveSplitter(QSplitter*, std::initializer_list<int>)`):
    redistributes splitter pane widths proportionally on `QEvent::Resize` and `QEvent::Show` while
    updating stored ratios whenever the user manually drags `splitterMoved`.
  - Wired `theme::bindResponsiveSplitter` and `QFormLayout::ExpandingFieldsGrow` into `MapEditorPanel`
    (`{18, 60, 22}`), `TilesetEditorPanel` (`{20, 54, 26}` + dynamic `tilePreview` scaling in
    `resizeEvent`), `WorldPanel` (`{20, 80}`), `SpriteEditorPanel` (`{20, 80}`), `CreatureForgePanel`
    (`{28, 72}` with 2x2 preset grid so `controlsScroll` never clips horizontally),
    `bestiary/RecordEditor` (`{22, 78}` + outer `QVBoxLayout(this)` so the splitter fills the Database
    tab instead of stopping at `1000px`), `bestiary/BestiaryEditor` (`{22, 78}` + expanding creature
    portrait height), `EventsPanel` (`{20, 52, 28}`), and `MusicPanel` (`{22, 78}`).
  - `MapCanvasWidget` and `WorldMapCanvasWidget` now track `user_zoom_multiplier` and recompute fit
    zoom on every `resizeEvent` via `fitZoomForCurrentSize()`. `TilePaletteWidget` scales tile icon
    and grid cell sizes (`40..72` px) with sidebar width. `MusicPanelSession`, `ArrangementCanvas`,
    `StepSequencerCanvas`, and `PianoRollCanvas` stretch cells and rows dynamically with viewport
    width and height.
- **Nested module helper folders across `studio/` (`module-folders-p1`, `module-folders-p2`)**:
  - `studio/` root now holds only `main.cpp`, `MainWindow.{hpp,cpp}`, the 9 tab panel entry points
    (`GameView`, `MapEditorPanel`, `TilesetEditorPanel`, `WorldPanel`, `SpriteEditorPanel`,
    `CreatureForgePanel`, `DatabasePanel`, `EventsPanel`, `MusicPanel`), and shared cross-module
    primitives (`AurPaths`, `CommandStack`, `UndoCommands`, `RhiBackend`).
  - Every module's helper files live in a dedicated nested folder:
    - `studio/theme/`: `StudioChrome.{hpp,cpp}`, `FramelessWindow.{hpp,cpp}`, plus all theme/style files.
    - `studio/playtest/`: `QtRenderer.{hpp,cpp}`, `PlaytestDebugDialog.{hpp,cpp}` (extracted from `GameView`).
    - `studio/map/`: `MapCanvas.{hpp,cpp}`, `MapEditUndo.{hpp,cpp}`, `TilePaletteWidget.{hpp,cpp}`,
      `TileTexture.hpp`, `EditorDefs.hpp`.
    - `studio/pixel/`: `PixelSurface.{hpp,cpp}`, `PixelEditSession.{hpp,cpp}`,
      `PixelFrameOps.{hpp,cpp}`, `SpriteCanvas.{hpp,cpp}`.
    - `studio/tileset/`: `TilePngIo.{hpp,cpp}` (extracted from `TilesetEditorPanel`).
    - `studio/world/`: `WorldModel.{hpp,cpp}`, `WorldSettingsPanel.{hpp,cpp}`,
      `WorldPanelActions.cpp`, plus all existing world helpers.
    - `studio/forge/`: `CharacterSheetWidget.{hpp,cpp}`, `ForgeAssetOps.{hpp,cpp}` (extracted from
      `CreatureForgePanel`).
    - `studio/bestiary/`: `BestiaryData.{hpp,cpp}`, `BestiaryEditor.{hpp,cpp}`,
      `CreaturePortrait.{hpp,cpp}`, `RecordEditor.{hpp,cpp}`, `RecordFields.{hpp,cpp}`.
    - `studio/events/`: `EventCommandOps.{hpp,cpp}` (extracted from `EventsPanel`).
    - `studio/music/`: `MusicPanelSession.cpp`, `MidiDeviceIo.{hpp,cpp}`, `SongHistory.{hpp,cpp}`,
      `SharedAudio.{hpp,cpp}`, plus all existing DAW helpers.
