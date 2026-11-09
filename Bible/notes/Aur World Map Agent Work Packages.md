# Aur World Map: Agent Work Packages

Status: WP0 LANDED 2026-11-08 (`1d9094a`); WP1 LANDED 2026-11-08 (`1be483e`)
— repeat_view_x camera (clamp both axes, 24 px margin, centred small maps;
tileAt misses outside the single copy), one-copy CPU/GPU draw with margin
mask, "Repeat world horizontally (seam view)" toggle default OFF in the
overlay menu, camera cleanup (WP0.4) folded in, test_camera_repeat_view
green. Seam-crossing stroke and undo behaviour unchanged (data wrap intact).
WP2.S1 LANDED 2026-11-08 (`a9d2726`) — "Add starter set" button,
`addStarterTerrainTiles()` and the "Biome starters" category removed;
`aur/forge.hpp` include dropped from WorldPanel (engine generator kept,
pinned by test_forge); landform entries resolve tiles by NAME with silent
fallback instead of the hardcoded `{-1,7,7,6,4,5,72}` + warning dialog.
WP2.S2 (authored start markers in data) remains owner-gated.
WP3 LANDED 2026-11-08 (`6ea8eb0`) — `studio/world/WeatherRender.{hpp,cpp}`
(deterministic field→image: X-periodic advected fBm, posterized 5-band
cloud alpha, shadow darkening, wind-slanted rain streaks, dithered snow,
~1 MP image cap) and `WeatherPreviewField.{hpp,cpp}` (coherent low-frequency
noise for forced types; climate mode mixes project weather profiles per
biome climate); `WorldWeatherOverlay.cpp` shrank to a compositor (build
field → render → draw once per copy, fog wash kept); context gained a `map`
pointer. Tests: `test_weather_render` (determinism ×3, uniform-field seam
continuity, clear-field transparency) and `test_weather_preview_field`
(coherence, profile mixing, invalid-map guard). 53 checks, ctest 7/7.
NOTE: the seam test asserts continuity on a uniform field (the renderer is
seam-safe; field-authored gradients legitimately step at the meridian).
WP3 visual acceptance (looks-right captures for all weather types) still
owner-verified; the brief's ≤8 ms budget is unmeasured (per-texel loops).
Next: WP4 WorldSim hookup (owner decisions: temperature seeding, sim
persistence), WP6 cleanup.

Briefs for agents covering the World Map editor: hotfixes, single-copy view, starter removal, weather renderer v2, WorldSim hookup, and the globe view. Based on a read of `WorldPanel`, `WorldMapCanvas`, `WorldCamera`, `WorldWeatherOverlay`, `WorldClimateOverlays`, `WorldMarkerOverlays`, `TerrainRhiWidget`, `WorldModel`, `WorldStrokeState`, `WorldGenerationJob`, `MapView`, `HdSurface` and the P3.0/P3.1 docs.

**Not yet reviewed (agents must read first):** `studio/shaders/world_sculpt.frag`, `engine/aur/worldsim.hpp/.cpp`, `engine/aur/world_topology.hpp`, `engine/aur/clock.hpp`, `CommandStack.hpp`, and wherever `generate_starter_world_terrain_tiles` lives. Nothing below has been compiled; line references are by function, not line number.

## Order of work

| # | Package | Size | Gate |
| --- | --- | --- | --- |
| WP0 | Hotfix bundle | small | none |
| WP1 | Single-copy world view (stop endless wrapping) | small-medium | owner overturns the P3.0 "repeating X camera" rule (done in this brief) |
| WP2 | Remove starters | small | S2 needs owner confirmation |
| WP3 | Weather renderer v2 (looks right, no sim needed) | medium | none |
| WP4 | WorldSim hookup (Live mode) | medium | this is P3.2 (weather slice only); owner approval given in this brief |
| WP5 | Globe view | medium-large | P3.4; owner request in this brief |
| WP6 | Perf and robustness cleanup | small | none |

