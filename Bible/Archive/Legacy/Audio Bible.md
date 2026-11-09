# 🎵 CHRONO TRILOGY — Audio Bible

> Companion to the World Building Bible v1.0 & Combat Design Doc v1.0
> DAW: Bitwig Studio
> Primary Synth: Arturia Pigments
> Additional: Korg & Yamaha VSTs, Arturia Collection
> Adaptive Model: FFXIV (time-of-day) + Halo (layered stems) + CT (area motifs)
> Engine: Godot 4.x Audio Buses + AnimationPlayer crossfades

---

## 1. Audio Philosophy

> **The music is a living character.** It breathes with the
> 48-minute clock, reacts to the player's footsteps, escalates
> with combat intensity, and remembers which Era you are in.
> It never simply "loops." It evolves.

Three guiding principles:

1. **Seamlessness.** The player should never hear a hard cut.
   Every transition — exploration to combat, day to night,
   forest to cave — is a crossfade, a stem swap, or a filter
   sweep. No jarring stops.

2. **Identity through motif.** Each Era, each major character,
   and each game in the trilogy carries a core melodic motif.
   These motifs are rearranged, fragmented, inverted, and
   layered across every track, the way Martin O'Donnell and
   Michael Salvatori wove the Halo theme through every genre
   from orchestral to electronic.

3. **The clock is the conductor.** The 48-minute cycle is not
   just a gameplay timer. It is the metronome of the world.
   Dawn sounds different from Night. Dusk sounds different
   from Day. The BPM, instrumentation, and emotional color
   shift with every phase.

---

## 2. Composer Inspirations & Style Guide

The score draws from five distinct voices. No single track
fuses all five. Instead, different scenes and systems lean
into different influences.

| Composer | Core Sound | Where It Lives |
|----------|-----------|----------------|
| **Motoi Sakuraba** | Warm symphonic prog rock. Hammond organ, odd time signatures, lush string pads over driving guitar. | Boss battles. Major story cutscenes. Era 3 (Industrial). |
| **Nobuo Uematsu** | Fast-paced progressive rock meets sweeping fantasy melody. Piano-driven hooks, tempo shifts, emotional climaxes. | Main overworld theme. Turn-Based combat (Dawn / Game 1). Towns. Credits. |
| **Yuki Kajiura** | Ethereal vocal pads, intimate chamber strings, haunting choirs, minor-key beauty. | Emotional scenes. Romance / Support conversations. Era 1 (Ancient). Night phase. |
| **Hiroyuki Sawano** | Epic hybrid orchestral-electronic. Massive taikos, growling synth bass, drop-tuned guitars, vocal shouts. | Tactical combat (Day / Game 2). Super-bosses. The "Real" Ending. |
| **Falcom Sound Team jdk** | High-BPM jazz-rock fusion. Slap bass, sax solos, bright synths, funk grooves. | Real-Time combat (Dusk / Game 3). Towns in Era 2 (Classical). Minigames. Tavern scenes. |

### 2.1 The "No Fusion" Rule
Each track should lean into **one primary influence** with at
most one secondary accent. Examples:

- ✅ A boss theme that is 80% Sakuraba prog rock with a Kajiura
  choir pad underneath.
- ✅ A town theme that is 90% Falcom jdk jazz with a Uematsu
  piano melody on top.
- ❌ A track that tries to be Sakuraba + Sawano + Kajiura + jdk
  simultaneously. It becomes noise.

### 2.2 Motif System (Halo Philosophy)
- **World Motif:** A 4- to 8-note melodic phrase that represents
  the trilogy's world. It appears in every main theme, rearranged
  per Era.
- **Character Motifs:** Each of the 4 party members gets a short
  leitmotif. These play during their Dual Techs, Support scenes,
  and personal story beats.
- **Era Motifs:** Each of the 5 Time Periods has a harmonic
  signature (key, scale, tempo range) that colors every track
  set in that Era.
- **Villain Motif:** A descending chromatic phrase that appears
  in fragments whenever the antagonist's influence is felt.
  It is never played in full until the final boss.

---

## 3. VST / Synth Toolkit

