# Aur Music/DAW: Agent Work Packages

Briefs for agents covering MIDI import quality, the playback engine, and removing the old oscillator synths. Based on a read of `midi.cpp`, `synth.cpp`, `VoiceCore`, `RealtimeMixer`, `SynthCores`, `YmFmCore`, `SnesPcmCore`, `SnesSamples`, the effect chain and FX rack, `SongDocument`, `OfflineRenderJob`, `MusicPanel`, `MidiSession`, `QtAudioOutput`, `ClipLauncherStates` and `ArrangementCanvas`.

Everything below is from reading the code. Nothing was compiled or listened to, so each finding carries a reproduction step. Agents should reproduce first, fix second.

**Assumption to confirm:** "the old synths" means the three oscillator timbres in `VoiceCore` (`PreviewSynth::Gm`, `Fm`, `RetroPsg`, including their offline path in `synth.cpp` and their voice path in `RealtimeMixer`). The two hardware-style cores (`MdFm` = YM2612-style, `Spc` = SNES sampler) are kept. If you meant something else, change WM1/WM5 before anyone starts.

## Why MIDI imports sound bad: ranked causes

| # | Cause | Confidence | Where |
| --- | --- | --- | --- |
| 1 | Realtime voice path never sorts scheduled voices, so multi-track songs play only the first track | high (by reading; verify with a 2-track test) | `RealtimeMixer::schedule/render` |
| 2 | Neither "retro" core can play a GM file: FM channels are monophonic and tracks fold onto 6 channels; SPC maps every GM program to one of 3 demo waveforms (`program % 3`) with 8 voices | certain (by design) | `SynthCores.cpp`, `YmFmCore`, `SnesPcmCore` |
| 3 | Hard clipping: 0.14 gain per voice, no headroom or limiter, `clamp(±0.95)` per sample | high | `RealtimeMixer::render`, `synth.cpp` |
| 4 | Import snaps note starts and ends separately to 1/16-note steps and keeps only the last program/volume/pan per channel | certain | `midi.cpp` `parse_track` |
| 5 | Playback ignores everything the import keeps: tempo map, CC/bend/pressure lanes, sustain pedal, mid-song program changes | certain | mixer, `synth.cpp`, `MidiSession` |
| 6 | Audio output buffer is about 12 ms, not the 100 ms the comment says; command-boundary work happens on the audio thread | likely (measure underruns) | `QtAudioOutput.cpp`, `RealtimeMixer::render` |
| 7 | Voice envelope puts the 125 ms release inside the note, so any note ≤125 ms never reaches full level | certain | `VoiceCore.cpp` `voice_envelope` |

Causes 1 and 7 live in the oscillator path that WM5 deletes. They still matter because the same class of bug must not be recreated in the replacement.

## Order of work

| # | Package | Notes |
| --- | --- | --- |
| WM0 | Repro and measurement harness | first, small |
| WM1 | Engine hotfix bundle | independent of the rest; do it before anything is removed |
| WM2 | MIDI import fidelity | independent of WM3/WM4 |
| WM3 | Unified song timeline + core interface | prerequisite for WM4 and WM5 |
| WM4 | SoundFont core (the real GM answer) | needs WM3 |
| WM5 | Remove the oscillator synths | after WM3, ideally with WM4 |
| WM6 | Hardware MIDI out on the shared timeline | needs WM3 |
| WM7 | Cleanup and ideas | any time |

## Rules for every agent

- Read `Bible/Operations/Agent Guide.md`, the root `HANDOVER.md` and `Bible/Operations/NOTES.md` first. Capture `git status --short` before editing and stage only the files in your package.
- Soft file cap \~1,000 lines (hard \~1,250). `MusicPanel.cpp` is already 1,376 lines, so new UI code goes in new files.
- Non-ASCII in C++ string literals must be `\uXXXX` escapes in `QStringLiteral`.
- Build with `cmd /c .\build.bat`; test with `ctest --test-dir build\Release -C Release --output-on-failure`. The engine stays Qt-free. No `/fp:fast`. Contract 4: the audio callback does no locking or allocation. Contract 5: identical inputs must give identical samples; keep golden-hash tests.
- Do not claim an audible result unless it was measured in the current run. Finish with the Result record template (scope, files, build/test results, evidence, known limits, next gate), then stop.