WP0 first. WP1, WP2 and WP3 are independent and can run in parallel on different agents. WP4 needs WP3. WP5 needs WP1 (shared texture ownership) and benefits from WP4.

## Rules for every agent (from the repo docs)

- Read `Bible/Operations/Agent Guide.md`, the root `HANDOVER.md` and the World Map Topology Handover first. Capture `git status --short` before editing; the tree has unrelated dirty files. Stage only the files in your package.
- File size: soft cap \~1,000 lines per file, hard ceiling \~1,250. `WorldMapCanvas.cpp` and `WorldPanel.cpp` are near the cap, so new code goes in new files.
- Any non-ASCII character in a C++ string literal must be a `\uXXXX` escape inside `QStringLiteral`. Never a raw multibyte character.
- Build with `cmd /c .\build.bat` (it loads the MSVC environment). Use the absolute `vswhere` path rule from `AGENTS.md`. Tests: `ctest --test-dir build\Release -C Release --output-on-failure`, plus the focused `TopologyTests` and `WorldSimTests`. Do not run only a compile and call it verified.
- The engine stays Qt-free. No `/fp:fast`. Keep `QPainter::save()` and `restore()` balanced.
- A new `Q_OBJECT` header must be on the AUTOMOC path (the moc pitfall recorded in `NOTES.md`).
- Do not claim a visual or timing result unless it was measured in the current run. Finish with the Result record template from the topology handover (scope, files, build/test results, evidence paths, known limits, next gate), then stop.

---

## WP0: Hotfix bundle

One small commit. Each item is a confirmed defect from code reading.

1. **Unbalanced `QPainter::restore()`**: `draw_weather_overlay` (`WorldWeatherOverlay.cpp`) calls `save()` once and `restore()` twice (one at the end of the inner block, one at the end of the function). Remove the second restore. The extra restore pops state belonging to the caller's painter pipeline.
2. **Duplicate wind/currents block in `WorldMapCanvasWidget::paintCanvas`**: the wind/currents overlay is drawn twice. The first block builds `FlowOverlayContext` with only 13 initializers, so `is_ocean_at` is an empty `std::function`. With **Ocean currents** enabled, `draw_flow_overlay` calls it and throws `std::bad_function_call`. It also draws the climate axes twice. Delete the first block. Keep the second (it has the `cell > 0` guard, `simulation_phase`, `simulation_timer.isActive()` and `is_ocean_at`).
3. **`redo()` does not refresh the GPU terrain**: `WorldMapCanvasWidget::redo()` ends with `update()`, while `undo()` ends with `refreshCanvas()`. After redo, the QRhi terrain and overlay widget stay stale until something else triggers a refresh. Use `refreshCanvas()`. Add `emit entitiesChanged()`-style parity checks while there.
4. **`WorldCamera::tileAt` stray nested `if (spec.wraps_x)`**: a leftover duplicated condition. It compiles but is misleading. Cleaned up as part of WP1.
5. **Height tool step is almost invisible**: `heightStep = 20` is in R16 units (0..65535), which is about 0.3 of the 0..1000 elevation range per stamp, and a stamp only fires when the cursor enters a new tile. Make strength a setting (suggest default about 1.5% of range, about 1000 R16 units), scale by a soft brush falloff, and keep captured rects per stroke as now. Verify visually; do not trust the number.

**Acceptance:** build and all CTest suites green; toggling Ocean currents no longer throws; undo then redo of a height stroke repaints the GPU terrain without moving the mouse.

---

## WP1: Single-copy world view (stop the endless wrapping)

**Decision (owner):** the editor view shows a wrapping world **once** by default, with the camera clamped to it. The data topology is unchanged: wrapped reads/writes, seam-crossing strokes, autotile and dirty-rect splitting all stay. This overturns the P3.0 note "camera X unbounded/repeating" **for the editor view only**. The runtime field camera is untouched. Update the topology handover accordingly.