### 3.1 What You Already Have
| Tool | Role |
|------|------|
| **Bitwig Studio** | DAW. Modular routing for stem exports. |
| **Arturia Pigments** | Primary synth. Wavetables for modern pads, analog oscillators for classic leads. |
| **Korg VSTs** | Vintage keys, classic polysynth pads. |
| **Yamaha VSTs** | FM bass, electric pianos, classic digital textures. |

### 3.2 Recommended Additions by Style

#### Sakuraba (Symphonic Prog Rock)
| VST | Purpose |
|-----|---------|
| Arturia B-3 V | Hammond organ. The backbone of Sakuraba's sound. |
| Ample Guitar LP (or Shreddage 3) | Clean and crunch rhythm guitars. |
| Ample Metal Eclipse | Heavy lead guitar for climactic sections. |
| GetGood Drums (One Kit Wonder) | Punchy, acoustic rock kit. |
| Spitfire Albion One | Sweeping brass and string stabs. |

#### Uematsu (Progressive Fantasy Rock)
| VST | Purpose |
|-----|---------|
| Arturia Piano V | Bright, cutting piano for fast arpeggios. |
| Spitfire BBC Symphonic | Full orchestral swells for choruses. |
| Arturia Pigments | Analog-style arpeggios and pad layers. |
| Steven Slate Drums | Tight, mix-ready rock kit. |
| Spectrasonics Omnisphere | Bells, mallets, hybrid textures. |

#### Kajiura (Ethereal / Vocal / Chamber)
| VST | Purpose |
|-----|---------|
| Output Exhale | Breath, chopped vocal textures, atmospheric "ahhs." |
| Heavyocity Vocalise | Cinematic vocal pads and phrases. |
| Spitfire Chamber Strings | Intimate, close-mic'd legato strings. |
| Arturia Pigments (wavetable) | Slow-evolving glassy pads. |
| Native Instruments Session Strings | Smaller ensemble for emotional cues. |

#### Sawano (Epic Hybrid / Electronic)
| VST | Purpose |
|-----|---------|
| Heavyocity DAMAGE 2 | Massive taiko ensembles, hybrid impacts, percussion loops. |
| Arturia Pigments (distorted) | Growling, saturated synth bass. |
| Ample Metal Eclipse | Drop-tuned power chords. |
| Output Signal | Rhythmic synth pulses and arps. |
| Spitfire Albion One (brass) | Huge, stabbing brass clusters. |

#### Falcom jdk (Jazz-Rock Fusion)
| VST | Purpose |
|-----|---------|
| Spectrasonics Trilian (or IK Modo Bass) | Slap bass, fingerstyle funk grooves. |
| SWAM Saxophones (or Vintage Brass) | Alto/tenor sax solos, brass stabs. |
| Arturia B-3 V | Jazz organ comping. |
| Arturia Piano V | Fast, bright piano runs. |
| GetGood Drums | Tight, funky kit with ghost notes. |
| Arturia Pigments | Bright lead synth for melodies. |

---

## 4. Music Categories & Track List

### 4.1 Overview
Every track in the trilogy falls into one of these categories.
The total count is a guideline; the Designer may add or remove
tracks as scope requires.

| Category | Count (per game) | Notes |
|----------|-----------------|-------|
| Main Theme / Title Screen | 1 | Arranged per game. Contains World Motif. |
| World Map (per Era) | 5 | One per Time Period. Varies by clock phase. |
| Town / Hub Themes | 4–6 | One per major settlement. |
| Dungeon / Exploration | 5–8 | Varied by environment type. |
| Standard Combat | 1 | Core battle theme. |
| Elite / Miniboss Combat | 1 | Escalated arrangement of standard. |
| Boss Combat | 3–5 | One per major boss tier. Multi-phase. |
| Super-Boss / Post-Game | 1–2 | Highest intensity. |
| Emotional / Story Scenes | 3–5 | Kajiura-influenced. Minimal. |
| Campfire / Tavern (Social Sim) | 2–3 | Falcom jdk or Uematsu warm style. |
| Menu / UI | 1 | Short loop. |
| Endings (4 per game) | 4 | Bad, Good, True, Real. Each distinct. |
| Credits | 1 | Arrangement of Main Theme. |
| **Total per game** | **~35–45** | |
| **Trilogy total** | **~105–135** | |

### 4.2 Detailed Track Breakdown

