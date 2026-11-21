# WANTED — Aur

> This file holds **Aur's own asks**. For estate-wide decisions, see `C:\Projects\WANTED.md`.

## Owner queue (priority order)
This queue may contain asks that are ready to execute and asks blocked on owner-only decisions. A blocked entry stays in its position; agents do not silently skip it or choose its asset. `HANDOVER.md` records the exact current action. Documentation cleanup is separate from this feature queue; it is tracked in the documentation/estate subsection of Active plans.

1. Owner next-track choice — W-01 through W-04 are now closed; choose the next active implementation track (e.g. WorldSim WP4/P3.2 temperature & persistence choices in `Bible/notes/worldsim-p31.md`, DAW P2 GUI soak in `Bible/notes/daw/plan.md`, or authorizing Stage 3 RPG skeleton work from `aur_master_expansion_hd_upgrade_plan.md`).
2. Qt desktop foundation — staged behind Aur's own requests; estate audit plan remains read-only/audit-first.
3. P-01 — parked on the side per owner direction (2026-11-21) until the other core tracks reach MVP readiness; `Bible/notes/platformer.md` P4 floor-asset choice remains owner-gated when resumed.

All active W-01..W-04 implementation items are closed and verified. The remaining Aur feature items (WorldSim P3.2/WP4, DAW P2 soak, Stage 3 RPG skeleton authorization, and parked P-01) await owner scope selection.

## Active plans
### Documentation and estate plans
- `aur_master_expansion_hd_upgrade_plan.md` — latest owner-added broad direction, active as a planning/source document. Its stages and tracker remain draft, not verified execution status or authorization; S-01.5b classified the RPG/HD overlap and owner-source boundaries. Root location currently triggers conformance's stray-file finding.
- `Bible/notes/doc-reconciliation.md` — S-01/P2 proposal reconciliation complete; retained for the separate root-proposal placement issue if owner requests it and the estate-policy handoff. Accessible Aur docs now use living-doc 2,000-line soft / 4,000-line hard caps; estate conflicts, precedence question, and limited rollout scope are in `HANDOVER.md`.
- `C:\Projects\Bible\notes\project-handoff-prep.md` and `qt-desktop-foundation.md` — estate-wide read-only/staged plans, outside Aur feature priority.

### Aur feature plans
- `Bible/notes/globe-view.md` — W-04 landed and visually verified on D3D12 + Vulkan; no unchecked work remains.
- `Bible/notes/globe-expand.md` — W-01 landed and visually verified; no unchecked work remains.
- `Bible/notes/platformer.md` — P-01 detailed scope; parked on the side per owner direction (2026-11-21) until the other core tracks are MVP-ready.
- `Bible/notes/Aur World Map Agent Work Packages.md` — WP4/WP6 reference; WP4 weather slice approved, temperature/persistence choices open; WP5 globe dependency superseded.
- `Bible/notes/worldsim-p31.md` — P3.1 complete; P3.2 editor integration owner-gated.
- `Bible/notes/daw/plan.md` — DAW master plan; P7a parked in `Bible/notes/daw/gm-gs.md`; P2 close awaits owner GUI soak; P9 is tracked in `groovebox-sequencer.md`.
- `Bible/notes/sc-cleanroom.md` — planning only; sequenced behind DAW work.
- `Bible/notes/groovebox-sequencer.md` — landed; retained for history and DAW P9 detail; no unchecked work remains there.

## Closed (verified; latest first)

- **Globe third LOD & RHI parity** (W-04 P3, closed 2026-11-21): refactored `studio/world/HdSurface.cpp`
  into helpers under 100 lines and made `build_hd_surface` genuinely region-scoped (16 Mpixel budget
  against `tile_bounds`, 2-tile detail border, X-wrapping); extended `TerrainLod` with `LodTier`
  (`Coarse`/`Hd`/`CloseRegion`), `closeRegionDensity` (64/128 px/tile), and `CloseRegionKey`; updated
  `world_sculpt.frag`, `globe.frag`, and `TerrainRhiWidget` for `isYUpInNDC()` Vulkan/D3D12 vertical
  parity, `#171b20` margin parity, explicit `Height`/`Biome`/`Water` dirty rects, `globeRevision_`
  live sculpt tracking, smooth curvature-to-flat blend at high zoom, and `globeDetailTexture_` close-up
  compositing; raised wheel zoom clamp/snap to 3200%. Verified: `aur_tests` 76/76, CTest 7/7, and
  opened D3D12 + Vulkan flat, orbit globe, and 3200% close globe renders.
- **Field beach & coastal map expansion** (W-01, closed 2026-11-21): `generate_field_beach_tiles(0)`
  in `engine/aur/forge/AssetGenerators.cpp` uses `terra` `pal[0]` warm golden sand with a 4 px
  non-joined shoreline berm; `Project::load` bootstraps `field_beach` (`edge_joined: true`,
  `joins: ["field_beach"]`); `aur::apply_coastal_beach` in `engine/aur/mapforge.cpp` lays out
  directional water, autotiled `field_beach`, and textured grass for `WorldPanel::expandHoveredTile`
  and MCP `map_generate` (`coast_side`); `MapEditorPanel::openMap` selects maps by `itemData`; and
  `render_check.py` captures visible windows DPI-aware. Verified: `aur_tests` 76/76, CTest 7/7,
  and opened `build/renders/beach_expansion_1280x690_z1.0.png`.
- **The globe on the GPU** (W-04 P2.5, closed 2026-11-19): the sphere's
  projection moved into `studio/shaders/globe.frag` (yaw, pitch and viewport
  uniforms, the surface a GPU texture), so a drag costs a uniform update
  instead of a CPU pixel rebuild; the overlay draws nothing in globe mode so
  the sphere is not doubled; the globe texture is `BGRA8` because
  `build_hd_surface` returns `Format_RGB32` (the `RGBA8` upload swapped the
  channels and the ocean read ochre); `render_check.py` now forwards the
  `map_id` and `globe` slots it was dropping. Verified: build, ctest 7/7,
  conformance 9/9, 26 render captures opened.
- **Bandit artwork** (W-03, closed 2026-11-18): `generate_enemy_graphic` draws the three
  bandits at 24×24, `enemy_generate` writes each sprite and sets the record's `gfx`;
  the Bestiary portrait now receives the project in the editor's constructor and shows it.
- **Map seed generator** (W-02, closed 2026-11-18): `engine/aur/mapforge.*` makes a town,
  a cave or a platformer room from a seed and appends its tiles with flags; the World tab
  asks for one and the MCP's `map_generate` writes one; `seeded_platformer` ships and the
  playtest capture shows the hero on its floor (`Aur --map <id>`).