**Current behaviour found:** `WorldCamera::constrain` returns early for non-wrapping maps (so bounded maps pan without limit) and for wrapping maps re-centres X with `std::remainder` (endless). `tileAt`, markers, hover, flow, weather, temperature and the CPU fallback all draw or hit-test repeated copies. The GPU path repeats via a `Repeat` U sampler plus `effects[3] = wrapsX`.

**Steps**

1. `WorldCameraSpec`: add `bool repeat_view_x = false`. In `constrain`: if `wraps_x && repeat_view_x`, keep today's behaviour (re-centre X, clamp Y). Otherwise clamp both axes to the map bounds with a small margin (24 px). A map smaller than the viewport stays centred. `tileAt`: only wrap X when `repeat_view_x`; otherwise return `{-1,-1}` outside the map.
2. `WorldMapCanvasWidget`: member `repeat_world_view = false`, `setRepeatWorldView(bool)`, include it in `cameraSpec(...)`, re-constrain and `refreshCanvas()` on change.
3. `paintCanvas`: keep `wraps_x` for data lookups. Add `const bool repeat_x = wraps_x && repeat_world_view;`. Use `repeat_x` for `first_x`/`last_x` and pass `repeat_x` as the `wraps_x` field of the overlay contexts (weather, flow, temperature, axes, markers, hover, day/night). Those contexts use that flag to mean "infinite strip", so with clamped `first_x`/`last_x` they draw one copy automatically.
4. CPU fallback: draw only copy 0 of the HD surface when `!repeat_x`.
5. GPU path, no shader change: with `gpuTerrain && wraps_x && !repeat_x`, fill the area outside `bounds` (left and right) in the overlay widget with `#171b20` so repeated texture is hidden. The overlay is a translucent widget above the QRhi widget, so this works.
6. UI: a checkable action in the Climate layers menu, "Repeat world horizontally (seam view)", default **off**. Fit zoom already pads 24 px per side; keep it.
7. Optional follow-up (do only if the mask is visibly poor): read `world_sculpt.frag` and add a separate uniform for "draw outside rect" instead of reusing `effects[3]`. Keep the `Repeat` sampler regardless, because seam height derivatives need wrapped neighbours.
8. Optional: "seam peek" (about 16 tiles of wrapped content, dimmed, beyond each edge, with a seam line) so seam strokes are still editable at the edge.

**Reference draft for the camera (not compiled; diff-checked only):**

```cpp
static void constrain(WorldCameraState &state, const WorldCameraSpec &spec)
{
    const QSizeF widget = spec.viewport;
    WorldCameraSpec bounds_spec = spec;
    bounds_spec.pan = state.pan;
    const QRectF bounds = mapBounds(bounds_spec, widget);
    if (bounds.isEmpty()) return;
    if (spec.wraps_x && spec.repeat_view_x) {
        const qreal vlim = std::max<qreal>(0.0, (bounds.height() - widget.height()) * 0.5);
        state.pan.setY(std::clamp(state.pan.y(), -vlim, vlim));
        state.pan.setX(std::remainder(state.pan.x(), bounds.width()));
        return;
    }
    constexpr qreal margin = 24.0;
    const qreal hlim = std::max<qreal>(0.0, (bounds.width()  - widget.width())  * 0.5 + margin);
    const qreal vlim = std::max<qreal>(0.0, (bounds.height() - widget.height()) * 0.5 + margin);
    state.pan.setX(std::clamp(state.pan.x(), -hlim, hlim));
    state.pan.setY(std::clamp(state.pan.y(), -vlim, vlim));
}
```

**Tests** (pure functions, same pattern as `test_stroke_line_tiles`): clamp with a map smaller and larger than the viewport; `tileAt` outside the map returns `{-1,-1}` when `repeat_view_x` is off and wraps when on; a stroke across the seam still produces both edge columns through `stroke_line_tiles`; existing `TopologyTests` unchanged.

**Acceptance:** live capture on the D3D12 QRhi path at fit, 100% and 400%: one copy only, no repeated slivers in the margins, markers drawn once, panning stops at the edges, seam-crossing paint plus undo still restores exactly. Toggling "Repeat" restores the old behaviour. Update `World Map Topology Handover.md`.

