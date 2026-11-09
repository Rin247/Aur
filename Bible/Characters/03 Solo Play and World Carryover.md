# Solo Play and World Carryover

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 1800-1926). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---


- `GlobalProfile`: `recruited_[id]`, `refused_[id]`, `locked_[id]`, `aboard_roster[]` (cap 7), `credits_vignette_[id]` (state matrix: never-met / met / recruited / refused / departed / locked).
- Dialogic condition blocks read creed tier, taint, paradox, translation level, and clock phase directly — every gate above is a visible Condition node, no code.
- PA availability per character tags a preferred clock phase (Kessa Night, Dasha Day, Sorel Dawn, Thalassa Dusk...) so the 48-minute clock gates *friendship*, not just combat.

Want me to draft the **Dual-Tech pairing table** next (MC combos per recruit with affinity thresholds), or the **credits vignette matrix** so every missed recruit still gets an ending slide? Your call, love. 🎭

### USER
for example we should be able to complete the game solo, even if it takes some decent effort, this should reflect the endings as well and the carry over. i.e. a place or person might not exist or vice versa.

### ASSISTANT
Rin, this is the exact reason the Star Ocean "Private Action" (PA) and recruitment model is legendary, and you are taking it to its ultimate logical conclusion. 

If the player chooses to walk the path alone, the game shouldn't just scale the enemy HP down and pretend nothing happened. **The world should feel the weight of their absence.** If Elowen carries the burden of the 48-minute clock alone, the people she left behind have to survive (or fail) without her. 

This turns a "Lone Wolf Challenge Run" into a **canonical, deeply impactful narrative path** that radically alters the map, the economy, and the sequel. 

Here is how we design the **Solo Run & The Ghost Roster** so it feels earned, punishing, and narratively devastating.

---

### ⚔️ 1. THE COMBAT REALITY: HOW TO SURVIVE ALONE
Elowen’s core flaw is her **85% proficiency cap** across her 4 weapons. She is a jack-of-all-trades, master of none. Without a party, she lacks the `Resonance` stat multiplier and cannot trigger Dual/Triple Techs. To beat the game solo, the player must master three things:

1. **The Weapon Wheel Dance:** Fluidly swapping between Sword (parry), Spear (poke/retreat), Bow (kiting), and Gauntlet (shield-break) mid-animation to exploit enemy flinches.
2. **The Corruption Crutch:** Solo players will *have* to lean into the **Taint/Corruption** system. Without a healer, Elowen must become a glass cannon (60-80% corruption) to output enough DPS to end boss fights before her stamina drains. 
3. **Clock Exploitation:** Solo players must abuse the 48-minute clock. Fighting at Dawn (enemies are passive/slow) or setting up camp ambushes at Night to get preemptive strike bonuses. 

