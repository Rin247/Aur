# Groovebox: 128-step sequencer + performance mode

Status: P0-P7 landed (engine model, playback, arpeggiator, studio widgets, and the
three oscillator synths retired). P8 next: close-out — docs and the estate gate.

## Goal

Finish the DAW's missing performance layer and make Aur Studio feel like a
groovebox rather than only a linear DAW:

1. A **128-step sequencer** (P9 of the DAW plan) that lives in the song
   document, plays in real time and offline, and drives the existing voices.
2. **Groovebox performance mode**: swing, per-step accents, slide/glide,
   ratchets, an arpeggiator, pattern chaining, and a hardware-style
   performance strip with keyboard control.

The DAW plan's remaining phases (P7a SoundFont, P7b DX7/PSG, P7c SPC UI,
P10 generator, P11 runtime integration, P12 plugin doc) are **not** in this
plan. This plan lands the sequencer and groovebox layer; the DAW plan keeps
its own queue.

## Contracts (binding)

1. **Additive song format.** The step sequencer lives under a new song key
   `song["step_patterns"]`; groovebox globals under `song["groovebox"]`. Both
   are absent by default and a song without them renders **bit-identical** to
   the pre-change path (the same rule the effects chain follows).
2. **v1/v2 compatibility.** A v1 song loads with no `step_patterns` and no
   `groovebox`; accessors return empty/default values; migration is
   idempotent and lossless.
3. **Engine stays Qt-free.** Everything in `engine/aur/audio/` uses only the
   standard library and `aur/json.hpp`.
4. **Audio-callback safety (contract 4).** No locks, allocation, logging or
   Qt inside `RealtimeMixer::render`. The step sequencer compiles to a
   pre-built, immutable event list at song swap (the command boundary).
5. **Determinism (contract 5).** Same song + seed + sample rate → identical
   offline and live output. The step sequencer's randomisers take an explicit
   seed stored in the pattern, so a reroll is reproducible.
6. **Offline/live parity.** The realtime mixer and `synthesize_preview` must
   agree on step-sequencer output within the documented tolerance (the
   effects chain already holds 5.8e-06; the sequencer must not be worse).
7. **Tests live in `tests/`** and are updated in the same change as the code
   they cover.
8. **File-size rule.** New feature = new file. No file over the 750-line soft
   limit grows; `MusicPanel.cpp` is already over the cap and must not grow.

## Current shape (verified 2026-11-11)

Read against the source, not from memory:

- `engine/aur/audio/` holds the Qt-free engine: `RealtimeMixer` (real-time
  voice/core mixer with a lock-free command boundary), `SongDocument` (v2
  song model), `VoiceCore` (three oscillator timbres), `SynthCores`
  (YM-FM and SPC hardware cores), `EffectChain` (10 built-ins + master
  chain), `synth.cpp` (offline `synthesize_preview`), `midi.cpp` (SMF
  import/export).
- `studio/music/` holds the Qt side: `PianoRollCanvas`, `ArrangementCanvas`,
  `VelocityLaneStrip`, `ScaleSelector`, `ClipLauncherStates`, `SongLibrary`,
  `MidiSession`, `QtAudioOutput`.
- `studio/MusicPanel.cpp` is **1,321 lines** (over the 750 soft cap). Its
  constructor is ~680 lines. New UI must go into new files.
- `SongDocument` already models: v2 tempo/time-signature maps, loop, markers,
  note props (velocity + stable ids), automation lanes (bend/pressure),
  first-class clips (`scenes[n].clips[track]`), arrangement placements, and the
  master effect chain.
- **There is no step sequencer anywhere.** Grep for "step_pattern" / "128"
  over `engine/` and `studio/` returns nothing.
- `RealtimeMixer::schedule()` reads only the scalar `tempo` and the note
  arrays. `synthesize_preview` does the same. The tempo map is _not_ honoured
  at playback, which the WM1.8 work package already flagged.

## Hazards found

1. **`RealtimeMixer::schedule` runs on the audio thread.** A song swap walks
   the JSON, builds vectors and rebuilds the effect chain inside `render()`.
   A step sequencer that compiles a 128-step pattern there would make this
   worse. The compile step must be hoisted out of the callback.
2. **`upcoming` is never sorted.** `schedule()` pushes voices in track order;
   `render()` activates voices until it hits one whose start is in the
   future. Voices from later tracks that start earlier are silently dropped
   (WM1.1). A step sequencer that appends voices after the note voices would
   inherit this bug.
3. **The master chain never runs in realtime.** `chainScratchL`/`chainScratchR`
   are declared but never resized, so `chainScratchL.size() >= frames` always
   fails and the realtime FX path is dead (WM1.2). Any new groovebox effect
   must not depend on the chain working until that is fixed.
4. **`synthesize_preview` has a 180 s / 600 s cap** and clamps notes to 512
   steps. A 128-step pattern at 16 steps/bar is 8 bars — inside the cap, but
   the sequencer must not inherit the clamp.
5. **`song["bars"]` drives the loop length**, not the note content. A
   sequencer pattern longer than `bars` would be cut by the mixer.
