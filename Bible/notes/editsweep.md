# Pixel/sprite/tileset editor defect sweep + P1 hardening

Status: D1 landed (1e5fce9); D2.S1 landed (07fd68f); D3.S1 landed (218ba11).
Next step is D4.S1 (QUndoStack rewrite).

## Goal

Fix the 13 verified editor defects from the frontier-model audit, add the
regression tests that pin them, then land the highest-return features in the
audit's own order (tile picker, entity selection, undo rewrite).

## Contracts (binding)

- Map JSON shapes unchanged: `layers.{ground,objects,above}` row arrays of
  ints, `entities` array with `{id,type,x,y,...}`, `w`/`h` ints.
- Entity `id` stays the spawn marker id for spawn (`"spawn"`); multi-instance
  entities get unique `id` = `<kind>_<n>`. Spawn facing is written to BOTH
  `dir` and `facing` (back-compat read of either).
- `PixelEditSession::resolve_anim_frames` semantics: substring fallback only
  when no anim matched, and the fallback matches whole-direction tokens
  (`right` must not match `up_right`), except `down` which still lists all.
- Tests: engine-side logic moves into `studio/PixelEditSession.cpp` and a new
  `studio/EditorDefs.hpp`; new standalone test executables registered as
  CTest names (editor logic is Qt-free where testable).

## Current shape (verified 2026-11-08)

- `MapEditorPanel.cpp` (1 canvas class + panel): snapshot undo stack of whole
  layers/entities, 4 tools where 0 and 3 fall through to painting.
- `PixelEditorPanel.cpp`: per-frame undo, `setFrame` clears history, click
  paints one pixel, 5 magic brush numbers.
- `TilesetEditorPanel.cpp`: checkbox flags write JSON directly, no undo/dirty.
- Suite: ctest 7/7 green at MSVC Release (post-P8).

## Hazards found

- The panels hand-roll undo with whole-layer JSON copies; fixing the doubled
  undo inside that design is cheap, the QUndoStack rewrite is a later phase.
- Entity lookup by "first of kind" is load-bearing for the current inspector;
  multi-instance needs the inspector to move onto a selected index.
- `resolve_anim_frames`'s substring fallback is documented behaviour; tighten
  it without breaking "down = all frames".

## Target layout

- `studio/EditorDefs.hpp` — shared entity-kind array, flag defs/colors,
  palette_color helper (single definitions).
- `MapCanvasWidget`: tool guard fix, single-undo-per-stroke, stroke re-arm,
  zoom signal, facing key fix, real tile palette (from tileset).
- `PixelCanvasWidget`: undo survives frame switch (per-frame stacks), push
  only on change, Bresenham stroke interpolation.
- `PixelEditSession`: token-boundary direction match.
- `TilesetEditorPanel`: dedupe label set, undoable + dirty flag edits.

## Phases

### D2 — tile picker widget (report feature 1), landed 2026-11-08

- [x] D2.S1 Shared `TilePaletteWidget` (thumbnail grid via make_tileset_tile,
      pinned map tileset with hidden dropdown, multi-select stamp, right-click
      eyedropper). Map editor uses it; the old list-based tile rows are gone;
      the canvas paints row-major multi-tile stamps via setSelectedStamp.
      `<editsweep-d2>`

### D3 — entity selection + multiples + inspector (report feature 3), landed 2026-11-08

- [x] D3.S1 Click-to-select/drag-move/Delete for entities (tool 0 now
      selects/moves; Del + button delete with an undo snapshot); multiple
      instances with unique `<kind>_n` ids (spawn keeps the "spawn" marker
      id); inspector edits the selected instance and shows id, facing,
      event, name, text and param fields. `<editsweep-d3>`

### D4 — undo rewrite (report structural 1)

Status: D4.S1 in progress (plan refined 2026-11-11 against the source; no code
yet).

Current shape (verified): all three panels hand-roll undo as deques of whole-
region JSON snapshots — `MapCanvasWidget` keeps `undo_layers`/`redo_layers`
(≤100 entries, `(layer_index, layer JSON)` pairs, entities stored under
`layer_index == -1`), `PixelEditorPanel` keeps `undo_frames`/`redo_frames`
(per-frame whole-frame snapshots, cleared on `setFrame`), and
`TilesetEditorPanel` keeps its own flag-undo pair. Every paint stroke pushes a
whole-layer copy; a dense map stroke copies kilobytes per tile.

- [ ] D4.S1a Shared command base: a `CellCommand`-style `QUndoCommand` holding
      a grid key (layer index / frame index), an affected-cell list with
      before/after values, and an apply/restore path over a single target JSON
      object. Map cells are tile ints; pixel cells are RGBA bytes; tileset
      cells are flag ints. `<editsweep-d4a>`
- [ ] D4.S1b MapCanvasWidget: paint strokes coalesce into one command per
      stroke (cell diffs collected as the pointer moves), entity moves/deletes
      become single commands, the 100-entry cap moves onto QUndoStack's limit.
      Behaviour pins: one undo per stroke, redo restores exactly, entity id
      preserved. `<editsweep-d4b>`
- [ ] D4.S1c PixelEditorPanel: history keyed by frame — switching frames does
      not clear history; a stroke is one command of pixel diffs. `<editsweep-d4c>`
- [ ] D4.S1d TilesetEditorPanel: flag edits become cell commands (one per
      checkbox toggle batch), sharing the same base. `<editsweep-d4d>`
- [ ] D4.S1e Tests: a Qt-free command-core test pinning apply/restore over
      diffs (map, pixel and flag shapes) in `tests/`, registered in CMake and
      main.cpp; suite green. `<editsweep-d4e>`

### D5+ — sprite studio (report feature 5) etc.

- [ ] D5.S1 Onion skinning, live preview in sprite mode, mirror-copy
      right→left, frame add/duplicate/delete/reorder. `<editsweep-d5>`
- [ ] D6.S1 Tile pixel editing via PixelSurface + PNG import/export. `<editsweep-d6>`

## Acceptance

- All 13 audited defects fixed and each pinned by a test or an asserted UI
  invariant; ctest green; no file over 400 lines without a Bible baseline.

## Rollback

Phase commits; revert per `editsweep-dN` tag.

## Decisions and deviations

- Bug fixes land before features (audit's own order 1).
- `QUndoStack` is phase D4, not D1: the doubled-undo and stroke bugs are fixed
  within the existing stack first so behaviour is pinned by tests before the
  rewrite.
