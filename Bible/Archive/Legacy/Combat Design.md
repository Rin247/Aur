# ⚔️ Combat Design Document

> Companion to the World Building Bible v1.0
> Systems: Turn-Based (CT) · Tactical (FE/TS) · Real-Time Action (ToM)
> Engine: Godot 4.x — GDScript
> Camera: Phantom Camera Plugin
> Grid: Shared NavigationRegion3D + GridMap

---

## 1. Combat Philosophy

All three games share one golden rule:

> **The battlefield IS the overworld.** Exploration uses smooth, *Secret of Mana / Chrono Trigger* style real-time walking. There is no loading screen or separate arena. When an encounter triggers, the camera swoops, the clock pauses, and combat engages directly on that exact same plane:
> - **Dawn / Game 1 (Turn-Based):** Characters snap into line-up slots right on the exploration plane (*Chrono Trigger* style).
> - **Day / Game 2 (Tactical):** The exploration plane overlays a tactical movement grid where height and positioning matter (*Fire Emblem / Triangle Strategy* style).
> - **Dusk / Game 3 (Real-Time Action):** Characters engage in real-time action combat seamlessly on the field (*Trials of Mana / Seiken Densetsu* style).

Every tree, rock, elevation change, and building that exists in the hub area is **live geometry** during combat.

### 1.1 Squad Size
- **4 active characters** at all times.
- Swappable at Rest Points (camps, inns, specific world-map nodes).
- No 8-person armies. Encounters are designed for tight, readable
  4v1 through 4v6 engagements.

### 1.2 Trope Subversion in Combat Roles
- The "Cute Bruiser" (small girl, massive hammer) can out-tank
  the veteran soldier.
- The "Badass Healer" (scarred, towering man) is the primary
  caster and support.
- Roles are **mechanical, not aesthetic.** Any body type can
  occupy any combat slot.

---

## 2. The Shared Grid & Battle Transition

### 2.1 The Trigger
Combat begins when one of the following occurs:

| Trigger Type | How It Happens |
|-------------|----------------|
| **Standard Encounter** | Player touches a visible enemy sprite on the overworld. |
| **Ambush** | Enemy attacks from behind/side. Player starts with a "Stunned" debuff for Turn 1. |
| **Preemptive Strike** | Player attacks an enemy from behind. Player gets a free action before enemies act. |
| **Scripted Boss** | A story event or Area3D zone triggers combat automatically. |
| **Time-Gated Encounter** | A rare enemy that only spawns during a specific clock phase (e.g., Dusk). |

### 2.2 The Camera Swoop (Golden Sun Style)
1. Player touches enemy `Area3D`.
2. `TimeManager.pause_clock()` is called.
3. Player movement is frozen (`freeze_player = true`).
4. **Phantom Camera Priority** flips:
   - Exploration Camera (high isometric) → Priority 0.
   - Combat Camera (low, dramatic, upward angle) → Priority 10.
5. Phantom Camera tweens smoothly between the two.
6. The `AnimationPlayer` crossfades audio stems
   (Exploration → Combat Intensity High).
7. Combat State initializes.

### 2.3 Positioning Per Game Mode

```text
CHRONO TRIGGER MODE (Turn-Based):
┌─────────────────────────────────────────────────┐
│                                                 │
│   [P1] [P2]           [E1] [E2]               │
│   [P3] [P4]           [E3] [E4]               │
│                                                 │
│   ← Player Slots       Enemy Slots →            │
│   (Marker3D nodes)     (Marker3D nodes)         │
└─────────────────────────────────────────────────┘
Characters snap to predefined Marker3D positions.
No free movement during the player's turn.
Dual Techs require specific slot adjacency.

TRIANGLE STRATEGY MODE (Tactical):
┌─────────────────────────────────────────────────┐
│                                                 │
│   [P1] . . . . . . . . . . [E1]               │
│   . . . . [P2] . . . . . . . .                 │
│   . . . . . . . . [E2] . . .                   │
│   . . [P3] . . . . . . . [E3]                 │
│   . . . . . . [P4] . . . . [E4]               │
│                                                 │
│   Characters START where they stood on the      │
│   overworld. They then move freely on the       │
│   GridMap using NavigationRegion3D paths.       │
│   Height (GridMap Y-axis) grants advantages.    │
└─────────────────────────────────────────────────┘

TRIALS OF MANA MODE (Real-Time):
┌─────────────────────────────────────────────────┐
│                                                 │
│          ╭─────────────────────╮                │
│          │                     │                │
│          │  [P1]    [E1] [E2] │  ← Arena3D     │
│          │      [P2]          │    (Area3D      │
│          │  [P3]        [E3]  │    boundary)    │
│          │          [P4]      │                │
│          │                     │                │
│          ╰─────────────────────╯                │
│                                                 │
│   Characters stay roughly where they stood.     │
│   An invisible Area3D box bounds the fight.     │
│   Free 360° movement, dodge, and combo inputs.  │
└─────────────────────────────────────────────────┘
```

