# WORLD SAGA — World Building Bible

> Three planned 2.5D JRPGs; engine and implementation details in this legacy bible are historical context.
> Inspirations: Octopath Traveler, Final Fantasy, Golden Sun, Chrono Trigger,
> Triangle Strategy, Trials of Mana, Clair Obscur: Expedition 33, Star Ocean,
> Tales of Series, Nier Automata, Final Fantasy X-2, Donkey Kong Country 2/3

---

## 1. Vision Statement

Three interconnected 2.5D JRPGs set in a **single, unified shared world**, spanning
five distinct time periods. The three currently planned games (Dawn, Day, and Dusk) operate
on the exact same underlying world geography and map data. What changes between games
and eras is the dynamic state: the active grid layout, the walkable collision plane,
object placement, NPC schedules, and temporal era states.

The planned trilogy is bound together by time travel, a 48-minute in-game clock, a global save
system that remembers player choices across all three titles, and four layered endings per game.
Night is a possible fourth game codename, but is currently unplanned and out of scope.

---

## 2. The Trilogy Structure

These are working codenames, not final titles. Game numbers remain useful for legacy references.

| Game | Working Codename | Combat Style | Inspiration | Party Size |
|------|------------------|--------------|-------------|------------|
| **Game 1** | **Dawn** | Turn-Based | Chrono Trigger (dual-techs, combo attacks, slot positioning) | 4 (swappable) |
| **Game 2** | **Day** | Tactical / Grid | Modern Fire Emblem / Triangle Strategy (height, terrain, scripted movement) | 4 (swappable) |
| **Game 3** | **Dusk** | Real-Time Action | Seiken Densetsu 3 / Trials of Mana (dodge, combo, elemental charged attacks) | 4 (swappable) |
| **Unplanned** | **Night** | Not defined | Not defined | Not defined |

- Squad-based only. No large-scale army battles.
- The three planned games share the **same overworld grid**. Battles happen
  exactly where the player is standing. No scene transitions.
- The 4-character active party can be swapped at camps, inns, or
  designated "Rest Points."

---

## 3. The World Map & Hub System

### 3.1 The Macro Layer — Progressive Zoom-Out Overworld & "Google Maps" World Editor
- A classic JRPG overworld map (inspired by *Chrono Trigger* and *Golden Sun*) featuring **progressive camera zoom-out tiers**.
- **World Editor Vision:** Designed as a **"Google Maps for 16-bit RPGs"** — a full GIS-style world editor where creators can seamlessly pan, zoom, spin the globe, place hubs, paint regional biomes, and link exit nodes across the entire planet Aur.
- **Early Game Exploration:** Starts tightly zoomed-in on localized regional continents (high detail, character sprite prominent, intimate exploration).
- **Mid-to-Late Game Exploration:** As the player unlocks advanced travel options (ships, airships, experimental vessels), the camera dynamically zooms out (Mode 7 style perspective scaling) to reveal wider regional maps and ultimately the full planetary map.
- Locations, towns, dungeons, and vessels are mapped across these seamless zoom tiers.
- The world map visually changes across the **5 Time Periods** (see §5).

### 3.2 The Micro Layer — Hub Areas
- Entering a town, dungeon, forest, or city transitions the player
  into a **Hub Area**.
- Hubs are fully realized 2.5D dioramas (Clair Obscur: Expedition 33
  philosophy) where exploration, NPC interaction, shopping, and combat
  all coexist on the same physical grid.
- There is **no separate battle screen**. If an enemy is touched, the
  camera swoops, the clock pauses, and combat begins in-place.

### 3.3 The Shared Grid
- The overworld, hub, and battlefield are the **same NavigationRegion3D**.
- Turn-Based: Characters snap to predefined Marker3D slots (left/right).
- Tactical: Characters use the painted GridMap + height data to move.
- Real-Time: An invisible Area3D "arena" bounds the fight zone.

---

## 4. The 2.5D Visual Direction

- **Sprites:** 2D pixel-art or hand-painted characters rendered on
  `Sprite3D` nodes with Billboard enabled (always face camera).
- **Environments:** Full 3D geometry (GridMap, CSG, or imported meshes)
  with volumetric fog, dynamic shadows, and time-of-day lighting.
- **Camera:** Phantom Camera plugin handles smooth transitions.
  - Exploration: High isometric / ¾ top-down angle.
  - Combat: Low, dramatic, upward-facing angle (Golden Sun / FF).
  - Cutscenes: Free cinematic paths.
- **Day/Night Lighting:** WorldEnvironment shifts sun angle, color
  temperature, and volumetric density based on the 48-minute clock.

---

## 5. The Five Time Periods (Chrono Trigger Structure)

The player travels between **5 distinct eras**. Each era alters the
world map, hub layouts, NPC populations, available quests, and enemy
rosters.

