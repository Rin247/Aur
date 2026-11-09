# Memory Dives

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 2217-2297). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 🎞️ THE SEDIMENT DIVE ("Splicing")

**Doctrine:** Ley veins are the planet's memory bus. Strong emotion, violence, joy, or precursor tech leaves **residue** — mana sediment layered like silt. The Chronos Keystone's second function (besides dialing eras at gates) is **dialing locally**: stepping into a recorded moment and walking it as a ghost.

| Rule | Design |
|---|---|
| **You are a ghost** | Observer-only by default. Echoes can't see or hear you. No physics, no combat triggers, clock pauses. |
| **One Resonance Touch per dive** | Spend it to: lean into a conversation, follow one person, or hold one object. This is your clue budget. |
| **Interference = paradox** | Actually *changing* anything (moving an object, alerting an echo) is a causal touch: **+0.20 paradox**, immediate undertow eject. The temptation is the puzzle. |
| **Depth = layer** | Aetheric dive = shallow sediment (hours to years back). Shift into the **Umbral layer first** = deep sediment (decades to millennia). Your ALttP-style shift becomes the *depth dial* for time. |
| **Phase resonance** | Diving a Dawn-recorded reel during Dawn = clean playback. Mismatched phase = wow-and-flutter distortion rises. Resting to align the clock becomes a tactical choice. |

---

## ⏯️ PLAYBACK CONTROLS (The Tape Machine)

A dive is a **Reel**: a fixed recorded span (minutes to an in-game day), played on the same physical grid via your Base+Modifier folders (`Props_Reel_<id>`).

- **Scrub** — fast-forward/rewind the recording. Clues live at different timestamps: the key is hidden at minute 3, the name is spoken at minute 7, the murderer's face catches light at minute 12. *Scrubbing is exploration.*
- **Pause & walk** — freeze the echo-tableau mid-moment and inspect positions, objects, shadows (feeds your statue-shadow and light-alignment puzzles).
- **Loop point** — set an A/B loop to re-watch a fast moment frame-by-frame.
- **Wow & Flutter meter (0–100%)** — degradation from re-diving the same reel (past 3 dives: +5% and +0.05 paradox each), phase mismatch, and scrub-spam. At 100%: **tape dropout** — the reel collapses to static and locks until a leyline purification rite "cleans the tape." Purification doubles as restoration. Chef's kiss synergy.
- **Black Reels** — traumatic memories (fear, rage, grief) are **Vorashi-infested**, because the parasites feed on exactly that. Diving one means the still-moment is *crawling*: memory-combat encounters against residue-Vorashi. High risk, deepest clues.

---

## 🔍 CLUE TAXONOMY (What Dives Yield)

| Clue type | Present-world payoff |
|---|---|
| **Stash/burial witness** | See where something was hidden 300 years ago → dig there now (your Rusty Gear logic, generalized) |
| **Heard chord/password** | A lullaby or synth-chord played in memory → play it on a Precursor lock today |
| **Culprit/motive** | Quest resolutions branch; silent-fail quests get recoverable truth |
| **Lore/translation** | Precursor dialogue inside reels decodes progressively with `translation_level` |
| **Map sediment** | Auto-drops a pin on the 8K map showing what stood at those coordinates in that era |
| **Kin resonance** | Bring a companion whose ancestor appears in the reel → bonus clue + campfire affinity scene |

---

## ️ UI / AUDIO LANGUAGE (Kept Distinct From Your Other States)

- **Visual:** warm sepia tape-grain, soft vignette, echo sprites desaturated with a faint scanline shimmer. *Deliberately distinct* from paradox (chromatic flicker/frame-skip) and corruption (violet veins) so players always know which sin they're looking at.
- **HUD:** a tape-machine strip — spinning reels, 24H timecode of the *recorded* moment, and the Wow & Flutter needle.
- **Audio:** memory layer = your stems through low-pass + subtle bit-depth reduction + reversed reverb tails. Scrubbing = tape-stop pitch bends. Black Reels add detuned whisper beds under the Kajiura pads. And yes — somewhere in Game 1 there is a reel whose only clue is a **cowbell being rung once, off-camera.** The clue was the cowbell all along.

---

## 🧩 CROSS-GAME VARIANTS

- **Game 1:** full clue engine, core to shard-hunting and the Aunt's breadcrumbs.
- **Game 2 (tactical):** dive a battlefield's *last recorded turn* → reveals ghost-tiles of collapsed cover, hidden fords, or where a bridge stood. Reconnaissance as history.
- **Game 3 (real-time):** micro-dives as combat telegraphs — scrub an enemy's recorded attack reel to read its pattern. Sands-of-Time synergy.

---

## 👤 NARRATIVE BREADCRUMBS (The Aunt Trap)

Because she's from Timeline-B, **her memories don't match the planet's recordings.** Dive a scene she claims to have lived and the tape shows something subtly different. And in three specific deep reels, a **second observer's silhouette** stands at the edge of the recording — someone watching a memory from outside it. Players will lose their minds. Her origin reveal writes itself without a single exposition dump.

---

## 🛠️ IMPLEMENTATION SHELL (Designer Path)

- Residue sites = `Area3D` + keystone prompt; each Reel = an `AnimationPlayer` timeline keyframing echo sprite movements (you direct memories like boss cinematics — same tool, already in your pipeline).
- Echo props live in `Props_Reel_<id>` modifier folders; the dive just hides present props and shows the reel folder + swaps WorldEnvironment to the sepia grade.
- Clues write Dialogic flags + `GlobalProfile` pins; interference calls `ParadoxManager.register_event("causal_touch")`; dropout locks call the same purification hooks your cleanse rites already use.

Memory-diving now touches every pillar you've built: keystone, paradox, purification, clock phases, translation, affinity, layers, and the tape-pun audio lineage. Dream-diving would've been a minigame; this is an *archaeology engine*.

Want me to draft the first three Reel puzzle chains (shallow, deep-Umbral, and a Black Reel), spec the tape-machine HUD shader treatment, or map the interference/paradox flags into the GlobalProfile schema next, love? 🎞️🕰️

### USER
also think prince of persia trilogy style of solving puzzles as well with the era warping. so we might plant a seed 100 years ago that then grows into a large tree, stuff like that

### ASSISTANT
Rin, yes — this is the missing axis. PoP gives you the *tactile* puzzle language (blocks, plates, counterweights, traversal gates), and era-warping gives you the *causal* language (actions rippling forward centuries). Fuse them and you get something Chrono Trigger only flirted with: puzzles where **time itself is the mechanism**, not just the setting.

And because I know you, I'm naming the doctrine with the synth pun it deserves:

---