---

## 3. Dawn (Game 1) — Turn-Based Combat (Chrono Trigger)

### 3.1 Core Loop
1. **Turn Order** is displayed as a vertical timeline on the right
   side of the screen (CT-style). Speed stat determines order.
2. On a character's turn, a **Command Menu** appears:
   - Attack
   - Skills / Magic
   - Dual Tech (if affinity requirement is met)
   - Item
   - Defend
   - Flee
3. Player selects a target. The character's `AnimationPlayer`
   plays the attack animation.
4. At the "impact frame" of the animation, a **Signal** fires
   `deal_damage()`, routed through `SecureInt` and `AntiTamper`.
5. Damage numbers pop. Enemy HP updates. Turn advances.

### 3.2 Dual Techs & Combo Attacks
- Unlocked when two characters reach a combined **Affinity
  threshold** (set via the Social Sim / Campfire scenes).
- Each Dual Tech has a **slot requirement**: the two characters
  must be in adjacent Marker3D slots (e.g., P1+P2 or P2+P3).
- The `AnimationPlayer` plays a combined, cinematic multi-sprite
  animation.
- **Example:**
  - Elara (Cute Bruiser) + Garrick (Badass Healer)
  - Affinity ≥ 8 → Unlocks **"Hammer of Mercy"**: Elara launches
    Garrick into the air; he casts a healing nova on impact while
    she smashes the ground for AoE blunt damage.

### 3.3 Elemental Interactions
| Element | Strong Against | Weak Against |
|---------|---------------|--------------|
| Fire | Ice, Nature | Water |
| Water | Fire | Earth |
| Earth | Water, Lightning | Nature |
| Lightning | Water, Wind | Earth |
| Wind | Earth | Lightning |
| Nature | Earth, Water | Fire |
| Light | Dark | Dark (mutual) |
| Dark | Light | Light (mutual) |
| Time | (Special) | (Special) |

- **Time Element** is unique. It does not follow the standard
  triangle. It interacts with the 48-minute clock and the 5 Eras.
  (See §9: Time Manipulation in Combat.)

### 3.4 Status Effects
| Status | Effect | Duration |
|--------|--------|----------|
| Burn | DoT (Fire) | 3 turns |
| Freeze | Skip next turn | 1 turn |
| Shock | -50% Speed | 2 turns |
| Poison | DoT (Nature) | 4 turns |
| Blind | -30% Accuracy | 2 turns |
| Haste | +50% Speed | 3 turns |
| Temporal Lock | Target is frozen in time (skip 2 turns) | 2 turns |

---

## 4. Day (Game 2) — Tactical Combat (Triangle Strategy / Fire Emblem)

### 4.1 Core Loop
1. Combat begins on the **existing GridMap**. No repositioning
   unless a scripted event dictates it.
2. Turn structure: **Player Phase → Enemy Phase** (Fire Emblem)
   OR **Individual Turn Order by Speed** (Triangle Strategy).
   *Designer's choice per encounter.*
3. On a character's turn:
   - **Move** within their Speed range (highlighted tiles via
     NavigationRegion3D pathfinding).
   - **Act** (Attack, Skill, Item, Wait).
4. After acting, the character's turn ends.

### 4.2 Height & Terrain Advantages
Because the GridMap is 3D, elevation is real.

| Height Difference | Effect |
|-------------------|--------|
| Attacker is HIGHER | +15% Damage, +10% Accuracy |
| Attacker is LOWER | -10% Damage, -10% Accuracy |
| Same elevation | No modifier |

