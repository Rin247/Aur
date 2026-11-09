# ⚙️ CHRONO TRILOGY — UI/UX & Technical Pipeline Bible

> Companion to the World, Combat, Audio, and Lore Bibles.
> Focus: User Interface, User Experience, Steam Integration, Modding Pipeline.
> Engine: Godot 4.x (Control Nodes, CanvasLayers, Themes).
> Backend: GodotSteam, JSON ModLoader.

---

## PART 1: UI/UX DESIGN & WIREFRAMES

### 1. UI Philosophy
- **2.5D Harmony:** The UI should feel like a physical layer over the 2.5D diorama. Use subtle drop shadows, slight transparency, and parchment/metallic textures that match the current Era.
- **Non-Intrusive:** During exploration, the HUD should fade to 20% opacity after 5 seconds of no input.
- **The "Gaslight" Rule:** As established in the Anti-Cheat doc, if `AntiTamper.corruption_level > 0.5`, UI elements will randomly jitter by 1-2 pixels, and text will occasionally misspell itself for 0.5 seconds. Legitimate players never see this.

### 2. The Global Theme Resource
Instead of styling every button individually, create a master `Theme` resource in Godot.
- **Fonts:** 
  - *Headers:* A clean, slightly stylized serif (e.g., Cinzel or Trajan) for that classic JRPG feel.
  - *Body Text:* A highly legible sans-serif (e.g., Noto Sans or Open Sans) for Dialogic and item descriptions.
- **Colors:** Map colors to the 5 Eras. When the player opens the menu in Era 3 (Industrial), the UI borders subtly shift to brass and smog-grey. In Era 1 (Ancient), they shift to glowing crystal-blue and vine-green.

### 3. Exploration HUD
Minimalist. Keeps the player's eyes on the beautiful 2.5D environments.

```text
┌─────────────────────────────────────────────────────────────┐
│ [Party Leader Portrait]                                     │
│ HP: ██████████  MP: ████████░░                              │
│                                                             │
│                                                             │
│                                                             │
│                                                             │
│                                                             │
│                                                             │
│                                     [48-Min Clock Dial]     │
│                                     (Shows current phase:   │
│                                      Dawn/Day/Dusk/Night)   │
│                                                             │
│ [Interact Prompt: "E"]                                      │
└─────────────────────────────────────────────────────────────┘
```
- **The Clock Dial:** A beautiful, diegetic-looking pocket watch or astrolabe in the bottom right. The hands move in real-time. The background color of the dial shifts (Pink → Yellow → Orange → Deep Blue) as the 12-minute phases tick by.
- **Interaction Prompts:** When near an NPC or chest, a clean, animated `[ E ]` or `[ A ]` button floats in 3D space above the object (using a `Sprite3D` or `Label3D`), not locked to the 2D screen.

### 4. Combat HUDs (Game Specific)