#### World Map Themes (5 Eras)
| Era | Working Title | Style Lead | Tempo | Key |
|-----|--------------|-----------|-------|-----|
| Era 1 (Ancient) | *Before the First Dawn* | Kajiura (ethereal vocals, sparse) | 70–85 BPM | D minor |
| Era 2 (Classical) | *Marketplace of Ages* | Falcom jdk (bright, bustling) | 130–150 BPM | G major |
| Era 3 (Industrial) | *Gears of Conflict* | Sakuraba (prog rock, odd meter) | 140–160 BPM (7/8) | E minor |
| Era 4 (Ruin) | *Ashes Remember* | Uematsu (melancholy piano + strings) | 60–75 BPM | B minor |
| Era 5 (Transcendent) | *The Last Horizon* | Sawano (hybrid, massive) | 120–140 BPM | C minor |

Each world map track exports **4 phase stems** (Dawn, Day,
Dusk, Night). See §6 for stem structure.

#### Boss Themes
| Boss Tier | Style Lead | Intensity |
|-----------|-----------|-----------|
| Standard Boss | Sakuraba prog rock + Uematsu melody | High |
| Mid-Game Boss | Sawano hybrid + Sakuraba organ | Very High |
| Late-Game Boss | Full orchestra + Sawano taikos + Pigments synth | Extreme |
| Super-Boss (Post-Game) | All five styles in a 6-minute suite | Maximum |
| Final Boss (Game 3) | World Motif deconstructed and rebuilt across all 5 Era keys | Maximum |

#### Ending Themes
| Ending | Style | Mood |
|--------|-------|------|
| Bad (The Pun) | Solo detuned music box playing the World Motif slowly. One sad trombone sting at the end. | Darkly comic. Unsettling. |
| Good | Uematsu-style triumphant orchestral reprise of the Main Theme. | Warm. Satisfying. |
| True | Kajiura-style chamber piece. Solo violin + piano + soft choir. Bittersweet. | Emotional. Revelatory. |
| Real / Secret | Silence for 3 seconds. Then a single Pigments pad swells into a new, never-heard motif (the Sequel Motif). | Mysterious. Anticipatory. |

---

## 5. Adaptive Audio — The FFXIV Day/Night System

### 5.1 Concept
Final Fantasy XIV shifts its zone music seamlessly between day
and night. We extend this to **four phases** tied to the
48-minute clock.

Each zone track is composed as a **single arrangement** with
four mix variations. The melody, chord progression, and tempo
remain recognizable, but the **instrumentation, energy, and
color** shift.

### 5.2 Phase Variations

| Phase | Instrumentation Shift | Energy | Filter / FX |
|-------|----------------------|--------|-------------|
| 🌅 Dawn | Soft intro. Piano or acoustic guitar leads. Strings enter gently. No heavy percussion. | Low → Medium | Slight high-pass filter. "Waking up" feel. |
| ☀️ Day | Full arrangement. All stems active. Drums, bass, melody, pads. | High | No filter. Full frequency spectrum. |
| 🌇 Dusk | Melody shifts to a lower register. Drums drop out or switch to brushes. A solo instrument (sax, violin, or organ) takes the lead. | Medium | Gentle low-pass at 12 kHz. Warmth. |
| 🌙 Night | Minimal. Piano or music box plays the melody. Ambient pads (Pigments wavetables). Sparse or no percussion. | Low | Low-pass at 8 kHz + subtle reverb increase. Intimate. |

### 5.3 BPM Shift
- Dawn: Base BPM.
- Day: Base BPM + 5–10%.
- Dusk: Base BPM − 5%.
- Night: Base BPM − 15–20%.

*In Bitwig, compose at the Day BPM. Use the Clip time-stretch
or tempo automation to create the other three phase exports.*

### 5.4 Godot Implementation
The `TimeManager` Autoload emits `phase_changed(new_phase)`.
The AudioManager listens and crossfades stems:

```text
TimeManager.phase_changed("Night")
    │
    ▼
AudioManager receives signal
    │
    ├── AnimationPlayer.play("Fade_To_Night")
    │       ├── Stem_Melody_Day.volume_db  →  -40
    │       ├── Stem_Melody_Night.volume_db →   0
    │       ├── Stem_Rhythm.volume_db       → -20
    │       ├── BGM_Melody bus LowPass      →  8000 Hz
    │       └── Reverb wet                  →  0.35
    │
    └── Crossfade duration: 3.0 seconds (smooth tween)
```

