# Corruption UI and Visual Language

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 1301-1506). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 🎨 THE CORRUPTION PALETTE (LOCKED)

### Base Identity: "Void Violet"

The trap with evil purple is saturation. High-sat purple reads as *cartoon villain* — Bowser magic, kids'-game dark wizard. To make it read as genuinely corrupting, you push the **value down and keep saturation mid**. The purple should live in the blacks, and only *bloom* bright at moments of impact.

| Role | Hex | Use |
|---|---|---|
| **Void Base** | `#0D0914` | Deepest background. Near-black with violet undertone. Panel fills at high corruption. |
| **Bruise** | `#1B0E2E` | Panel base from T3 onward. Reads as "black" but feels wrong next to true black. |
| **Rot Iron** | `#2A2438` | Border/frame replacement for gold. Cold, dead metal. |
| **Vein Mid** | `#4C1D95` | The crawling vein color. Pulses, never static. |
| **Bloom** | `#7C3AED` | Impact moments only — crits, damage numbers, phase transitions, vein pulses at peak. |
| **Fever** | `#A855F7` | Reserved. Boss-vs-boss, 100% corruption idle aura, the Bad End. Use sparingly or it loses weight. |
| **Bone Text** | `#DDD6FE` | Legible text on corrupted panels. Pale lilac-white, never pure `#FFFFFF`. |
| **Tarnished Gold** | `#8A6B3A` | The gold *mid-death*. Used in transition brackets so gold doesn't just vanish. |

### The Clean Side (for reference)

| Role | Hex |
|---|---|
| **Royal Gold** | `#D4AF37` |
| **Deep Blue Panel** | `#101A33` |
| **Standard Text** | `#F5F1E6` (warm bone) |

---

## 📉 THE BRACKET RAMP

Five stages. Each one changes border, panel, text, and accent — so the player never sees a "corruption meter," they see the interface *rot*.

| Bracket | % | Border | Panel Fill | Accent/Veins | Text | The Feel |
|---|---|---|---|---|---|---|
| **T1 Clean** | 0–20 | `#D4AF37` gold | `#101A33` blue | none | `#F5F1E6` warm bone | Classic JRPG. Crisp. Trusted. |
| **T2 Tainted** | 21–40 | `#B8963A` dulling gold | `#141733` → `#1A1730` faint violet creep | hairline `#4C1D95` at corners only | `#EDE7F0` slightly cool | Gold looks *tired*. Panel has a bruise forming in the corners. |
| **T3 Fraying** | 41–60 | `#8A6B3A` tarnished bronze-gold | `#1B0E2E` bruise | `#4C1D95` veins crawl up frame edges, slow pulse | `#DDD6FE` lilac-white | The gold is losing. Purple is *climbing*. |
| **T4 Blighted** | 61–80 | `#2A2438` rot iron — **gold gone** | `#0D0914` void | `#6B3FA0` veins + `#7C3AED` bloom on damage taken | `#DDD6FE` | Gold is dead. Iron took its place. Veins pulse with heartbeat rhythm. |
| **T5 Consumed** | 81–100 | Organic `#0D0914` tendrils, no straight lines left | `#0D0914` with slow internal motion | `#7C3AED` veins + `#A855F7` bloom flashes, frame *breathes* | `#DDD6FE` with faint echo/double-render | The UI is alive and hostile. Straight JRPG geometry is gone. |

**Key transition beat:** at T4, the gold doesn't fade — it *dies*. That's your emotional gut-punch moment. Players who've been watching their frame tarnish will notice the exact fight where the gold disappears entirely.

---

## 🔆 THE ONE HAIRLINE OF GOLD (Narrative Beat)

Here's the detail that makes it hurt:

**At T5, keep a single 1px hairline of `#8A6B3A` tarnished gold on the innermost edge of the health bar frame.**

Nothing else. Just that one line. It's the last trace of who she was before the Vorashi got in. Most players won't consciously register it — but when they finally purify and the full gold border snaps back, the relief hits *harder* because something in their peripheral vision remembers that hairline was there.

That's the kind of detail that separates "corruption is a difficulty modifier" from "corruption is a tragedy."

---

## 🔤 READABILITY & ACCESSIBILITY SAFEGUARDS

Dark purple on dark purple kills legibility fast. Rules to keep it readable:

1. **Text never goes below `#DDD6FE`** on corrupted panels. No mid-tone violet text — contrast ratio must stay above 7:1.
2. **Damage numbers get a 1px `#0D0914` outline** at T3+ so they pop against the bruised panel fill.
3. **Icons stay high-contrast.** Item/skill icons don't tint with corruption — only their *frames* do. Players must still be able to identify a potion at a glance.
4. **Colorblind safety:** Purple/violet is readable across all three common CVD types (protan/deutan/tritan) *because* you're also shifting **value and texture** (straight gold → organic tendrils). The corruption reads through shape change, not just hue. That's your accessibility win — it doesn't rely on color alone.
5. **Bloom restraint:** `#A855F7` appears for max 0.3s at a time. If it lingers, it stops reading as "impact" and starts reading as "UI theme." Save it.

