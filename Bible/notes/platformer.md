# The platformer engine: plan

Status: P0 done (the audit). Next: P1, the movement modes.

## Goal

The engine moves a hero on a top-down field: four directions, one speed,
no gravity and no jump. A SNES-style platformer needs a different set of
rules -- gravity, a jump, a body that is wider than a point, a camera
that scrolls sideways and rooms that are screens rather than a map.

The owner's ask (WANTED.md): "Implement SNES-style platformer games and
metroidvanias -- build SNES-style platformer and metroidvania game support
into the engine." The row says it needs its own plan before any code;
this is that plan.

## Contracts (binding)

- **The engine stays Qt-free.** The physics, the modes and the camera are
  plain functions and structs over `Json` and the tile flags, so they are
  tested without a widget and the studio only draws what they say.
- **A map's mode chooses its movement.** The maps already carry
  `"mode"` ("action" for the field, "world" for the globe), so a
  platformer map is `"mode": "platformer"`. Nothing else changes for the
  existing maps: today's field is the `topdown` mode, byte for byte.
- **The tile flags are the collision.** The map's collision layer already
  carries `SOLID`, `WATER` and the rest (`engine/aur/project.hpp`); the
  platformer adds one flag for a one-way platform and uses the same
  layers. The studio's tile palette already edits those flags.
- **The hero's body is a box, and the top-down mode keeps its point.**
  The mode owns the probe: the top-down mode probes the hero's centre as
  it does today, so the field's feel does not change.
- **The camera belongs to the mode.** The top-down camera is the current
  clamped, hero-centred one; the platformer's scrolls sideways with a
  vertical dead zone.

## Current shape (verified)

- `engine/aur/field.hpp` (210 lines) is the field scene. Its `update`
  moves the hero on four directions at a fixed 148 px/s, probing
  `tile_flags` at the hero's centre against `SOLID | WATER`, and its
  camera is the hero's position clamped to the map (wrap-aware in x).
  There is no velocity, no gravity, no jump and no body.
- `engine/aur/tactics.hpp` (59 lines) is the turn-based battle scene, and
  it is a skeleton: its `update` advances the turn and nothing else.
- `engine/aur/game.hpp` (127 lines) owns the scenes, the clock, the
  weather and the script queue. `Game::travel` is the only way between
  maps, and it is what a room transition uses.
- The maps are one grid each with exits that name a target map and a
  spawn (`db`/`events` in the project): the rooms of a metroidvania are
  already the maps, and the exits are already the doors.
- The collision comes from `Project::collision(map)`, which reads the
  map's collision layer and the tileset's per-tile flags.
- The tile palette in the studio already edits the gameplay flags per
  tile, so authoring a platformer's solid and one-way tiles needs no new
  editor.

## Hazards found

- **The field's feel is a contract.** The top-down movement is what the
  shipped game is played with: a fixed speed, four directions, the
  centre probe. Any change to it is a change to the game, so the mode
  split has to leave that path exactly as it is.
- **A point probe is not a body.** A platformer needs a width and a
  height, and the corners are where a body gets stuck. The probe moves
  into the mode, and the top-down mode keeps the point.
- **The tileset's flags are shared.** Adding a one-way flag changes what
  `Project::collision` returns, and the field reads those flags too. The
  new flag has to be invisible to the top-down mode.
- **The camera is not the camera.** The field's camera is centred on the
  hero and clamped; a platformer's scrolls sideways and its vertical
  dead zone means the camera does not jitter. Sharing one camera
  implementation would break both.
- **The wrap.** A world map wraps in x; a platformer's rooms do not.
  The wrap-aware camera code is in the field's `update`, so the split has
  to keep the wrap with the top-down mode.

## Target layout