---

## 6. Adaptive Audio — The Halo Layered Stem System

### 6.1 Concept
Martin O'Donnell and Michael Salvatori scored Halo with
**layered stems** that the engine mixes in real time. As the
player moves from a quiet corridor into a firefight, the
percussion and brass swell while the ambient strings recede.

We apply this to **environmental density** and **combat intensity**.

### 6.2 Stem Structure
Every zone track is exported from Bitwig as **4 to 6 stems**:

| Stem | Contents | Bus |
|------|----------|-----|
| `Stem_Rhythm` | Drums, bass, percussion, rhythmic guitars | `BGM_Rhythm` |
| `Stem_Melody` | Lead instrument (piano, guitar, sax, synth lead) | `BGM_Melody` |
| `Stem_Harmony` | Pads, rhythm guitar, secondary keys, counter-melody | `BGM_Harmony` |
| `Stem_Ambience` | Choir, vocal pads, atmospheric textures, reverb tails | `BGM_Ambience` |
| `Stem_Perc_Extra` *(optional)* | Taikos, orchestral hits, Sawano-style impacts | `BGM_Rhythm` |
| `Stem_Motif` *(optional)* | The World Motif or Era Motif, isolated | `BGM_Melody` |

### 6.3 Environmental Layering
As the player moves through different terrain, stems are
muted, un-muted, or filtered:

| Environment Transition | Audio Change |
|----------------------|--------------|
| Open field → Sparse woods | `Stem_Ambience` fades in. `Stem_Rhythm` drops 6 dB. |
| Sparse woods → Dense forest | `Stem_Rhythm` drops another 6 dB. `Stem_Harmony` takes the lead. Bird/insect SFX layer added. |
| Dense forest → Cave entrance | Low-pass filter sweeps from 20 kHz → 500 Hz over 2 seconds. `Stem_Rhythm` mutes. Reverb wet increases to 0.5. |
| Cave → Underground tunnel | Further muffle. Low-pass at 300 Hz. Only `Stem_Ambience` and `Stem_Melody` remain, both filtered. |
| Tunnel → Indoor building | Low-pass at 1 kHz. Reverb switches from "Cave" preset to "Room" preset. `Stem_Rhythm` remains muted. |
| Indoor → Open field (exit) | All filters reset to 20 kHz. All stems return to full volume over 3 seconds. |

### 6.4 Combat Intensity Layering
When combat begins and ends, the stems shift:

| State | Active Stems | Mix |
|-------|-------------|-----|
| **Exploration** | Melody + Harmony + Ambience | Rhythm at −20 dB. |
| **Combat Start** | All stems | Rhythm at 0 dB. Melody at 0 dB. Ambience at −15 dB. |
| **Boss Phase 2** | All stems + Perc_Extra | Perc_Extra at 0 dB. Melody pitch-shifted +1 semitone for tension. |
| **Boss Phase 3** | All stems + Perc_Extra + Motif | Motif plays in augmentation (half speed, dramatic). |
| **Combat End** | Melody + Harmony + Ambience | Rhythm fades to −20 dB over 4 seconds. |

### 6.5 Godot Implementation (AnimationPlayer)
Each transition is an **AnimationPlayer animation**:

```text
Animations to create per zone:
├── "Exploration_Default"
├── "Exploration_Dense_Forest"
├── "Exploration_Cave"
├── "Exploration_Indoor"
├── "Combat_Intensity_High"
├── "Combat_Boss_Phase2"
├── "Combat_Boss_Phase3"
└── "Combat_End"
```

Each animation tweens the `volume_db` property of the relevant
`AudioStreamPlayer` nodes over 1–4 seconds. No code required.
The triggers are `Area3D` zones (for environment) and the
combat state machine (for intensity).

---

## 7. Spatial Audio — Caves, Tunnels, Indoors

### 7.1 The Muffle Effect
When the player enters an enclosed space, the music should
sound like it is coming from "outside" or "above." This is
achieved with a **Low-Pass Filter** on the Master bus.

### 7.2 Godot Audio Bus Layout

