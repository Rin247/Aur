# The globe's levels of detail: plan

Status: P0, P1, P2 and P2.5 done; P3 (W-04: third visible-region LOD and Globe RHI stabilization) is active at position #1 in `WANTED.md` (P-01 parked on the side per owner direction 2026-11-21). Next step: P3.1.

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

- **Full-map HD surfaces are cached, not generated on each zoom change.** A close-up region needs a distinct bounded cache and must not trigger a full-map render for every camera movement.
- **Corrected in P1: an HD map has no `ground` layer.** The audit assumed the
  coarse picture was the ground tiles; an HD world map has only `biomes`,
  `elevation`, `moisture`, `temperature` and `terrain_overrides`. So the coarse
  picture is the *same* terrain function at one pixel per tile
  (`build_hd_surface(..., 1)`), not tiles.
- **The detail pass is sub-tile geometry.** At one pixel per tile the waves,
  rivers, foam and forest would be noise, so `build_hd_surface` draws them
  only at a density of 4 or more. Its optional region currently crops after
  generating the full image; it is not yet suitable for a dense close-region
  cache.
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

- [x] P2.1 The projection: the grid onto a UV sphere, the longitude wrapping
  and the latitude clamped.
- [x] P2.2 The poles: a cap or a cube map, so the pinch is not a hole.
- [x] P2.3 The globe view: the world as a sphere, with the zoom and the
  level of detail working on it.
- [x] P2.4 The capture: the globe, its pole, and the capture opened.

Acceptance: the capture shows the world as a sphere; the gate is clean. Proved by
`build/renders/world_globe_1280x690_z1.0.png`, and `tests/test_globe_projection.cpp`
pins the projection, the wrap, the poles and the shading.

### P2.5 The sphere on the GPU

- [x] P2.5.1 The projection moved into the fragment shader
  (`studio/shaders/globe.frag`): the same orthographic inverse, the disc
  test, the wrap and the limb shading, with only yaw, pitch and viewport
  as uniforms. The surface stays a GPU texture; a drag no longer rebuilds
  it pixel by pixel on the CPU.
- [x] P2.5.2 The overlay stops double-drawing: in globe mode the flat grid,
  markers and margin mask are skipped (`WorldMapOverlayWidget::paintEvent`
  returns when `globeOnGpu()`), so the sphere is the whole view.
- [x] P2.5.3 The channel swap: `build_hd_surface` returns `Format_RGB32`
  (BGRA in memory), so the globe texture is created as `QRhiTexture::BGRA8`
  and the upload is zero-copy. Uploading it into `RGBA8` put blue where
  red belonged -- the ocean read ochre and the beaches cyan.
- [x] P2.5.4 The harness forwards its flags: `render_check.py` dropped the
  `map_id` and `globe` slots when calling `shoot`, so the "world_globe"
  capture was actually the flat map. Both are now passed.

Acceptance: the globe spins smoothly (the owner's 5 fps complaint is gone),
the water is blue, and there is one sphere, not two. Proved by
`build/renders/world_globe_1280x690_z1.0.png` opened 2026-11-19; ctest 7/7,
conformance 9/9.

### P3 The close-up visible region and RHI parity (W-04)

- [ ] P3.1 Region-scoped `build_hd_surface` and 3-tier LOD model (`studio/world/HdSurface.cpp`,
  `studio/world/TerrainLod.{hpp,cpp}`, `tests/test_terrain_lod.cpp`): split `build_hd_surface`
  into helpers under the 100-line function soft limit, allocate/render only the requested
  `tile_bounds` (with a 2-tile detail border and X-wrapping) and compute the 16 Mpixel budget
  against `tile_bounds` so close regions render at 64–128 px/tile and full globes use up to
  22 px/tile; extend `TerrainLod` with `kCloseRegionPixelsPerTile = 32.0`, `LodTier`
  (`Coarse`, `Hd`, `CloseRegion`), `selectLodTier`, `closeRegionDensity`,
  `closeRegionTileBounds`, `globeCloseRegionTileBounds`, and `CloseRegionKey`, pinned in
  `tests/test_terrain_lod.cpp`.
- [ ] P3.2 RHI parity, live sculpt revision tracking, and 3-tier LOD rendering
  (`studio/shaders/world_sculpt.frag`, `studio/shaders/globe.frag`,
  `studio/world/TerrainRhiWidget.{hpp,cpp}`, `studio/world/WorldTerrainPaint.{hpp,cpp}`,
  `studio/world/WorldMapCanvas.{hpp,cpp}`, `studio/world/WorldMapCanvasInput.cpp`): pass
  `rhi()->isYUpInNDC()` for Vulkan/D3D12 vertical parity, align outside-disc margin to
  `#171b20`, handle `Height`/`Biome`/`Water` dirty rects explicitly, track `globeRevision_`
  against `WorldModel::revision()`, blend globe curvature smoothly to flat at high zoom, and
  composite the cached close-region surface in both `TerrainRhiWidget` (`globeDetailTexture_`)
  and `WorldTerrainPaint`.
- [ ] P3.3 Visual verification and closeout: run `aur_tests`, CTest 7/7, and
  `tests/render_check.py` (opening `build/renders/world_1280x690_z1.0.png` and
  `build/renders/world_globe_1280x690_z1.0.png`), run conformance, and close W-04 in
  `Bible/notes/globe-view.md`, `WANTED.md`, and `HANDOVER.md`.

Acceptance: region-scoped `build_hd_surface` matches cropped full-map output without allocating
the whole planet; 3-tier LOD and `CloseRegionKey` tests pass in `test_terrain_lod.cpp`; D3D12
and Vulkan share right-side-up orientation and `#171b20` margins; live sculpt edits update the
globe immediately; `aur_tests` and CTest 7/7 pass; world and globe captures are opened and
verified. End tag: `globe-lod-p3`.

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
- **Open: the third level (W-04).** The HD surface's own density is capped at 32 per tile (22 for a 256x128 map under the 16 Mpixel budget), so at the 3200% ceiling a tile covers about 125 logical pixels and the surface is upsampled. Although `build_hd_surface` accepts a region, it currently renders the full image and crops afterward; P3 must make generation genuinely region-scoped and use a cache key covering map identity/revision, tile region, density, and relevant rendering inputs. This plan is not the next queue item until the earlier owner choices are resolved or reprioritized.
- **The 3200% ceiling is the owner's** (2026-11-12): 1600% is still tiny on
  a 16" 2560x1600 laptop.