#### Dawn (Game 1): Turn-Based (Chrono Trigger Style)
```text
┌─────────────────────────────────────────────────────────────┐
│ [Enemy 1 HP Bar]          [Enemy 2 HP Bar]                  │
│                                                             │
│                  (3D Battlefield)                           │
│                                                             │
│ [P1 Portrait]                                               │
│ HP/MP Bars                                                  │
│ ┌──────────────┐                        [Turn Timeline]     │
│ │ 1. Attack    │                        │ 1. Elara (P1)     │
│ │ 2. Skill     │                        │ 2. Goblin (E1)    │
│ │ 3. Dual Tech │                        │ 3. Garrick (P2)   │
│ │ 4. Item      │                        │ 4. Vex (P3)       │
│ │ 5. Defend    │                        │ 5. Boss (E2)      │
│ └──────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

#### Day (Game 2): Tactical (Triangle Strategy Style)
```text
┌─────────────────────────────────────────────────────────────┐
│ [Turn Banner: PLAYER PHASE]                                 │
│                                                             │
│                  (3D Grid Battlefield)                      │
│            [Hovering Tile Info: "High Ground +15% ATK"]     │
│                                                             │
│ ┌──────────────────────────┐                                │
│ │ UNIT: Elara              │      [Minimap with Fog of War] │
│ │ Move: 3  Atk: 1          │                                │
│ │ Terrain: Forest (Eva+20) │                                │
│ │ [Move] [Action] [Wait]   │                                │
│ └──────────────────────────┘                                │
└─────────────────────────────────────────────────────────────┘
```

#### Dusk (Game 3): Real-Time (Trials of Mana Style)
```text
┌─────────────────────────────────────────────────────────────┐
│ [Boss HP Bar: "The Conductor - Phase 2"]                    │
│                                                             │
│                  (3D Arena Battlefield)                     │
│                                                             │
│                                                             │
│ [P1 Leader HP/MP]                                           │
│ [Skill 1 CD] [Skill 2 CD] [Charged Atk: ████░░]            │
│                                                             │
│           [Ally 2]  [Ally 3]  [Ally 4]  <-- Tag Swap Icons  │
└─────────────────────────────────────────────────────────────┘
```

### 5. The Social Sim / Campfire UI
This is where the player spends time managing the "Live-Sim" elements. It should feel warm, inviting, and tactile.

- **The Layout:** A 2D overlay (`CanvasLayer`) that blurs the 3D background behind it (using a `ColorRect` with a `BackdropMaterial` and `Blur` shader).
- **The Gift-Giving Minigame:**
  1. Player opens inventory.
  2. Drags an item (e.g., "Spicy Curry") onto a character's portrait.
  3. **Reaction Animation:** The portrait swaps to a "Happy" or "Disgusted" expression.
  4. **Affinity Meter:** A small heart meter next to their portrait fills up (or cracks) with a satisfying SFX.
  5. **Dialogic Trigger:** The UI fades out, and the Dialogic 2 timeline takes over for the conversation.

### 6. The World Map & Time Travel UI
- **The Map:** A stylized, hand-drawn parchment map.
- **Time Gates:** Glowing, animated nodes on the map. 
- **The "Era Shift" Transition:** When the player selects a different Era, the map doesn't just fade to black. The parchment burns away, or the ink washes away like watercolor, revealing the map of the new Era underneath, accompanied by the cross-era audio stinger.

### 7. Accessibility (Crucial for Steam)
Include an "Accessibility" tab in the Options menu from Day 1.
- **Text Scaling:** Slider from 100% to 200%. (Ensure all `Control` nodes use `Size Flags: Expand/Fill` so the UI doesn't break when text grows).
- **Colorblind Modes:** Protanopia, Deuteranopia, Tritanopia filters. (Crucial for elemental puzzle solving and status effect icons).
- **Dyslexia Font Toggle:** Swaps the body text font to OpenDyslexic.
- **Combat Timing Assist:** For Dusk (Game 3, Real-Time), an option to slow down the "Perfect Dodge" window or add visual flashes when an enemy is about to attack.

---

## PART 2: STEAM INTEGRATION (GODOTSTEAM)

### 1. What is GodotSteam?
GodotSteam is a custom fork of the Godot Engine that has the Steamworks API baked directly into it. You download it, use it as your editor, and export with it. It gives you access to Steam's backend via simple GDScript commands.

### 2. Achievement Mapping
Design achievements that reward exploration, trope-subversion, and the 48-minute clock.

| Achievement ID | Name | Description | Trigger |
|----------------|------|-------------|---------|
| `ACH_CUTE_BRUISER` | Small But Mighty | Deal 9,999 damage with Elara. | `CombatManager.check_damage()` |
| `ACH_NIGHT_OWL` | Creatures of the Night | Win 10 battles during the "Night" phase. | `TimeManager.phase_changed` + Combat Win |
| `ACH_PARADOX` | Temporal Instability | Trigger a Time Paradox in combat. | `TimeMagic.apply_paradox()` |
| `ACH_BAD_END` | Kazoo of Destiny | Witness the Bad Ending. | `GlobalProfile.set_flag("ending_bad")` |
| `ACH_REAL_END` | The First Chord | Unlock the Secret Real Ending. | `GlobalProfile.set_flag("ending_real")` |

**Implementation Snippet:**
```gdscript
# When the player wins their first Night battle:
func _on_night_battle_won():
    Steam.setAchievement("ACH_NIGHT_OWL")
