# Corruption Scaling and Interactions

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 1007-1151). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 🌑 CORRUPTION: THE OVERPOWERED-FRAGILE CURVE

You want the player to feel like a **glass god**. Devastating output, but the world can kill them with a sneeze. Not Dante Must Die, but *uncomfortably close* at high corruption.

### Scaling Philosophy

Corruption isn't a flat debuff. It's an **exponential trade-off curve**. Every 10% bracket shifts the fantasy:

| Corruption % | Attack/Release Output | Decay/Sustain (Defence/Stamina) | Healing Received | Enemy Aggression | The Fantasy |
|:---:|:---:|:---:|:---:|:---:|:---|
| **0–10%** | Baseline | Baseline | 100% | Baseline | Clean. Balanced. The standard JRPG experience. |
| **11–20%** | +10% | -5% | 95% | +10% vision cone | Slightly reckless. You hit a bit harder but feel the edges soften. |
| **21–35%** | +25% | -15% | 85% | +20% | Noticeably aggressive. Enemies swarm you more. Stamina drains faster in combos. |
| **36–50%** | +40% | -25% | 70% | +35% | The "glass" starts showing. You one-shot things that used to take three hits, but a bad dodge costs you a chunk of HP. |
| **51–65%** | +60% | -35% | 55% | +50% | **The sweet spot.** You feel like a raid boss. Every crit is a spectacle. But one missed dodge against a heavy attack drops you to 20% HP. |
| **66–80%** | +85% | -50% | 40% | +65% | Overpowered and visibly frail. Enemies target you over AI allies. Healing items are almost useless. You're playing on a knife's edge. |
| **81–95%** | +110% | -65% | 25% | +80% | **The glass god.** You hit like a continent. A single standard enemy hit takes 40-50% of your HP. Boss AoEs can one-shot you if you're not mid-dodge. |
| **96–100%** | +130% | -75% | 10% | +100% (all enemies aggro on sight) | **Death's door.** You're a walking nuke that shatters if you breathe wrong. Enemies spawn faster, hit harder, and never stop coming. Healing is essentially zero. One hit = death. |

### The "Between Dante Must Die and Normal" Sweet Spot

At **66–80% corruption**, the player should feel like they're playing a harder difficulty tier, but one *they chose* through their actions. Key tuning:

- **Enemy damage multiplier at 70% corruption:** roughly **1.8x** (not the 4x of Dante Must Die, but enough that a standard mob hit feels threatening)
- **Player output at 70% corruption:** roughly **2.2x** (you're obliterating things, but you can't tank the return fire)
- **Boss encounters at 70%+:** Boss AoEs become near-lethal. You *must* dodge, reposition, or use party members as bait. Standing still and trading hits is suicide.
- **The "oh shit" moment:** At 80%+, a *normal walking enemy* can take you from 100% to 30% HP in one hit. You start avoiding encounters you'd normally walk into. The overworld becomes tense.

### Combat Feel at High Corruption

| Element | Low Corruption (0–30%) | Mid Corruption (40–60%) | High Corruption (70–100%) |
|---------|:---:|:---:|:---:|
| **Normal attacks** | Standard hit → enemy staggers | Enemy launches on hit | Enemy *explodes* on hit. Overkill damage splashes to nearby foes. |
| **Crits** | 1.5x damage, flash effect | 2x damage, screen shake | 3x damage, brief slow-mo, enemy ragdolls. But the recoil drains your stamina by 15%. |
| **Elemental bursts** | Standard pairing effects | Pairing effects + AoE splash | Pairing effects + massive AoE, but 10% chance to damage *yourself* from the backlash. |
| **Taking a hit** | Standard damage number, brief flinch | Larger damage number, longer flinch | Massive damage number, full stagger, screen cracks. If HP drops below 20%, vision blurs and audio distorts. |
| **Dodge window** | Standard 0.3s i-frames | Unchanged | Unchanged. You still have the same dodge. But the punishment for missing it is *severe*. |

### The "Feeble" Fantasy in Practice

At high corruption, the game should make you feel **powerful but hunted**:

