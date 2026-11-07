# DAW: plan

Status: P3 closed-out; P4.1–P4.3 MIDI device layer + count-in landed
(`f1420e3`, `1146cf8`, `4f07f1b`). P6.1 clips landed (`7d0b8da`). P5 started:
P5.1 lane accessors (`532de0b`) + P5.2 lane strip (`721e043`) landed.
Remaining in P4: clip/arrangement export (lands with P6). Next: P5 remainder
(multi-select/marquee, copy/paste, snap/quantise, scale highlight, zoom,
playhead, audition) or P6 remainder (launch quantisation, arrangement).

Master plan for the Music Studio DAW initiative, seeded from the owner-supplied
gameplan and corrected by a source audit on 2026-11-06. One note file per major
decision lands beside this one (`decisions.md`, then per-phase notes as they
run). Phase commits use `[daw-pN.S]` messages and `daw-pN` tags; work one
milestone at a time and keep the app releasable.

## Goal

An Ableton-like DAW for SNES/SPC and Mega Drive/DX7-style music inside Studio:
a Qt-free audio engine shared by Studio and the game runtime, a real-time
sequencer and mixer, session grid and arrangement, a proper piano roll,
lossless MIDI, a GM/GS-style instrument, FM and SPC-style instruments, an
effects suite, an arpeggiator, a 128-step sequencer, and a seeded scratchpad
generator.

## Audit corrections (verified against source 2026-11-06)

The supplied gameplan is mostly accurate. Corrections and additions:

1. **`MusicEngine` is dead code.** It exists only in `engine/aur/music.hpp`
   (22 lines) and nothing in the repo includes it or calls it — not
   `game.cpp`, not `field.hpp`, not Studio. The gameplan's "runtime must keep
   playing songs by id" describes an _aspiration_, not a current contract.
   There is no P11 call-site contract to preserve; P11 defines one fresh.
2. **Map→song reference exists as data but is unread.** `project.cpp:299`
   writes `m["music"] = nullptr` on every new map, and
   `project.cpp:92-97` loads `projects/<id>/music/*.json` into
   `Project::music`. `game.cpp` never reads `map["music"]`, and no map in
   `projects/sylvi/maps/` carries a non-null music id. P11 must wire the
   lookup, not preserve a broken one.
3. **The clamp claim needs sharpening.** `synth.cpp:235` clamps note step to
   `65535/120` = 546 and length to 512; `synth.cpp:219-221` clamps total song
   length to 4096 steps. `midi.cpp:395` clamps export step to
   `65535/ticks_per_step` = 546 (120 ticks/step at 480 PPQ). So: preview
   clamps at 546 steps _and_ 4096 total steps; export clamps at 546 steps.
   The UI's 128-bar (2048-step) range is inside the synth's 4096 cap, so the
   real user-visible bug is bar ~34+ truncation in both preview and export.
4. **The v1 song format carries two song-level keys the gameplan's contract
   list omits:** `gs` (bool, GS-flavour flag) and `reverb` (song-level reverb
   send, e.g. `4` in `overworld.json`). They must stay loadable.
5. `MusicPanel.cpp` is 1,430 lines total (not just the constructor); the
   ~390-line/39-connect constructor claim was not independently re-counted
   and is treated as approximate until P1 measures it.
6. `MidiDeviceIo.cpp` is 156 lines, WinMM, `MIM_DATA`-only — confirmed.
7. `tests/test_midi.cpp` already covers import/export basics; P0 extends it
   rather than starting from zero.

Sizes at audit: `MusicPanel.cpp` 1,430 / `.hpp` 146 / `synth.cpp` 290 /
`midi.cpp` 390 / `MidiDeviceIo.cpp` 156 / `music.hpp` 22. Everything must land
under the estate sizing rule (~1,000-line soft cap, +25% overhead).

## Contracts (binding)

1. `projects/<id>/music/*.json` loading stays (v1 reader kept); new format is
   v2 with migration + backup on first save.