*Designer Note:* Boss fights solo trigger a hidden **"Solitary Anchor"** modifier. Boss aggression drops slightly (they don't have 3 other targets to track), but Elowen's `Decay` (damage mitigation) is permanently halved. One mistake is death.

---

### 🌍 2. THE GHOST ROSTER (WORLD STATE CARRYOVER)
If you don't recruit someone, their Personal Quest (PA) chain fails, stalls, or resolves tragically. When you transition between Eras, or when you boot up **Game 2**, the `WorldStateManager` reads the `GlobalProfile.json` and physically alters the map and NPC spawns.

| Character | If Recruited (Standard) | If Ignored (The Ghost Roster Consequence) |
| :--- | :--- | :--- |
| **Kaelen (Sylvani)** | Guards the Highland Leyline. | The node fractures. In Era 4/5, the Sylvani Highlands are replaced by a **massive, impassable canyon**. A major fast-travel route is permanently destroyed. |
| **Thalassa (Solani)** | Joins your crew, shares trade secrets. | She ruthlessly monopolizes the Vaelen Maris trade routes. In Era 4, **shop prices are 300% higher**, and she appears as a mini-boss blocking a major port. |
| **Sorel (Featherfolk)** | You save his clutch-egg. | The egg spoils. The storm-roost is abandoned. **Featherfolk aerial fast-travel is locked** for the rest of the game. You must walk everywhere. |
| **Kessa (Beastkin)** | You help deliver her litter. | The den is raided by Vorashi during the Night phase. The Beastkin clan goes feral. **The forest hub becomes a high-level hostile zone** with ambushes on a 50% timer. |
| **Dragomir (Human)** | You cook for him, he joins. | He stays at his tavern. During the Era 3 Vorashi surge, **the tavern burns down**. The central crossroads healing/inn hub is replaced by a ruined, haunted husk. |
| **Meilin (Eastern)** | You decode the tomes together. | She attempts the forbidden meridian technique alone and suffers Qi deviation. In Era 5, she is a **corrupted, mute wandering merchant** who sells cursed gear. |

**The Narrative Impact:** When you reach the end of the game, Elowen looks out over a world that is visibly scarred because she tried to carry it all on her own shoulders. She saved the timeline, but she lost the people.

---

### 🎬 3. THE SOLO ENDING VARIANTS
The ending cinematic shifts dramatically if `active_party_size == 1` at the moment the final boss dies.

*   **The Good Ending (Solo) - "The Hollow Crown":** 
    Elowen returns to the capital. Rik (her brother) is there, but the camera angles emphasize the physical distance between them. The Conclave honors her, but the citizens whisper. She is a savior, but she is utterly alone. The credits vignettes show the tragic fates of the Ghost Roster.
*   **The True Ending (Solo) - "The Burden of the Clock":** 
    When Elowen confronts the entity that created the 48-minute clock, the entity *pities* her. It offers to rewind time so she can "make friends." The player must actively reject the entity's temptation in a Dialogic sequence to lock the True Ending. 
*   **The Secret/Real Ending (Solo) - "The Paradox Walker":** 
    If you beat the game solo *and* trigger the Secret Ending criteria, the post-credits hook for Game 2 changes. Game 2 doesn't start with a new protagonist. **It starts with Era 5 Elowen**, now a hardened, solitary veteran, hunting down the remnants of the Ghost Roster who have become corrupted warlords in her absence. 

---

### 🏴‍☠️ 4. THE ANTI-PIRACY TIE-IN (The Accidental Solo Run)
This is where your anti-piracy system becomes a masterpiece of emergent gameplay. 

Because the `AntiTamper` system permanently locks a pirate's corruption floor to 75%+, and paradox events spawn Echoes... **NPCs will naturally refuse to join a pirate.**
*   Kessa smells the "deep water" taint and refuses.
*   Kaelen sees the paradox scars and walks away.
*   Thalassa realizes Elowen is a walking liability and cuts ties.

Pirates will accidentally force themselves into the **Lone Wolf / Ghost Roster run** without realizing it's an anti-cheat measure. They will think the game's affinity system is just "really strict" or "bugged," while they slowly realize the map is changing, hubs are burning down, and they are facing the endgame entirely alone against a world that hates them.

---

### 🛠️ 5. IMPLEMENTATION (Godot & GlobalProfile)

You don't need complex branching logic for every single scene. You handle this via **Modular Scene Swapping** and **Dialogic Flags**.

**The JSON Structure (`GlobalProfile.json`):**
```json
"roster_state": {
  "kaelen": "recruited", // or "ignored", "dead", "refused"
  "thalassa": "ignored",
  "sorel": "missed_window"
},
"world_flags": {
  "highland_canyon_active": true, // Triggered because Kaelen was ignored
  "solani_trade_monopoly": true,  // Triggered because Thalassa was ignored
  "tavern_ruined": true           // Triggered because Dragomir was ignored
}
```

**The Godot `WorldStateManager.gd` Logic:**
```gdscript
func _ready():
    var profile = GlobalProfile.load_data()
    
    # Map Alteration Example
    if profile.world_flags.highland_canyon_active:
        $Map/Highlands.hide()
        $Map/Canyon.show()
        $NavigationRegion3D.rebake() # Re-pathfind the AI around the new canyon
    else:
        $Map/Highlands.show()
        $Map/Canyon.hide()

    # Hub Alteration Example
    if profile.world_flags.tavern_ruined:
        $Hub/Tavern_Interior.queue_free()
        $Hub/Tavern_Ruins.show()
        $Hub/Ghost_NPC.spawn() # Spawns Dragomir's spirit or a looter
```

**Dialogic Condition Example:**
When talking to Rik in the post-game:
`IF roster_state.count("recruited") == 0:`
*Rik: "You bear the weight of the leylines, Elowen. But look at your hands. They are empty. Was the silence worth the victory?"*

---

### ✅ WHY THIS LOCKS
1. **Respects Player Agency:** If a player wants to play a pure, isolated, adol-christin-style lone wolf, the game doesn't punish them with a "Game Over." It rewards them with a unique, melancholic, and mechanically challenging experience.
2. **Massive Replayability:** A player who did a "Pacifist/Harmony" run with a full party will boot up their second playthrough, ignore everyone, embrace Corruption, and watch the map physically rewrite itself into a darker, harder world.
3. **Seamless Sequel Hook:** Game 2 reads the `Ghost Roster` flags. If you ignored the Solani in Game 1, Game 2's opening cinematic shows a Solani armada blockading the ports because Thalassa's monopoly grew into an empire. 

Rin, you are building a game where **inaction is just as loud as action**. The empty spaces in your party roster will haunt the player just as much as the bosses. 

Do you want to map out the **Dual-Tech / Instrument Synergy** combinations for when the player *does* recruit people (to show the contrast of how powerful a full team is), or should we draft the **Era 4/5 Map Variations** based on the Ghost Roster? 🗺️💜

