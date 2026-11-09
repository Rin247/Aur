# Combat Overview

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 148-176). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 6. COMBAT SYSTEMS — DETAILED

### Game 1: Turn-Based (Chrono Trigger Model)

- **Turn Order:** Determined by `Tempo` stat. Displayed as a timeline bar (CT-style).
- **Positioning:** Characters snap to `Marker3D` slots. Enemies occupy physical space. AoE attacks hit based on spatial overlap.
- **Dual/Triple Techs:** When affinity between 2+ party members exceeds threshold, combined attacks unlock. Triggered via adjacent positioning + simultaneous action selection.
- **Elemental Pairing:** Weapons carry elemental tags. Combining elements on the same target triggers hybrid effects (Fire + Wind = Plasma Burn, Water + Earth = Mudslide, etc.).
- **Timegate Combat:** Some bosses fight *across* eras. Phase 1 in Antiquity → Phase 2 shifts to Industrial. Arena changes mid-fight.

### Game 2: Tactical (Triangle Strategy Model)

- **Grid:** The overworld terrain *is* the grid. Height, facing, cover, and elemental terrain all matter.
- **Movement:** NavigationRegion3D pathfinding. Ghost tiles show valid range. Click to move.
- **Divine Pulse:** 3 charges per battle. Rewinds 1 full turn. UI shows a "page turning backward" animation.
- **Height Advantage:** Attacking from higher ground grants +15% damage. Backstab (hitting the rear Area3D) grants critical.
- **Formation Bonuses:** Specific party arrangements unlock passive buffs (e.g., diamond formation = +Decay, line formation = +Tempo).

### Game 3: Real-Time (Trials of Mana Model)

- **Movement:** Free within arena bounds (Area3D container). Camera tracks via Phantom Camera.
- **Attack System:** Light/heavy chains via AnimationPlayer hitbox activation. Weapon hitboxes are Area3Ds toggled on/off during animation frames.
- **Dodge:** I-frames via CollisionShape3D disable during dodge animation (0.3s window).
- **Charge Attacks:** Hold attack → charge animation → release for amplified damage. Getting hit interrupts.
- **Sands of Time Rewind:** Hold rewind → last 10 seconds of position/animation play backward. Sepia filter + VHS audio effect. Limited by a "Sand Gauge" that refills over time.
- **Tag System:** Swap active character instantly. AI controls inactive members with basic patrol/attack routines.

---