---

## WM0: Repro and measurement harness

1. Add test fixtures built in code (no binary files): a 3-track song with a track starting at step 0, a triplet figure, a swung eighth figure, a held chord with sustain pedal, a mid-channel program change, a volume fade to 0, a tempo change, and a Type 0 file with 3 channels.
2. Add a headless `render_song_wav` helper (offline) so results can be listened to and hashed.
3. Add counters to `RealtimeMixer`: voices active, samples above 0.95 (clip count), callback render time, and underrun count (from `QtAudioOutput`). Show them in the existing FX status label.
4. A/B protocol to record in the Result record: (a) original MIDI in an external player with a GM bank; (b) the same file imported and re-exported through Aur, played the same way (isolates import loss); (c) Aur preview (isolates playback).

---

## WM1: Engine hotfix bundle

Each item has a repro. Fix them in separate commits.

1. **Unsorted `upcoming` (voice path).** `schedule()` pushes voices in track order and `render()` activates them with a loop that stops at the first voice whose start is in the future (the comment claims "kept sorted at build"; nothing sorts). Voices from later tracks that start before an earlier track's last note are reached only after their notes have already ended and are then silently dropped. Loop wrap and `audition()` push to the same unsorted deque. Repro: a 2-track song where track 2 starts at step 0 and track 1 has notes through bar 8; track 2 is inaudible. This path is deleted by WM5, but fix it first only if WM5 is not landing the same week; do not leave the preview broken in between.
2. **Master FX chain never runs in realtime.** `chainScratchL/R` are declared "sized at the command boundary" but never resized anywhere, so the `chainScratchL.size() >= frames` guard always fails. The core path then stores `pl = 0` into the post-FX meters. The offline render (`synth.cpp`) does apply the chain, so preview and export disagree. Size the scratch in `schedule()` (or better, at `setSampleRate` / a max block size) and add a test that a non-bypassed chain changes the realtime output.
3. **Core-path loop wrap replays every note at once.** At wrap, `rebaseEvents(nextWrapFrame - frameCounter)` is called when `frameCounter == nextWrapFrame`, so the shift is about 0. The event cursor rewinds, `consumeUpTo` then issues every note whose start is already in the past. The shift must be `+loopFrames`. Repro: loop a 4-bar md\_fm song for two passes.
4. **Audio-thread allocation and heavy work.** `render()` calls `schedule()` at the song swap. That walks the JSON, pushes into deques, builds the effect chain, and (core path) calls `SynthCoreVoice::setup`, which reallocates `YmFmCore`/`SnesPcmCore` (about 2 MB SPC sample pool, FX buffers) and BRR-encodes the demo samples. Big imports stall the callback at start. Build an immutable `PreparedSong` on the GUI/worker thread and hand it over with an atomic pointer swap; the audio thread only adopts it.
5. **`QtAudioOutput` buffer size.** `sink->setBufferSize(sampleRate / 10)` takes bytes: 4,410 bytes of float stereo is about 550 frames, about 12.5 ms, not the 100 ms the comment states. Compute `sampleRate * 0.1 * channels * sizeof(float)`, prefer the device's preferred format, and plumb the real rate through. `ClipLauncherStates::framesPerBar` hard-codes 44,100 and the piano-roll playhead/MidiSession conversions should read the mixer rate instead.
6. **Gain staging.** Voice amplitude 0.14 per note with a per-sample hard clip distorts dense imports; `std::cos/std::sin` for pan runs per voice per sample. Precompute pan gains per voice, add headroom (master gain about -6 dB), and insert an always-on soft limiter after the mix (the `Limiter` effect already exists) even when the user chain is empty.
7. **Envelope shape.** `voice_envelope` applies a 125 ms release before `voice.end`, shortening every note; notes ≤125 ms (every 1-step note at 120 bpm) are a ramp. Whatever replaces this must release after note-off, not inside the note. (Moot once WM5 lands; keep as a rule for new voices.)
8. **Tempo map ignored.** `RealtimeMixer`, `synthesize_preview` and `MidiSession` use only the scalar `tempo` (the first tempo found at import). The import stores a full `tempo_map`. Honour it (WM3 does this centrally). Also, `ArrangementCanvas` reads `t["value_a"]` for tempo labels but the JSON key is `"bpm"`, so the lane always shows 120.
9. **Audition is silent when stopped.** `audition()` queues a voice that is only rendered while the `QAudioSink` runs, and the sink starts only in `playPreview`. Keep a persistent audition path (the output idles at near-zero cost) or start the sink on demand.
10. **Offline render.** `starts(total_frames + 1)` is a vector of vectors: about 7.9 M entries (about 190 MB) for 180 s, about 635 MB for 600 s. Replace with a sorted event list. `max_seconds = 180` silently truncates longer songs; report truncation. `OfflineRenderJob` publishes `result`/`error` with relaxed atomics; use release/acquire.
11. **Long files.** Notes are clamped to 512 steps in the mixer and offline path.