---

## WP2: Remove the starters

Two possible meanings, so two tickets. The agent must not guess which applies; S1 is safe, S2 needs the owner.

**S1: Starter tile set and biome starters (UI):**

- In `WorldPanel.cpp`/`.hpp`: remove the "Add starter set" button, `addStarterTerrainTiles()`, the "Biome starters" entry in the category combo, and the `forge.hpp` dependency if nothing else uses it. Grep `generate_starter_world_terrain_tiles` and `"biomes"` category (`terrain_tile_category` returns it for names starting with `biome_`). Decide whether to delete the engine generator or leave it unused.
- **Coupling to fix first:** the landform combo hard-codes tile indices `{-1, 7, 7, 6, 4, 5, 72}` (River, Lake, Beach, Mountain range, Hills, Cave). These probably point at starter tiles. Replace with name-based lookup (`terrain_tile_category` plus the tile `name`), and hide landform entries whose tile is missing instead of showing a warning dialog.
- Tiles already saved in `tilesets.json` stay unless the owner asks for a data cleanup.

**S2: Authored start points and areas (data), needs owner confirmation:**

- `earthlike_world.json` ships four markers (`start`, `southern_ruins`, plus two more). Before deleting anything, grep the whole repo (engine, studio, MCP, project JSON) for those ids and for exits whose `target.spawn` references them. Also grep `worldgen.cpp` for any generated start location.
- The project start map is unchanged by this work. Report every reference found; remove only the markers nothing depends on.

**Acceptance:** build and tests green; the world editor opens the Sylvi project with no warning dialogs; the landform brush still works or hides cleanly.

---

## WP3: Weather renderer v2

### Why it looks bad now (confirmed in `WorldWeatherOverlay.cpp`)

- Forced modes paint one flat tint over the whole map.
- Clouds are three outlined vector ellipses at a hashed per-stride grid, 12 to 30 px regardless of zoom. At fit zoom the stride is about 18 tiles, so you see tiny islands of cartoon clouds.
- "Drift" is a sine wobble, not advection. Weather type is re-rolled per cell by hash every `tick/100` and every 3 game-hours, so it pops.
- Rain and snow are a screen-space lattice (40 px spacing) that is not tied to clouds, terrain or zoom. In climate mode the drops are three lines under each cloud.
- Smooth alpha vector shapes sit over crisp pixel-art terrain.
- An unused `ocean` variable is computed per cell.

### Target design

A **pure, deterministic renderer** that turns a weather field into a low-res image, then one nearest-neighbour draw. No per-frame particle loops.

```cpp
struct WeatherField {            // map-tile resolution, row-major, W*H
  int width = 0, height = 0;
  std::vector<float> cloud, rain, snow, wind_u, wind_v, temperature; // 0..1 (wind -1..1)
  std::uint32_t seed = 1;
  double time_s = 0.0;           // drives advection and rain phase
};
struct WeatherRenderParams {
  double hour = 8.0;
  int texels_per_tile = 4;       // lower for huge maps; cap image to ~1 MP
  QRgb ambient = 0xffffffff; float ambient_darkness = 0.f;
  float sun_dx = -0.6f, sun_dy = 0.8f;   // cloud shadow direction
  bool shadows = true;
};
QImage render_weather_image(const WeatherField&, const WeatherRenderParams&); // Format_ARGB32_Premultiplied
WeatherField make_preview_field(const Project*, const Json& map,
                                const std::string& weather /*climate|clear|...*/,
                                std::uint32_t seed, double time_s);
```

New files `studio/world/WeatherRender.{hpp,cpp}` and `studio/world/WeatherPreviewField.{hpp,cpp}`. `WorldWeatherOverlay.cpp` shrinks to: build or fetch cached image, draw it into `bounds` once (or per visible copy when `repeat_x`), balanced `save`/`restore`.

**Look**

