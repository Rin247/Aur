# Aur implementation plan

Open items gathered from `WANTED.md` (Open: world-map tile expansion, map seed generator, Qt desktop
foundation), `PROJECT.md` (§"Open defects"), and `HANDOVER.md` (§"Still open (owner)").
`ROADMAP.md` holds the stages; this file holds the items inside them, grouped by area.

| document | holds |
|---|---|
| `ROADMAP.md` | the stages and the standard a stage meets |
| **this file** | every open item, by area, with its evidence and its gate |
| `WANTED.md` | the owner's queue, pointing here by area/ID |

**How an item is closed.** The gate named in the row is passed, the row is deleted from here,
and git holds the history. An item with no gate is an intention.

## Area W — World map

| ID | item | evidence | gate |
|---|---|---|---|
| **W-01** | **Expand a world map tile into a map.** Pick a tile on the world map, send it to the map editor, grow it into a map that keeps the direction (a beach on the west stays on the west). `pick_terrain` names the cell's terrain and the side its land is on, and the map generator keeps that side — the tool that picks the tile, and the beach the field's tileset still lacks, are the open parts. | `WANTED.md` Open ("Expand a world map tile into a map"); `PROJECT.md` §"What works"; plan `Bible/notes/globe-expand.md` | expansion works with direction preserved; build and suite green |
| **W-02** | **Map seed generator.** The estate seeds a character, a creature, an effect, clothing, a prop and a tile, but not a map — every field map was made by hand. | `WANTED.md` Open ("A map seed generator"); plan `Bible/notes/map-generator.md` | a generator produces a field map; build and suite green |
| **W-03** | **Three creatures have no artwork.** `bandit`, `bandit_archer` and `bandit_boss` name graphics the project does not have, so the Bestiary's portrait says so for them; the game's renderer draws a default graphic instead — the creature in the game is not the creature the record names. | `PROJECT.md` open defect 1 | artwork present, or the records name the missing graphic honestly |

## Area P — Platformer

| ID | item | evidence | gate |
|---|---|---|---|
| **P-01** | **Platformer engine floor tile.** P1–P3 landed (movement modes, physics, one-way platform, cameras); P4 waits on a solid floor tile — the owner picks the tile, then walkable field floors become the platformer's floor. | `HANDOVER.md` §"Still open (owner)"; `PROJECT.md` §"What works" (tile flags Solid/One-way); plan `Bible/notes/platformer.md` | P4 is owner-accepted |

## Area D — DAW

| ID | item | evidence | gate |
|---|---|---|---|
| **D-01** | **DAW P7a (GM/GS instrument) parked by owner decision** 2026-11-11, mid-wiring; resume point in `Bible/notes/daw/gm-gs.md`. P2 close awaits the owner's GUI soak; P5 remainder closed; P8 effects framework landed. | `WANTED.md` Active plans (`Bible/notes/daw/`); `HANDOVER.md` §"Active plans" | parked phases unparked by the owner; plan phases pass ctest |

## Area Q — Qt desktop foundation

| ID | item | evidence | gate |
|---|---|---|---|
| **Q-01** | **Qt desktop foundation P0 audit only.** Audit AIO/Vulkan behavior, legacy source/header layout, professional toolbars/settings/keybindings, canonical `Aur` identity (no Qt/desktop role suffixes); preserve the higher-priority active Aur queue. Implementation waits until the existing queue allows it. | `WANTED.md` Open ("Qt desktop foundation") and Active plans (`C:\Projects\Bible\notes\qt-desktop-foundation.md`); `HANDOVER.md` | audit findings recorded with `file:line` citations; no foundation code lands before the queue allows it |

## Order of work

1. **W-01** (tile→map expansion) and **W-02** (map seed generator) — the owner's queue, in that order.
2. **W-03** and **P-01** — the remaining `PROJECT.md` and platformer items, as the queue allows.
3. **D-01** and **Q-01** — owner-gated; nothing lands until the owner unparks or the queue allows.