**Terrain Modifiers:**

| Terrain | Effect |
|---------|--------|
| Forest / Dense Brush | +20% Evasion for occupant |
| Water / Shallow | -1 Movement. Fire attacks deal half damage. |
| Lava / Hazard | DoT at start of turn if standing on it. |
| Rooftop / Ledge | Cannot be reached by melee unless adjacent. Ranged bonus. |
| Indoor / Building | Ranged attacks cannot target through walls (Line of Sight check). |

### 4.3 Class-Specific Movement
| Class Archetype | Move Range | Special |
|----------------|------------|---------|
| Cute Bruiser (Tank) | 3 tiles | Can destroy destructible walls/obstacles. |
| Badass Healer (Caster) | 4 tiles | Can cast on allies within 5-tile radius. |
| Sniper / Rogue | 5 tiles | Can attack from 4-tile range. |
| Edgy Mage (Glass Cannon) | 3 tiles | AoE spells cover 2x2 or 3x3 tile areas. |

### 4.4 Pair-Up / Adjacency Bonuses
- If two allied characters are **adjacent** (orthogonal), they
  gain a small stat buff (+5% Def, +5% Accuracy).
- If affinity between them is ≥ 6, the buff doubles.
- This encourages the player to think about **positioning**
  relative to their Social Sim relationships.

### 4.5 Scripted Tactical Events
- Some battles include **mid-fight scripted events**:
  - Reinforcements arrive from a specific edge.
  - A bridge collapses (GridMap tiles are removed in real time).
  - An NPC ally appears and fights alongside the player for
    3 turns.
  - The 48-minute clock advances by one phase mid-battle,
    changing enemy behavior (see §7).

---

## 5. Dusk (Game 3) — Real-Time Action Combat (Trials of Mana)

### 5.1 Core Loop
1. Combat begins in the **Arena3D** (invisible Area3D boundary).
2. The player directly controls one character (the "Leader").
   The other 3 are AI-controlled allies (LimboAI Behavior Trees).
3. The player can **tag-swap** the Leader at any time (D-pad or
   hotkey), similar to Trials of Mana.
4. Combat is continuous. No turn order. No ATB gauge.

### 5.2 Input Mapping
| Input | Action |
|-------|--------|
| Move Stick / WASD | 360° movement within Arena3D |
| Light Attack (X / J) | 3-hit combo chain |
| Heavy Attack (Y / K) | Charged / Launcher / AoE slam |
| Dodge / Roll (B / Space) | i-frames (0.3s invincibility) |
| Skill Button (A / L) | Class-specific ability (cooldown-based) |
| Charged Skill (Hold A) | Elemental Charged Attack (ToM style) |
| Tag Swap (D-pad / 1-2-3) | Switch Leader character instantly |
| Item (D-pad Down / I) | Use consumable (brief pause) |

### 5.3 Dodge & i-Frames
- Dodging grants **0.3 seconds of invincibility**.
- A successful dodge right before an enemy attack connects
  triggers a **"Perfect Dodge"**: time slows for 1 second
  (Engine.timeScale = 0.3), and the player gets a free
  counter-attack window.
- This rewards skilled play and mirrors the Golden Sun /
  Tales of series dodge feel.

### 5.4 Elemental Charged Attacks (Trials of Mana)
- Each character has an elemental affinity.
- Holding the Skill button charges an elemental attack.
- The longer the charge (up to 3 seconds), the larger the AoE
  and the higher the damage multiplier (1x → 2.5x).
- Releasing at full charge triggers a cinematic AnimationPlayer
  sequence.

### 5.5 Ally AI (LimboAI Behavior Trees)
The 3 non-controlled allies follow a visual Behavior Tree:

```text
Root (Selector)
├── Branch 1: Is Leader HP < 30%?
│   └── Sequence: Move to Leader → Use Healing Skill → Retreat
├── Branch 2: Is an Enemy in Melee Range?
│   └── Sequence: Attack Enemy → Dodge if Enemy Winds Up
├── Branch 3: Is an Enemy Casting a Spell?
│   └── Sequence: Move to Flank → Use Interrupt Skill
└── Branch 4: Default
    └── Sequence: Follow Leader at 2m distance → Idle Animation
```

