# SC (Sound Canvas-style) clean-room rewrite: plan

Status: planning only — long-term M2/M3 companion to the DAW initiative (`Bible/notes/daw/`). No code yet. Owner reference material: `Bible/notes/SC/` (decompiled SCVA parameter dumps, structural scratchpad only) and `Bible/notes/chat-VST and SC.txt` (goals: SC-88/Pro-style GS engine + JV-1080-style structured synthesis; target the Capcom PS1-era sound).

## Goal

A clean-room, licence-safe GS/rompler instrument for the DAW: SC-88 Pro-style architecture (16-part multi-timbral mixer, per-part reverb/chorus sends, insertion EFX routing, GS CC/SysEx response) with modern internals (ZDF filters, user-sample support, unbounded polyphony optional). "Sound Canvas-like" is workflow/behaviour only; no Roland samples, firmware, or decompiled code may be copied into the repo.

## Contracts (binding)

1. `namespace aur::audio`, Qt-free, files under `engine/aur/audio/`, one responsibility per file, <400 lines each.
2. Audio-callback safety: no locks, allocation, logging, or Qt in the render path (DAW plan contract 4).
3. Deterministic render (DAW plan contract 5); noise must come from a seeded PRNG owned by the instance.
4. All host-settable parameters via explicit setters + a (name, min, max, default) parameter table used to generate Studio UI bindings.
5. Licence: no Roland PCM/ROM content, no decompiled-code translation; every bundled asset's licence recorded in `Bible/` (DAW plan contract 9).
6. Instruments implement the shared `IInstrument`-style seam established by VoiceCore/RealtimeMixer so they drop into the same mixer.

## Current shape (verified)

- `engine/aur/audio/VoiceCore.{hpp,cpp}`: per-sample DSP with Gm/Fm/RetroPsg timbres; golden PCM hash tests pin current output (`tests/test_synth_golden.cpp`).
- MIDI layer already carries SysEx through import/export (`tests/test_midi.cpp`), so GS SysEx parsing has a transport to plug into at P4/M2.
- Decompiled reference dumps sit under `Bible/notes/SC/` as read-only structural study material.

## Hazards found

- Licence: Roland pursues emu/ROM infringement aggressively; the decompiled dumps must inform *architecture understanding* only. Any phase that ships code must be written from public specs (SC-88 Pro MIDI Implementation manual, Freeverb-style DSP literature), never from the dump.
- The dumps are large binary-ish text; they must never be processed by repo tooling or committed into source trees (they live in `Bible/notes/` and stay there).

## Target layout

- `engine/aur/audio/GsPart.{hpp,cpp}` — one MIDI part (bank/program, CC state, sends).
- `engine/aur/audio/GsEngine.{hpp,cpp}` — 16-part mixer, voice allocation, per-part routing to system/insert EFX.
- `engine/aur/audio/GsEfx.{hpp,cpp}` — insert-effect algorithms (overdrive, hexa-chorus, phaser, auto-wah) as parameterised blocks.
- `engine/aur/audio/GsReverb.{hpp,cpp}` + `GsChorus.{hpp,cpp}` — comb/all-pass based system effects.
- `engine/aur/audio/GsSysEx.{hpp,cpp}` — address/data parser mapping GS SysEx to parameter setters (no allocation on the parse path).
- `engine/aur/audio/JvTone.{hpp,cpp}` — JV-style 4-tone structured layer (per-tone TVF/TVA, noise "air" sources).
- `tests/test_gs_engine.cpp`, `tests/test_brr_or_sample_path.cpp` — determinism hashes, CC/SysEx response, voice-stealing, effect parity offline vs realtime through the existing parity harness.

## Phases

### P0 Safety net
- [ ] Pin current GM-lite behaviour: golden hash for a GS-engine-bypass song through the existing mixer (exists via test_synth_golden; extend to a 16-part fixture).
- [ ] Fixture MIDI files with GS SysEx (reset, reverb macro, drum-kit swap) round-tripping through `test_midi` untouched.
Acceptance: suite green with fixtures committed. Tag `sc-p0`.

### P1 Part + parameter table
- [ ] `GsPart` with full CC state (1/7/10/11/64/71/72/73/74/91/93/98/99), bank select, drum flag.
- [ ] Parameter table + setter API per contract 4; UI-binding generation deferred to P3.
Acceptance: setter tests cover every table row. Tag `sc-p1`.

### P2 Voice + filters
- [ ] Sample-playback voice with band-limited interpolation, ZDF low-pass (TVF), multi-stage TVA with velocity-rate scaling.
- [ ] User-sample loading path (wav → instance-owned buffer) with licence check at load.
Acceptance: impulse/step response tests; deterministic hash. Tag `sc-p2`.

### P3 GS engine + EFX
- [ ] 16-part engine, voice stealing (release-stage/velocity policy), sends matrix, system reverb/chorus, insert EFX blocks.
- [ ] Offline-vs-realtime parity through the existing OfflineRenderJob harness.
Acceptance: parity within documented tolerance; soak without underruns. Tag `sc-p3`.

### P4 SysEx + drum kits
- [ ] GS SysEx parser → parameter setters (address `40 01 30`-style mapping), drum-kit variations, GS reset.
- [ ] Fixture MIDI files from P0 configure the engine end-to-end.
Acceptance: fixture songs play identically to hand-set parameters. Tag `sc-p4`.

### P5 Studio integration
- [ ] Parameter-table-driven editor panel, preset save/load into the project song JSON (additive keys, v2 migration untouched).
Acceptance: manual checklist recorded; instrument selectable per DAW track. Tag `sc-p5`.

## Acceptance

- All DAW-plan acceptance items still pass (this engine is additive, not a replacement).
- No Roland PCM, firmware, or decompiled-derived code in the repo (review gate).
- Deterministic hashes stable across runs and offline/live paths.

## Rollback

Revert to the last `sc-pN` tag; the GM-lite voices remain the fallback instruments at every phase (mirrors DAW plan contract on v1 fallbacks).

## Decisions and deviations

- 2026-11-07: plan created from owner chat notes; no code. Sequenced behind DAW P7a/P7b so the instrument lands when the mixer/effect seams exist. The decompiled dumps are study-only (licence hazard above).