- **Clouds:** coverage = smoothstep of the (X-wrapped, bilinear) cloud field against fBm value noise advected by wind × `time_s`. The noise lattice must be periodic in X with period equal to the map width in texels, or it tears at the seam. Posterize alpha into 4 to 5 bands (lit top, shaded underside) to match the pixel-art look. Tint by the day/night ambient (`GameClock::ambient()` is already used by the terrain shader and the CPU fallback).
- **Cloud shadows:** render the same density image shifted by the sun offset in a dark colour first, then the clouds. This gives them presence over the terrain.
- **Rain:** gated by `rain` and cloud density. Draw 3-texel slanted streaks (slant from wind) into the same image with hashed placement, animated by `time_s`. At fit zoom the result reads as rain curtains (darker blue-grey striations under the cloud), not noise. Cap streak density per texel.
- **Snow:** white dither dots where precipitation and `temperature` are below freezing. Snowpack (WP4) tints the terrain via a dithered white overlay, not particles.
- **Fog:** low-alpha bands from moisture, strongest at dawn.
- **Storm:** darker clouds and a deterministic lightning flash (seed and tick pick a cell with `rain > 0.8`; local radial flash, not a full-map rectangle).
- **Preview field (interim, neutral mode):** forced types build a spatially coherent field (low-frequency noise, not per-cell hash). `climate` mixes types by the project's weather profiles per biome climate (`climate_weather` and `project->meta["weather"]["profiles"]`), blended smoothly across biome boundaries. This is the existing "Preview" behaviour, made good. It is not a second source of truth: Live mode (WP4) replaces the field entirely.

**Budget:** render ≤ 8 ms for 256×128 at 4 texels/tile (1024×512), recomputed at the 10 Hz simulation timer or when hour, map or zoom bucket changes, never per mouse move. Cache key `(map, field revision, texels, hour bucket)`. For 2560×1280 maps use 1 texel/tile or render the visible region only. Print measured ms in a test, like the WorldSim tests do.

**Tests:** headless (`QImage`, no GPU). Golden FNV-1a checksum of `render_weather_image` for a 32×16 field and fixed seed, following the `build_hd_surface` checksum precedent. Seam test: columns 0 and W-1 of the rendered image are continuous (no step larger than a threshold). Determinism: same inputs give the same checksum twice.

**Acceptance:** live captures at fit and 400% for clear, cloudy, rain, snow, storm, fog and climate. They must read as weather over terrain, not stickers. No flicker or popping while the timer runs; the Weather overlay remains independent of day/night; CPU fallback unchanged.

---

## WP4: WorldSim hookup (Live mode)

This is the weather slice of P3.2. Owner approval is given for this slice only. Pin tools, storm injection and global climate sliders remain gated. Read `engine/aur/worldsim.hpp` first; the field and function names below are from the P3.1 plan, not from the source.

1. `studio/world/WorldSimSession.{hpp,cpp}`: owns one `aur::worldsim::WorldSim` per open world map. Configure from `MapView::seed()`, the map size, `WorldModel` heights downsampled to tile resolution, and the sea level. Seed the temperature field from the map's `temperature` layer. **Open decision:** the P3.2 text says authored values import as pins; the temperature layer is generator output, not hand-authored, so default to seeding without pins and pin only on explicit author tools later. The owner should confirm.
2. Advance on the existing `simulation_timer` with a user-set rate, strict per-timer tick cap (suggest 8), single-threaded (P3.1 measured about 1.1 ms/tick at 256×128). Height edits mark the session dirty and call `set_heights` debounced (about 500 ms) without resetting the other fields.
3. `WeatherField snapshot()` adapter from sim fields (cloud, rain, snowpack, wind u/v, temperature). Feed it to `render_weather_image` in place of `make_preview_field`.
4. Mode switch: **Preview (neutral)** vs **Live (simulation)**. The existing "Run simulation" menu item becomes "Live simulation". The Weather dropdown in Live mode becomes a global bias or anomaly, not a second renderer. Keep Preview independent of sim state (the P3.2 "neutral mode independence" requirement).
5. Wind arrows: when Live, `draw_flow_overlay` samples the sim wind field instead of its own latitude formula, so there is one source of weather truth. Ocean currents stay analytic (the sim has none).
6. Persistence: do not write sim state into map JSON yet. Show tick, sim day and `hash()` in the footer. **Open decision:** whether to persist later.
7. Do not fold `axial_tilt` into this work (a recorded separate gap).

