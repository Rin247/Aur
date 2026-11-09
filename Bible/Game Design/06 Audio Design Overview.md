# Audio Design Overview

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 220-247). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 9. AUDIO DESIGN PHILOSOPHY

**Composer Inspirations (not fusion, but *vocabulary*):**
- **Motoi Sakuraba** → Warm symphonic prog rock. Hammond organ. Odd time signatures. Battle themes.
- **Nobuo Uematsu** → Fast-paced progressive fantasy. Memorable melodies. Boss themes.
- **Yuki Kajiura** → Ethereal vocal pads. Intimate strings. Emotional beats. Night/exploration themes.
- **Hiroyuki Sawano** → Epic hybrid. Massive taiko. Choir. Phase 2 boss climaxes.
- **Falcom Sound Team jdk** → High-BPM jazz/rock fusion. Town themes. Energetic exploration.

**Adaptive Audio Rules:**
- All tracks exported as **stems** (Rhythm, Harmony, Melody, Ambience, Vocals).
- **FFXIV Model:** Time-of-day shifts crossfade stems. Dawn = acoustic forward. Night = pads forward, percussion drops.
- **Halo Model:** Spatial layering. Caves muffle via LowPass filter. Dense forests mute percussion, boost strings. Combat swells rhythm + brass.
- **Boss Escalation:** Boss music is the area theme "taken one step further." Same key, same melody, but heavier instrumentation, faster tempo, added choir.
- **Instrument-Weapon Synergy:** Rhythm inputs during combat trigger temporary stat buffs AND shift the audio mix (e.g., landing a drum-beat combo brings the percussion stem forward for 8 seconds).

**VST Toolkit:**
- Arturia Pigments (synths, pads, arps)
- Arturia B-3 V (Sakuraba organ)
- Ample Sound / Shreddage 3 (guitars)
- Spitfire Audio (orchestra, chamber strings)
- Heavyocity DAMAGE 2 / Vocalise (Sawano percussion, Kajiura vocals)
- Spectrasonics Trilian / Modo Bass (Falcom slap bass)
- GetGood Drums / Steven Slate Drums (rock kits)
- SWAM Saxophones (jdk brass)

---

