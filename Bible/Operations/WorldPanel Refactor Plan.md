# WorldPanel Refactor Plan
## Audit
- `studio/WorldPanel.cpp` is 3,100 lines and `studio/WorldPanel.hpp` is 178 lines; both are below the hard 800-line limit only for header, and the implementation is a major size offender.
- Tracked in `WANTED.md` as Aur ask **#51** so the remaining work stays visible across handoffs; keep the plan and wanted entry until acceptance is verified.
- The canvas, QRhi terrain renderer, overlay renderer, map generation, palette/sidebar/toolbar setup, entity dialogs, camera, paint tools, overlays, and canvas-scoped undo/redo are currently coupled in the WorldPanel implementation.
- `studio/world/MapView.{hpp,cpp}` and `WorldMapHelpers.{hpp,cpp}` already exist in the working tree as untracked files. Reuse these seams rather than duplicating their responsibilities.
- Existing `WorldMapHelpers.cpp` duplicates file-scope helpers still in `WorldPanel.cpp`. `engine/aur/clock.*` contains a runtime `WeatherSystem`, but no shared deterministic climate-profile selector matching the editor's preview behavior was found; preserve the editor selector until a behavior-preserving shared operation exists.
- The test executable currently links only `aur_engine` and is not set up for Qt image rendering. A golden test therefore requires a narrow, deterministic Qt surface-builder API and Qt Gui linkage; avoid a flaky screenshot test of the whole widget.
- The working tree already has unrelated user changes in root guidance, WANTED, topology handover, archive reference, Bible notes, and untracked `studio/world/`; do not stage or commit them as part of this refactor.

## Ordered execution
1. **Surface safety net and header hygiene** — expose the existing deterministic surface-rendering operation through a testable Qt Gui seam, add a seeded-map image checksum regression test, add it to `aur_tests`, deduplicate `<memory>`, include required standard headers, and forward-declare pointer-only Qt widget types. This first extraction is intentionally the testability prerequisite for the later renderer split; it must preserve the existing pixels.
2. **Typed tools** — define `WorldTool`, use it through canvas signals, panel dispatch and paint/input handling, and initialize the combo from the ordered tool-name table.
3. **Map access and helper extraction** — expand `MapView` with safe elevation/biome accessors and migrate direct row access where used by the panel; move the remaining helper definitions and their callers to `studio/world/`; keep the image builder accepting `(map, project, QRect region)` and preserve output. Revisit climate selection only if the engine exposes an exactly compatible deterministic selector.
4. **Terrain renderer boundary** — move `TerrainRhiWidget` into its own files; replace its canvas owner pointer/friend access with an explicit immutable terrain snapshot supplied by a provider.
5. **Preview/overlay boundary** — introduce preview and overlay state, retain existing public behavior through one overlay setter, define ordered overlay drawing against a const context, and extract weather rendering first without changing composition order or the day/night CPU fallback.
6. **Shared camera/editing tools** — extract camera operations, project-level command history, and the press/drag/release tool contract into the shared Studio folder; preserve current undo/redo and input semantics.
7. **Panel composition** — move toolbar/sidebar setup into focused components, move generation progress/cancellation/request tracking into `WorldGenerationJob`, and consolidate add/edit entity dialog construction via entity descriptors.

## Per-step gate
For each numbered step: record touched-file sizes; make only moves/extractions or type/API substitutions required by that step; update tests in the same step; build and run the focused/full tests before committing; inspect the diff and stage only that step's files. Update `Bible/Operations/NOTES.md` and this plan as verified decisions or layout changes accrue. Do not commit pre-existing unrelated working-tree changes.

## Acceptance
- Behavior and generated surface pixels remain unchanged; seeded surface checksum and existing suite pass.
- New modules each own real moved code, have build wiring, and no friend access is needed for extracted collaborators.
- `WorldPanel.cpp` is below 800 lines, project builds, tests pass, and Bible notes explain the resulting boundaries.

## Progress
- [x] Audit, persistent plan, and baseline build/test verification completed.
- [x] Header dependency cleanup committed as `a535da2`.
- [x] Step 1: extracted the HD surface renderer to `studio/world/HdSurface.{hpp,cpp}`, kept its default full-map pixels intact, added the optional region crop API and seeded RGB32 checksum test (`7986464436153998528`), and verified Release build plus both CTest tests.
- [x] Step 2: canvas/panel tool state and placement dispatch now use `WorldTool`; the combo is populated from its ordered name table. Release build and tests passed; code commit: `8c63a85`.
- [x] Adjacent World tab request: moved World immediately after Map Editor; Release build passed; code commit: `8ed26ab`.
- [ ] Step 3 implementation is in the working tree: extracted helper calls, expanded `MapView`, added accessor tests, and migrated camera dimensions/tileset/wrap access. Build and CTest passed; commit is pending.
- [ ] Steps 4–7 remain outstanding. Update this checklist only when a step's build/test gate passes and its code commit exists.