**Tests:** adapter test (sim to `WeatherField` is deterministic given a fixed seed and tick count); the existing `WorldSimTests` stay green; a neutral-mode test shows the Preview image is unchanged by sim ticks.

**Acceptance:** visible rain-shadow behind a mountain range and snowpack building and melting over sim-days on a generated world; stepping and batching ticks give identical snapshots; no authored data is modified.

---

## WP5: Globe view (P3.4)

A map-screen visualization of the same wrapped fields and sun direction. It must not fork simulation or terrain data.

1. **Share GPU data first.** Extract the height/biome/water/LUT textures and samplers out of `TerrainRhiWidget` into a `WorldGpuTextures` owner used by both the sculpt widget and the globe widget, so each world is uploaded once and dirty rects feed both. Keep the repeat-U / clamp-V samplers.
2. `studio/world/GlobeRhiWidget.{hpp,cpp}` plus `studio/shaders/world_globe.{vert,frag}`; register in CMake the same way as `world_sculpt` (qsb compile and resource). Fullscreen triangle, ray-sphere intersection per pixel, longitude and latitude to texture UV: `u = lon/(2*pi)` repeating, `v = 0.5 - lat/pi` clamped. The world is equirectangular and the pole pinch is expected.
3. Lighting: the day/night terminator from a sun direction derived from `preview_hour` (and an optional season declination uniform that defaults to 0, because seasonal tilt is not implemented). Atmosphere rim. Optional cloud layer from the `WeatherField` as a small RGBA8 texture (WP3/WP4), drawn as the same posterized clouds.
4. Interaction: drag to rotate (longitude wraps, latitude clamps near the poles), wheel zoom, a Home button that recentres on the equator. Marker dots for locations and exits via the same entity list. Toggle "Map | Globe" in `WorldPanel`; map editing tools are disabled in Globe mode (view only).
5. Do not hardcode 256x128. Use the actual map dimensions.

**Tests/evidence:** headless texture-sharing test (single upload, both widgets bound); live captures on D3D12: the seam meridian shows no discontinuity at several rotations; cost measured at 1280x720.

**Acceptance:** rotating the globe through the seam shows continuous terrain, day/night follows the Time control, the weather clouds match the flat map's weather, and the flat editor behaves exactly as before.

---

## WP6: Performance and robustness cleanup

- `TerrainRhiWidget::view()` is called about ten times per `render()`; each call builds a `TerrainView` (copying `map_id`) and recomputes `mapBounds()` through `cameraSpec` and `MapView::current`. Fetch the view once per frame and pass it down.
- `WorldGenerationJob::start` stores one `progress_` member shared by all requests. A second `start()` replaces it while the first watcher's `finished` handler still uses the member (hide, `deleteLater`, `nullptr`), so the second handler can dereference null. Capture the dialog per request in the lambda.
- `WorldMapCanvasWidget::undo` falls into the whole-layer branch if `model_rects` is non-empty but `world_model` is null. Guard it.
- Add `QPainter` balance checks: a debug assert wrapper around overlay draw calls in the overlay pipeline.

---

## Decisions the owner still has to make

1. **Starters (S2):** do you want the authored markers removed from the Sylvi world, or only the tile/biome starter set removed from the UI?
2. **Temperature in the sim:** seed from the generated layer (suggested) or pin it.
3. **Persist sim state** in the map JSON, or keep it as editor session state for now.
4. **Repeat default:** this brief defaults the editor to a single copy (opt-in repeat). Say so if you want the opposite default.