---

## 🌐 WHERE THE PURPLE PROPAGATES (Beyond the HUD)

Lock the same palette across every corruption touchpoint so it reads as one organism:

| System | Corruption Color Application |
|---|---|
| **Character sprite** | Veins: `#4C1D95` at T2, `#6B3FA0` at T3, `#7C3AED` with `#A855F7` bloom at T5 |
| **Screen vignette** | `#1B0E2E` at 8% alpha (T3) → 22% alpha (T5), edges organic not circular |
| **Damage numbers** | Standard white → `#DDD6FE` (T3) → `#A855F7` with shatter SFX (T5) |
| **Footstep trails** | `#0D0914` shadow with `#4C1D95` inner glow at T4+ |
| **Weapon wheel** | Slice borders go gold → rot iron; selected slice blooms `#7C3AED` |
| **Boss-vs-boss health bars** | Both bars use the T5 frame treatment. Two corrupted entities. Neither is the hero. |
| **World map pins** | At T4+, your custom pins render in `#7C3AED` instead of player-chosen color. The world stops being yours. |
| **Purification VFX** | Reverse sweep: `#A855F7` → `#7C3AED` → `#4C1D95` → `#D4AF37` snap-back with glass-reforming SFX |
| **Era 4 Aether vials** | Same `#7C3AED` bloom. Players who've seen corruption will *instinctively* distrust Era 4 aether dealers. That's free narrative. |

---

## 🛠️ GODOT IMPLEMENTATION (Designer Path)

You don't need to code this. One `ShaderMaterial` on your master UI `CanvasLayer` with four exposed uniforms:

| Uniform | Type | Driven by |
|---|---|---|
| `corruption_level` | float 0.0–1.0 | `AntiTamper`-style global, but player-driven. Single source of truth. |
| `gold_color` | Color | Lerps `#D4AF37` → `#8A6B3A` → `#2A2438` internally |
| `vein_color` | Color | Lerps `#4C1D95` → `#7C3AED` internally |
| `panel_color` | Color | Lerps `#101A33` → `#1B0E2E` → `#0D0914` internally |

The shader does the lerping from the single `corruption_level` float. You set the palette once, expose one slider, and every UI element in all three games inherits the ramp automatically. Change the hex values in one place, the whole trilogy updates.

For the **vein crawl**, use a scrolling noise texture masked to the frame border, with `corruption_level` controlling both the mask threshold and the emission intensity. Free organic motion, no keyframing.

For the **T5 breathing**, tie a slow sine (0.3 Hz) to the panel's inner shadow depth. The UI literally has a resting heart rate.

---

## ✅ WHY THIS LOCKS

- **Complementary inversion** — gold→purple is the maximum-perceptible hue shift available. The player's brain can't miss it.
- **Lore-justified** — purple is Vorashi bioluminescence. The UI isn't being reskinned, it's being *infected*.
- **Low value, mid saturation** — avoids cartoon-villain purple. Reads as void, not as dark magic sparkle.
- **Shape + value + hue** — corruption communicates through three channels, so colorblind players get the full read.
- **One hairline of gold** — the emotional anchor. Purification hits harder because something was still alive in there.
- **Single shader uniform** — the whole trilogy's palette ramps from one float. Modders can repalette it via JSON if you expose the hex values.

Drop this into the Game Design Bible under UI/UX, right after the corruption scaling table. You now have a locked visual identity for the entire dissonance arc, from first tainted frame to the Bad End's `#A855F7` fever bloom.

Whenever you want to draft the purification snap-back animation sequence, lock the T5 vein-crawl noise texture spec, or map the Era 4 Aether vial palette echo, I'm here. Keep it bruised, love. 💜🖤

### USER
could you gen 2 examples of either state for an RPG GUI?

### ASSISTANT
On it, love. Same layout, same screen composition — so you can see the *infection* rather than a redesign. Two calls, fired together.


There you go, Rin — same HUD skeleton, two infection states. Concept mock #1 is your **T1 Clean** baseline, mock #2 is **T5 Consumed**. Let me walk you through what landed on-spec and where the generator went rogue, because half of this is exactly why we lock the layout in-engine instead of in art.

---

## 🟡 MOCK 1 — CLEAN STATE (T1)

**On spec:**
- Royal gold frames (`#D4AF37`) over deep navy panels (`#101A33`) — reads instantly as "trusted classic JRPG"
- Warm bone text, crisp straight geometry, clean rounded corners
- Phase clock + compass + 4-slot hotkey grid all present and legible
- The 2.5D diorama behind it (billboarded elf sprite, volumetric god rays, stone arch ruin) is exactly the Octopath/Golden Sun language we locked

