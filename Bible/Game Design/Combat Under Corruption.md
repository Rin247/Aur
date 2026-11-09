# Combat Under Corruption

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 1198-1300). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## ⚔️ COMBAT: CORRUPTION NULLIFIES CORRUPTION

This is the mechanical heart of the system. The elemental advantage system we built (Ignis, Terran, Aqua, Zephyr + hybrids) operates on a simple rule: **matching elements neutralize, opposing elements amplify.**

### The Nullification Rule

| Your Element vs. Enemy Element | Result |
|:---|:---|
| **Opposing** (e.g., your Aqua vs. enemy Ignis) | **Amplified damage.** Standard elemental advantage. +25% damage. |
| **Matching** (e.g., your Aqua vs. enemy Aqua) | **Nullified.** Elemental bonus is zero. Pure stat struggle. ATK vs DEC. No multiplier. |
| **Corruption vs. Corruption** | **Hard nullified.** All elemental bonuses are zero. Both parties lose elemental affinity entirely. It becomes a pure ATK/DEC/Sustain/Release stat comparison. |
| **Corruption vs. Non-Corruption** | Corrupted attacker gains **+15% ATK** against non-corrupted targets (fear/terror bonus) but **-10% DEC** (you're reckless, they're desperate). |
| **Non-Corruption vs. Corruption** | Non-corrupted attacker gains **+10% DEC** (defensive resolve) but **-15% ATK** (hesitation/fear penalty). |

### What This Means Tactically

**At low corruption (0–30%):**
- You fight corrupted enemies with elemental advantage. Standard JRPG tactics. Exploit weaknesses.
- Corruption is a *debuff* you want to cleanse.

**At mid corruption (40–60%):**
- You start losing elemental advantage against corrupted enemies. Your matching element is nullified.
- You're stronger in raw output but losing tactical flexibility.
- Corruption is becoming a *trade-off*.

**At high corruption (70–100%):**
- Against corrupted enemies, **all elemental advantages vanish.** It's a pure power struggle. Your ATK vs their DEC. Your Sustain vs their Release.
- You hit like a god. But so do they. And you're fragile.
- Against non-corrupted enemies, you're terrifying. They hesitate. You don't.
- Corruption is now a **deliberate combat stance.** You *choose* to be corrupted to fight corrupted enemies on even ground, sacrificing your elemental toolkit for raw power.

### The "Power Struggle" Visual

When corruption nullifies corruption in combat:

- Both your elemental aura and the enemy's elemental aura **shatter simultaneously** with a glass-break SFX.
- The screen flashes white for 0.3 seconds.
- Both combatants are stripped to their base stat visual—no elemental glow, no particle effects. Just raw, brutal physical/magical output.
- The UI damage numbers switch from elemental-colored to **pure white** with a heavy impact font.
- The music drops all melodic elements. Only percussion remains. A war drum. Two fighters. No magic. Just force.

### Boss Encounters at High Corruption

At 80%+ corruption, when you fight a **corrupted boss:**

- The boss's elemental phases are **disabled.** No fire phase, no water phase. Just raw, escalating attacks.
- The boss AI switches to "desperate survival" mode. It fights like it's fighting *you*, the raid boss.
- The boss's health bar appears at the **top of the screen** (standard), but YOUR health bar also appears at the top, next to it. Two boss bars. The game treats this as a **boss vs. boss fight.**
- The music is a dueling arrangement—your boss theme and the enemy's boss theme layered over each other, fighting for dominance. Whichever side is winning gets the louder stem.

---

## 🔗 TIE-IN WITH EXISTING SYSTEMS