```text
Master Bus
├── Effect 0: LowPassFilter  (cutoff_hz: 20000 default)
├── Effect 1: Reverb         (wet: 0.0 default)
│
├── BGM_Rhythm    → outputs to Master
├── BGM_Melody    → outputs to Master
├── BGM_Harmony   → outputs to Master
├── BGM_Ambience  → outputs to Master
│
├── SFX           → outputs to Master
├── UI_SFX        → outputs to Master
└── Dialogue      → outputs to Master
```

### 7.3 Area3D Trigger Script
Every cave, tunnel, or building entrance has an `Area3D` with
this script:

```gdscript
extends Area3D

@export var muffle_cutoff: float = 500.0
@export var reverb_wet: float = 0.4
@export var transition_time: float = 2.0

var bus_idx: int
var lowpass: AudioEffectLowPassFilter
var reverb: AudioEffectReverb

func _ready():
    bus_idx = AudioServer.get_bus_index("Master")
    lowpass = AudioServer.get_bus_effect(bus_idx, 0)
    reverb = AudioServer.get_bus_effect(bus_idx, 1)
    body_entered.connect(_on_enter)
    body_exited.connect(_on_exit)

func _on_enter(body):
    if body.is_in_group("player"):
        var tween = create_tween()
        tween.tween_property(lowpass, "cutoff_hz",
            muffle_cutoff, transition_time)
        tween.parallel().tween_property(reverb, "wet",
            reverb_wet, transition_time)

func _on_exit(body):
    if body.is_in_group("player"):
        var tween = create_tween()
        tween.tween_property(lowpass, "cutoff_hz",
            20000.0, transition_time)
        tween.parallel().tween_property(reverb, "wet",
            0.0, transition_time)
```

### 7.4 Preset Values by Space

| Space | LowPass Cutoff | Reverb Wet | Reverb Room Size |
|-------|---------------|------------|-----------------|
| Open Field | 20,000 Hz | 0.0 | 0.1 |
| Sparse Forest | 16,000 Hz | 0.05 | 0.2 |
| Dense Forest | 12,000 Hz | 0.1 | 0.3 |
| Small Cave | 800 Hz | 0.35 | 0.5 |
| Large Cavern | 500 Hz | 0.5 | 0.8 |
| Narrow Tunnel | 300 Hz | 0.4 | 0.4 |
| Indoor Room | 1,200 Hz | 0.2 | 0.3 |
| Cathedral / Throne Room | 2,000 Hz | 0.6 | 0.9 |
| Underground Dungeon | 400 Hz | 0.55 | 0.7 |

---

## 8. Combat Audio Integration

### 8.1 Battle Music Escalation
The standard battle theme is the **area theme taken one step
further.** If the player is in a forest, the battle music uses
the forest's key, tempo, and melodic motif, but adds drums,
raises the BPM by 15–20%, and layers a counter-melody.

| Zone | Exploration Feel | Battle Escalation |
|------|-----------------|-------------------|
| Peaceful meadow | Uematsu piano, 80 BPM | Same melody + Sakuraba organ + driving drums, 100 BPM |
| Bustling town | Falcom jdk jazz, 140 BPM | Same groove + heavier guitars + aggressive sax, 155 BPM |
| Dark dungeon | Kajiura strings, 70 BPM | Same motif + Sawano taikos + distorted bass, 90 BPM |
| Industrial zone | Sakuraba prog, 150 BPM | Same odd meter + full orchestra + Pigments synth stabs, 165 BPM |

### 8.2 Boss Phase Music Shifts
Boss music evolves as the fight progresses. Each phase adds a
layer or changes the arrangement:

| Phase | Musical Change |
|-------|---------------|
| Phase 1 (100%–70% HP) | Base boss theme. Area motif + new boss counter-melody. |
| Phase 2 (69%–40% HP) | Drums double in intensity. A second harmony voice enters. Key shifts up a minor third. |
| Phase 3 (39%–10% HP) | Full orchestra + choir enter. Tempo +10%. The boss's counter-melody becomes the lead. |
| Phase 4 (9%–0%, optional) | Everything drops out except a solo instrument playing the World Motif. Then the full ensemble crashes back in for the final 10 seconds. |

### 8.3 Godot Implementation
Boss phase changes are driven by the boss's HP threshold.
When the threshold is crossed, the `AnimationPlayer` plays
the corresponding stem-mix animation:

