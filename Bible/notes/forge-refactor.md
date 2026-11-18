# Creature Forge refactor: plan

Status: P1.1 — nothing moved yet; `engine/aur/forge.cpp` is intact at 1,240 lines.
Brief: `Aur refactor.md` (owner-supplied, kept at the root; do not delete).

## Goal

Split the 1,240-line `engine/aur/forge.cpp` monolith into focused units behind the
existing public `engine/aur/forge.hpp`; replace the 32×48-coordinate renderer that
silently scales by 3/2 and 4/3 with a native 48×64 layered renderer; give the Forge
mode tabs, the missing character metadata (variant, affinity, alignment, record
role), appearance overrides and colour swatches, and a 4-way turnaround preview.

## Contracts (binding)

- Every declaration now in `forge.hpp` keeps its signature and behaviour;
  `mcp.cpp` (`random_character_spec`, `compile_character`) and
  `tests/test_forge.cpp` compile unchanged.
- New in `forge.hpp`: `RaceGroup`, `RaceInfo`, `character_race_catalog()`,
  `alignment_options()`, `affinity_options()`, `entity_role_options()`,
  `hair_style_options()`, `accessory_options()`.
- `tests/test_forge.cpp` invariants: 48×64; 24 frames; 64 rows of 48 chars;
  `down_0` carries palette indices `1 2 3 4 6 7 8 9 A B C D F`; elf tips at
  `[13][7]` and `[13][40]` are `'1'`; identical bottom baseline across
  child/teen/adult for `down`, `down_right`, `up`, `right`; strictly descending
  top pixel child > teen > adult; visible eye `C` in `down_0` and `down_right_0`;
  feminine and masculine frames differ; every call is deterministic.
- Project JSON: `characters[id]` keeps `name`, `age_group`, `race`, `graphic`,
  `spec` and gains `variant`, `affinity`, `alignment`, `entity_role`.
- Zero `setStyleSheet` in `studio/` (Hydra theme stack; conformance check).
- Soft 750 lines per file; helpers live in `studio/forge/` and `engine/aur/forge/`.

## Current shape (verified)

- `engine/aur/forge.cpp` — 1,240 lines, 59,309 bytes. `rect`/`pixel`/`ellipse`
  multiply coordinates by 3/2 horizontally and 4/3 vertically whenever the grid is
  48×64 (`forge.cpp:56-60`, `:76-80`, `:98-102`); `character_frame` is authored in
  32×48 (`forge.cpp:142-406`).
- Hardcoded marks on every character: a dark ellipse across the right eye
  (`forge.cpp:221`), gold trim plus a metal pixel on the forehead
  (`:259-260`), an unexplained gold block on the torso (`:333`).
- Only `sylvani`/`solani` (ears), `beastkin` (ears/tail), `featherfolk` (wings)
  and `gharuk` (3 shoulder pixels) branch at render time. `variant`, `age_group`
  beyond child/teen, `hair_style` (except curly), `role` and `affinity` have no
  visual effect.
- `studio/CreatureForgePanel.cpp` — 367 lines, one column: identity, race, gender,
  presets, then creature/effect/clothing/prop/tile rows. No variant, affinity,
  alignment or record role. `createNpc` persists name/age/race/graphic/spec only.
- `studio/forge/CharacterSheetWidget.cpp` — draws `down_0` only; one frame, no
  direction control.
- `engine/aur/forge.hpp` — 36 lines, no catalog/group API.
- Baseline verified: ctest 7/7 (75 checks), conformance 9/9, 24 render captures,
  2026-11-16 (`PROJECT.md ## Verified`).

## Hazards found (corrections to the brief)

1. The brief's `refreshColorSwatches()` uses `setStyleSheet`; that is banned. Use
   `btn->setIcon(QIcon(pixmap))` with a filled `QPixmap` (the brief corrects itself
   later; only the corrected form is acceptable).
2. `directionCombo->addItem(dir, dir)` with `const char*` is an ambiguous QVariant
   conversion; build `QString` values explicitly.
3. The brief's ground shadow paints `P_METAL` (bright grey) under every character.
   Use `P_OUTLINE` + `P_BOT_SHADE`.
4. Elf tips at `(7,13)`/`(40,13)` only survive because ears are drawn **before**
   the head and the tips sit outside the head radius; keep that order.
5. Bottom baseline 61 depends on the ground shadow being drawn in every frame,
   child included; do not make it conditional on legs.
6. Top-pixel ordering (child > teen > adult) comes from `head_shift` 7/3/0 →
   11 > 7 > 4; nothing may be painted above `crown_y - 3`.
7. `random_character_spec` writes both `role` (adventurer/mage/guard/traveller/
   villager) and `entity_role` (main_character/npc/follower). The renderer reads
   `role` for trim; the panel persists `entity_role`. Do not conflate them.
8. Affinity-forcing races today: spirits, treesprites_dryads, fairies_nymphs,
   sirens_mermaids, elementals, veylkai, undead. Vampires and were_creatures are
   not. The catalog's `uses_affinity` must match or `dump_json` determinism breaks.
9. Feminine and masculine frames differ only via `torso_w` 16 vs 19; keep the
   spread so the test's inequality holds.
10. The dirty tree (`AIO-Win.bat`, `CMakeLists.txt` pre-link kill helper, `cmake/`,
    four docs) belongs to another session. Do not revert it; add the forge sources
    to the same `CMakeLists.txt`.