**Acceptance:** a test per item, all CTest suites green, and the WM0 A/B recorded before and after.

---

## WM2: MIDI import fidelity (`midi.cpp`)

**What the importer does today (confirmed):**

- `midi_tick_to_step` rounds to the nearest 1/16 note. A note's start and end are rounded separately, so a note's length is the difference of two roundings. Three triplet notes in two steps become two notes on the same step. Staccato and swing are flattened. The song keeps `ticks_per_quarter` but notes do not use it.
- Per channel it keeps only the **last** program (`state.program = first` each time) and the **last** CC7 volume and CC10 pan. A track that fades volume to 0 at the end is imported silent; a channel that changes instrument mid-song uses the final instrument for the whole song.
- All other CCs, pitch bend and pressure are stored as lanes, which is good, but playback ignores them (WM1.8, WM3). RPN pitch-bend range and sustain are stored as plain CC lanes and never interpreted.
- `bars` is clamped to 128 even for longer songs, and the first tempo found sets `tempo`.
- Any system-common/real-time byte (0xF1–0xF6, 0xF8–0xFF) throws "Unsupported MIDI system event", failing the whole import.
- Tracks are named `Channel N` with no instrument name.

**Tasks**

1. **Sub-step timing.** `SongDocument` v2 already defines `ticks_per_step = 120`, and notes already carry an optional 5th "props" object (`set_note_prop`). Store `to` (start tick offset from the rounded step, signed) and `te` (end tick offset) in props when the note is off-grid. The WM3 timeline uses `step*120 + to`. The piano roll keeps drawing snapped to the grid, shows off-grid notes with a marker, and "Quantize" clears props. v1/v2 files without props read exactly as today (contract 1: additive).
2. **Program and bank changes:** add lane id 130 (`kLaneProgram`, values are the program number) and record program changes as lane points (bank MSB/LSB likewise or as a combined lane). The track keeps its initial program. WM3 emits them as events.
3. **CC7 and CC10** become lanes (like CC11 already is); `track.volume`/`pan` hold the first value at or before tick 0 instead of the last.
4. **Sustain (CC64) and RPN 0 (bend range):** keep as lanes, and tell the timeline how to interpret them (WM3/WM4). Add `bend_range` to the track (default 2) from RPN 0.
5. **Do not abort on real-time bytes;** skip them and count them in the report.
6. **Import report dialog:** notes imported, notes moved by more than 1/32 note (and the maximum error in ms), programs used (with GM names), lanes kept, events dropped. Quality loss must be visible instead of silent.
7. **Track naming:** use the track name meta event, else `GM instrument name (Ch N)`; drums get `Drums (Ch 10)`.
8. Remove the `bars` clamp for imports (or raise it) and size from the last note.