```text
Boss HP crosses 40%
    │
    ▼
Boss script emits signal: phase_changed(3)
    │
    ▼
AudioManager receives signal
    │
    └── AnimationPlayer.play("Combat_Boss_Phase3")
            ├── Stem_Perc_Extra.volume_db → 0
            ├── Stem_Motif.volume_db → 0
            ├── BGM_Melody bus pitch_shift → +3 semitones
            └── Crossfade: 2.0 seconds
```

---

## 9. Time Period Audio Identity

Each of the 5 Eras has a distinct sonic palette that colors
every track set within it.

| Era | Primary Instruments | Harmonic Language | Production Style |
|-----|--------------------|--------------------|-----------------|
| Era 1 (Ancient) | Kajiura vocals, solo strings, Pigments glass pads, hand drums | Modal (Dorian, Aeolian). Sparse chords. | Dry, intimate, lots of reverb tails. |
| Era 2 (Classical) | Falcom jdk band: sax, slap bass, piano, bright synths | Major keys. ii-V-I jazz progressions. | Bright, punchy, forward mix. |
| Era 3 (Industrial) | Sakuraba Hammond organ, electric guitars, odd-meter drums | Minor keys. 7/8 or 5/4 time. Chromatic movement. | Mid-heavy, saturated, compressed. |
| Era 4 (Ruin) | Uematsu solo piano, muted strings, music box, distant choir | Minor keys. Slow harmonic rhythm. Unresolved cadences. | Lo-fi textures, tape hiss, wide reverb. |
| Era 5 (Transcendent) | Sawano hybrid: taikos, synth bass, full orchestra, vocal chants | Epic minor keys. Modal interchange. Huge dynamic range. | Wide, massive, sub-bass heavy. |

### 9.1 Cross-Era Tracks
When the player travels between Eras (Time Gate activation),
a short **stinger** plays: a 3-second motif that blends the
outgoing Era's instrumentation with the incoming Era's key.
This reinforces the feeling of temporal displacement.

---

## 10. Social Sim & Campfire Audio

### 10.1 Campfire Scenes
- Warm, acoustic, intimate.
- Uematsu-style guitar or piano. Falcom jdk-style soft jazz
  for tavern scenes.
- No heavy percussion. No electric guitars.
- The campfire crackle SFX sits **under** the music, not over it.

### 10.2 Romance / Deep Support Scenes
- Kajiura influence: solo violin or cello + piano + soft
  Pigments pad.
- The character's personal leitmotif is woven into the
  arrangement.
- Music is sparse. Let the dialogue breathe.

### 10.3 Affinity Level Music Shift
As affinity with a character increases, their campfire theme
gains layers:

| Affinity | Music State |
|----------|-------------|
| 1–3 | Solo instrument (piano or guitar). |
| 4–6 | + Soft strings or a second instrument. |
| 7–9 | + Full arrangement. Character leitmotif appears. |
| 10+ | + A subtle vocal hum (Kajiura-style). The theme feels "complete." |

---

## 11. Anti-Piracy Audio Integration

### 11.1 The Detune Effect
If `AntiTamper.is_compromised` is true, the game slowly detunes
all audio over time. This is deeply unsettling and very hard
for the pirate to diagnose.

```gdscript
# In AntiTamper.gd _process():
if is_compromised:
    var detune = 1.0 - (corruption_level * 0.15)
    AudioServer.set_bus_pitch_scale(
        AudioServer.get_bus_index("Master"), detune)
```

| Corruption Level | Pitch Scale | Audible Effect |
|-----------------|-------------|----------------|
| 0.0 | 1.0 | Normal. |
| 0.2 | 0.97 | Barely noticeable. Slightly "off." |
| 0.5 | 0.925 | Clearly flat. Sounds like a dying tape. |
| 0.8 | 0.88 | Obviously wrong. Music sounds sickly. |
| 1.0 | 0.85 | A quarter-tone flat. Unmistakably broken. |

### 11.2 The "Bad End" Audio Pun
If a pirate reaches the Bad Ending, the sad trombone sting
is replaced with a **recording of the main theme played on
a slightly out-of-tune kazoo.** The credits roll in silence
after that.

### 11.3 Stem Dropout
At corruption_level > 0.6, random stems begin to **drop out**
mid-track for 2–3 seconds, then return. The pirate hears the
drums vanish, then come back. The melody cuts, then resumes.
It feels like a broken CD.

