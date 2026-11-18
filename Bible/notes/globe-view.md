# The globe's levels of detail: plan

Status: P0 and P1 done. Next: P2, the sphere. The open LOD step is the
third level -- a denser surface for the 3200% ceiling.

## Goal

The owner's picture (2026-11-12): "the more you zoom in the more it renders in
'HD' while the opposite is true as you zoom out. Think Google Maps, so a sphere
is highly desirable if even possible or feasible to begin with."

Two pieces: the **level of detail** (the fine picture when you are close, the
coarse one when you are far) and the **sphere** (the world as a globe rather
than a flat sheet).

## Contracts (binding)

- **The globe's surface is already HD.** `world::has_hd_surface(map)` decides
  which renderer a map uses, and the HD surface is a generated, detailed
  picture of the terrain (the studio's `HdSurface`, through the RHI). A map
  either has one or does not; the zoom does not change it.
- **The zoom is a factor, not a state.** `zoom_factor` goes into the
  `TerrainView` every frame, so the renderer already knows how close the
  person is.
- **A level of detail is a choice of two pictures of the same place.** The
  fine one when a tile covers enough screen, the coarse one when it does not.
  The crossover is a screen measurement, not a preference.
- **The world is a flat grid that wraps in x.** A sphere is a projection of
  that grid: the longitude wraps, the latitude does not, and the poles are
  where the projection pinches.

## Current shape (verified)

- The world map renders through `TerrainRhiWidget` (the RHI path, for the HD
  surface) and the CPU path (the tiles, `make_tileset_tile`), chosen by
  `world::has_hd_surface(map)`.
- `WorldMapCanvasWidget::terrainView()` passes `zoom_factor` into the
  `TerrainView` each frame, so the renderer knows the zoom; nothing uses it to
  choose a level of detail yet.
- The zoom's clamp is 5% to 3200% (`studio/world/WorldCamera.hpp`), and the
  spin's range matches it. The owner asked for the 3200% ceiling.
- The map's terrain is a 256x128 grid that wraps in x (`wraps_world_x`) and
  is flat: the "globe" is a cylinder with no poles.

## Hazards found

- **The HD surface is generated once, not per zoom.** A level of detail has to
  choose between two cached pictures; it cannot regenerate at every zoom.
- **Corrected in P1: an HD map has no `ground` layer.** The audit assumed the
  coarse picture was the ground tiles; an HD world map has only `biomes`,
  `elevation`, `moisture`, `temperature` and `terrain_overrides`. So the coarse
  picture is the *same* terrain function at one pixel per tile
  (`build_hd_surface(..., 1)`), not tiles.
- **The detail pass is sub-tile geometry.** At one pixel per tile the waves,
  rivers, foam and forest would be noise, so `build_hd_surface` draws them
  only at a density of 4 or more.
- **The crossover is where it goes wrong.** Too high and the coarse picture
  looks broken; too low and the far view costs what the close one does.
- **A sphere pinches at the poles.** The longitude's tiles shrink to nothing
  at the poles and the terrain's aspect distorts; a UV sphere needs the poles
  handled (a cap, or a cube map) rather than ignored.
- **The flat map is the editor's canvas.** The editor paints the grid; a
  sphere is a *view* of it, so the painting stays flat and the globe is a
  renderer, or the editor's work has to be re-projected.

## Target layout

```
studio/world/TerrainLod.{hpp,cpp}    the crossover: which picture, at this zoom
studio/world/WorldTerrainPaint.{hpp,cpp}  one level of terrain, drawn
studio/world/GlobeView.{hpp,cpp}     the sphere: the projection, the poles
studio/world/WorldMapCanvas.*        the zoom's levels, and the globe view
tests/test_terrain_lod.cpp           the crossover, without a widget
```

## Phases

### P0 Safety net

- [x] P0.1 The audit: how the two renderers are chosen, what the zoom
  already reaches, what the grid is, and where a sphere would pinch. Above.

Acceptance: the audit names the files and the rule, and the plan is
committed on its own.

### P1 The zoom's two levels

- [x] P1.1 The crossover: a screen measurement (pixels per tile) that picks
  the HD surface above it and the coarse picture below it. The measurement is
  the drawn width over the map's width, `kHdSurfacePixelsPerTile = 3.0`.
- [x] P1.2 The renderer takes the zoom into account; the far view is the
  coarse one. `WorldMapCanvas` builds and caches both pictures and
  `WorldTerrainPaint` draws whichever was chosen.
- [x] P1.3 The test: `tests/test_terrain_lod.cpp` pins the measurement, the
  crossover on both sides, the "a map with no HD surface never draws one"
  rule, and the two pictures' sizes.

Acceptance: ctest green (70 checks); zooming out of a map with an HD surface
renders the coarse picture, and zooming in renders the HD one. Proved by
`build/renders/world_1280x690_z1.0.png` (HD) and `..._z0.5.png` (coarse).

### P2 The sphere

- [ ] P2.1 The projection: the grid onto a UV sphere, the longitude wrapping
  and the latitude clamped.
- [ ] P2.2 The poles: a cap or a cube map, so the pinch is not a hole.
- [ ] P2.3 The globe view: the world as a sphere, with the zoom and the
  level of detail working on it.
- [ ] P2.4 The capture: the globe, its pole, and the capture opened.

Acceptance: the capture shows the world as a sphere; the gate is clean.

## Acceptance

- Zooming in renders the HD picture, zooming out the coarse one, and the
  crossover is measured rather than guessed.
- The world can be seen as a sphere, and the poles are not a hole.
- Every rule above is pinned by a test that needs no widget.
- The capture is opened, and the gate is clean.

## Rollback

Phase commits; revert to the tag `globe-lod-p1` for the level of detail and
`globe-lod-p2` for the sphere.

## Decisions and deviations

- **The globe is a view, not the editor's canvas.** The editor paints the
  flat grid; the sphere is how the person looks at it. A sphere you could
  paint on would need the painting re-projected, and the map editor is not
  the place to do that.
- **Both pictures are generated once and cached.** The level of detail
  chooses between them; it does not regenerate on a zoom change. Crossing
  over is a cache lookup.
- **The coarse picture is the same terrain at one pixel per tile**, not the
  ground tiles: an HD world map has no ground layer, and the owner's picture is
  the same place at 8-bit depth rather than a different rendering.
- **The crossover is in logical screen pixels per tile, not physical.** The
  camera and the fit are logical, so the rule is the same on every machine;
  3.0 puts the fit (3.9 at the estate floor) on the HD side and the zoomed-out
  view on the coarse side.
- **Open: the third level.** The HD surface's own density is capped at 32 per
  tile (22 for a 256x128 map, under the 16 Mpixel budget), so at the 3200%
  ceiling a tile covers ~125 logical pixels and the surface is upsampled. A
  denser picture for the visible region -- `build_hd_surface` already takes a
  region -- is the next step, and it needs its own cache key.
- **The 3200% ceiling is the owner's** (2026-11-12): 1600% is still tiny on
  a 16" 2560x1600 laptop.
