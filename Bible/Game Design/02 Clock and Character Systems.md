# Clock and Character Systems

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 65-109). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 3. THE 48-MINUTE CLOCK

**Structure:** 48 real-time minutes = 1 full in-game day cycle. Split into four 12-minute phases:

| Phase | Time | Lighting | Enemy Behavior | Music |
|-------|------|----------|----------------|-------|
| **Dawn** | 00:00–11:59 | Warm gold, long shadows, mist burning off | Passive. Patrols slow. Some enemies dormant. | Soft, hopeful. Lower BPM. Acoustic/strings forward. |
| **Day** | 12:00–23:59 | Full sun, crisp visibility | Standard aggression. Full spawn tables. | Full arrangement. Standard BPM. All stems active. |
| **Dusk** | 24:00–35:59 | Orange/purple. Shadows lengthen. | Increased patrol speed. Ambush chance rises. | Tempo shifts. Minor key swap. Percussion drops. |
| **Night** | 36:00–47:59 | Dark blue/silver. Volumetric fog. | Hyper-aggressive. Vision cones expand 50%. Rare spawns. | Darker arrangement. Kajiura-style pads. Slower BPM. |

**Rules:**
- Clock **pauses** during combat, dialogue, cutscenes, and menus.
- Clock **advances** via resting (camp/inn/tent), fast travel, or specific story triggers.
- Some events are **phase-locked** (e.g., Chrono Shard III only visible at Dusk in a specific alley).
- The clock is **real-time** but can be modded via `TimeManager.PHASE_DURATION` for accessibility.

---

## 4. STAT SYSTEM — THE ADSR/PWM MATRIX

**Core Stats (4 primary + 2 derived):**

| Stat Name | Synth Origin | RPG Function | Mechanical Role |
|-----------|-------------|--------------|-----------------|
| **Attack** | ADSR: Attack / PWM: Pulse Width | Raw offensive output | Base weapon damage × proficiency multiplier. Governs burst potential and armor penetration. |
| **Decay** | ADSR: Decay / PWM: Roll-off | Damage mitigation | Reduces incoming damage over time. Governs armor durability, status tick resistance, and stagger recovery speed. |
| **Sustain** | ADSR: Sustain / PWM: Duty Cycle | Endurance pool | Total stamina/mana capacity. Governs how long you can maintain combat actions, sprint, or channel spells before depletion. |
| **Release** | ADSR: Release / PWM: Modulation Depth | Critical/Elemental output | Governs crit chance, elemental burst damage, status application rate, and post-combat regen speed. |
| **Resonance** *(derived)* | Harmonic overtone | Synergy multiplier | Scales with affinity bonds, instrument-weapon combos, and party positioning. Multiplies team-wide output. |
| **Tempo** *(derived)* | BPM / Clock speed | Action economy | Governs turn order (turn-based), action gauge fill rate (real-time), and movement range (tactical). |

**Stat Growth:**
- Stats grow via combat use, job evolution, and mentorship bonds.
- No traditional "level up." Instead: **Proficiency Tiers** (Apprentice → Journeyman → Artisan → Master → Transcendent).
- Each tier unlocks new weapon skills, job evolutions, or instrument combos.
- The Elf Princess caps at **85% proficiency** across all 4 weapons (her inexperience flaw), while dedicated specialists hit 130–150%.

**UI Presentation:**
- Stats displayed as waveform visualizations (ADSR envelope shapes).
- Hovering a stat shows the synth glyph + a 2-line explanation.
- Players who know synthesis get the wink. Everyone else sees clean RPG numbers.

---