| Era | Working Title | Visual Tone | Key Theme |
|-----|--------------|-------------|-----------|
| Era 1 | *(Designer to define)* | Ancient / Primal | Origin of the world's magic |
| Era 2 | *(Designer to define)* | Classical / Flourishing | Rise of civilizations |
| Era 3 | *(Designer to define)* | Industrial / Conflict | War and technological shift |
| Era 4 | *(Designer to define)* | Decay / Ruin | Consequences of past choices |
| Era 5 | *(Designer to define)* | Far Future / Transcendent | Resolution or apocalypse |

- Time travel is a **core mechanic**, not just a story beat.
- Actions in one era can alter the landscape, NPCs, or item
  availability in another (butterfly effect).
- Some secrets are only accessible by visiting the same location in
  multiple eras.

---

## 6. The 48-Minute Clock & Day/Night Cycle

### 6.1 Structure
- **Total cycle:** 48 real-time minutes.
- **4 Phases**, each lasting **12 minutes:**

| Phase | Time | Gameplay Effect |
|-------|------|-----------------|
| 🌅 Dawn | 0:00 – 12:00 | Healing items slightly boosted. Undead enemies weakened. |
| ☀️ Day | 12:00 – 24:00 | Standard modifiers. Shops open. NPCs active. |
| 🌇 Dusk | 24:00 – 36:00 | Ambush chance increases. Some NPCs head home. |
| 🌙 Night | 36:00 – 48:00 | Enemies stronger/more aggressive. Player stealth bonus. Rare nocturnal encounters. |

### 6.2 Rules
- The clock **pauses** during combat, cutscenes, and dialogue.
- The clock **advances** when the player rests at a Camp, Tent, or Inn.
- The clock affects:
  - Random encounter tables and enemy strength modifiers.
  - NPC schedules, dialogue, and shop availability.
  - Scripted story events with minor variations (same conclusion).
  - Music (tempo, instrumentation, stems — see Audio Bible).
  - Lighting and volumetric atmosphere.

### 6.3 Rest & Ambush System
- Resting is **mandatory** to avoid fatigue debuffs.
- While resting, the player can be ambushed (if in a dangerous zone
  at Night) or receive a preemptive-strike bonus (if resting at Dawn
  in a safe zone).
- Some story events only trigger if the player rests at a specific
  location during a specific phase.

---

## 7. The Four Endings (Per Game)

Each game in the trilogy has **4 endings**, structured as follows:

