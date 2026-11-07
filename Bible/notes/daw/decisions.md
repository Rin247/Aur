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

- **2026-11-07 — Clip model (P6.1).** A clip object keeps the flat
  `scenes[n].clips[track]` slot layout the session grid already renders (null
  = empty slot) and adds first-class fields inside each non-null clip:
  `name` (default "Track·Scene"), `start` (step, default 0), `length` (steps,
  default 16 = one bar), `muted` (bool, default false), `color` (optional int
  tag), plus the existing `notes`. All fields additive — old songs with bare
  `{notes}` clips read unchanged (defaults supplied by accessors). Note steps
  inside a clip stay clip-relative (0 = clip start), which `notesForBar`
  already produced; `makeScenePreview` unchanged while clips are 1 bar.
  Launch quantisation and arrangement timeline placements stay future work.
- **2026-11-07 — Audio backend (P2): Qt6 `QAudioSink`.** `Qt6::Multimedia` is
  already a required dependency of both `aur_game` and `aur_studio`
  (CMakeLists.txt:13,38,84), so no new linkage; the engine's real-time output
  goes through a narrow `AudioOutput` interface so a WASAPI/miniaudio swap
  later touches one file. Callback rules (contract 4) still apply: the sink's
  read callback only pulls from the mixer ring, never allocates or locks.

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
- **2026-11-07 — P1 closed.** Tag `daw-p1`; commits `daw-p1.1` (synth/midi
  moved to `engine/aur/audio/`, plain `namespace aur` kept — sub-namespace
  deferred until the engine gains more modules) and `daw-p1.2` (MusicPanel
  split). Suite 28/28, no behaviour change.
- **2026-11-07 — MusicPanel split layout.** `studio/music/PianoRollCanvas`
  (canvas widget + piano-roll constants), `studio/music/MidiSession` (QObject
  owning device I/O, recording clock, playback scheduler; talks to the panel
  via `noteRecorded`/`statusChanged`/`outputError` signals), `studio/music/
  SongLibrary` (makeTrack/notesForBar/initializeScenes/songIdFromName/
  quickSong/stereoPcmToWav — Qt-light helpers). MusicPanel host is 921 lines
  (above the 750 soft cap): the ~380-line constructor is the next extraction
  candidate; do not grow the host.
- **2026-11-07 — commitRecordedNote changed shape deliberately.** Recording
  commit now flows MidiSession→panel via the `noteRecorded` signal instead of
  the session reaching into `Project` directly. Same behaviour (note appended,
  sorted by step, UI refreshed) but the session no longer touches project
  state; revisit if recording needs transactional undo (P3).
- **2026-11-07 — P2 core shape.** `VoiceCore` (`render_voice_sample`,
  `voice_envelope`) is the single DSP path; `synthesize_preview` and
  `RealtimeMixer` both call it. `PreviewSynth` and `audio::Timbre` share
  underlying values (0/1/2). Golden PCM hashes verified unchanged after the
  move — sample-exact.
- **2026-11-07 — P0 timing "quirk" resolved.** The preview formula
  `step·rate·60/(bpm·4)` was correct all along (the P0 probe had counted
  interleaved samples at 8 kHz: 1000 frames/step, not 125). No engine change
  needed; P2 parity confirmed both paths agree.
- **2026-11-07 — RealtimeMixer contract details.** `loadSong` swaps a pending
  song under a mutex consumed at the next command boundary (top of a render
  call), never mid-fill. Voices scheduled ahead are kept, not erased, once
  their start passes (early bug: future voices were erased at frame 0 —
  parity test caught it). Loop wrap reschedules template voices at absolute
  `k·loopFrames` offsets; frameCounter is monotonic. Parity is asserted over
  the first loop pass only — a looping mixer cannot match a non-looping
  offline render past the wrap (drum noise re-seeds per pass).
- **2026-11-07 — Qt 6.12 QAudioSink callback API.** `start` takes a
  `QSpan<float>` callback, not the old `QIODevice*` overload; buffer sized at
  100 ms. MusicPanel preview no longer writes temp WAVs or uses
  QMediaPlayer; `stereoPcmToWav` stays for export paths (P11).
- **2026-11-07 — parity test numbers.** 8 kHz, 1-bar song, 16k frames:
  max_delta ≈ 3.2e-5 (≈1 int16 LSB), rel 1.2e-5, threshold rel < 5e-4.
- **2026-11-07 — MIDI-out clock sync (P2.5).** `MidiSession::attachClock`
  binds the session to the mixer's `playheadFrames/playheadBpm/
  playheadActive` atomics; `pumpOutput` converts mixer frames → ms via
  `frames·60000/(bpm·4·rate)` from a `clockAnchorFrames` snapshot taken in
  `startOutput`. Wall-timer path stays as fallback when no clock is attached.
  Sample rate comes from `QtAudioOutput::kSampleRate` (single definition).
- **2026-11-07 — OfflineRenderJob (P2.6).** `synthesize_preview` grew
  optional trailing args (`const std::atomic<bool>* cancel`,
  `const RenderProgress&`) — defaults keep every existing call site and the
  golden/parity contracts untouched. Cancel returns an empty PCM; callers
  distinguish cancel (empty result, empty error) from failure (empty result,
  non-empty error). Job is one-render-per-instance, non-copyable,
  destructor cancels and joins via `std::future`.
- **2026-11-07 — P3 approach.** Song v2 is additive JSON keys over the same
  document (`format:"2"`, `ticks_per_step:120`, `tempo_map`,
  `time_sig_map`, `loop`, `markers`) rather than a parallel binary format:
  every v1 reader keeps working and migration is idempotent and lossless.
  v1 scalar keys (`tempo`, `beats`, `loop_bar`) stay on disk and act as
  fallbacks; accessors prefer v2 maps.
- **2026-11-07 — SongDocument::save writes `<path>.bak` from the previous
  file before overwriting** (contract 1). Only when a previous file exists.
- **2026-11-07 — SongHistory snapshot model.** begin_note_edit snapshots the
  track's notes before mutation; commit_note_edit pairs it with the
  post-edit state. Undo/redo store the opposite payload captured at
  traversal time. Track-props commits snapshot the whole track object;
  wiring into the UI panels is deferred to P5, where the piano roll replaces
  its destructive mutation path.
- **2026-11-07 — CMake near-miss recorded.** During P3.3 a careless edit
  renamed the `aur_studio` target to `aur_tests` mid-session; restored
  immediately and verified (Studio still builds). Lesson: edit target
  blocks by their full header text, never a bare `add_executable(` anchor.