2. Existing v1 keys stay readable: `name, tempo, bars, beats, loop_bar,
preview_synth, gs, reverb, tracks[{name, channel, program, bank, volume,
pan, reverb, chorus, drum, notes[[step,pitch,length,velocity]]}],
scenes[{name, clips}]`.
3. New engine lives in `engine/aur/audio/`, Qt-free; Studio and game link it.
4. Audio-callback safety: no locks, allocation, logging, or Qt in the callback;
   lock-free command queue in, immutable snapshots out, meters via atomics.
5. Determinism: same song + seed + sample rate → identical offline and live
   output (float tolerance documented in the phase note).
6. SMF Type 0/1 import/export keeps working; round-trip tests keep passing.
7. Offline render/export/generation run off the GUI thread with progress and
   cancel; exposed in-app and via MCP tools, never as a separate CLI.
8. Every edit command is undoable via a shared Studio command-stack (built in
   the common layer first, reused by the world editor).
9. Legal: no Roland/Sound Canvas samples or unlicensed content; every bundled
   asset's licence recorded in `Bible/`. "Ableton-like" is workflow only.
10. Tests live in `tests/`, updated in the same change as the code they cover.

## Milestones

- **M1 Foundation (P0–P6):** real-time engine, song v2, lossless MIDI, piano
  roll, session + arrangement, existing GM-lite/FM-lite/PSG voices in real time.
- **M2 Sound (P7a, P8):** GM/GS SoundFont instrument, effects suite.
- **M3 Style/tools (P7b, P7c, P9, P10):** DX7/OPN FM, SPC/BRR sampler,
  arpeggiator + MIDI effects, 128-step sequencer, seeded generator.
- **M4 Ship (P11, P12):** runtime integration, exports, plugin decision doc.

## Phases

### P0 Safety net — in progress

- [x] Failing test for the 546-step clamp (song with a note at step 1000,
      preview and export both truncate today) — pinned as a regression test at
      step 600 in `tests/test_song_clamp.cpp`; buffer length + first-sound
      position + MIDI round-trip, mapping-independent.
- [x] Golden render-hash tests over a fixture set of v1 songs through the
      current synth (deterministic PCM hash) — `tests/test_synth_golden.cpp`
      pins FNV-1a hashes for the Gm/FM/PSG voices (8 kHz, 30 s, fixed fixture
      song) plus replay determinism.
- [x] MIDI import/export round-trip tests for currently supported fields —
      already covered by `tests/test_midi.cpp` (PPQN 96/384/480/960, tempo,
      T.S., bank/program, volume/pan, drum channel, SysEx, re-import); the
      clamp regression adds the beyond-546 step round-trip.
- [x] Fix the clamp in its own commit with tests; keep `synthesize_preview`
      and track keys readable.
      Acceptance: green suite including the new tests; `daw-p0` tag.

P0 closed 2026-11-06: suite 28/28 green, tags `daw-p0.1` + `daw-p0.2`.
Note found during clamp work: the preview step→frame mapping renders a note
later than `step * sample_rate * 60/(bpm*4)` implies (factor-of-2 observed);
regression test is mapping-independent on purpose. Revisit when the real-time
engine (P2) defines the timing contract.

### P1 Extraction, no behaviour change

- [x] Move `synth`, `midi` into `engine/aur/audio/` (`aur::audio` namespace
      deferred — both files already live in plain `namespace aur`; naming a
      sub-namespace now would churn every call site for no P1 gain; revisit
      when the engine gains more modules). CMake + all includes updated;
      build green, suite 28/28, tag `daw-p1.1`.
- [x] Split `MusicPanel` into PianoRollCanvas (`studio/music/`, 310 cpp
      lines), MidiSession (`studio/music/`, device I/O + recording + output
      scheduling behind signals, 370), SongLibrary (`studio/music/`, track/
      scene/quick-song/WAV helpers, 190) + a 921-line MusicPanel host.
      Sizes recorded 2026-11-07; build green, suite 28/28, `daw-p1.2`.
      Note: MusicPanel host is still above the 750-line soft cap — the
      constructor (~380 lines of UI wiring) is the next extraction candidate
      when P2+ touches this code; do not let it grow.
- [x] `MusicEngine` stays as-is (unused) until P11.
      Acceptance: build + tests green, no behaviour change, sizes recorded —
      P1 closed 2026-11-07.

### P2 Real-time engine