---

## 6. Party & Class System

### 6.1 Archetype Table (Trope-Subverted)

| Character | Trope | Combat Role | Visual |
|-----------|-------|-------------|--------|
| Elara | Cute Bruiser / Little Miss Badass | Tank / Physical DPS | Small girl, frilly dress, war hammer 3x her size |
| Garrick | Badass Adorable / Gentle Giant | Healer / Support Caster | Towering scarred veteran, soft-spoken, carries a staff and knitting needles |
| Vex | Edgy Rogue / Sniper | Ranged DPS / Scout | Lean, hooded, dual crossbows, dry humor |
| Maren | Battle-Hardened Mage | AoE Caster / Glass Cannon | Middle-aged woman, heavy armor, wields a greatsword in one hand and a spell tome in the other |

*These are archetypes, not fixed characters. The actual cast is
Designer-defined per game. The principle is: **never assume role
from appearance.***

### 6.2 Stat Blocks (JSON-Driven for Modding)
Each character's combat stats are stored in `data/characters.json`:

```json
{
  "elara": {
    "name": "Elara",
    "trope": "Cute Bruiser",
    "base_hp": 420,
    "base_attack": 55,
    "base_defense": 60,
    "base_speed": 30,
    "base_magic": 10,
    "element": "blunt",
    "dual_tech_partner": "garrick",
    "dual_tech_affinity_req": 8,
    "likes_food": ["Spicy Curry", "Grilled Meat"],
    "dislikes_food": ["Salad", "Herbal Tea"]
  }
}
```

### 6.3 Leveling & Growth
- XP is shared equally among the 4 active members.
- Benched characters receive 50% XP.
- Stat growth is **class-weighted**, not gender-weighted.
- Every 5 levels, the player chooses one of two **Growth Paths**
  (e.g., Elara: "Earthshaker" for more AoE, or "Bulwark" for
  more defense).

---

## 7. The 48-Minute Clock in Combat

### 7.1 Phase Modifiers
The clock is **paused** during combat, but the phase at which
combat **started** applies modifiers:

| Phase | Player Modifier | Enemy Modifier |
|-------|----------------|----------------|
| 🌅 Dawn | Healing skills +20% | Undead enemies -15% ATK |
| ☀️ Day | No modifier | No modifier |
| 🌇 Dusk | Ambush resistance +25% | Enemy crit rate +10% |
| 🌙 Night | Stealth opener bonus | Enemy ATK +15%, Speed +10% |

### 7.2 Mid-Battle Phase Shift (Rare)
- In Tactical mode, a scripted event can cause the clock to
  advance mid-battle (e.g., a 20-minute siege defense).
- When the phase shifts:
  - Lighting changes (WorldEnvironment tween).
  - Music stems crossfade (Day → Night melody).
  - Enemy Behavior Trees re-evaluate (Night = more aggressive).
  - A UI banner reads: *"Dusk falls over the battlefield..."*

### 7.3 Rest & Fatigue
- If the player has not rested in 2 full clock cycles (96 minutes),
  the party gains the **"Exhausted"** debuff: -10% to all stats.
- Resting at a Camp or Inn clears Exhausted and advances the clock
  by one full phase.

---

## 8. Affinity & Dual Tech System

### 8.1 Affinity Levels
| Level | Threshold | Unlock |
|-------|-----------|--------|
| 1 | 0 | Basic combat synergy (+2% shared stats) |
| 2 | 3 | Support dialogue at camp |
| 3 | 5 | Adjacency bonus in Tactical mode doubles |
| 4 | 8 | Dual Tech unlocked |
| 5 | 12 | Romance / Deep Support scene unlocked |
| 6 | 15 | Ultimate Dual Tech unlocked (cinematic) |
| 7 | 20 | Secret post-game combo (Real Ending content) |

### 8.2 Dual Tech Execution (Turn-Based)
1. Both characters must be in adjacent Marker3D slots.
2. The player selects "Dual Tech" from the Command Menu.
3. A combined AnimationPlayer sequence plays (2-4 seconds).
4. A single `deal_damage()` signal fires with combined stats.
5. Both characters' turn gauges reset.

### 8.3 Dual Tech Execution (Real-Time)
1. The player tags to the partner character within 2 seconds of
   the first character's Charged Attack landing.