11. `Aur refactor.md` is the brief, not a project document. §39 bans stray MDs at
    the root, but deleting an owner-supplied file is owner-only — leave it and
    record it in `HANDOVER.md`.

## Target layout

```
engine/aur/forge.hpp                 public API (unchanged + catalog)
engine/aur/forge/SpeciesCatalog.cpp  palette, catalog, options, validation, random spec
engine/aur/forge/CharacterRenderer.cpp  native 48x64 Canvas/Rig pipeline + compile_character
engine/aur/forge/AssetGenerators.cpp creature/effect/clothing/prop/tile generators
studio/forge/ForgePanelTabs.cpp      the four tab builders
studio/forge/ForgeColorSwatches.cpp  swatch buttons, overrides, setIcon refresh
studio/CreatureForgePanel.*          header, wiring, create*/persist (<= 400 lines)
studio/forge/CharacterSheetWidget.*  4-way turnaround + single direction
```

## Phases

### P0 Safety net
- [x] Baseline recorded: ctest 7/7 (75 checks) on 2026-11-16; `test_forge.cpp`
      already pins the engine contract and stays green through every phase.
Acceptance: no code yet.

### P1 Pure split (no behaviour change)
- [ ] P1.1 `AssetGenerators.cpp`: move `generate_creature_graphic`,
      `generate_effect_graphic`, `generate_clothing_graphic`,
      `generate_prop_graphic`, `generate_tile_asset`,
      `generate_starter_world_terrain_tiles` and the helpers they need verbatim
      (drop the unreachable 48×64 scaling branches, which they never hit).
- [ ] P1.2 `SpeciesCatalog.cpp`: move `build_palette`, `character_race_options`,
      `character_variant_options`, validation and `random_character_spec` verbatim;
      add `RaceGroup`/`RaceInfo`/`character_race_catalog` and the option lists with
      the same ids and labels `forge.cpp` already returns.
- [ ] P1.3 `CharacterRenderer.cpp`: move `character_frame` + `compile_character`
      verbatim, delete `engine/aur/forge.cpp`, update `CMakeLists.txt`.
Acceptance: `aur_tests` builds; `test_forge` green; `dump_json` of every generated
asset byte-identical to the pre-split output.

### P2 Native 48×64 renderer (behaviour change)
- [ ] P2.1 Rewrite `CharacterRenderer.cpp` on the `Canvas`/`Rig` pipeline: native
      48×64 coordinates, no 3/2 or 4/3 scaling, no junk pixels; layers for body,
      species anatomy (leaf ears, beast ears/muzzle/tail, wings, tusks, horns,
      decay), hair style, clothing/role trim, accessories.
- [ ] P2.2 Extend `tests/test_forge.cpp`: variant changes output, `role` changes
      trim, `hair_style` changes output, accessory changes output, every direction
      non-empty, `left_0` is the mirror of `right_0`, catalog groups sum to 15.
- [ ] P2.3 Render the Forge page and open the PNG.
Acceptance: all old invariants plus the new ones pass; the Forge PNG shows no
smudge over the eye and no forehead/torso blocks.

### P3 Turnaround preview
- [ ] `CharacterSheetWidget`: 4-way (down/right/up/left) grid with labels, single
      frame mode, scale fitted to the pane; toolbar checkbox + direction combo in
      the panel.
Acceptance: toggling renders four labelled views; single mode renders the chosen
direction; harness PNG matches.

### P4 Tabbed panel and persistence
- [ ] Sidebar `QTabWidget`: Character / Creatures / Gear & FX / Tiles.
- [ ] Character tab groups: Identity & Metadata (name, record role, age, gender,
      presentation, alignment), People & Origin (grouped race combo with disabled
      Mortal/Supernatural headers, variant, affinity), Appearance (hair, bottom,
      accessory, colour swatches), presets, actions.
- [ ] `createNpc` persists `variant`, `affinity`, `alignment`, `entity_role`.
- [ ] Colour overrides reset with the seed; swatch refresh uses `setIcon` only.
Acceptance: all four tabs render, every control changes the preview, a created
character record carries the four new keys, `rg setStyleSheet studio` is 0.

### P5 Verification and docs
- [ ] Build `aur_studio` + `aur_tests`; ctest; render sweep with the PNGs opened;
      conformance gate.
- [ ] Update `PROJECT.md` (verified block, decisions), `HANDOVER.md`,
      `IMPLEMENTATION.md`/`WANTED.md` (plan listed, finished rows closed).
- [ ] Delete closed phases from this plan; delete the plan once nothing is open.

## Acceptance

ctest green; conformance clean; Forge PNG shows clean sprites and a 4-way
turnaround; `characters[id]` carries variant/affinity/alignment/entity_role;
no file in `engine/aur/forge/` or `studio/forge/` over 750 lines;
zero `setStyleSheet` under `studio/`.

## Rollback

One commit per step named `[forge-refactor-pN.S]`; revert to the last green commit
and fix the plan before retrying. P1 is a pure move, so a revert restores the
monolith exactly.

## Decisions and deviations

- P2 is a behaviour change and is kept in its own commit, per the plans rule.
- The brief's tab movement of Tiles into `TilesetEditorPanel` is deferred: the
  tile generator stays a Forge tab for now (the tileset editor owns authored
  tilesets, the Forge owns procedural tiles); revisit if the owner asks.
- Broader expansion tracks in the brief (W-01, W-02, W-04, WorldSim, runtime
  systems) are out of scope here and stay in `WANTED.md`.