**Tests:** the WM0 fixtures: import then build the timeline and assert every onset is within 1 tick of the source; the fade-to-0 track is audible; the program-change fixture has two programs; export then re-import preserves ticks within 1.

---

## WM3: Unified song timeline and core interface

Today three separate schedulers decode the song JSON: `RealtimeMixer::schedule`, `synthesize_preview` (twice, voice and core), and `MidiSession::startOutput`. They disagree on tempo, clamps (512 steps, 4,095 steps, 180 s), program handling and lanes.

1. `engine/aur/audio/SongTimeline.{hpp,cpp}`: builds a sorted, immutable vector of `TimelineEvent {sample, kind, channel, a, b}` from a v2 song JSON. Kinds: NoteOn, NoteOff, ControlChange, PitchBend, ChannelPressure, ProgramChange, SysEx. It resolves the tempo map piecewise (tick to sample), flattens arrangement placements, applies note props (sub-step ticks), expands lanes into events (with a thinning rule for dense lanes), and reports loop length. Built off the audio thread; Qt-free; deterministic.
2. `IMidiCore` interface: `setup(rate)`, `noteOn/noteOff`, `controlChange`, `pitchBend`, `channelPressure`, `programChange`, `allNotesOff`, `render(float* l, float* r, size_t n)`. `YmFmCore` and `SnesPcmCore` already expose most of this; add thin adapters.
3. A `SongPlayer` that walks the timeline and drives any `IMidiCore` (replaces `SynthCoreVoice`'s event translation). The realtime mixer, offline render and (WM6) MidiSession all consume the same `SongTimeline`, so tempo, loops and lanes agree everywhere.
4. Loop wrap rebases by `loopLength` (fixes WM1.3 structurally); no allocation on the audio thread.
5. Golden-hash tests over a fixed fixture for each core.

---

## WM4: SoundFont core (the real GM renderer)

The two hardware-style cores are instruments, not a General MIDI renderer, so GM imports will never sound right through them: `fmChannelForTrack = track % 6` onto monophonic FM channels, `applyFmTrackTone` loads 13 built-in patches by `program % 13`, and `applySpcTrackTone` chooses 3 demo waveforms by `program % 3` with 8 voices. A sample-based SoundFont core is the roadmap's outstanding P7a.

- **Library:** TinySoundFont (`tsf.h`, single header, MIT license; confirmed against the project's own README and header in this session). Known limits per its own header: no modulators, no chorus/reverb send generators, a basic low-pass filter. The existing `FxRack` (chorus/delay/reverb) can supply CC91/93/94 sends, which the cores already honour.
- FluidSynth and similar are LGPL; do not link them statically without an explicit decision.
- **Soundbank:** needs a General MIDI `.sf2` with a redistributable license. Agents must read the actual license text of any candidate before committing a bank (candidates commonly cited are GeneralUser GS and FluidR3\_GM; neither license was checked here). Keep large binaries out of normal git history (LFS or a fetch script) and add the bank path as a project setting; tests use a tiny generated SF2.
- **Implementation notes:** one `tsf` instance per player; it is not thread-safe, so all calls come from the audio thread via `SongPlayer`. Load the SF2 off the audio thread. Channel setup through the `tsf_channel_*` API (program, bank, pitch wheel, controllers); check which controllers (7, 10, 11, 64) it handles and implement the rest in the adapter. Drums on channel 10 use the drum preset mapping. Cap polyphony (about 64–128) with a steal-oldest/quietest policy.
- If the bank is missing, play silence and show a clear warning, not a different instrument.

**Acceptance:** an imported multi-track GM file renders through the core in the A/B with correct instruments per channel, drums on ch10, sustain and bend working, no clipping at the limiter, deterministic golden hash with the tiny test font.

---

## WM5: Remove the oscillator synths

Only after WM3 lands and a core other than the oscillators can play any song (WM4 preferred).

**Delete**

- `engine/aur/audio/VoiceCore.{hpp,cpp}`: `Voice`, `Timbre`, `render_voice_sample`, `voice_envelope`, `voice_from_note`.
- `synth.cpp`: the voice path of `synthesize_preview` (everything after `synthesize_core_preview`), and `PreviewSynth::Gm/Fm/RetroPsg`. `synth.hpp` keeps the offline render entry point on top of `SongPlayer`.
- `RealtimeMixer`: `Timbre`, `voices`, `upcoming`, `loopVoices`, `pendingAuditions` voice path, `timbreForSong`, and the unused `PlayCommand`. Rebuild `audition()` on the persistent core.
- `MusicPanel`: the `gm`, `fm`, `retro_psg` combo entries; the `#include "aur/audio/synth.hpp"` (nothing in the panel uses it); the tooltip text.
- Tests that cover the voice path (grep `tests/` for `Timbre`, `voice_envelope`, `synthesize_preview`, `Voice`) — replace with timeline/core tests.
- Docs: `Roadmap.md` and `Project Overview.md` both state the built-in previews are oscillator synthesis.

**Migrate**

- Song key `preview_synth`: `gm`/`fm`/`retro_psg` (and any unknown value) map to the new default source; `md_fm` and `spc` stay. Write the new key (for example `source`) on save and tolerate the old key on load (contract 1).
- The combo becomes "Sound source": SoundFont (default), Mega Drive FM, SPC sampler.
- Before deleting anything, grep the whole repo (`aur_game`, MCP tools, tests, forge) for `synth.hpp`, `PreviewSynth`, `VoiceCore`.

**Core hygiene for the two kept cores**

- Per-track source (later) so a song can mix SoundFont and a chip core; for now warn when a song exceeds a core's limits (6 FM channels, 8 SPC voices).
- `SynthCoreVoice::setup` (allocation, BRR demo encode) must run off the audio thread (WM1.4).

---

## WM6: Hardware MIDI out on the shared timeline

`MidiSession::startOutput` today: scalar tempo; bank/program/volume/pan sent once at t=0; no CC/bend/pressure lanes; no mid-song program changes; `startStep` clamped to 4,095; the whole loop clamped to 180 s with every later event pinned to the last millisecond (so a song longer than 3 minutes dumps its tail at once); no all-notes-off at the loop wrap.

- Generate events from `SongTimeline` (convert sample to ms or use the mixer clock as now), stream them lazily instead of pre-expanding, remove the 4,095/180 s caps, send all lanes, and send all-notes-off at wrap.
- Keep the mixer playhead as the clock when attached (already supported).

---

## WM7: Cleanup and ideas

- `MusicPanel.cpp` is 1,376 lines: move the FX strip, MIDI device strip and import/export into their own files; `MusicPanel.hpp` includes `ArrangementCanvas.hpp` twice.
- Piano roll: show off-grid notes (WM2.1), per-note props editing, GM instrument names on track headers.
- Instrument browser with audition once a SoundFont is loaded; per-track mute/solo/volume meters; stem and WAV export; loudness normalization on export.
- Import presets ("game music" vs "performance"): quantize strength, merge channels, drop expression lanes.
- Render-to-WAV regression test per core; underrun and clip counters visible in the UI.
- Arrangement tempo/time-signature lane: edit points, not just labels.

---

## Decisions the owner still has to make

1. **Which synths are "old"?** This brief assumes the oscillator `gm`/`fm`/`retro_psg`. Confirm, and confirm that `md_fm` and `spc` stay.
2. **Soundbank:** which GM `.sf2`, whether it may live in the repo, and how large a download is acceptable.
3. **Sub-step timing representation:** note props (`to`/`te`, additive and recommended) vs a model change to ticks.
4. **Default source for old songs:** SoundFont once WM4 lands; until then, which core should "gm" songs map to (SPC or MD FM)?