6. **The piano roll is 1/16-note quantized** to song steps; a step
   sequencer's 16th-note grid maps exactly onto the existing step unit
   (4 steps/beat, 16 steps/bar), so the two editors agree without a new
   coordinate system.
7. **`MusicPanel.cpp` is over the soft cap.** Any new strip must be a new
   widget file plus a small wiring block in the panel.

## Target layout

New engine files (all Qt-free, in `engine/aur/audio/`):

| file                     | responsibility                                       |
| ------------------------ | ---------------------------------------------------- |
| `StepSequencer.hpp/.cpp` | pattern model, compile to events, swing/timing       |
| `Arpeggiator.hpp/.cpp`   | arp modes, rates, gate, octave, chord modes          |
| `GrooveBox.hpp/.cpp`     | groovebox global settings (swing, ratchet, humanise) |

New studio files (Qt side, in `studio/music/`):

| file                           | responsibility                            |
| ------------------------------ | ----------------------------------------- |
| `StepSequencerCanvas.hpp/.cpp` | the 128-step grid widget                  |
| `GrooveBoxPanel.hpp/.cpp`      | the performance strip (swing, arp, chain) |

Song document additions (`SongDocument`):

```jsonc
{
  "step_patterns": [
    {
      "name": "Pattern 1",
      "track": 0, // which song track this pattern plays
      "channel": 9, // MIDI channel (9 = drums)
      "program": 0,
      "length": 128, // steps in the pattern (1..128)
      "resolution": 4, // steps per sequencer cell (1,2,4,8,16)
      "seed": 12345, // deterministic seed for randomisers
      "steps": [
        // sparse: only non-empty cells are stored
        {
          "step": 0,
          "pitch": 36,
          "velocity": 110,
          "accent": 1,
          "slide": 0,
          "ratchet": 0,
          "tie": false,
        },
      ],
    },
  ],
  "groovebox": {
    "swing": 0, // 0..100, percent of the second half delayed
    "humanize": 0, // 0..100, deterministic timing jitter
    "arp": { "mode": "off", "rate": 4, "gate": 80, "octave": 1 },
    "chain": [0, 1], // pattern indices played in order
  },
}
```

## Phases

### P0 Safety net

- [x] Audit the engine, the song document and the studio layer against the
      DAW plan; record the verified gaps in this file.
- [x] Confirm the baseline builds and the suite is green before any change.
- [x] Add regression tests that pin the current behaviour of a song with no
      `step_patterns` key (bit-identical render, same PCM hash).

Acceptance: baseline `ctest` is 7/7 green; the audit notes above are in this
file.

### P1 Step-sequencer engine model

- [x] `StepSequencer.hpp/.cpp`: the pattern struct, the pattern reader over
      `song["step_patterns"]`, and the compiler that turns a pattern into a
      sorted event list (`step`, `pitch`, `velocity`, `length`).
- [x] Swing: a compiled pattern's odd cells are delayed by
      `swing · (cell/2)` of a cell; swing is a groovebox global.
- [x] Humanise: a deterministic per-step jitter derived from the pattern
      seed, scaled by the humanise amount.
- [x] Ratchet: a step with `ratchet = n` fires n times inside its cell.
- [x] Accent: a step with `accent` scales its velocity.
- [x] `SongDocument` accessors: `step_patterns()`, `step_pattern_at()`,
      `set_step_pattern()`, plus `groovebox()` / `set_groovebox()`.
- [x] `SongDocument::migrate` leaves a song without the keys untouched.
- [x] Tests: compile a known pattern, assert the event list; assert swing
      shifts the odd cells; assert ratchet multiplies; assert humanise is
      deterministic for a given seed and differs between seeds.

Acceptance: `tests/test_step_sequencer.cpp` green; a song without the keys
compiles to zero events.

### P2 Sequencer playback in the engine

- [x] `RealtimeMixer`: compile the patterns at the song swap and schedule
      their voices alongside the note voices. The compile happens in
      `schedule()` (the command boundary), never inside the per-frame loop.
- [x] Fix the unsorted `upcoming` deque while the mixer is being touched:
      sort by start frame at build time so later tracks are not dropped.
- [x] `synthesize_preview`: render the compiled pattern events too, so the
      offline export matches the live preview.
- [x] The pattern's length drives the song's loop length when the song has
      no arrangement placements, so a 128-step pattern loops as written.
- [x] Tests: realtime/offline parity for a pattern-bearing song; a pattern
      that starts on step 0 in a later track is audible.

Acceptance: parity within 5e-04 relative; the new pattern tests green; the
existing golden PCM hashes unchanged for songs without patterns.

### P3 Arpeggiator

- [x] `Arpeggiator.hpp/.cpp`: modes `off, up, down, updown, random, chord,
    asplayed`; rates 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32; gate 1..100 %;
      octave span 0..3; hold.