---

## 12. Sound Effects (SFX) Guidelines

### 12.1 Combat SFX
| Action | Sound Character |
|--------|----------------|
| Sword slash | Bright, metallic, short tail. |
| Hammer impact (Cute Bruiser) | Deep, resonant, with a slight comedic "boing" undertone. |
| Healing magic | Warm chime, ascending arpeggio, soft reverb. |
| Fire spell | Crackling, whoosh, low-end rumble. |
| Time magic | Reversed cymbal + granular Pigments texture + clock tick. |
| Critical hit | Layered impact + brief musical sting (1 note). |
| Perfect Dodge (Real-Time) | Sharp "whoosh" + brief time-stop SFX (vinyl scratch or tape stop). |

### 12.2 UI SFX
- Menu navigation: Soft, warm click. Not harsh.
- Confirmation: Gentle chime.
- Back / Cancel: Low, muted tap.
- Error / Invalid: Dull thud. Never a harsh buzz.
- Text scroll (Dialogic): Soft typewriter tick. Speed adjustable.

### 12.3 Ambient SFX (Per Environment)
| Environment | Layers |
|-------------|--------|
| Open field | Wind, distant birds, grass rustle. |
| Dense forest | Close birds, insects, leaf crunch underfoot. |
| Cave | Water drips, low rumble, echoing footsteps. |
| Town (Day) | Crowd murmur, market calls, distant music. |
| Town (Night) | Crickets, distant dog bark, muffled tavern music. |
| Industrial zone | Clanking, steam hiss, distant machinery hum. |
| Ruin | Wind through broken stone, distant collapse, silence. |

---

## 13. Export Pipeline — Bitwig to Godot

### 13.1 Step-by-Step

```text
1. COMPOSE in Bitwig.
   └── Arrange the full track at "Day" tempo and full energy.

2. CREATE PHASE VARIATIONS.
   └── Duplicate the arrangement.
   └── Adjust tempo, mute/unmute instruments, change
       instrumentation for Dawn, Dusk, Night.

3. EXPORT STEMS.
   └── File → Export Audio → Export as Stems.
   └── Export each stem as a separate WAV (44.1 kHz, 24-bit).
   └── Name convention:
       {Era}_{Zone}_{Phase}_{StemName}.wav
       Example: Era2_Forest_Day_Stem_Rhythm.wav

4. IMPORT INTO GODOT.
   └── Drag WAV files into res://audio/stems/.
   └── Godot imports them as AudioStreamWAV.

5. ASSIGN TO AUDIOSTREAMPLAYERS.
   └── Each stem gets its own AudioStreamPlayer node.
   └── Route each player to its corresponding Audio Bus.

6. BUILD ANIMATIONPLAYER TRANSITIONS.
   └── Create animations for each environment/combat state.
   └── Keyframe the volume_db of each stem.

7. TEST.
   └── Walk through every zone. Enter every cave.
   └── Trigger every combat encounter.
   └── Listen for hard cuts, volume spikes, or filter pops.
```

### 13.2 File Naming Convention

```text
/audio
  /stems
    /era1
      /world_map
        Era1_WorldMap_Dawn_Stem_Melody.wav
        Era1_WorldMap_Dawn_Stem_Harmony.wav
        Era1_WorldMap_Dawn_Stem_Ambience.wav
        Era1_WorldMap_Day_Stem_Melody.wav
        ...
    /era2
      ...
  /combat
    Game1_Standard_Battle_Stem_Rhythm.wav
    Game1_Standard_Battle_Stem_Melody.wav
    ...
  /boss
    Boss_Tier1_Phase1_Stem_Rhythm.wav
    ...
  /sfx
    /combat
    /ui
    /ambient
  /stingers
    TimeGate_Era1_to_Era2.wav
    ...
```

### 13.3 Audio Format Settings
| Setting | Value | Reason |
|---------|-------|--------|
| Sample Rate | 44,100 Hz | Standard. Godot default. |
| Bit Depth | 24-bit (WAV) → Godot compresses on import | High quality source. |
| Channels | Stereo for music. Mono for positional SFX. | Stereo music fills the mix. Mono SFX spatializes correctly. |
| Loop | Enabled for exploration and combat stems. Disabled for stingers and one-shots. | Seamless looping is critical. |
| Loop Mode | Forward (not Ping-Pong) | Avoids audible "bounce." |