```
engine/aur/movement.hpp        the modes and the hero's body (new)
engine/aur/movement.cpp        the physics: the top-down probe, gravity, the jump
engine/aur/camera.hpp          the cameras: the clamped one, the side-scrolling one (new)
engine/aur/camera.cpp
engine/aur/field.hpp           the field scene, now choosing its mode
engine/aur/game.hpp            the game, choosing the scene by the map's mode
studio/...                     the studio: the tile flags and the map's mode
tests/test_movement.cpp        the physics, without a widget (new)
```

## Phases

### P0 Safety net

- [x] P0.1 The audit: what the engine's movement is, what the collision
  is, what the camera is, and what the maps already give a room model.
  Above.

Acceptance: the audit names the file and the rule, and the plan is
committed on its own.

### P1 The movement modes

- [x] P1.1 `movement`: the modes (`topdown` and `platformer`), the
  hero's body and its velocity, and the mode that owns the probe.
- [x] P1.2 The field scene picks its mode from the map's `mode`, and
  the top-down path is unchanged: the same speed, the same centre probe,
  the same feel.
- [x] P1.3 The test: the top-down movement moves as it did (the same
  positions after the same inputs), and the platformer's falls.

Acceptance: ctest green; the top-down test pins today's behaviour, so
the split cannot change it by accident.

### P2 The platformer's physics

- [ ] P2.1 Gravity, a jump with a variable height, and a body that is
  wider than a point: the corners are what get stuck, so the box is
  swept per axis.
- [ ] P2.2 The one-way platform flag, invisible to the top-down mode.
- [ ] P2.3 The test: falling, landing, jumping, the jump's height, and
  the one-way platform (up through, not down).

Acceptance: ctest green; the physics are a pure function of the state
and the tile flags.

### P3 The cameras

- [ ] P3.1 The side-scrolling camera: the x follows the hero and is
  clamped to the room, the y follows with a dead zone, and the map does
  not wrap.
- [ ] P3.2 The field keeps its camera exactly as it is.
- [ ] P3.3 The test: the camera's position for a hero in a room, the
  dead zone's edges, and the clamp.

Acceptance: ctest green; the field's camera is pinned by the same test
that pins its movement.

### P4 The map and the proof

- [ ] P4.1 A platformer map: the mode, the tiles, the exits and the
  spawns -- a small room in the shipped project, played with.
- [ ] P4.2 The studio: the map's mode is editable, so a map can be made
  a platformer, and the tile palette's flags cover the one-way platform.
- [ ] P4.3 The proof: the render sweep captures the playtest in a
  platformer room, and the capture is opened.

Acceptance: the capture shows the room and the hero on the floor; ctest
green; the gate clean.

## Acceptance

- A platformer map is playable: the hero runs, jumps, lands on the solid
  tiles and passes through the one-way ones, and the camera scrolls.
- The shipped top-down game plays exactly as it did: the same movement,
  the same camera, the same feel.
- Every rule above is pinned by a test that needs no widget.
- The capture is opened, and the gate is clean.

## Rollback

Phase commits; revert to the tag `platformer-p3` for the physics and the
camera, and to `platformer-p1` for the mode split.

## Decisions and deviations

- **The mode is the map's, not the project's.** A project can hold both
  a field map and a platformer map, and the map's `mode` already exists
  ("action", "world"). A platformer map is "platformer".
- **The modes are a movement, not a scene.** The field scene draws the
  map and the hero in both modes; what changes is how the hero moves and
  how the camera follows. A separate scene would duplicate the whole
  map drawing for one set of rules.
- **The body settles by less than a pixel.** The collision catches the
  falling hero a step after the landing, so the resting place is a
  fraction below the floor. The test measures the settle instead of
  pretending it is exact.
- **The physics are a pure function.** The hero's state and the tile
  flags go in, the new state comes out: that is what makes the rules
  testable without a widget, and the field's movement is already close
  to that shape.
- **The rooms are the maps.** The exits already name a target map and a
  spawn, so a metroidvania's doors are the doors the field already has.
  What a platformer needs on top is the camera and the physics, not a
  new room model.