| System | Corruption Interaction |
|:---|:---|
| **48-Minute Clock** | At 70%+ corruption, the Night phase music is YOUR boss theme even during exploration. Dawn/Dusk music is muted. The world is darker when you're in it. |
| **Creed Axis (Morality)** | High corruption + High Harmony = Tragic Martyr. Your boss music has a mournful, melodic undertone. NPCs flee but whisper prayers for you. High corruption + High Dominance = Tyrant. Your boss music is aggressive, triumphant. NPCs flee in terror. |
| **Affinity Bonds** | At 60%+ corruption, camp dialogue with bonded companions shifts. They stop joking. They start pleading. At 80%+, they speak to you like you're already gone. |
| **Purification** | Cleansing corruption at a Leyline Node plays a **reverse audio sweep**—your boss theme slowly strips layers until the standard exploration melody returns. The UI cracks heal. The gold border re-emerges. It feels like *waking up*. |
| **Anti-Piracy Karma** | If a pirate is playing AND has high corruption, the boss music plays at all times. Even in menus. Even on the title screen. They can never escape it. |
| **GlobalProfile** | Corruption level at game end is saved. Game 2 reads it. If you ended Game 1 at 90%+ corruption, Game 2's opening cinematic shows the world *reacting to your residual corruption.* NPCs in Game 2 whisper about "the Blighted Walker." |

---

## 🎮 DESIGNER IMPLEMENTATION NOTES

1. **Audio State Machine:** Create a `CorruptionAudioState` enum with five tiers (`Clean`, `Tainted`, `Fraying`, `Blighted`, `Consumed`). Each tier triggers a different stem arrangement via AnimationPlayer crossfade. Boss theme at `Blighted` and `Consumed`.

2. **UI Shader Parameters:** Tie `corruption_level` to a UI shader uniform. At 0.0: clean gold borders. At 1.0: organic dark tendrils, cracked frames, flickering text. The shader handles all visual degradation without swapping UI assets.

3. **Elemental Nullification Check:** In the damage calculation function, before applying elemental multipliers, check if both attacker and defender have `corruption_level >= 0.7`. If true, set elemental multiplier to `1.0` (nullified) and switch damage type to `PURE`. Trigger the "shatter" VFX/SFX.

4. **Boss-vs-Boss UI:** At 80%+ corruption fighting a corrupted boss, spawn a second health bar at the top of the screen for the player. Label it with the player's name + "Blighted" tag. Use the same boss health bar style. Two bars. Two threats.

5. **Music Dueling:** For boss-vs-boss encounters, export both boss themes as stems. Crossfade the dominant stem based on which side has higher HP percentage. If the player is winning, their theme swells. If the boss is winning, the boss theme swells. At 50/50, both play at equal volume. It sounds *chaotic* and *incredible*.

---

## ✅ WHY THIS LOCKS

- **UI as feedback:** No clean corruption meter. The interface *is* the corruption. Players feel it before they quantify it.
- **Audio identity shift:** You don't just *fight* bosses. At high corruption, you *are* the boss. The music, the SFX, the enemy reactions—all of it tells the player they've crossed a threshold.
- **Elemental nullification creates tactical depth:** Corruption isn't just "harder mode." It's a *different* mode. You lose elemental flexibility but gain raw power. Against corrupted enemies, it's the only way to fight on even ground.
- **Boss-vs-boss framing:** Two health bars. Dueling music. The game acknowledges that you're no longer the hero. You're the other monster in the room.
- **Purification as relief:** Cleansing corruption isn't just a stat reset. It's an *audio/visual exhale.* The boss theme fades. The gold border returns. The world breathes again. It feels like salvation.

Drop this into your design doc alongside the corruption scaling table, the Creed Axis thresholds, and the elemental hybridization matrix. You now have a complete corruption ecosystem that touches every layer of the game—UI, audio, combat, narrative, and world reactivity.

Whenever you want to draft the boss-vs-boss dual health bar layout, lock the corruption audio state machine transitions, or map the elemental nullification VFX sequence, I'm here. Keep tuning the dissonance. 💚🎵⚔️

### USER
I was thinking of using very dark black or purplish for corruption which should make it easier to theme the UI for example, so we go from RPG gold to evil purple

### ASSISTANT
Rin, this is the right call, and here's why it's *mechanically* correct, not just aesthetically nice: **purple sits directly opposite yellow/gold on the color wheel.** That's a true complementary inversion. The human eye doesn't read a gold→purple shift as "the UI got darker" — it reads it as *"the UI has been taken over by something else."* That's exactly the psychological beat you want for corruption.

And it's already lore-locked: purple is the Vorashi bioluminescence color. You're not picking a villain color — you're picking *the parasite's* color. The UI is being infected, not restyled.

Let me lock the palette so it doesn't drift across UI, sprites, VFX, and particles.

---