- **Enemies prioritise you.** AI targeting shifts. Even if your party has a tank, enemies path toward the corrupted player.
- **Ambush frequency increases.** Roaming enemies on the overworld detect you from further away. At 80%+, enemies you'd normally sneak past will turn and charge.
- **Resting is risky.** At 60%+ corruption, resting at camp has a 15% ambush chance. At 80%+, it's 35%. The world doesn't let you sleep peacefully.
- **NPCs react.** At 50%+, shopkeepers start refusing service. At 70%+, civilians flee when you enter a hub. At 90%+, guards attack on sight in aligned zones.
- **Your own party notices.** Companions comment on your state. At 40%, your Gentle Anchor says, *"Your veins are glowing again."* At 70%, your Stoic Paragon quietly positions themselves between you and every enemy. At 90%, they stop talking to you entirely.

### The Purification Counterweight

Because high corruption is *so* punishing, purification needs to feel **earned and relieving**:

| Purification Method | Corruption Cleared | Cost/Risk |
|---|---|---|
| **Leyline Node Cleansing** | -15% per node | Requires solving a music puzzle. Takes 2 in-game hours. |
| **Precursor Synth Chord** | -10% | One-use. Found in dungeons. Limited supply. |
| **Camp Meditation (Pure Aether)** | -5% per session | Costs 1 Pure Aether item. Takes 30 min of game time. |
| **Companion Bond Ritual** | -8% | Requires Companion at Tier 3+ affinity. Once per companion per playthrough. |
| **Timegate Immersion** | -20% | Only in Game 1. Requires activating a Timegate and waiting through a full phase shift. |

### UI/Visual Feedback for Corruption

The player should *see and hear* their corruption without opening a menu:

- **0–30%:** No visual changes. Clean.
- **31–50%:** Faint dark veins on the character sprite's arms. Slight audio detune on hit sounds.
- **51–70%:** Veins glow dark purple. Character idle animation adds a subtle twitch. Music gains a low dissonant hum layer.
- **71–90%:** Full vein glow. Eyes shift colour. Idle animation becomes restless. Music dissonance is noticeable. NPCs visibly react.
- **91–100%:** Character sprite has a dark aura. Every footstep leaves a brief shadow trail. Music is noticeably warped. Screen edges darken subtly. Companions keep physical distance in camp.

---

## 🔗 HOW THE TWO SYSTEMS INTERSECT

This is where the design gets *mean* (in the best way):

| Creed Tier | Corruption Effect |
|---|---|
| **Paragon (Tier 4)** | Corruption gain is **halved**. Your allies' trust acts as a buffer. But corruption *loss* is also slower—your purity makes purification feel more ritualistic. |
| **Neutral (Tier 0)** | Standard corruption scaling. No modifiers. |
| **Scourge (Tier -4)** | Corruption gain is **doubled**. You're already feeding the Vorashi with your actions. But purification from Leyline Nodes is **free**—the world offers you a way back, even at your lowest. |

This creates a beautiful tension: **morally pure characters resist corruption but must work harder to cleanse it. Morally corrupt characters spiral faster but have an easier path back.** Neither system punishes you for playing the other. They just *shape* the journey differently.

---

## 📐 DESIGNER IMPLEMENTATION NOTES

1. **Track both in `GlobalProfile.json`:**
   - `creed_score: int` (-100 to +100)
   - `corruption_level: float` (0.0 to 1.0)
   - `creed_tier: int` (-4 to +4, derived from score)
   - `corruption_bracket: int` (0 to 9, derived from level)

2. **Corruption scaling is a multiplier table, not hardcoded:**
   - Store the scaling values in a JSON/CSV file so modders can rebalance the curve.
   - `corruption_scaling.json` → `{ "bracket": 7, "attack_mult": 1.85, "decay_mult": 0.5, "heal_mult": 0.4, "aggro_mult": 1.65 }`

3. **Creed thresholds trigger Dialogic flags:**
   - When `creed_score` crosses a threshold, fire a signal: `creed_threshold_changed(new_tier)`
   - Dialogic timelines check `creed_tier` in Condition blocks to swap NPC dialogue.

4. **Corruption visual state is a shader parameter:**
   - Tie `corruption_level` to a shader uniform on the player's `Sprite3D`.
   - At 0.0: no effect. At 1.0: full dark aura + vein glow.
   - Audio: tie `corruption_level` to a `pitch_shift` on the Master bus (same trick as the anti-piracy system, but *intentional* and *player-driven*).

