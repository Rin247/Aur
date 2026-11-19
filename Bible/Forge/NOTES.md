# Forge notes

The character/creature forge is split by responsibility, one sentence each:

| file | responsibility |
|---|---|
| `engine/aur/forge.hpp` | the one public header: `compile_character`, the race catalog and option lists, and the six generators |
| `engine/aur/forge/SpeciesCatalog.cpp` | palette building, the 6 Mortal + 9 Supernatural race table, variant/alignment/affinity/role/hair/accessory options, validation and the seeded spec |
| `engine/aur/forge/CharacterRenderer.cpp` | the native 48×64 `Canvas`/`Rig` renderer and `compile_character`'s 8 directions × 3 steps |
| `engine/aur/forge/AssetGenerators.cpp` | creatures, effects, clothing, props, tiles and the starter terrain set |
| `studio/CreatureForgePanel.*` | the panel: header, preview bar, wiring, `create*`, persistence |
| `studio/forge/ForgePanelTabs.cpp` | the four tab builders (Character, Creatures, Gear & FX, Tiles) |
| `studio/forge/ForgeColorSwatches.cpp` | palette swatch buttons and the `setIcon` refresh |
| `studio/forge/CharacterSheetWidget.*` | four-way turnaround and single-direction preview |
| `studio/forge/ForgeAssetOps.*` | ids, unique ids, role presets |

**Why the split.** `engine/aur/forge.cpp` reached 1,240 lines against a 750 soft limit
and 1,250 hard limit, and its renderer was authored in 32×48 coordinates that a helper
multiplied by 3/2 horizontally and 4/3 vertically. The brief is
`Bible/notes/forge-brief.md`.

**Renderer contract is in `tests/test_forge.cpp`**: 48×64, 24 frames of 64 rows × 48
chars, palette indices `1 2 3 4 6 7 8 9 A B C D F` on `down_0`, elf tips at `[13][7]`
and `[13][40]`, one baseline across child/teen/adult, top pixel strictly descending
child > teen > adult, an eye on `down_0` and `down_right_0`, feminine ≠ masculine,
`left_0` the mirror of `right_0`, and variant/role/hair/accessory each changing the
output.

**Enemy sprites.** `generate_enemy_graphic` (`forge/AssetGenerators.cpp`) draws the
humanoid enemies at 24×24 — the same size the creature generator uses — from a
three-row look table (bandit, bandit_archer, bandit_boss) with a 16-colour palette
per row, four idle frames and the same `idle`/`attack` anims as a creature. The
MCP's `enemy_generate` writes the sprite and sets the record's `gfx`.

**The Bestiary had never shown artwork.** `CreaturePortrait` was never given the
project in `BestiaryEditor`'s constructor, so `artwork()` saw a null project and
every creature said "The project has no artwork". The constructor now calls
`portrait->setProject(project)`; a fresh open shows the sprite.

**Two traps found the hard way.** The beast variant has to change the pixel art, not
only the palette — the palette is separate from the frames, so a variant that only
recolours is invisible to a frames comparison. And a `QGroupBox`/tab title containing a
bare `&` is read as a mnemonic and renders as an underscore; write `&&`.
