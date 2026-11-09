# Synth cores delivery — verification notes (2026-11-08)

## What was delivered

The frontier model's MD/SPC synth drop lives in `reference/synths/` (not yet
integrated into `engine/aur/audio/`). Contents: `YmFmCore` (6ch x 4op FM, DAC,
SSG-EG, LFO, 13 built-in patches), `SnesPcmCore`/`BrrCodec`/`SnesEcho` (BRR
codec, Gaussian interpolation, 8-tap FIR echo), an FX rack (chorus/flanger,
ping-pong delay, reverb, CC91/93/94 sends, bit-exact bypass), three plain-assert
test suites, a WAV demo renderer, a `build_and_test.sh` and a CMakeLists.

## Verification on this machine (MinGW g++ 16.1, `C:\Dev\mingw64`)

- All three suites build clean at `-O2 -Wall -Wextra -Wpedantic` and pass.
- CMake + Ninja + CTest: 3/3 pass. Note: MinGW runtime DLLs must be on PATH or
  every test exits `0xc0000135` (missing DLL, not a test failure).
- `-O0` builds pass for all three suites.
- UBSan (`-fsanitize=undefined -fsanitize-trap=undefined`, trap mode needs no
  runtime lib): clean on all three suites. ASan is NOT available in this MinGW
  distribution (no libasan/libubsan runtime) — the frontier model's ASan claims
  are unverified here; MSVC ASan is the fallback when integrating.
- Demo renderer runs; WAVs regenerate in the output dir. Stats (python check):
  both files ~418,918 frames (9.5 s), peak 0.548 (YM) / 0.360 (SNES), RMS
  0.154 / 0.132, 100% nonzero. Finite, no clipping, audible levels. Not yet
  listened to by a human.
- g++ 16.1 emits `-Wmismatched-new-delete` noise against the tests'
  `operator new`-counting shim (test replaces `operator delete` with `free`);
  the tests already suppress it via `#pragma GCC diagnostic ignored`. Harmless.

## Known gaps before integration (DAW P7b/P7c)

- Frontier model built with g++ 13 only; MSVC and clang untested. MSVC build
  must be verified against the Studio build (VsDevCmd, see HANDOVER build notes).
- Not bit-exact to silicon: YM EG uses average per-tick increments; SSG-EG
  ignores SL; Gaussian table is a fitted curve (`setGaussTable()` accepts the
  exact 512-entry ROM table); echo FIR runs at host rate, not 32 kHz.
- FX constants are structural starting points, untuned by ear.
- README's deliberate interpretations (48-byte patch layout + 55-byte sysex
  frame; 2-bit panning; `allSoundsOff`/`allNotesOff` split) are recorded in
  `reference/synths/README.md`.
- Line counts: all files under 400 lines as required by the WANTED entry.

## Files/layout

Everything is co-located in `reference/synths/` (headers beside sources);
tests beside sources rather than `tests/` — acceptable for a reference drop,
move tests to `tests/` when integrating into the engine.

## Defect: GM patches an octave high (fixed 2026-11-08, `cc5740d`)

Reported by the owner as "MIDI imports sound like ass, instruments reversed /
all wrong", appearing 2-3 days before 2026-11-08.

Cause: in `engine/aur/audio/VoiceCore.cpp` the `phase` local is derived as
`voice.phase[0] / (2*pi)` (cycles, 0..1), but `voice.phase[0]` itself is
radians (0..2*pi). Families 5/6 (organ, pad) and the parity branches of 10 and
14 built their ramp straight from radians while the surrounding code assumed a
0..1 cycle, so the ramp swept twice per cycle. 32 of 128 GM programs therefore
rendered an octave above the requested note; program 56 (trumpet) came out a
fifth high. Program 48 (strings) measured 882 Hz for A4.

Detection note: zero-crossing counting is **not** reliable here — harmonically
rich timbres at the correct pitch report double the crossings, so an earlier
probe wrongly showed the whole library an octave high and a second probe
wrongly showed everything at 220 Hz (sub-harmonic lock). Pitch must be
estimated by normalised autocorrelation preferring the shortest lag, which is
what `tests/test_synth_pitch.cpp` does.

Fix: use the normalised phase for every ramp/parity term. Golden hashes are
unchanged because the golden song only uses a patch family that was already
correct; the new pitch tests cover the rest of the library.

Regression guard: `test_synth_pitch_all_programs` (all 128 programs must sound
A4 at 440 Hz) and `test_synth_pitch_follows_note` (pitch tracks C4/A4/C5/C6
across patch families).
