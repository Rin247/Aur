# The globe's tile becomes a map: plan

Status: P0 done (the audit). Next: P1, the pick and the send.

## Goal

The world map knows where the land is and what it is; the map editor
makes the places you walk in. Right now they do not talk: a field map is
made by hand, and the world map's tile is only a picture.

The owner's words (2026-11-12): "we should be able to pick a tile on the
worldmap and send it to the map editor and then let us seed/gen it as
well, say we pick a half beach/half water tile and then basically
'expand it' in the map editor if that makes sense? so it needs to stay
consistent with the direction as well, i.e. a beach on the west side
should always generate a beach there and not otherwise, is that doable?"

**Yes, it is doable.** The direction is the hard part, and the tileset
already knows how to carry it: the autotile's edge variants.

## Contracts (binding)

- **The picked tile carries two things: the terrain and the side the land
  is on.** A coastal tile on the globe is the ocean's edge variant, and
  the land is on the side the variant faces.
- **The expansion keeps the direction.** A beach on the west side of the
  picked tile is a beach on the west side of the map, and the map's
  corner and edge tiles are the autotile's variants for that side. The
  consistency is the autotiler's: a map laid out with the water on the
  west draws the water's west variants, wherever it is laid out.
- **The generator is the engine's** (`engine/aur/mapforge.*`), beside the
  other generators, and the same seed gives the same map.
- **The map it makes is a project map.** The same shape the editor
  writes: the layers, the entities, the mode, the spawn.

## Current shape (verified)

- The world map's tileset (`world`, 74 tiles) has `sand`, `grass`,
  `forest`, `mountain`, `hills` and the ocean's tiles, and two autotiles:
  `ocean` (edge_joined) and `road`. The ocean's edge variants are the
  "half beach, half water" tiles: a coastal cell holds the ocean's edge
  variant, and the land is on the side the variant faces.
- The field map's tileset (`terra`, 110 tiles) has grass, the paths,
  tallgrass, the shadows and the water, with two autotiles: `path` and
  `water` (edge_joined). **There is no sand or beach tile in the field's
  tileset** -- `terra` and `cave` have none; only the world's has sand.
- The forge's tile generator (`engine/aur/forge.cpp`) makes a "sand"
  tile, and the map generator (`engine/aur/mapforge.*`, new) makes the
  map's own tiles with their flags.
- The map editor and the world tab are separate pages, and the map
  editor's map list is the project's maps, so a map made anywhere is
  listed everywhere.

## Hazards found

- **The field has no beach.** A coastal tile cannot be expanded into a
  map with a beach in it until the field's tileset has one, and the
  beach's edge variants (sixteen tiles, like the water's) with it. The
  forge's generator can make the sand; the autotile's variants are the
  work.
- **"Half beach, half water" is an autotile variant, not a tile kind.**
  The picked tile says "ocean, edge to the west"; the land's side comes
  from the variant, not from the terrain name.
- **The generator's flags are per tile.** The expansion's own tiles (a
  solid floor for a platformer, the beach) have to be added to the
  tileset, or the field's grass becomes solid in every map.
- **A map's edges are the autotiler's job.** The expansion has to lay
  the edges out so the autotiler picks the right variants; a map with
  sand on the west and water on the east is two different edge runs.

## Target layout

```
engine/aur/mapforge.{hpp,cpp}     the generator: the kinds, the terrain, the sides
studio/world/WorldMapCanvas.*      the pick: the tile, its terrain and its side
studio/world/WorldPanel.*          "expand this tile" and the map editor's hand-off
studio/mapeditor/...               the map editor opens the map it is given
tests/test_mapforge.cpp            the generator, without a widget
```

## Phases

### P0 Safety net

- [x] P0.1 The audit: the world's tiles and autotiles, the field's tiles
  and autotiles, what "half beach, half water" is, and what the field
  is missing. Above.

Acceptance: the audit names the files and the tiles, and the plan is
committed on its own.

### P1 The pick and the send

- [ ] P1.1 The world map's tool picks a tile and reports it: the
  terrain, the autotile it is a variant of, and the side the land is on.
- [ ] P1.2 "Expand this tile" in the World tab sends the pick to the
  map editor, which opens the map the expansion makes.
- [ ] P1.3 The test: the pick of a coastal tile names the side, and the
  same tile always names the same side.

Acceptance: ctest green; picking a coastal tile and expanding it opens
a map in the map editor.

### P2 The expansion

- [ ] P2.1 The generator takes the terrain and the side and lays the
  map's edges out to match: the beach on the west, the water on the
  east.
- [ ] P2.2 The expansion's tiles are added to the tileset with their
  flags.
- [ ] P2.3 The test: the west beach stays on the west, the corner tiles
  are the variants for that corner, and the same seed gives the same map.

Acceptance: ctest green; the expanded map's edges read as the picked
tile.

### P3 The field's beach

- [ ] P3.1 A sand tile in the field's tileset, generated by the forge.
- [ ] P3.2 The sand's autotile, with its edge and corner variants, so a
  beach meets the water the way the water meets the land.
- [ ] P3.3 The capture: the expanded map, its beach edge, and the
  capture opened.

Acceptance: the capture shows the beach on the side the picked tile
named; the gate is clean.

## Acceptance

- A tile picked on the world map can be expanded into a map, in the map
  editor.
- The expansion's terrain and direction are the picked tile's: a beach on
  the west stays on the west.
- The map is a project map the editor, the game and the tools all read.
- Every rule above is pinned by a test that needs no widget.
- The capture is opened, and the gate is clean.

## Rollback

Phase commits; revert to the tag `globe-expand-p2` for the expansion and
`globe-expand-p1` for the pick.

## Decisions and deviations

- **The direction comes from the autotile variant.** A coastal cell holds
  the ocean's edge variant, and the variant says which side the land is
  on. That is where the direction already lives; the generator reads it
  rather than working it out a second way.
- **The expansion is a generator, not a special case.** The map forge
  already makes a town, a cave and a platformer room; the globe's tile
  is another kind of input to it.
- **The field's beach is its own phase.** Without a sand tile and its
  autotile there is nothing for a coastal expansion to put on its edge,
  and the tile is the owner's to choose.
