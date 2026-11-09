# Classes and Jobs

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 110-147). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 5. CLASS/JOB SYSTEM

**Design Principles:**
- No "Novice" class. Every character starts with a **Base Job** that reflects their narrative role.
- Jobs **evolve** (Ragnarok Online tree) but can be **swapped freely** in and out of combat (FFV/X-2/Bravely Default).
- Every job has **2 weapon slots** (standard party) or **4 weapon slots** (Elowen only).
- Fluid combat swapping via AnimationTree blend states. No loading, no leaving the grid.

**Job Evolution Tree (Example Branch):**

```
Base: Warden (Tank/Protector)
  ├─ Tier 2: Shield Maiden (Defensive specialist)
  │    ├─ Tier 3: Bulwark (Pure tank, taunt/aggro)
  │    └─ Tier 3: Spellbreaker (Anti-magic tank)
  ├─ Tier 2: Vanguard (Offensive tank)
  │    ├─ Tier 3: Juggernaut (Heavy AoE)
  │    └─ Tier 3: Duelist (Single-target burst)
  └─ Tier 2: Tidekeeper (Support tank, hydration/thermal routing)
       ├─ Tier 3: Anchor (Party-wide defense buff)
       └─ Tier 3: Current-Weaver (Terrain manipulation)
```

**Weapon Slot Rules:**

| Character Type | Slots | Proficiency Cap | Rationale |
|----------------|-------|-----------------|-----------|
| Elowen (Protagonist) | 4 + Magic | 85% per weapon | Versatility over mastery. Reflects inexperience. Adapts to any boss. |
| Standard Party Member | 2 + Magic | 130–150% per weapon | Depth over breadth. Masters their tools. |
| Mentor/NPC Allies | 1–2 | 100–120% | Focused. Reflects their cultural specialization. |

**In-Combat Swap Mechanic:**
- Hold swap button → time slows to 20% → radial Weapon Wheel appears → select → release → 0.3s animation blend → stats recalculate → immediate action.
- For Elowen: 4-slice wheel. For party: 2-slice toggle.
- AnimationTree BlendSpace handles the visual transition seamlessly.

---