- [x] The arpeggiator turns a held chord (or a pattern's pitches) into a
      note stream that is compiled into the same event list as the pattern.
- [x] Random mode uses the pattern seed so it is deterministic.
- [x] Tests: each mode over a known chord yields the expected pitch order;
      the gate ratio changes the note length; random mode is reproducible.

Acceptance: `tests/test_arpeggiator.cpp` green.

### P4 Groovebox globals

- [x] `GrooveBox.hpp/.cpp`: swing, humanise, master groove (a fixed
      velocity/timing offset), and the chain (pattern order).
- [x] `SongDocument::groovebox()` / `set_groovebox()` with defaults.
- [x] The chain plays pattern A for its length, then pattern B, then loops.
- [x] Tests: the chain order produces the expected merged event sequence;
      defaults read as neutral.

Acceptance: `tests/test_groovebox.cpp` green.

### P5 Studio: the 128-step grid

- [x] `StepSequencerCanvas`: a painted grid, one row per pattern, 128
      columns, cells drawn by state (empty / on / accent / slide / ratchet).
- [x] Click toggles a cell; drag paints; right-click clears; the pitch of a
      cell comes from the selected track's instrument or from a per-pattern
      pitch column.
- [x] Keyboard: arrow keys move the cursor, Space toggles, Delete clears,
      Page Up/Down change the pitch.
- [x] Undo: every edit goes through `SongHistory`'s track-snapshot path.
- [x] The canvas is a new file; `MusicPanel.cpp` gains only wiring.

Acceptance: the widget builds; the panel shows a "Step Sequencer" tab.

### P6 Studio: the groovebox performance strip

- [x] `GrooveBoxPanel`: swing, humanise, arp mode/rate/gate/octave, chain
      order, pattern length, and a Play/Stop pair bound to the mixer.
- [x] A "Perform" strip with the pattern's mute/solo buttons.
- [x] The strip is keyboard-navigable and every action is reachable without
      a pointer.
- [x] `MusicPanel` wires the panel to the mixer and to the song document;
      the file does not grow past its current size (extraction if needed).

Acceptance: the panel builds; changing swing changes the audible timing in
the offline render.

### P7 Retire the three oscillator synths

The GM/FM/RetroPSG oscillators in `engine/aur/audio/VoiceCore.hpp/.cpp` are the
oldest synths in the engine and the least complete: no filter, no velocity
layers, no per-part sends, and three hand-written waveforms that approximate
GM families by index arithmetic. The hardware cores (YM-FM, SPC) replaced them.

- [x] Delete `engine/aur/audio/VoiceCore.hpp` and `.cpp`; drop them from
      `CMakeLists.txt`.
- [x] `PreviewSynth` keeps `MdFm` and `Spc` only. `Gm`, `Fm` and `RetroPsg`
      are removed, and their song ids (`gm`, `fm`, `retro_psg`) fall through to
      the YM-FM core so old songs still play.
- [x] `RealtimeMixer`: the oscillator voice list goes away. Every track's
      notes become `CoreNoteEvent`s for the single `SynthCoreVoice`, so the
      mixer has one rendering path.
- [x] `synth.cpp`: the oscillator render loop goes away; `synthesize_preview`
      always renders through the core path.
- [x] The step-sequencer events join the same core event list, so the
      groovebox plays through the same instrument as the piano roll.
- [x] `MusicPanel`: the preview-synth dropdown lists the cores only.
- [x] Tests: the golden hash, pitch and parity tests for the removed synths
      go with them; the core-synth versions stay.

Acceptance: the build is green, `ctest` is green, and a song with no
`preview_synth` key still renders.

### P8 Close-out

- [ ] Update `Bible/notes/daw/plan.md` (P9 becomes landed), `WANTED.md`,
      `HANDOVER.md` and `Bible/Operations/NOTES.md`.
- [ ] Full `ctest` green, `python C:\Projects\tools\conformance.py` green.
- [ ] Record the verified block in the project's `PROJECT.md` if one exists,
      or in this plan's status line.

Acceptance: the estate conformance gate exits 0.

## Acceptance

- A song with no `step_patterns` renders bit-identically to before the
  change (the same golden PCM hash).
- A 128-step pattern plays in real time and in the offline export, and the
  two agree within the documented tolerance.
- The grid, the strip and the engine are covered by tests in `tests/`.
- Every new file is under the 750-line soft limit; no existing file over the
  limit grew.
- `ctest` is green and the estate conformance gate exits 0.

## Rollback

Revert to the last `groovebox-pN` tag. The song format keys are additive, so
a reverted build ignores them; the old render path is untouched.

## Decisions and deviations

- **The sequencer is a song-level feature, not a clip type.** The DAW plan
  says "128-step clip type compiling to events". A clip lives inside a scene
  and a scene inside a track, which would make the sequencer a per-track
  sub-editor with no standalone play. A song-level `step_patterns` array
  plays on its own, can be chained, and can be exported. Deviation recorded.
- **The sequencer grid reuses the song's step unit** (4 steps per beat,
  16 per bar). A 128-step pattern is therefore 8 bars of 16th notes, which
  is the usual hardware-groovebox reading of "128 steps", and it lines up
  with the piano roll and the arrangement canvas.
- **Swing lives on the pattern's compiled events**, not on the mixer, so the
  offline path gets it for free and the two never disagree.
- **The arpeggiator compiles to events too**, rather than running as a
  per-frame effect, so it is deterministic and offline/live identical.
