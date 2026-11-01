# Cosmology and Eras

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 309-348). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

# 📗 BIBLE II: WORLD DESIGN DOCUMENT
### *"What It Is, Who Lives There, and Why It Hurts"*

---

## 1. COSMOLOGY & PLANETARY HISTORY

**The Planet: Eldoria**
- Sylvani name: *Aethel-Vael* ("The Breathing Root")
- Solani name: *Terra-Nova* ("The New Shore")
- Human name: *Eldoria* (adopted post-Seed Ship era)

**Origin:**
Eldoria is a **testing facility** built by a Precursor race that went extinct for unknown reasons. The planet's ecosystem, leyline network, and native inhabitants were left to evolve autonomously. The Precursors' technology now reads as "lost mysticism" to modern races.

**The Seed Ship Event (Era 1):**
Humans crash-landed via a **Seed Ship** (not a colony vessel—a genetic ark). Stranded with no way home, they adapted over thousands of years, discovered the planet's ambient mana (part of the ecosystem), warred with native races, and eventually **forgot their own heritage and technology**. Their tech became legend, then myth, then "magic."

**The Ley Veins:**
Subterranean mana rivers connecting all continents. They pulse with planetary rhythm. The 48-minute clock is a *surface expression* of leyline tidal cycles. Spirits, elementals, and dragons are sustained by leyline flow. When the leylines die, so does magic.

---

## 2. THE FIVE ERAS

| Era | Name | Aesthetic | Key Conflict | Timegate Access |
|-----|------|-----------|--------------|-----------------|
| **Era 1** | The Primordial Dawn | Sci-fi ruins overgrown with organic life. Seed ship wreckage. Proto-magic discovery. | Humans vs. native races. Survival. Adaptation. | Game 1 only (core mechanic) |
| **Era 2** | The Dark Age | War-torn. Fortified cities. Ash skies. Vorashi first surge. | All races vs. Vorashi. Sylvani Aetheric Implosion. | Game 1 + Game 2 side-quests |
| **Era 3** | The Middle Stability | Classic JRPG fantasy. Matriarchal courts. Trade routes. Magic normalized. | Political intrigue. Solani expansion. Crossbreeding normalization. | Game 1 + Game 2 + Game 3 |
| **Era 4** | The Industrial Weave | Steampunk + magic. Clockwork mana-engines. Early firearms with runic infusions. | Technology vs. tradition. Cultivation clans clash with western artillery. | Game 1 + Game 3 side-quests |
| **Era 5** | The Severed Age | Nano/cyberpunk/biopunk. Mana-dead. Neon arcologies. Elves as second-class. | Corporate exploitation. Aether extraction. Identity. | Game 1 (post-game) + Game 3 |

**Era Transition Rules:**
- The world physically changes between eras. Same grid, different props/lighting/NPC populations.
- Actions in one era can alter terrain in another (e.g., burying the Rusty Gear in Era 1 → it becomes a factory power core in Era 4).
- The `WorldStateManager` handles era swaps by toggling Node3D folders and swapping WorldEnvironment.

---

