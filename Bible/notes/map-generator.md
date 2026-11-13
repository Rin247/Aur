# The map seed generator: plan

Status: P1 and P2 done (2026-11-13). Next: P3, the MCP's map_generate
and the capture.

## Goal

The estate can seed a character, a creature, an effect, a piece of
clothing, a prop and a tile. It cannot seed a *map*: the world generator
makes the globe, and every field map in the project was made by hand.

The owner's words (2026-11-12): "I realized we lack a map seed generator
as well."

A seeded map is also what the platformer's room needs: a room with a
floor and platforms, generated the same way everything else is.

## Contracts (binding)

- **The engine owns the generator.** `engine/aur/mapforge.*` beside the
  other generators, Qt-free, and the same seed gives the same map.
- **A generated map brings its own floor.** The tileset's flags are per
  tile, so a platformer's floor cannot be the field's grass. The
  generator adds the tiles it needs to the tileset, with their flags,
  and uses them.
- **The map is a project map.** The generator writes the same shape the
  map editor writes: the layers, the entities, the mode, the spawn and
  the exits. A generated map opens in the map editor and plays in the
  playtest like any other.
- **The world tab is where it is asked for.** "Generate new world…" is
  the globe's; the field's is its own button beside it.

## Current shape (verified)

- `engine/aur/forge.hpp` has the generators: the character, the
  creature, the effect, the clothing, the prop and the tile. The tile
  one takes the palette and the seed and returns the rows; the map's
  is missing.
- The world generator (`engine/aur/worldgen.cpp`, 509 lines) makes the
  globe: a sphere with a climate profile, a topology and a seed. It is
  not a field map.
- `Project::collision` reads the collision from the *tileset's* flags
  through the map's ground and objects layers, so a map's solid floor
  is a tileset decision.
- The MCP has `tile_generate` (it generates a tile and adds it to a
  tileset) and `map_put` (it writes a map), but no `map_generate`: the
  tool that would make one is missing.
- The MCP's server rewrites the project when it saves, and its startup
  seeds the starter lore into the database (the "elementals" race
  appeared the first time it ran). A generated map must not carry that
  side effect.

## Hazards found

- **The flags are per tile.** A generated platformer that used the
  field's grass would make the grass solid in every map. The generator
  has to bring its own tiles.
- **A generated map is a real map.** It goes into the project, so it
  has to be the shape the editor, the game and the MCP all read, or it
  will be a broken map with a good name.
- **The tileset's palette is the tileset's.** A generated tile uses the
  palette of the tileset it is added to, and the tile's rows are palette
  indexes, so the generator cannot pick colours of its own.
- **The world generator is not it.** The globe is a sphere with a
  climate profile; a field map is a grid of tiles with entities. They
  share the word "world" and nothing else.

## Target layout

```
engine/aur/mapforge.{hpp,cpp}    the generator: the kinds, the map, its tiles
engine/aur/mcp.cpp                the map_generate tool
studio/world/WorldPanel.{hpp,cpp}  "Generate map…" and its dialog
tests/test_mapforge.cpp           the generator, without a widget
```

## Phases

### P0 Safety net

- [x] P0.1 The audit: what the generators are, what a map needs, and
  where the world generator differs from a field map. Above.

Acceptance: the audit names the file and the rule, and the plan is
committed on its own.

### P1 The engine's generator

- [x] P1.1 `mapforge`: the kinds (a town, a cave and a platformer room),
  the seed, the size, and the map's layers and entities.
- [x] P1.2 The tiles the map needs are added to the tileset with their
  flags -- a solid floor for the platformer, a wall for the town.
- [x] P1.3 The test: the same seed gives the same map, the kinds differ,
  and the platformer's floor is solid while the field's grass is not.

Acceptance: ctest green; the generator needs no widget and the test says
so.

### P2 The studio

- [x] P2.1 "New map…" in the World tab, beside "Generate new
  world…": the kind, the seed, the size, and the name.
- [x] P2.2 The map is added to the project and opened, and its mode is
  set from the kind.

Acceptance: a generated map opens in the map editor and plays.

### P3 The tools and the proof

- [ ] P3.1 The MCP's `map_generate`, so the tools can make one the way
  they make a tile.
- [ ] P3.2 The render sweep captures a generated platformer room in the
  playtest, and the capture is opened.

Acceptance: the capture shows the room and the hero on its floor; the
gate is clean.

## Acceptance

- A seeded map can be generated from the studio and from the tools, and
  it is a map the editor, the game and the MCP all read.
- The platformer's room has a solid floor of its own, and the field's
  grass stays walkable.
- The same seed gives the same map, and the test says so.
- The capture is opened, and the gate is clean.

## Rollback

Phase commits; revert to the tag `mapforge-p2` for the studio and
`mapforge-p1` for the generator.

## Decisions and deviations

- **The generator is its own file, not more of the forge.** The forge is
  1,240 lines and over its limit already; a map generator is a different
  responsibility (a grid, entities and tiles) from a character or a
  tile.
- **The generator brings its own tiles.** The flags are per tile, so a
  platformer's floor cannot be borrowed from the field's tiles.
- **The world's globe keeps its own generator.** It is a sphere with a
  climate profile, not a grid, and the owner asked for a map.