2. The second character automatically triggers the follow-up.
3. A brief cinematic slow-motion plays (Engine.timeScale = 0.2
   for 1.5 seconds).

---

## 9. Time Manipulation in Combat

### 9.1 The Time Element
- Unique to the trilogy's lore. Does not follow the standard
  elemental triangle.
- Time-elemental attacks interact with the **48-minute clock**
  and the **5 Eras**.

### 9.2 Time Skills (Examples)

| Skill | Effect | Cost |
|-------|--------|------|
| Temporal Lock | Freeze one enemy for 2 turns. | High MP |
| Rewind | Undo the last action taken by an enemy (restore HP, remove buff). | Very High MP |
| Fast Forward | Skip the enemy's next turn entirely. | High MP |
| Era Shift | Change the battlefield's "Era" for 3 turns, altering terrain and enemy resistances. | Ultimate Cost |
| Chrono Burst | Deal damage equal to the number of turns elapsed in the current battle. | Scales |

### 9.3 Time Paradox Mechanic
- If the player uses **Rewind** or **Era Shift** too many times
  in a single battle (≥ 3), a **"Temporal Instability"** meter fills.
- At 100%, a random "Paradox Event" occurs:
  - An enemy is duplicated.
  - A random ally is sent to a different Era (removed from battle
    for 2 turns).
  - The turn order is completely randomized.
- This prevents time magic from being a "win button" and adds
  risk/reward tension.

---

## 10. Anti-Cheat Combat Integration

### 10.1 SecureInt in Combat
All combat-critical values use `SecureInt` (XOR-encrypted in RAM):
- Player HP, MP
- Enemy HP
- Damage values during calculation
- Turn counters
- Affinity scores

### 10.2 The Honey Pot
If Cheat Engine forces a value jump > 200 on any SecureInt:
- `AntiTamper.trigger_silent_corruption()` fires.
- From that moment:
  - Player damage is secretly multiplied by `randf_range(0.7, 0.9)`.
  - Critical hits have a 30% chance to **heal the enemy instead**.
  - The battle music slowly detunes via
    `AudioServer.set_bus_pitch_scale()`.
  - UI text occasionally misspells ("Attak", "Heel", "Magik").

### 10.3 The Pirate "Bad End" Pun
- If corruption_level exceeds 0.8 during a boss fight, the boss
  gains a secret phase where it is **literally invincible**.
- The game does not tell the player why.
- The only "fix" is to buy the game on Steam.

---

## 11. Enemy AI Behaviors (LimboAI)

### 11.1 Standard Enemy Behavior Tree

```text
Root (Selector)
│
├── [Priority 1] Is AntiTamper.is_compromised?
│   └── Sequence: Apply cursed RNG → Act erratically
│
├── [Priority 2] Is Player in Attack Range?
│   └── Sequence: Face Player → Play Attack Anim → deal_damage()
│
├── [Priority 3] Is TimeManager.current_phase == "Night"?
│   └── Sequence: Increase Speed by 15% → Patrol Aggressively
│
├── [Priority 4] Is Ally Enemy HP < 25%?
│   └── Sequence: Move toward Ally → Use Healing Item/Skill
│
└── [Priority 5] Default
    └── Sequence: Patrol Waypoints → Play Idle Anim
```

### 11.2 Boss AI (Multi-Phase)
Bosses use **Hierarchical Behavior Trees** with phase thresholds:

| Boss HP | Phase | Behavior Change |
|---------|-------|-----------------|
| 100%–70% | Phase 1 | Standard attack patterns. Learnable tells. |
| 69%–40% | Phase 2 | New moveset. Summons adds. Music intensity rises. |
| 39%–10% | Phase 3 | Desperation. Faster attacks. Arena hazards activate. |
| 9%–0% | Phase 4 (Optional) | True form. Time Manipulation attacks. Music shifts to boss theme. |

---

## 12. Boss Design Philosophy

- Every boss should **teach** the player a mechanic before using it
  lethally.
- At least one boss per game should require the player to use
  the **Time Element** or the **48-minute clock** to win.
- At least one boss per game should be a **callback** to a
  previous Era or previous game in the trilogy.