### 7.1 Bad Ending — "The Pun"
- Triggered by a catastrophic player failure or dark choice.
- Designed as a **darkly comedic pun** or ironic twist
  (Nier Automata philosophy: "You let the world burn. The credits
  roll over the sound of a single sad trombone.").
- Accessible early. Does not require special criteria.

### 7.2 Good Ending
- The standard "you saved the day" conclusion.
- Achieved by completing the main story normally.
- Rolls standard credits. Returns to main menu.

### 7.3 True Ending
- Requires the player to make key story choices, defeat optional
  mid-game bosses, or unlock specific lore entries.
- Reveals the deeper narrative truth of the trilogy's world.
- Rolls extended credits. Leads into a post-credits scene.

### 7.4 Secret / "Real" Ending
- Unlocked **only** after meeting obscure, hidden criteria
  (similar to FFX-2's 100% completion or DKC2/3's secret exits).
- Plays **immediately after the True Ending's post-credits scene**.
- The screen glitches. The music detunes. A hidden cinematic plays.
- **This ending directly sets up the sequel.**
- Criteria examples:
  - Spare a specific NPC at Midnight.
  - Find a hidden item in Era 1 and carry it to Era 5.
  - Complete all optional dungeons across all 5 time periods.
  - Achieve max affinity with all 4 party members.

### 7.5 Inter-Trilogy Continuity
- The `GlobalProfile.json` (stored in the player's Documents folder)
  tracks which endings were seen, which flags were set, and which
  secrets were found.
- Game 2 reads Game 1's profile on boot. Game 3 reads both.
- The Secret Ending of Game 3 is the **true finale of the trilogy**
  and contains the definitive sequel hook.

---

## 8. Optional & Secret Content

### 8.1 Early Game Secrets
- Hidden caves or paths visible on the world map but inaccessible
  until the player finds a specific key item or visits in the correct
  time period.
- Optional "tutorial bosses" that teach advanced mechanics.
- Secret dialogue with seemingly unimportant NPCs that foreshadows
  the True/Real endings.

### 8.2 Mid Game Secrets
- Hidden dungeons unlocked by solving environmental puzzles across
  two or more time periods.
- Optional party members or weapon upgrades found off the critical path.
- "Wanderer Encounters": rare, time-of-day-specific boss fights
  (e.g., a phantom that only appears at Dusk in Era 3).

### 8.3 Late Game Secrets
- Post-story "New Game+" dungeons that remix enemy encounters.
- Lore archives that reveal the true history of the world.
- Callback bosses from previous eras, now empowered.

### 8.4 Post-Game Content
- Super-bosses that require knowledge of all 5 time periods.
- A hidden "6th Era" or "Time Between Times" accessible only after
  the True Ending.
- The Real Ending questline, which is essentially a short bonus
  chapter leading into the sequel.

---

## 9. Character Archetypes & Trope Subversion

### 9.1 Design Philosophy
- **Flip traditional JRPG gender coding.**
- Women can be tanks, bruisers, and frontline warriors.
- Men can be healers, casters, and support characters.
- Embrace tropes like: Cute Bruiser, Badass Adorable, Little Miss
  Badass, Gentle Giant Healer, Battle-Hardened Mage.
- Not a strict 1:1 ratio. The goal is **variety and surprise**.

### 9.2 The Active Squad
- 4 characters active at any time.
- Characters can be swapped at rest points.
- Each character has:
  - A combat role (Tank / DPS / Healer / Support / Caster / Hybrid).
  - A social personality (Likes, Dislikes, Quirks).
  - A personal story arc tied to one or more Time Periods.
  - Affinity levels that unlock Dual Techs / Combo Attacks.

### 9.3 Social Sim / Live-Sim Elements
- **Campfire & Tavern Scenes:** The party gathers, cooks, talks,
  and bonds. The player can initiate conversations, give gifts,
  and choose dialogue options.
- **Food System:** Each character has Likes and Dislikes. Cooking
  or gifting preferred meals boosts affinity. Wrong choices lower it.
- **Romance / Support Options:** High affinity unlocks personal
  side-stories, romance paths, and unique combat synergies.
- **Inspirations:** Fire Emblem Support Conversations, Bethesda /
  D&D dialogue checks, Persona social links.

---

## 10. Time Travel & Manipulation Mechanics

- Time travel is both a **narrative device** and a **gameplay tool**.
- The player can:
  - Travel between the 5 Eras at designated "Time Gates."
  - Manipulate local time in puzzles (speed up / reverse / freeze
    objects in the environment).
  - Encounter "Echoes" of party members from other time periods.
- Some enemies exist **outside of time** and are unaffected by
  temporal manipulation.
- The 48-minute clock is diegetically linked to the time-travel
  mechanic (the world's "temporal heartbeat").

---

## 11. Interconnected Lore & Sequel Hooks

- All three games take place in the **same world**, potentially
  overlapping in time.
- Characters, items, and locations from Game 1 appear in Games 2
  and 3 (and vice versa).
- The Global Profile tracks:
  - Endings seen.
  - Secret flags set.
  - Key story choices (spare/kill, save/sacrifice).
  - Optional content completed.
- The **Real Ending** of Game 3 is the canonical bridge to the
  sequel project.
- Post-credits scenes in each game tease the next entry.

---

## 12. Target Platform & Distribution

- **Primary Platform:** Steam (Windows + Linux).
- **No Apple/macOS support** (by design, to simplify development).
- **GodotSteam** wrapper for:
  - Achievements.
  - Cloud Saves.
  - Leaderboards (Clear Time, Lowest Level Boss Kill, Speedrun).
  - Steam Workshop (Mod distribution).
- **Modding Support:** JSON-based data files in a public `Mods/`
  folder. ModLoader Autoload merges overrides at runtime.

## 12. Vertical Biomes & Domains (Underwater & Sky Stages)

In addition to ground overworlds and hub dioramas, the shared world features expansive vertical domains:
- **Underwater Domains:** Submerged oceanic kingdoms, sunken ruins, and deep-sea trenches. Character movement shifts to diving/swimming physics, buoyancy, oxygen/breath management, and specialized aquatic combat encounters with muffled acoustic filtering.
- **Sky Domains:** Floating continents, cloud sea archipelagos, and soaring aeries accessible via airship or temporal flight gates. Features wind currents, altitude-based atmospheric lighting, and open-sky acoustic reverb.
- Both vertical biomes integrate seamlessly with the 48-minute clock, era-switching time travel, and the shared-plane combat transitions.

---

## 13. Technical Pillars (Summary)

| System | Tool / Method |
|--------|--------------|
| Engine | Godot 4.x (GDScript) |
| 2.5D Rendering | Sprite3D + Billboard + Volumetric Lighting |
| Camera | Phantom Camera Plugin (Priority-based swoops) |
| World Grid | GridMap + NavigationRegion3D (shared for exploration & combat) |
| Dialogue & Social Sim | Dialogic 2 Plugin |
| Enemy AI | LimboAI (Visual Behavior Trees) |
| Anti-Cheat / Anti-Piracy | SecureInt (XOR memory encryption) + AntiTamper (Silent Karma) |
| Clock | TimeManager Autoload (48-min cycle, 4 × 12-min phases) |
| Global Save | GlobalProfile.gd → JSON in Documents folder |
| Modding | ModLoader.gd → JSON merge from `Mods/` folder |
| Audio | Bitwig Stems → Godot Audio Buses (Halo/FFXIV adaptive layering) |
| Steam | GodotSteam (Achievements, Leaderboards, Workshop) |

---

*Document Version 1.0 — November 2026*
*Author: Rin*
*Role: Creative Director / Designer*

---