```

### 3. Cloud Saves
Since your `GlobalProfile.json` lives in the OS Documents folder, you need to tell Steam to sync that specific file so players can move from their Desktop to their Steam Deck seamlessly.
- In the Steamworks Partner backend, enable Cloud Saves.
- Set the byte limit (e.g., 10MB).
- In GodotSteam, use `Steam.readFile()` and `Steam.writeFile()` instead of Godot's native `FileAccess` for the Global Profile, so Steam automatically intercepts and syncs it.

### 4. Leaderboards
JRPG fans love optimization. Create these hidden leaderboards:

1. **Speedrun (Game Clear Time):** Tracks `OS.get_ticks_msec()` from "New Game" to "Final Boss Death", minus time spent in paused menus.
2. **Lowest Level Clear:** Tracks the combined level of the 4 active party members when the final boss dies.
3. **Pacifist / No-Rest Run:** Tracks if the player beat a dungeon without using the "Rest" mechanic.

**Implementation Snippet:**
```gdscript
# On Final Boss Death:
var clear_time_seconds = TimeManager.get_total_playtime()
Steam.uploadLeaderboardScore(leaderboard_id, clear_time_seconds)
```

---

## PART 3: THE MODDING & EXPORT PIPELINE

### 1. The Export Folder Structure
When you export the game for Windows and Linux, the folder structure on the player's PC should look exactly like this to support the `ModLoader.gd` we built earlier:

```text
ChronoTrilogy_Game1/
│
├── ChronoTrilogy.exe          (The Windows executable)
├── ChronoTrilogy.sh           (The Linux executable)
├── steam_api64.dll            (Required for Steam DRM & API)
├── libsteam_api.so            (Required for Linux Steam API)
├── global_profile.json        (Created by the game on first boot)
├── data.pck                   (The locked, encrypted base game files)
│
└── Mods/                      (Created by the player or Steam Workshop)
    ├── Super_Hammer_Mod/
    │   └── weapons.json       (Overrides base hammer stats)
    └── Kajiura_Fan_Pack/
        ├── audio_config.json  (Re-routes music stems)
        └── audio/
            └── custom_theme.ogg
```

### 2. Steam Workshop Integration
Instead of forcing players to download mods from NexusMods and drag them into the `Mods/` folder, Steam Workshop does it automatically.

1. **Steamworks Backend:** Create a "Microtransaction/Workshop" item in the Steamworks partner portal.
2. **GodotSteam Code:** Add a Workshop listener to your `ModLoader.gd`:

```gdscript
func check_workshop_mods():
    if Steam.isSteamRunning():
        # Get all Workshop items the player is subscribed to
        var subscribed_items = Steam.getSubscribedItems()
        
        for item_id in subscribed_items:
            var item_info = Steam.getItemInstallInfo(item_id)
            if item_info["ret"]:
                # item_info["folder"] is where Steam downloaded the mod
                var mod_path = item_info["folder"]
                inject_mod_data(mod_path) # Re-use our existing JSON merge logic!
```

### 3. The "Modder's Toolkit" (Release Day DLC)
To encourage modding, release a free, 50MB "Modder's Toolkit" DLC on Steam on launch day.
- **Contents:**
  - Blank JSON templates for Weapons, Characters, and Enemies.
  - A PDF guide on how to use Dialogic 2 to write custom campfire scenes.
  - A Blender template file with the exact camera settings and lighting rigs used for the 2.5D sprites.
  - A Bitwig project file showing how the stems are routed for the Halo-style layering.
- **Why?** This turns your player base into your content team. Within a month, the Workshop will be flooded with new eras, new characters, and new music.

### 4. Linux & Windows Export Checklist
Since you are skipping Apple (a very smart move for a solo/small team), your export pipeline is streamlined.

- [ ] **Windows:** Export as `.exe`. Ensure `steam_api64.dll` is in the same folder. Test on a clean Windows VM to ensure no missing Visual C++ redistributables.
- [ ] **Linux:** Export as `.x86_64` (or `.sh`). Ensure `libsteam_api.so` is in the same folder. 
- [ ] **File Paths:** Audit all GDScript. Ensure you never used `C://` or hardcoded Windows paths. Always use `res://` for game files, `user://` for local settings, and `OS.get_system_dir()` for the Global Profile.
- [ ] **Steam Deck Compatibility:** Add the "Steam Deck Supported" tag in the backend. Because Godot 4's Vulkan renderer scales beautifully, and your UI uses relative `Control` anchors, it should plug-and-play on the Deck. Test the text scaling to ensure it's readable on the 7-inch screen.

---

## 4. The "Director's Dashboard" (Debug Tools)
As the designer, you need a way to test the 48-minute clock, the endings, and the anti-cheat without playing for 40 hours. 

Create a hidden `DebugMenu.tscn` that only loads if `OS.has_feature("editor")` is true.
- **Time Slider:** A slider that instantly forces the `TimeManager` to Dawn, Day, Dusk, or Night.
- **Affinity Maxer:** A button that sets all party affinity to 20 (Max) to test Dual Techs and Romance scenes.
- **Ending Selector:** 4 buttons that instantly trigger the Bad, Good, True, or Real ending cutscenes.
- **Pirate Simulator:** A toggle switch that manually sets `AntiTamper.is_compromised = true` so you can watch the UI scramble and the audio detune to ensure it works correctly.

---

*Document Version 1.0 — November 2026*
*Author: Rin*
*Role: Creative Director / Designer*

---