- Post-game super-bosses should require **all 5 Eras' mechanics**
  and max Affinity Dual Techs.
- Boss music should **evolve** the area's theme by adding
  percussion, raising BPM, and layering a counter-melody
  (Uematsu / Sakuraba tradition).

---

## 13. Combat UI / UX

### 13.1 Turn-Based (Game 1)
- **Right side:** Vertical Turn Order timeline (CT-style portraits).
- **Bottom:** Command Menu (Attack / Skill / Item / Defend / Flee).
- **Top-left:** Party HP/MP bars.
- **Center:** Damage numbers, status effect icons.

### 13.2 Tactical (Game 2)
- **Grid overlay:** Translucent tiles showing Move Range (blue)
  and Attack Range (red).
- **Unit info panel:** Bottom-left card showing selected unit's
  stats, terrain bonus, and height modifier.
- **Turn banner:** "PLAYER PHASE" / "ENEMY PHASE" sweep.

### 13.3 Real-Time (Game 3)
- **Bottom-left:** Leader HP/MP + Skill cooldown icons.
- **Bottom-right:** 3 ally portraits with mini HP bars (tag-swap).
- **Top-center:** Boss HP bar (when applicable).
- **Center:** Combo counter, damage numbers, Perfect Dodge prompt.

### 13.4 Universal
- All UI is rendered on a **CanvasLayer** (always on top).
- All HP/MP values displayed are pulled from `SecureInt.get_value()`.
- If `AntiTamper.corruption_level > 0.5`, UI text occasionally
  scrambles for 1-2 frames.

---

## 14. Encounter Balancing Guidelines

### 14.1 Difficulty Curve
| Story Beat | Enemy Level Range | Party Level Target |
|-----------|-------------------|--------------------|
| Early Game (Era 1-2) | 1 – 10 | 1 – 12 |
| Mid Game (Era 3) | 12 – 25 | 14 – 27 |
| Late Game (Era 4-5) | 25 – 40 | 28 – 42 |
| Post-Game / Super Boss | 45 – 60 | 40 – 50 |

### 14.2 Encounter Density
- **Overworld:** 1 encounter per ~45 seconds of walking (visible
  enemies, not random).
- **Hubs / Dungeons:** 1 encounter per ~25 seconds.
- **Night phase:** Encounter density increases by 30%.
- **Rest Points:** No encounters within a 10m radius.

### 14.3 Enemy Variety per Zone
- Minimum 3 enemy types per zone.
- Minimum 1 elite / miniboss per dungeon.
- At least 1 time-gated rare enemy per Era.

---

## 15. Modding Hooks for Combat

All combat data is JSON-driven and moddable:

| File | Contents |
|------|----------|
| `data/weapons.json` | Weapon stats, elements, descriptions |
| `data/characters.json` | Base stats, growth paths, dual tech pairings |
| `data/enemies.json` | Enemy stats, AI behavior tree references, loot tables |
| `data/skills.json` | Skill names, damage formulas, cooldowns, MP costs |
| `data/dual_techs.json` | Combo names, affinity requirements, animation references |
| `data/encounters.json` | Enemy group compositions per zone and time phase |

The `ModLoader.gd` merges any community JSON overrides at boot.
Steam Workshop subscriptions are auto-injected.

---

## 16. Quick Reference — Combat State Machine

```text
[EXPLORATION]
     │
     │  Enemy Area3D body_entered()
     ▼
[TRANSITION]
     ├── TimeManager.pause_clock()
     ├── Phantom Camera → Combat Priority
     ├── AnimationPlayer → Combat_Intensity_High
     ├── Player.freeze_player = true
     ▼
[COMBAT_ACTIVE]
     ├── Game 1: Turn-Based loop
     ├── Game 2: Tactical phase loop
     ├── Game 3: Real-Time loop
     │
     │  All enemies defeated OR Flee
     ▼
[COMBAT_END]
     ├── TimeManager.resume_clock()
     ├── Phantom Camera → Exploration Priority
     ├── AnimationPlayer → Exploration_Quiet
     ├── Player.freeze_player = false
     ├── XP / Loot distributed
     ▼
[EXPLORATION]
```

---

*Document Version 1.0 — November 2026*
*Author: Rin*
*Role: Creative Director / Designer*

---