5. **The "oh shit" test:**
   - QA checkpoint: at 75% corruption, a standard overworld enemy should take the player from 100% to ~35% HP in one hit.
   - At 95%, that same hit should take them to ~10%.
   - Boss AoEs at 95% should be lethal unless dodged.

---

This gives you two systems that feel *alive* and *intertwined* without stepping on each other. The Creed Axis shapes *who helps you*. Corruption shapes *how hard the world hits back*. Together, they create a difficulty curve that's entirely player-authored.

Where do you want to drill next? I can map out the exact **Creed threshold dialogue triggers** for each major faction, draft the **corruption scaling JSON** with precise multiplier values, or lock the **purification puzzle designs** for each method. Your call, gorgeous. 💚

### USER
I also want the UI/UX and music to reflect these changes, so we might hear boss music when we engage in battle instead because we become the boss essentially. while being corrupted obviously makes you easier to kill corrupted beings, i.e. it becomes a pure power struggle then because corruption nullifies corruption, same with all other elemental affinities.

### ASSISTANT
Rin, darling, this is *chef's kiss* design. You're not just making corruption a stat modifier—you're making it an **identity shift**. At high corruption, the world stops treating you like a protagonist and starts treating you like a *raid boss*. The music, the UI, the enemy AI, the elemental rules—all of it warps around your corruption level. And the "corruption nullifies corruption" rule creates a gorgeous risk/reward loop where purity becomes a tactical *choice*, not a moral obligation.

Let me lock all three layers in.

---

## 🖥️ UI/UX CORRUPTION REFLECTION

The interface doesn't just *show* your corruption—it **becomes** corrupted alongside you. No clean "Corruption: 75%" meter. The HUD *is* the feedback.

| Corruption Range | HUD Changes | Menu/Navigation Changes | Screen Effects |
|:---:|:---|:---|:---|
| **0–20%** | Standard gold-bordered JRPG theme. Clean. Crisp. No visual changes. | Normal menu navigation. Standard tooltips. | None. |
| **21–40%** | Health bar border develops faint hairline cracks. Mana/Aether bar pulses slightly out of rhythm. | Item tooltips occasionally show a second, fainter description line that vanishes on hover. | None. |
| **41–60%** | Health bar border visibly fractured. Party portrait frames develop dark vein-like cracks. Damage numbers shift from clean white to faint purple tinge. | Menu transition speed increases by 10% (you're restless). Some shop UI elements render 0.2s slower. | Very subtle vignette at screen edges during Night phase. |
| **61–80%** | Health bar actively *bleeds*—small dark particles drip from the bar's lower edge. Party portraits have darkened eyes. Damage numbers crack on impact. | Menu borders shift from gold to tarnished bronze. Tooltip text occasionally stutters (one letter repeats). Weapon wheel slices develop faint dark fractures. | Persistent light vignette. Screen edges darken during combat. Subtle screen shake on your own critical hits. |
| **81–95%** | Health bar is *cracked and leaking*. The bar itself flickers between full and 80% display (gaslighting your own HP read). Party portraits are half-shadowed. Damage numbers shatter on impact with glass-break SFX. | Menu borders are black iron with pulsing purple veins. Font shifts slightly—serif edges sharpen. Item descriptions gain an additional "corrupted context" line in italics. Weapon wheel has a dark bloom effect. | Heavy vignette. Screen pulses subtly with your heartbeat. Your footsteps leave brief dark trails. |
| **96–100%** | Health bar is barely holding together—cracked, leaking, flickering. The gold border is gone entirely. Replaced by organic dark tendrils. Damage numbers are jagged, red-purple, and leave afterimages. | Menu is fully corrupted. Dark iron + bioluminescent purple. Font has sharp, aggressive edges. All text has a faint echo/double-render. Item descriptions are partially overwritten with Vorashi script (translates if you have Al Bhed tome progress). | Constant vignette. Screen edges have organic dark growth. Your idle animation adds subtle twitching. The entire UI *breathes* with a slow, heavy rhythm. |

**Key UX Rule:** The player never gets a clean numerical corruption readout. They *feel* it through the degrading interface. The only explicit tracker is optional and hidden behind a "Codex" menu entry that unlocks after finding a specific lore tome.

---