---

## 14. Modding Hooks for Audio

### 14.1 JSON-Driven Audio Config
All audio assignments are stored in `data/audio_config.json`:

```json
{
  "zones": {
    "era2_forest": {
      "stems": {
        "rhythm": "Era2_Forest_Day_Stem_Rhythm",
        "melody": "Era2_Forest_Day_Stem_Melody",
        "harmony": "Era2_Forest_Day_Stem_Harmony",
        "ambience": "Era2_Forest_Day_Stem_Ambience"
      },
      "bus_mapping": {
        "rhythm": "BGM_Rhythm",
        "melody": "BGM_Melody",
        "harmony": "BGM_Harmony",
        "ambience": "BGM_Ambience"
      },
      "muffle_cutoff": 12000,
      "reverb_wet": 0.1
    }
  },
  "combat": {
    "standard_battle_bpm_boost": 20,
    "boss_phase2_pitch_shift": 3
  }
}
```

### 14.2 Custom Music Mods
- Modders can drop their own `.ogg` or `.wav` files into
  `Mods/{ModName}/audio/`.
- The `ModLoader.gd` checks for audio overrides in the same
  JSON config.
- If a mod provides a file matching an existing stem name,
  the ModLoader swaps the AudioStreamPlayer's stream reference.
- This allows the community to replace any track in the game
  with their own composition.

---

## 15. Audio State Machine (Quick Reference)

```text
[GAME BOOT]
     │
     ▼
[MAIN MENU THEME]
     │  Player starts game
     ▼
[WORLD MAP / ZONE THEME — Day Phase]
     │
     ├── TimeManager → phase_changed("Dusk")
     │   └── Crossfade to Dusk stems (3s)
     │
     ├── Player enters Area3D (Cave)
     │   ├── LowPass → 500 Hz
     │   ├── Reverb wet → 0.4
     │   └── Stem_Rhythm muted
     │
     ├── Player touches Enemy
     │   ├── AnimationPlayer → "Combat_Intensity_High"
     │   ├── All stems active, Rhythm at 0 dB
     │   └── BPM feels +20% (pre-composed, not runtime)
     │
     ├── Boss HP < 40%
     │   ├── AnimationPlayer → "Combat_Boss_Phase3"
     │   ├── Stem_Perc_Extra fades in
     │   └── Stem_Motif enters (augmentation)
     │
     ├── Combat Victory
     │   ├── AnimationPlayer → "Combat_End"
     │   ├── Rhythm fades to −20 dB (4s)
     │   └── Resume zone theme at current phase
     │
     ├── Player exits Area3D (Cave)
     │   ├── LowPass → 20,000 Hz
     │   ├── Reverb wet → 0.0
     │   └── All stems restored (3s)
     │
     ├── TimeManager → phase_changed("Night")
     │   └── Crossfade to Night stems (3s)
     │
     └── AntiTamper.is_compromised
         └── Master pitch_scale slowly decreases
             Music detunes over 30+ minutes
             Stems randomly drop out at corruption > 0.6
```

---

## 16. Master Checklist for the Composer

Before exporting any track, verify:

- [ ] The track contains or references the **World Motif** or
      the relevant **Era Motif**.
- [ ] The track sits in the correct **key** for its Era.
- [ ] The track follows the **"No Fusion" rule** (one primary
      influence, at most one secondary accent).
- [ ] **4 phase variations** exist (Dawn, Day, Dusk, Night).
- [ ] All **stems** are exported as individual WAV files.
- [ ] The **loop points** are seamless (no audible click or gap).
- [ ] The file follows the **naming convention**.
- [ ] The track has been tested in Godot with the
      **Audio Bus routing** and **AnimationPlayer crossfades**.
- [ ] The track does **not clip** the Master bus (peak below −1 dB).
- [ ] The track has been tested with the **LowPass Filter**
      engaged (cave simulation) to ensure it still sounds
      coherent when muffled.
- [ ] The **AntiTamper detune test** has been run (does the
      track still sound "almost right" when pitched down 5%?
      If it falls apart entirely, the harmonic content is too
      dense. Simplify).

---

*Document Version 1.0 — November 2026*
*Author: Rin*
*Role: Creative Director / Designer / Composer*

---