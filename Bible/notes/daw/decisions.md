# DAW: decisions

Settled with evidence before code; each entry dated. Amend rather than argue.

## Open (must settle in the phase that needs them)

- **Audio backend** (P2): Qt6 `QAudioSink` vs miniaudio vs WASAPI direct.
  Decision input: latency target and thread model; lean `QAudioSink` first
  (already linked via Qt6Multimedia for deployment) with the engine's output
  interface abstract enough to swap. Confirm `Qt6Multimedia` is already a
  dependency of `aur_studio` before committing to it.
- **SoundFont loader** (P7a): TinySoundFont-style single-header vs own
  implementation; which redistributable SoundFont (licence must permit
  bundling) — verify licence text before any download lands in the repo.
- **MIDI I/O library** (P4): keep WinMM behind an interface first; cross-platform
  abstraction only if a second platform actually needs it.
- **Plugin story** (P12): decision document only; no SDK integration before M4.
- **Tick resolution / v2 format** (P3): 480 PPQ keeps the existing
  `ticks_per_step = 120` mapping; 960 doubles precision. Prefer keeping 480
  unless triplet/32nd automation proves the need.

## Settled

- **2026-11-06 — `MusicEngine` is currently dead code.** No call sites exist
  (`grep MusicEngine` matches only `music.hpp`). P11 defines the runtime
  contract fresh; no compatibility shim needed before then.
- **2026-11-06 — map music reference is written but never read.**
  `project.cpp:299` writes `m["music"] = nullptr`; `game.cpp` never consults
  it; no Sylvi map sets it. P11 wires the lookup rather than preserving it.
- **2026-11-06 — clamp facts:** preview clamps step to 546 (`65535/120`) and
  song length to 4096 steps (`synth.cpp:219-221,235`); export clamps step to
  546 (`midi.cpp:395`, 120 ticks/step). Note length clamps at 512 in preview.
  The UI's 128 bars (2048 steps) fits inside 4096, so truncation starts at
  step 547 (≈ bar 35).
- **2026-11-06 — v1 keys beyond the gameplan list:** `gs` (bool) and song-level
  `reverb` exist in real songs (`overworld.json`) and must stay loadable.
- **2026-11-06 — song storage:** `Project::music` is a `JsonObject` keyed by
  song id, loaded from `projects/<id>/music/*.json` (`project.cpp:92-97`);
  `MCP` has no music/song tools today (P10 adds the generator tool fresh).
- **2026-11-06 — sizes at audit:** MusicPanel.cpp 1,430 / .hpp 146 /
  synth.cpp 290 / midi.cpp 390 / MidiDeviceIo.cpp 156 / music.hpp 22.
- **2026-11-06 — tests:** `test_midi.cpp` already exercises import/export;
  P0 extends it. No music test file exists yet; `test_worldsim.cpp` is the
  pattern for the new focused mode if needed.
- **2026-11-06 — P0 closed.** Suite 28/28; tags `daw-p0.1` (clamp fix +
  regression) and `daw-p0.2` (golden PCM hashes Gm `cb85984999374d93`,
  FM `593f65526dc8c07b`, PSG `e956398a8d885a99` at 8 kHz/30 s fixture).
- **2026-11-06 — preview step→seconds mapping quirk.** A note at step 600
  (tempo 120) renders at ~frame 1,200,000 at 8 kHz (≈150 s), i.e. 0.25 s/step,
  not the 0.125 s/step the naive `60/(bpm*4)` formula implies. The regression
  test is deliberately mapping-independent; P2 must define the real timing
  contract and reconcile the formula.
- **2026-11-06 — unrelated working-tree churn stashed.** A brace-restyle of
  `studio/world/*` files (WorldCommandHistory, WorldEntityDialogs,
  WorldGenerationJob, WorldStrokeState, WorldPanel.hpp) sat uncommitted in the
  tree with no task; stashed as `daw-p0 churn-residue` (drop or re-apply
  deliberately).
- **2026-11-06 — owner-reported UI issues logged in WANTED.md:** map editor
  first-boot zoom; World Time Settings merged into the World editor.
