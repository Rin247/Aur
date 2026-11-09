# Audio Under Corruption

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 1152-1197). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 🎵 AUDIO CORRUPTION REFLECTION: YOU BECOME THE BOSS

This is where it gets *mean* in the best way. The adaptive audio system we built with stems doesn't just shift for time-of-day or combat intensity. It shifts for **your corruption state**.

### The Core Rule

**At 0–40% corruption:** You hear the world's music normally. Exploration themes, town themes, standard Sakuraba/Uematsu adventure tracks.

**At 41–70% corruption:** The world's music starts reacting to *you*. Town music gains a low, uneasy drone layer. Exploration music drops its melody stem and keeps only rhythm + ambience. NPCs hum nervously when you pass.

**At 71–100% corruption:** **You hear boss music.** Not the enemy's boss music. *Yours.* The game treats you as the threat. The exploration track is replaced by a heavy, Sawano-style war drum + distorted choir arrangement. The world is afraid of you.

### Detailed Audio Shift Table

| Corruption Range | Exploration Music | Town/Hub Music | Combat Music | Enemy Encounter SFX |
|:---:|:---|:---|:---|:---|
| **0–20%** | Full arrangement. Melody forward. Standard BPM. | Warm, inviting. Standard town theme. | Standard battle theme for the area. | Normal "!" alert sting. |
| **21–40%** | Melody stem volume drops 15%. Ambience stem gains a faint low hum. | Standard theme but with a subtle minor-key undertone. | Standard battle theme. | Normal alert. Slightly sharper pitch. |
| **41–60%** | Melody drops 30%. Rhythm stem gains a slow, heavy kick drum layer. Ambience adds distant dissonant strings. | Town theme plays but the percussion stem is muted. Strings carry the melody alone. Feels hollow. | Battle theme gains a secondary percussion layer. Enemy attack SFX hit harder. | Alert sting is lower, more urgent. |
| **61–80%** | Melody is nearly silent. Rhythm is dominant—heavy, slow, predatory. Kajiura-style vocal pads enter but *detuned*. | Town theme is replaced by a sparse, uneasy arrangement. Music box melody over silence. NPCs stop humming. | **Boss-tier arrangement.** Area theme "taken one step further" but from the *enemy's perspective*. Heavy brass. Taiko. The music treats YOU as the threat. | Alert sting is a full orchestral hit. Enemies vocalize fear. |
| **81–100%** | **Full boss theme.** Sawano choir. Distorted guitar. Massive taiko. The exploration track IS your boss music. The world is the arena and you are the raid boss walking through it. | Hub music is replaced by a single, sustained low drone + distant choir. NPCs flee or hide. No ambient chatter. | **You are the boss.** Enemy encounters trigger YOUR boss theme at full intensity. Enemy attack SFX are desperate, panicked. Their hits sound weaker against you. | Alert sting is a full orchestral *slam*. Followed by silence. Then your footsteps. |

### The "You Are The Boss" Audio Moment

At 80%+ corruption, when you enter combat against regular enemies:

1. The standard "battle start" swoop happens.
2. Instead of the area's battle theme, **your boss theme kicks in**—the one that's been building during exploration.
3. The enemy's attack animations play, but their SFX are *muffled, desperate*. Like they're fighting something they can't comprehend.
4. Your attacks hit with **boss-level impact SFX**—deep bass thuds, screen-shake rumbles, glass-shatter overtones.
5. When the enemy dies, there's no victory jingle. Just silence. Then your footsteps resuming.

**At 95%+ corruption, the music is almost entirely yours.** The world's melodies are gone. Only your rhythm remains. The Kajiura vocal pads are replaced by a single, sustained, detuned note that bends slowly downward. It sounds like the planet is *dying around you*.

### Instrument-Weapon Synergy Shift

Your instrument-weapon combos also change:

| Corruption | Instrument Effect |
|:---:|:---|
| **0–40%** | Standard buffs. Chord progressions grant team-wide stat boosts. Melodic, harmonious. |
| **41–70%** | Instrument combos grant personal buffs only. The chord progressions shift to minor keys. Team buffs are halved. |
| **71–100%** | Instrument combos grant **debuffs to enemies** instead of buffs to allies. Your music *hurts*. Chord progressions are dissonant, aggressive. Playing a combo near an enemy causes them to flinch/stagger. |

---

