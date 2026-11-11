# DAW: vendored third-party dependencies

Licences are recorded here because `Bible/` is this project's own reference;
the estate third-party table in the root `AGENTS.md` holds only checkouts that
were *read*, not dependencies that ship inside the project.

| dependency | version | licence | vendored | used by | notes |
|---|---|---|---|---|---|
| TinySoundFont | 0.9 | MIT | 2026-11-11, `engine/aur/audio/tsf.h` | P7a `Sf2Core` | Single-header C synthesizer. Licence header kept verbatim in the file. Not thread-safe — all calls from the audio thread via `SynthCoreVoice`. Known limits per its own header: no modulators, no chorus/reverb send generators (the existing `FxRack` supplies CC91/93/94 sends), basic low-pass filter. Upstream: <https://github.com/schellingb/TinySoundFont> |
