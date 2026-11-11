# DAW P7a: GM/GS SoundFont instrument — plan

Status: P0 landed (`daw-p7a.1`). Next: P1 — the `Sf2Core` adapter gets wired
into `RealtimeMixer` and `synthesize_preview` as `SynthCoreVoice::Kind::Sf2`.

## Goal

The preview path renders through the two hardware cores only, after the
oscillator synths were retired on 2026-11-11. That makes GM imports sound like
a YM-FM chip: the FM core folds every track onto 6 monophonic channels and
maps programs by `program % 13`, which is the exact defect the DAW plan's
work-package audit ranked as the top cause of "MIDI imports sound wrong".
P7a adds a sampled GM/GS instrument so a General MIDI file plays back with
roughly the right instrument per channel and drums on channel 10.

## Contracts (binding)

1. **Engine stays Qt-free.** The core lives in `engine/aur/audio/`, uses only
   the standard library and `aur/json.hpp`, and follows the audio-callback
   rules (no locks, allocation, logging or Qt inside `render`).
2. **Licence verified before anything lands.** No `.sf2` enters the repo until
   the licence text is on disk beside it. Candidates:
   - Loader: **TinySoundFont** (`tsf.h`), MIT, single header —
     [schellingb/TinySoundFont](https://github.com/schellingb/TinySoundFont).
   - Bank: **MuseScore General** — MIT, per-sample copyright list published
     ([MuseScore handbook](https://musescore.org/en/handbook/2/soundfonts-and-sfz-files),
     [Debian copyright file](https://people.debian.org/~tg/sf/MuseScore_General_Lite.copyright)).
     GeneralUser GS was considered and rejected: the original author grants
     unrestricted use, but third-party re-hosts mix in Yamaha-copyrighted
     samples and are flagged as a licensing gray area
     ([Musical Artifacts](https://musical-artifacts.com/artifacts?tags=generaluser)).
3. **Additive song key.** `song["soundfont"]` = path string; absent key = the
   old core path, bit-identical. `preview_synth` gains an `"sf2"` value that
   selects the new core.
4. **Same IMidiCore-style interface** the hardware cores expose: `setup`,
   `noteOn/noteOff/controlChange/pitchBend/programChange/allNotesOff`,
   `renderFrame`. The mixer and the offline renderer drive it identically.
5. **Determinism (contract 5).** Same song + bank + rate → identical output.
   Golden-hash tests pin the output.
6. **Tests in `tests/`, same change as the code.**
7. **File-size rule.** New feature = new file. `MusicPanel.cpp` stays under
   its current size; no file over the soft limit grows.

## Current shape (verified)

- The engine already has the right seam: `SynthCoreVoice` (in
  `engine/aur/audio/SynthCores.hpp`) is a note-event adapter over the two
  hardware cores, with `setEvents`/`appendEvents`/`consumeUpTo`/`renderFrame`.
  `RealtimeMixer` and `synthesize_preview` both drive it, and the
  step-sequencer events join the same list. A sampler core slots in as a third
  `Kind`.
- `PreviewSynth` is `{MdFm, Spc}`; `coreKindForSong()` in `RealtimeMixer.cpp`
  maps the `preview_synth` string to a kind.
- No `.sf2` in the repo, no loader vendored.
- `MusicPanel` preview dropdown lists the two cores.

## Hazards found

1. **TinySoundFont is not thread-safe** — all calls must come from one thread
   (the audio thread via `SynthCoreVoice`, like the FM core).
2. **Loading the bank is expensive** (tens of MB parse + sample decompress).
   Must happen at song swap, off the audio thread (WM1.4 rule), then swap in.
3. **`tsf.h` is C, not C++** — wrap it in a C++ class in `SynthCores` style;
   do not include it in a header consumed by Qt translation units.
4. **Bank lookup order**: GM drums live on channel 10 (index 9). TinySoundFont
   presets need the drum-kit preset selected explicitly per channel.
5. **Determinism risk**: `tsf` voice-stealing order must be pinned, or a
   dense MIDI file renders differently per run. Pin polyphony and check the
   golden hash is stable across runs.

## Target layout

| file                                         | responsibility                                                                       |
| -------------------------------------------- | ------------------------------------------------------------------------------------ |
| `engine/aur/audio/tsf.h` (vendored)          | the TinySoundFont implementation                                                     |
| `engine/aur/audio/Sf2Core.hpp/.cpp`          | `SynthCoreVoice::Kind::Sf2` adapter: bank load, channel setup, note/control dispatch |
| `engine/aur/audio/SynthCores.cpp` (extended) | dispatch to the new kind                                                             |
| `tests/test_sf2.cpp`                         | loader round-trip, note-on render, drums on ch10, golden hash                        |
| `reference/sf2/`                             | the bank + its licence file (git-ignored or LFS — decide at P1)                      |

## Phases

### P0 Safety net

- [x] Vendor `tsf.h` (MIT, licence header kept) and record it in the
      project's vendored-dependency table (`Bible/notes/daw/third-party.md`).
- [x] Generate a tiny test bank in code (one preset, one looped 440 Hz sine,
      no download) so tests run without a bundled bank.
- [x] `test_sf2.cpp`: load the generated bank, render a note, assert nonzero
      and deterministic (plus note-off silences). Suite 55/55.

Acceptance met. Two parser lessons recorded for P1's bank work: the pdta
chunk record sizes are enforced (`shdr` 46/record, `pmod`/`imod` 10, range
generators pack lo|hi in one u16), and zone bags delimit their gen ranges
cumulatively — the bag after the last zone must point one past the final gen.

### P1 Core adapter

- [ ] `Sf2Core` implementing the `SynthCoreVoice` surface for kind `Sf2`.
- [ ] Channel model: 16 channels, ch10 → drum preset; CC7/10/11/64/91/93/94
      routed; RPN 0 bend range honoured.
- [ ] Wire into `RealtimeMixer::schedule` and `synthesize_preview`.
- [ ] `preview_synth: "sf2"` selects it; a missing bank renders silence with a
      clear warning, never a different instrument.

Acceptance: parity test offline vs realtime; deterministic golden hash with
the generated bank.

### P2 Bank integration

- [ ] Add the bank path as a project setting (default `~/.Aur/sf2/`), not a
      compile-time constant.
- [ ] MusicPanel: sound-source dropdown gains "General MIDI (SoundFont)";
      a label shows which bank loaded, or "no bank found".
- [ ] Licence file recorded beside the bank; estate third-party table row.

Acceptance: Studio plays a multi-instrument GM file with the right families;
drums on ch10; no clipping at the limiter.

### P3 Close-out

- [ ] Update DAW plan P7a status, WANTED, HANDOVER.
- [ ] ctest green; conformance gate scoped run clean of new findings.

## Acceptance

- A multi-instrument GM file plays with roughly the right instrument per
  channel and drums on channel 10.
- The engine stays Qt-free; the callback does no allocation.
- Deterministic golden hash with the generated test bank.
- Old songs (`preview_synth` md_fm/spc/gm) behave exactly as before.

## Rollback

Revert to the last `daw-p7a` tag. The song key is additive; absent key = old
path untouched.

## Decisions and deviations

- **Loader: TinySoundFont (MIT, single header).** FluidSynth is LGPL and
  static linking it needs an explicit decision the owner has not made; a
  from-scratch SF2 reader is out of proportion. Recorded in `decisions.md`.
- **Bank: MuseScore General** (MIT, per-sample copyright list published).
  GeneralUser GS rejected over the Yamaha-sample gray area. The final bank
  choice stays owner-confirmable: the plan ships with the _generated_ test
  bank only, so nothing large lands until the owner signs off.
- **Interface note (deviation):** the DAW plan's work-package brief sketched
  a free-standing `IMidiCore`; the landed seam is `SynthCoreVoice::Kind`.
  Adapters stay thinner and the mixer already knows the shape.