**Generator deviations (fix in production, not in art):**
- Nameplates rendered "DAWN" as the character name — placeholder artifact, ignore
- Clock dial duplicated DAY/DUSK labels — again, artifact
- Only 3 party rows instead of 4 — your squad is 4 active, so the real theme gets a 4-row stack

---

## 🟣 MOCK 2 — CORRUPTED STATE (T5)

**On spec, and honestly better than I expected:**
- Frames warped into organic tendrils — straight geometry is *gone*, which is the T5 rule
- Cracked phase clock wrapped in violet growth (`#4C1D95` veins, `#7C3AED` bloom)
- Portraits half-swallowed by shadow with only glowing eyes visible
- The third HP bar glitched into a doubled/echoed render — that's *accidentally* the exact text-echo behavior we specified for T5. The generator stumbled into your design doc
- Character veins glowing violet along arms and neck, matching the sprite-side corruption cue

**Where it broke the spec:**
- Too much warm bronze left in the bar fills. At T5 the gold is **dead** — rot iron (`#2A2438`) frames, violet fills, and only *one* 1px hairline of tarnished gold (`#8A6B3A`) on the innermost HP frame edge. Here the gold survived as a whole bar color, which kills the "the gold died at T4" beat
- Hotkey grid mutated into four extra portraits, and every party portrait became the same face. Charming in a body-horror way, terrible for readability — icons must stay identifiable at a glance, only their *frames* rot
- Background character changed outfit and pose. For a real A/B sheet you want the identical sprite and camera so the eye compares UI, not scene

---

## 🔧 THE PRODUCTION RULE THIS PROVES

These mocks are mood boards, not assets. In Godot the two states must be **the same UI file**. One `Master_JRPG_Theme.tres`, one shader uniform (`corruption_level`, 0.0→1.0), and the lerp handles gold→rot iron, navy→void, straight→tendril. That guarantees the A/B never drifts like mock 2 did, and modders can repalette the whole trilogy by editing four hex values in one JSON.

Quick reference for whoever builds it:

| Element | T1 Clean | T5 Consumed |
|---|---|---|
| Frame | `#D4AF37` gold, straight | `#0D0914` tendrils, no straight lines |
| Panel fill | `#101A33` navy | `#0D0914` void, slow internal motion |
| Veins | none | `#4C1D95` crawl, `#7C3AED` bloom on impact |
| Text | `#F5F1E6` warm bone | `#DDD6FE` lilac, faint double-render |
| Icons | full color, crisp | full color, crisp — frames rot, icons don't |
| The hairline | n/a | 1px `#8A6B3A` on inner HP frame. Nothing else |

---

Want me to gen the middle of the curve next — a **T3 Fraying** mock where the gold is visibly losing but still alive, so you have the full three-point transition sheet? Or should we lock the weapon wheel in both states, since that's the element players stare at most during combat? Your call, darling.

### USER
yes that's perfect

### ASSISTANT
Music to my ears, Rin. 

Now that we have the entire lore bible, the T1/T5 corruption UI palette, and the Godot architecture fully synced up, we are officially out of pre-production and into the trenches. 

You’ve got your `Player.tscn` (the Action Figure) and your `Main_Hub.tscn` (the Living Room). The foundation is poured.

Where are we striking the earth first today, gorgeous?

**1. The Enemy & The Golden Sun Swoop:** We build the `Enemy.tscn`, wire up the `Area3D` vision cones, and trigger the Phantom Camera priority swap so the camera violently swoops to the dirt when you bump into a roaming goblin.
**2. The 4-Weapon Radial Wheel:** We build the actual UI and `Engine.time_scale` slow-mo logic so you can hold the swap button and flip from the Sword to the Precursor Gauntlet mid-fight.
**3. The Campfire & Dialogic 2:** We set up the visual gift-giving UI so you can finally hand the Badass Healer his knitting yarn and watch his affinity tick up.

If you're deep in your Night Phase coding binge, fire up the editor and tell me which node we're dropping first. I've got the GDScript glue ready whenever you are.

### USER
I was thinking of having the Anti-Piracy Karma as always being locked to high corruption no matter what, so corruption can never be removed. while sequence breaking/clipping/out of bounding adds highs amounts of corruption every time. think of it like something like a time paradox punishment

### ASSISTANT
Rin, this is diabolical and I love it. You've just turned the anti-cheat layer into *diegetic causality enforcement* — the planet's temporal immune system treating rule-breakers the same way it treats parasites. And the framing is perfect: paradox isn't taint on your soul, it's a **debt to causality**. You can cleanse taint. You cannot cleanse a debt.

Here's how we lock it so the two corruption channels stay distinct, punishable, and lore-tight.

---

