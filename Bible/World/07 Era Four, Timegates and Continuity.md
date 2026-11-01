# Era Four, Timegates and Continuity

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 692-769). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 12. ERA 4: THE SEVERED AGE (DETAILED)

**The Severing:**
To kill Ouroboros-Prime, the planetary coalition detonated the Ley Veins. Ambient mana dropped to near-zero. Spirits, elementals, and natural magic ceased. Eldoria became functionally Earth-like.

**The Elven Plight:**
- Elves retain internal Aetheric Loci. They passively seep low-grade mana through sweat, breath, and blood.
- This became the resource: **Aether**. Extracted via syphon-cuffs. Distilled into inhalants/injectables.
- Grades: Raw (street stim) → Refined (medical) → Pure (cybernetic capacitor feed).
- Elves are second-class citizens. Hunted in low-security districts. Age markers (hair greying, ear elongation, skin pallor) make identification trivial.
- Modded/wealthy elves graft synthetics to blend in.
- Elf-human hybrids lack passive mana bleed but retain longevity. Mid-tier discrimination.

**Aesthetic:**
- Carbon-weave architecture. Neon leyline scars. Aether-vending kiosks. Silent drones.
- No ambient magic. Capacitor hums. Hydraulic whines. Bioluminescent flora (genetically modified).
- Cultivation still works via internal meridian refinement but progresses slower. Elite cultivators = living supercomputers.

---

## 13. THE TIMEGATES (STARGATE HOMAGE)

**Visual Design:**
- Futuristic but ancient. Precursor engineering.
- 3D Torus (ring) with scrolling glyph shaders. Event horizon: flat plane with water/ripple shader.
- Activation: GPUParticles3D "kawoosh" burst. Heavy bass sound.
- Transition: World dissolve shader. Screen burns away into digital/magical ash.

**Mechanical Rules:**
- Game 1 only (core traversal). Games 2 & 3: dormant relics for lore/secrets.
- Requires the **Chronos Keystone** to activate.
- Ties user's heartbeat to planetary mana flow (narrative justification for the 48-minute clock).
- Cannot be used during combat.
- Each gate is locked to a specific era. The Keystone "dials" the destination.

---

## 14. CROSS-GAME CONTINUITY RULES

| Mechanism | Function |
|-----------|----------|
| `GlobalProfile.json` | Saved to Documents folder. Shared across all 3 games. Tracks endings, flags, choices, pins. |
| Steam Cloud | Backs up `user://` folder. Ensures cross-device continuity. |
| `WorldStateManager` | Reads GlobalProfile on boot. Adjusts terrain, NPC placement, era-specific props. |
| `ModManager` | Reads `user://mods/` on boot. Validates JSON. Merges overrides. |
| `SteamManager` | Achievements, leaderboards, workshop. Bridges Dialogic → Steam API. |

---

## 15. DESIGNER'S GLOSSARY (QUICK REFERENCE)

| Term | Meaning |
|------|---------|
| Aetheric Locus | Biological mana storage organ (Western races) |
| Meridian | Chi circulation pathway (Eastern races) |
| Hygro-Rhizome | Dermal capillary cluster for moisture/nutrient absorption |
| Chorion | Flexible vascular membrane wrapping ovoviviparous embryos |
| Nidal Chamber | Internal egg-retention cavity |
| Ley Vein | Subterranean mana river |
| Vorashi | Parasitic aquatic entities (Dark Water) |
| Aetheric Implosion | The Sylvani "mana bomb" that scarred the continent |
| Chronos Keystone | Timegate activation McGuffin |
| Vascular Tide | Elven/orc reproductive cycle (not external bleeding) |
| Aether | Era 4 resource. Distilled elven internal mana. |
| Brine-Master | Solani salt-filtration specialist |
| Dune-Weaver | Rare long-lived Gharuk elder |
| Vaelen-Kai | Sylvani/Solani hybrid identity |
| The Chorus | Dragon telepathic/harmonic language |
| Strain α / Strain β | Vampire / Were-creature genetic classification |
| Resonance Sync | Affinity/bond mechanical multiplier |
| Forge-Tide | Gharuk courtship/battle ritual window |
| Tide-Pledge | Formalized mentorship bond (Elves) |
| Heat-Click | Formalized mentorship bond (Gharuk) |

---

---

