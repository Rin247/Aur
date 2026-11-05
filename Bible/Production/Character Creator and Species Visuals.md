# Character Creator and Species Visuals

> User-confirmed visual and creator requirements. These later clarifications take precedence over any conflicting assistant-generated examples in the source archive.

## Coverage

The Character Forge must cover the whole cast and asset workflow: main characters, NPCs, followers/companions, creatures, pets, and other listed beings. It must support every people and supernatural/anomalous category in the World Bible and project race database, not just the current Sylvani protagonist. At minimum this includes the six mortal peoples—Sylvani, Solani, Gharuk, Kaeli (humans), Featherfolk, and Beastkin—plus the listed spirits, elementals, undead, dragons (Veyl'kai), vampires, and were-creatures. Keep supernatural and anomalous origins/categories distinct from the mortal peoples; do not imply that every entry is a normal playable race.

Generated characters remain weapon-free. Weapons are separate props/assets.

Character records need a role such as main character, NPC, or follower/companion; creature and pet records remain creature/enemy assets. Support the established alignment model (Law/Neutral/Chaos × Good/Evil) as authored metadata, independently of the separate morality/creed and corruption systems. Do not infer moral alignment from race, species, or appearance.

## User-confirmed color and phenotype direction

- **Sylvani:** Nordic elf inspiration. Skin varies mostly from light pink to borderline tan; retain their distinctive leaf-like ears and established hair/eye variation.
- **Solani:** Mediterranean/African elf inspiration. Skin varies from borderline tan through dark ebony; keep them visually distinct from Sylvani while retaining shared elven ancestry and leaf-like ears.
- **Gharuk:** Skin varies from moss/olive green through copperish tones to dark ebony. Do not assign the Solani skin range or identity to Gharuk.
- **Kaeli (humans):** Use the real-world range of human skin colors, not a narrow default palette.
- **Beastkin:** Use real-world mammal coat colors and markings, with selectable or reproducible variants such as wolves, cats, dogs, tigers, and lions. Support further mammal variants without reducing every Beastkin to a wolf.
- **Featherfolk:** Use the real-world range of bird plumage colors and patterns, with selectable or reproducible variants, rather than a single generic feather palette.
- **Spirits and elementals:** Key their dominant palette to elemental affinity.
- **Undead:** Generally use decay, bone, and spectral palettes rather than having their overall appearance dictated by elemental affinity. Affinity may remain a secondary accent where appropriate.

These are population-level design ranges, not mandatory appearance templates for every individual. Seeded generation should provide variation within each people/category and keep skin, fur, feather, hair, and supernatural coloration conceptually distinct.

## Creator behavior

- Make race/category selection explicit and persist it in both the character record and generated graphic specification.
- Provide species-appropriate appearance variants where useful; changing the seed should reproduce the same choice and appearance.
- For spirits, elementals, and undead, expose or deterministically select an elemental affinity, while keeping undead coloration primarily decay-based.
- Allow creators to classify main characters, NPCs, and followers/companions without changing their race; creature/pet generation remains available in the same Character Forge workflow.
- Store alignment separately from creed/morality and corruption. Alignment does not alter the character's race-derived appearance.

## Sprite art and animation modules

Keep the workflow modular: the **Character Forge** creates and edits sprite art and entity metadata; the **Animation Studio** sequences sprite frames into directional/pose clips, sets playback timing and loop behavior, and previews them. Creating a sprite must not require authoring animations, and animation editing must not be hidden inside sprite generation. The pixel/frame editor remains the place for drawing and adjusting individual frames.
- Keep project race data editable; generation must reject unsupported identifiers explicitly rather than silently generating a human default.