Sequencer + mixer + audio output behind `IInstrument`; replace WAV +
`QMediaPlayer`; MIDI out from the same clock; offline render on a worker
thread through the same code. Acceptance: soak without underruns, measured
latency recorded, offline/live hash equality.

Status 2026-11-07 (updated, second pass): MIDI-out now derives its event
clock from the mixer's playhead atomics (`playheadFrames`/`playheadBpm`/
`playheadActive`, anchored at start, converted via
`frames·60000/(bpm·4·rate)`) — hardware MIDI no longer drifts against the
audio. `resetPlayback()` rewinds the playhead on stop/restart. Offline
render is threaded: `OfflineRenderJob` (start/cancel/progress,
cooperatively cancelled between voice frames), tested sample-exact against
the synchronous path plus a 600 s cancel case — suite 30/30, `daw-p2.5`,
`daw-p2.6`.
Remaining before close: manual soak + latency measurement in Studio (owner
GUI run, unautomatable from here). Tag `daw-p2` only after the soak.

### P3 Song v2 + undo

Tick time base, tempo/time-signature maps, markers, loop points, first-class
clips/slots/scenes/arrangement, automation lanes, per-note properties; v1
migration with backup; typed commands on the shared undo stack; no 128-bar/
4096-step caps.

Progress 2026-11-07:
- [x] P3.0 v1 contract pinned (`test_song_v1.cpp`, overworld + inn
      flavours) — `daw-p3.0`.
- [x] P3.1 shared `studio/CommandStack` extracted;
      `WorldCommandHistory` rebased, no behaviour change — `daw-p3.1`.
- [x] P3.2 `SongDocument` v2 model: additive migration (format/
      ticks_per_step/tempo_map/time_sig_map/loop/markers), `.bak` backup
      save, map accessors with v1 fallbacks — `daw-p3.2`.
- [x] P3.3 `SongHistory` typed undo/redo for note and track edits on the
      shared stack (`test_song_history.cpp`, incl. 100-deep bounded
      history) — `daw-p3.3`.
- [x] Remaining for P3 close: wire SongHistory into PianoRollCanvas/
      MusicPanel edits (replacing destructive mutation), per-note
      properties, automation lanes, first-class arrangement clips.
      (P3.4–P3.5 landed 2026-11-07: SongHistory wired into the piano roll and
      panel with Ctrl+Z/Y and undoable track props; Alt+drag velocity editing
      with velocity shading; SongDocument per-note prop API + tests. Automation
      lanes and arrangement clips moved into P6 where their editors live.)

### P4 MIDI fidelity

Lossless import (ticks, tempo/T.S. maps, CC, pitch bend, aftertouch→automation,
SysEx, markers), export for arrangement/clip/tracks, device layer with
timestamps + SysEx input, MIDI record with count-in; round-trip tests.

Progress 2026-11-07:
- [x] P4.1 import/export: tempo_map + time_sig_map (all events, not just the
      first), markers (meta 0x06), CC lanes (all controllers), pitch bend and
      channel/poly aftertouch folded into automation lanes
      (track["automation"][{cc,points:[{step,value}]}]; bend=128,
      pressure=129), full export/import round-trip tests incl. a hand-built
      synthetic MIDI file — `daw-p4.1` (`f1420e3`), suite 35/35.
