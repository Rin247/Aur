# Social Systems and Endings

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 177-219). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 7. SOCIAL SIM & AFFINITY

**Core Loop:**
1. **Camp/Tavern/Inn** → Interact with party members
2. **Gift Giving** → Present food/items → Likes/Dislikes trigger affinity changes
3. **Dialogue** → Branching conversations via Dialogic 2. Choices affect affinity, lore access, and romance flags.
4. **Combat Synergy** → High affinity unlocks Dual Techs, formation bonuses, and instrument combo amplifiers.
5. **Mentorship** → Cross-gender training bonds. Formalized via rituals. Scales with shared combat time.

**Affinity Tiers:**

| Tier | Threshold | Unlocks |
|------|-----------|---------|
| Stranger | 0–19 | Basic dialogue. Standard combat AI. |
| Acquaintance | 20–39 | Camp banter. Minor stat synergy (+1%). |
| Companion | 40–59 | Support conversations. Dual Tech access. |
| Confidant | 60–79 | Deep lore. Romance option opens. +3% synergy. Instrument combos. |
| Bonded | 80–99 | Full romance/platonic bond. Legacy traits. +5% synergy. Unique ending flags. |
| Transcendent | 100 | Secret post-credits content. Cross-game continuity bonuses. |

**Romance Philosophy:**
- Romance is **optional** and **never mandatory** for progression.
- Any gender pairing is valid. No BL/GL gate. Affinity tracks `Combat Sync`, `Camp Trust`, and `Romance Interest` as separate axes.
- A fully bonded platonic friendship provides identical mechanical bonuses to a romance.
- The player can pursue multiple bonds but only one "Transcendent" bond per playthrough.

---

## 8. THE FOUR ENDINGS — STRUCTURE

| Ending | Trigger | Tone | Reward |
|--------|---------|------|--------|
| **Bad (The Pun)** | Use a specific skill exactly 100 times, then confront final boss. | Absurdist comedy. Party transforms into kitchen countertops. | Steam Achievement: "Formica Knight." Trading Card. |
| **Good (Standard)** | Beat final boss without True/Secret criteria met. | Bittersweet. Standard JRPG wrap-up. | GlobalProfile flag. Standard credits. |
| **True (Time Manipulator)** | Collect all 5 Chrono Shards (phase-locked, era-specific). Defeat the *real* final boss (Phase 2). | Epic. Reveals the 48-minute clock's true creator. Sawano choir kicks in. | Unlocks post-credits sequence check. |
| **Secret/Real (Cross-Game Hook)** | True Ending + obscure criteria (bury the Rusty Gear in Antiquity, spare the Midnight NPC, rest at a specific camp during Night phase). | Mind-bending. Post-credits glitch. Sets up Game 2. | GlobalProfile flag carries to Game 2. Alters opening cinematic. Unlocks secret character. |

**Sequel Hook Mechanic:**
- The Secret Ending writes to `GlobalProfile.json` (saved to Documents folder, shared across all 3 games).
- Game 2 reads this on boot. If `Secret_Ending_Seen == true`, the opening cinematic changes, a secret character joins the roster, and specific world-state flags alter terrain/NPC placement.

---