- [x] P4.2 device layer: input timestamps (`messageReceived(msg, timestampMs)`
      from MIM_DATA's ms stamp), SysEx input via four prepared 4 KiB
      MIM_LONGDATA buffers recycled after delivery (`sysExReceived(data, ms)`
      signal, buffers re-queued on MidiDeviceIo), record count-in in
      MidiSession (`setCountInBars`, recordStep offset past the count-in,
      realtime events dropped during count-in, status shows remaining
      seconds) — `daw-p4.2` (`1146cf8`), suite 36/36.
- [x] P4.3 MusicPanel count-in control (QSpinBox 0–8 bars, default 1, applied
      on record arm) — `daw-p4.3` (`4f07f1b`).
- [ ] Remaining: clip/arrangement export (lands with P6 clips).

### P5 Piano roll

Custom model-view, stable note ids, multi-select/marquee, move/resize/
duplicate, copy/paste, snap incl. triplets, quantise, velocity + automation
lanes, scale highlight, full pitch range, zoom, playhead, audition — all
undoable.

Progress 2026-11-07:
- [x] P5.1 automation lane accessors in SongDocument (`lane_at/_mut/
      create_lane/lane_points/set_lane_point/clear_lane_point/lane_value_at`,
      bend=128, pressure=129) with `test_automation_lanes.cpp` — `daw-p5.1`
      (`532de0b`), suite 37/37.
- [x] P5.2 VelocityLaneStrip (`studio/music/`) docked under the piano roll:
      velocity ladder per note-on step + one row per automation lane
      (CC/bend/pressure), click/drag to draw points, bend lanes render
      signed/centred and clamp to ±8192; all edits go through
      `begin_track_edit`/`commit_track_change` per the P6.2 undo-path rule —
      `daw-p5.2` (`721e043`).
- [x] P5.3 + P5.4 marquee multi-select (drag on empty grid, Ctrl to add),
      group move/resize/velocity-drag on the whole selection, Delete removes
      all selected, Ctrl+click toggles, copy/paste (Ctrl+C/V, paste at step 0
      of the copied span) and duplicate (Ctrl+D, appended one span right) —
      `daw-p5.3+p5.4` (`70113b3`), suite 37/37. Note creation moved to
      Shift+click (plain click on empty grid now marquees); documented in
      the piano-hint label update that follows in the UI pass.
- [x] P5.5 snap grid incl. triplets + quantise: Ctrl+G cycles off → 1/16 →
      1/8 → 1/4 → triplet (bar thirds) → off, indicator painted top-right;
      snapped() applies to note create and move; Ctrl+Q quantises the
      selection to the current grid as one undoable NoteEdit —
      `daw-p5.5` (`0927a76`).
- [x] P5.6 playhead overlay + audition: PianoRollCanvasWidget::
      attachPlayhead reads the mixer's playheadFrames/Bpm/Active atomics on a
      33 ms QTimer and draws a step-positioned line; RealtimeMixer::audition
      queues a one-note Voice applied at the render command boundary, wired
      to note-create clicks — `daw-p5.6` (`88410f8`).
- [x] P5.7 zoom + pitch window: Ctrl+wheel stepped zoom levels
      (8/12/16/24/32/48 px per step, zoom swept as a control, never an
      axis), full MIDI pitch range 12–107 with plain-wheel pitch-window
      scrolling via pitchOffsetRows; stepWidth() accessor keeps the scroll
      mapping correct at every zoom — `daw-p5.7` (`aead0a2`).
- [x] P5.9 stable note ids + scale highlight: `SongDocument::note_id/
      ensure_note_id` (id lives in note_props["id"], lazily assigned,
      monotonic counter, v1 notes read as −1); scale highlight from
      song-level `key_root`/`key_scale` (13-mode table, bit i = degree i
      above root) tinting the in-scale piano keys — `daw-p5.9` (`9e03b9d`).
      Key/scale selector UI lands with the song-properties editor.
- [x] P5.11 key/scale selector UI: `ScaleSelector` (`studio/music/`) row under
      the lane strip — root combo (C..B) + scale combo (13 modes, "—" =
      none); writes song `key_root`/`key_scale` via MusicPanel (both keys
      erased on "none"), synced from the song on every load via
      `syncScaleSelector` — `daw-p5.11` (`536cf54`).
- [x] P5.12 id-stable selection: `NoteIndex` (`studio/music/`, header-only)
      maps note-array indices ↔ stable ids; undo/redo in the piano roll now
      convert the selection to ids before the mutation and back to indices
      after, so the selection survives reordering — `daw-p5.12` (`6c18073`).
- [x] P5.14 ids on note create/paste/duplicate: Shift+click create calls
      ensure_note_id with the canvas's `nextNoteId` counter; paste/duplicate
      assign fresh ids to the created notes (the copied ClipNote's source id
      is deliberately NOT reused — duplicates are new notes); setSong seeds
      `nextNoteId` past the max existing id so loaded songs never collide —
      `daw-p5.14` (`ce7274b`).

### P6 Session and arrangement

Painted clip grid with launch quantisation, queued/playing states, scene
launch/stop, record into slot, clip edit in piano roll; arrangement timeline
with placements, loop brace, markers, tempo/T.S. lanes.

Progress 2026-11-07:
- [x] P6.1 first-class clips: additive fields over the v1 `{notes}` clip shape
      (name/start/length/muted/color) with `SongDocument::clip_at/_mut/
      clip_info/set_clip_info` accessors (defaults for v1 songs), session grid
      buttons now show clip name + bar count + note count, new clips get
      first-class fields — `daw-p6.1` (`7d8b8da`... see git log),
      `test_song_clips.cpp`, suite 36/36.
- [x] P6.2 automation lanes land via the P4.1 `track["automation"]` model
      (bend=128, pressure=129 lanes, import/export round-tripped); undo wiring
      uses the P3.4 `begin_note_edit`/`commit_note_edit` path because lanes live
      on the track object alongside notes — a NoteEdit commit snapshots only
      notes, so lane edits must go through `begin_track_edit`/
      `commit_track_change` (TrackProps). No lane *editor* UI yet; that arrives
      with the P5 piano-roll velocity/automation lane strip.
- [ ] Remaining: launch quantisation + queued/playing states, scene record
      into slot, arrangement timeline with placements/loop brace, lane editor UI.

### P7a GM/GS instrument

SoundFont sampler core (interpolation, envelopes, filter, loops, velocity
layers, modulator subset), 16-part GS model (bank select, drum kits, CC
1/7/10/11/64/91/93/94, RPN/NRPN, GS SysEx reset), per-part effect sends;
bundled redistributable SoundFont with licence recorded; GM-lite fallback with
a visible message.

### P8 Effects

`IEffect` framework (parameters, automation, tempo sync, bypass), built-ins
(delay, reverb, chorus, flanger, phaser, EQ, filter, compressor, limiter,
distortion/bitcrusher, tremolo, widener, SNES echo FIR), send buses, master
chain, meters, CPU budget; deterministic offline parity tests.

### P7b FM and PSG

DX7-style 6-op (32 algorithms, `.syx` import), YM2612-style 4-op + DAC,
SN76489 PSG, real-parameter patch editors.

### P7c SPC-style sampler

BRR encode/decode, sample import with loop points, ADSR/GAIN, 8-voice limit,
echo FIR emulation, hardware-profile warnings. SPC file export and sound
driver are out of scope (research note only).

### P9 MIDI effects + step sequencer

Arpeggiator (up/down/up-down/random/chord/as-played, rate incl. triplets, gate,
octave, swing, hold), chord/scale/random/velocity/length effects; 128-step
clip type compiling to events.

### P10 Generator

Seeded scratchpad generators (drums, bass, chords, arps, motifs, fills) from
style profiles (Overworld, Battle, Village, Dungeon, Boss, Town, Title,
Victory) and hardware profiles; parameters seed/key/mode/tempo/bars/density/
complexity/swing; reroll per part; seed stored in clip metadata; scratch slots
only, never a full arrangement; deterministic + musical-constraint tests;
in-app panel + MCP tool; replaces `generateQuickSong`.

### P11 Game integration and export

Runtime `MusicEngine` becomes a façade over the engine (play by id, loop
points, transitions, layers); wire the unread `map["music"]` lookup; WAV/OGG/
stems with loop-aware tails; instrument/sample packaging; MIDI export of any
clip or arrangement.

### P12 Plugin decision + cleanup

CLAP/VST3 wrapping + hosting recommendation doc; stale docs into `Bible/`;
size limits met; plan trimmed.

## Acceptance

- No-GPU/no-audio-device suite green: v1/v2 migration, MIDI round-trips,
  sequencer timing, launcher state machine, instrument/effect determinism,
  generator constraints, undo/redo per command type.
- Soak: long-session glitch-free playback, measured latency recorded.
- Pre-change songs load, play and export identically (or within documented
  tolerance).
- Manual checklist recorded: record from keyboard with accurate timing,
  launch clips with quantisation, edit a clip while playing, change an effect
  during playback, export/re-import MIDI, generate + reroll a scratchpad clip.

## Rollback

Revert to the last `daw-pN` tag; fix the plan before retrying. The v1 reader
and GM-lite voices stay until P11 acceptance.

## Decisions and deviations

See `decisions.md` (settled with evidence before code; updated as phases run).
