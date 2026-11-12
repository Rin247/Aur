Status: D4.S1b, D4.S1b1, D4.S1b2 and D4.S1e landed; D4.S2 (the owner's two
visual reports) landed with the render harness that proves them. Next open
phase: D5.S1.

## Goal

Fix the editor defect sweep, add the regression tests that pin them, then
land the highest-return features in the audit's own order (tile picker,
entity selection, undo rewrite).

## Contracts (binding)

- Map JSON shapes unchanged: `layers.{ground,objects,above}` row arrays of
  ints, `entities` array with `{id,type,x,y,...}`, `w`/`h` ints.
- A tileset tile is an OBJECT: `tiles[i].flags` is an int bitmask, not an
  array. A frame is an array of ROW STRINGS. Both were wrong in the first
  cut of the undo base and the tests agreed with the wrong shape, so the
  tests pinned fiction.
- Entity `id` stays the spawn marker id for spawn (`"spawn"`); multi-instance
  entities get unique `id` = `<kind>_<n>`. Spawn facing is written to BOTH
  `dir` and `facing` (back-compat read of either).
- `PixelEditSession::resolve_anim_frames` semantics: substring fallback only
  when no anim matched, and the fallback matches whole-direction tokens
  (`right` must not match `up_right`), except `down` which still lists all.
- Tests: engine-side logic moves into `studio/PixelEditSession.cpp` and a new
  `studio/EditorDefs.hpp`; new standalone test executables registered as
  CTest names (editor logic is Qt-free where testable).

## Current shape (verified 2026-11-11)

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
- A fixture that does not match the real document makes the tests agree with
  a fiction. The undo base was written against `graphics[i].pixels` and
  `tiles[i].flags` as arrays; the project has neither. See "Fixture lesson".

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

Status: D4.S1b landed on the QUndoStack; the base is being corrected.

Current shape (verified): all three panels hand-roll undo as deques of whole-
region JSON snapshots — `MapCanvasWidget` keeps `undo_layers`/`redo_layers`
(≤100 entries, `(layer_index, layer JSON)` pairs, entities stored under
`layer_index == -1`), `PixelEditorPanel` keeps `undo_frames`/`redo_frames`
(per-frame whole-frame snapshots, cleared on `setFrame`), and
`TilesetEditorPanel` keeps its own flag-undo pair. Every paint stroke pushes a
whole-layer copy; a dense map stroke copies kilobytes per tile.

- [x] D4.S1a Shared command base: a `CellCommand`-style `QUndoCommand` holding
      a grid key (layer index / frame index / tile index), an affected-cell
      list with before/after values, and an apply/restore path over a single
      target JSON object. Map cells are tile ints; pixel cells are RGBA bytes;
      tileset cells are flag ints. `<editsweep-d4a>`
      **Landed** (`2eb32ce` + `32c3793`, suite 56/56). Operates on `aur::Json`,
      not `QJsonObject` — the editors' currency. Out-of-range and missing
      regions are safe no-ops. Fixture lesson: `JsonArray{JsonArray{ints}}`
      flattens — rows must be wrapped in `Json`. D4.S1b next: wire the 7
      push sites in `MapEditorPanel.cpp` (978 lines) onto QUndoStack.
- [x] D4.S1b MapCanvasWidget: paint strokes coalesce into one command per
      stroke, entity moves/deletes become single commands, the 100-entry cap
      moves onto QUndoStack's limit. Behaviour pins: one undo per stroke,
      redo restores exactly, entity id preserved. `<editsweep-d4b>`
      **Landed** as three commits (`4bf4ce5` map, `ef5c89f` pixel,
      `2ccb6a3` tileset). Decision: a `SnapshotCommand` (region key +
      before/after region copies) went in first so undo ownership, redo
      clearing and the entry cap moved across at once; per-cell diffs follow
      as D4.S1b2.
- [x] D4.S1b1 Correct the shared base against the real document shapes.
      **Three defects, all found by measuring the base against
      `projects/sylvi/` and not against its own fixtures:**
      1. `RegionKey::MapLayer` resolved `layers.<name>[row]` — ONE ROW — while
         the editors store the whole layer. A map-layer undo therefore wrote
         a whole layer into a single row and corrupted the map. The region now
         resolves the whole layer, and a cell in it is addressed by the diff's
         own (row, col).
      2. `RegionKey::TileFlags` resolved `tiles[i].flags` as an ARRAY. The
         project stores `tiles[i].flags` as an int bitmask, so the region
         never resolved and the tileset undo silently did nothing. It is now
         `TileObjects`: the tiles array, one cell per tile object.
      3. `RegionKey::GraphicPixels` resolved `graphics[i].pixels`, which the
         project does not have (frames are arrays of row strings). The pixel
         editor already targets the live frame array directly, so the region
         is dropped.
      The fixtures that hid all three are rewritten against the real shapes:
      a tile carries an int `flags`, a frame is an array of row strings.
      `<editsweep-d4b1>`
      **Landed.** `resolveRegion()` is now one shared resolver (the editor
      and the command core cannot disagree about which layer a key means),
      and `Json` grew an `operator==` so a recorder can tell a changed cell
      from an unchanged one.
- [x] D4.S1b2 Per-cell diffs at the converted sites. A map stroke records
      only the cells it changed, so a dense stroke no longer copies kilobytes
      per tile. The stroke bookkeeping moves to `studio/MapEditUndo.{hpp,cpp}`
      because `MapEditorPanel.cpp` is already over the 750-line soft limit.
      `<editsweep-d4b2>`
      **Landed.** `MapEditUndo` records the cells a stroke touched (the first
      note for a cell wins, so repainting a cell keeps the original value)
      and pushes one `CellCommand` with a diff per cell. Entity strokes stay
      on the whole-array snapshot: an insert shifts every later index, so an
      index-addressed diff would not restore the array.
- [x] D4.S1e Tests: a Qt-free command-core test pinning apply/restore over
      diffs (map, pixel and flag shapes) in `tests/`, registered in CMake and
      main.cpp; suite green. `<editsweep-d4e>`
      **Landed.** `tests/test_undo_commands.cpp` is thirteen cases over the
      real shapes: layer cells by (row, col), tile objects, entities, the
      direct frame region, out-of-range and stale commands, and the recorder's
      own one-command-per-stroke behaviour. Suite 7/7.

### D4.S2 — the owner's two visual reports (landed 2026-11-11)

- [x] D4.S2.1 The map editor opened zoomed out instead of fitted. The fit ran
      in the constructor, against a canvas that still had its minimum size, so
      the zoom was computed for a 512-wide widget and kept when the real
      canvas arrived. `fitToView()` now defers to the first show/resize that
      has a real size; the World editor's canvas got the same treatment (its
      `first_show` fit had the same defect). `<editsweep-d4s2>`
- [x] D4.S2.2 The world map showed black bars above and below the map. The
      GPU terrain shader painted the margin outside the map with its own void
      colour, RGB (9,12,15), while the CPU path and the canvas paint the
      margin `#171b20`. The shader's margin is now `#171b20`, so the two
      paths agree. Measured in `build/renders/world_1280x770_z1.0.png`. `<editsweep-d4s2>`
- [x] D4.S2.3 The map editor's tool combo did not match the tool indices the
      code used: the combo said "Paint Tile" at index 0, which the canvas
      treats as Select/Move, so the paint tool painted nothing and the combo
      said "Erase Tile" for the slot that paints. The canonical order is now
      0 Select/Move, 1 Paint Tile, 2 Place Entity, 3 Erase Tile, and the
      combo follows it. `<editsweep-d4s2>`
- [x] D4.S2.4 A render harness, because "it builds" is not "it looks right".
      `tests/render_check.py` drives the shipped executable through `--tab`,
      `--zoom` and `--size`, renders the Map Editor and World pages at both
      estate sizes, and sweeps the zoom as a control (0.75 / 1.0 / 1.5 of the
      fit). The PNGs land in `build/renders/`; AGENTS.md says to open them.
      `<editsweep-d4s2>`

### D5 — sprite studio (report feature 5)

- [ ] D5.S1 The sprite studio's four tools, as four steps. Audited 2026-11-11:
      the pixel editor's **Graphic** mode is complete (palette, brush, zoom,
      preview, undo) and its **Sprite** mode is a frame list and a direction
      and pose picker; everything below is what a sprite editor needs to
      draw a walk cycle.
  - [x] D5.S1.1 Onion skinning: the canvas draws the previous and the next
        frame of the sprite's sequence ghosted behind the current one, tinted
        and at a low alpha, with a toggle in the sprite toolbar.
        **Landed.** The previous frame is warm and the next one cool, so a
        pose reads left to right; the skin is on by default in sprite mode.
        The render harness gained the pixel page and `--sprite`, so the
        tools can be looked at.
  - [x] D5.S1.2 Live preview in sprite mode: the preview button cycles the
        sprite's own frame sequence at the animation's fps. Today the button
        is graphic-mode only, so a sprite's frames are never seen moving.
        **Landed.** `PixelEditSession::resolve_anim` is now the one owner of
        the anim a direction/pose pair names (the pose-direction anim, then
        the bare pose, then none), and it carries the fps and the loop flag
        with it; the frame list, the preview and the frame tools read the
        same answer. The sprite controls moved to the sprite row, because
        the graphic toolbar was clipping their labels.
        `<editsweep-d5s1>`
  - [x] D5.S1.3 Mirror right→left: the right half of the current frame is
        mirrored onto its left half, as one undoable step.
        **Landed.** The mirror is a pure function over a frame's rows in
        `studio/PixelFrameOps.{hpp,cpp}`, so it is tested without a widget;
        the right half is mirrored onto the left, so the result is symmetric
        and the mirror of a symmetric frame is itself.
        `<editsweep-d5s1>`
  - [x] D5.S1.4 Frame add / duplicate / delete / reorder over the sprite's
        sequence, each one undoable.
        **Landed.** The four edits are pure functions over the graphic JSON
        in `studio/PixelFrameOps.{hpp,cpp}`: add a blank frame, copy a
        frame's pixels under a new name, take a frame out of the sequence
        (and out of the graphic when no anim names it), and swap a frame
        with its neighbour. A structural change is one undoable snapshot of
        the whole graphic, the way the entity edits take the whole array.
        `<editsweep-d5s1>`
  **D5.S1 landed** (all four steps): the sprite editor has the onion skin,
        a live preview of its own sequence, the mirror, and the frame tools.
        The render harness renders the sprite page, and the app's `--sprite`
        and `--preview` flags put it in sprite mode with the preview running,
        so the tools can be looked at.
  The structural edits are pure functions over the graphic JSON in
  `studio/PixelFrameOps.{hpp,cpp}`, so they are testable without Qt; the
  pixel canvas's undo stack takes the whole graphic as the snapshot for a
  structural change, the way the entity edits take the whole array. The
  render harness gains the pixel page, and `--sprite` puts the editor in
  sprite mode so the tools can be looked at.
  `<editsweep-d5s1>`

### D7 — the chrome (owner's sketch, 2026-11-11)

- [x] D7.S1 The file menu becomes a hamburger in the title bar, and the
      project toolbar slots under it. The menu row is gone, so every editor
      gets that row back for its own toolbars. The hamburger is drawn (three
      lines) rather than shipped as an asset, and the file commands -- open,
      save, reload -- keep their shortcuts behind it.
      `<editsweep-d7s1>`
  - [x] D7.S2 The frame is the app's own: `WM_NCCALCSIZE` gives the client
      area the whole window, so the chrome reaches the screen edge and the
      window has no OS border to drag. The edge resize was already there
      (`WM_NCHITTEST` answers `HTLEFT` and its corners); what was missing
      was the other half of the pair.
      `<editsweep-d7s2>`
  - [x] D7.S3 One control cluster and canvases that hug their space. The
      project's buttons became icon buttons in the title row beside the
      hamburger, the tab bar moved into the title row too (the pages are a
      stack below), and the two rows the editors were losing are gone. Every
      canvas now fits the space it is given: the map, the world and the pixel
      surface all scale to the pane with a small margin, and the pixel
      surface learns the pane's size from the scroll area's viewport, which
      is the only place that knows it.
      `<editsweep-d7s3>`

### D6 — tile pixel editing (report feature 6)

Audited 2026-11-11. The tileset editor has a tileset picker, a tile list, a
preview, the name and the gameplay-flag checkboxes — **no way to change a
tile's pixels**. The data is ready for it: a tile is
`{name, pal, flags, rows}` where `rows` is an array of row strings of palette
indices, and the tileset carries `palettes` (an array of palettes of hex
colours) and `tile` (the tile's size). `make_tileset_tile()` already renders
a tile to a `QImage`, which is exactly what a PNG export needs.

- [ ] D6.S1 The tile pixel editor, as three steps.
  - [x] D6.S1.1 `PixelSurface`: one pixel-editing surface, two editors.
        The pixel editor's canvas edits "a rows array with a palette"; that
        is what a tile is too. The surface moves to
        `studio/PixelSurface.{hpp,cpp}` and the pixel editor's canvas binds
        it to the current frame. A pure move: no behaviour change, and the
        render harness and the suite pin it.
        **Landed.** `studio/PixelSurface.{hpp,cpp}` owns the brush, the zoom,
        the undo and the painting; `studio/PixelCanvas.{hpp,cpp}` is the
        binding that points it at a graphic's frame. The canvas lost 447
        lines and the panel gained a surface it does not have to know about.
        `<editsweep-d6s1>`
  - [x] D6.S1.2 The tileset editor's pixel canvas: a `PixelSurface` over the
        current tile's rows and the tileset's palette, with the paint
        undoable over the tileset document.
        **Landed.** The canvas sits between the tile list and the inspector,
        the colour picker is the tile's own palette (`tile.pal` into the
        tileset's `palettes`), and the flag toggles moved onto the surface's
        stack, so one Ctrl+Z undoes the last edit whether it was a pixel or
        a flag. The editor opens with the first tile selected, so the canvas
        has something to show.
        `<editsweep-d6s1>`
  - [x] D6.S1.3 PNG import and export: export the current tile (or the
        current graphic frame) to a PNG file, and import a PNG into the
        current tile with every pixel snapped to the nearest colour in the
        tile's palette. The snapping is a pure function over the palette, so
        it is tested without a widget.
        **Landed.** The tileset editor has Export PNG and Import PNG; the
        pixel editor has a PNG button beside Copy and Cut. The snapping
        (`snap_rgba_to_rows`) is a pure function over the RGBA bytes and the
        palette: a transparent pixel takes the palette's transparent entry
        and every other pixel takes the nearest colour.
        `<editsweep-d6s1>`
  **D6.S1 landed** (all three steps): a tile's pixels are editable, and a
        tile or a frame goes in and out of a PNG.
### D9 - the studio's own tools: scrolling and volume (owner reports 2026-11-12)

Audited 2026-11-12. Two things a person expects from a studio and does not
get:

- **The DAW does not scroll.** The session grid is a table and scrolls; the
  piano roll is in a scroll area; the **arrangement timeline is a painted
  canvas with no scroll offset**, so a song wider or taller than the window
  cannot be reached.
- **There is no master volume**, in the DAW or in the playtest. The mixer
  has a master effect chain but no gain, and the **playtest has no audio at
  all**: the engine's `Game` is visual, so the game the studio authors plays
  silent while the DAW previews its music.

- [ ] D9.S1 The arrangement timeline scrolls: it is sized to its content and
      put in a scroll area, so the bars and the tracks beyond the window are
      reachable.
- [ ] D9.S2 A master volume: the mixer gains a master gain (the GUI thread
      sets it, the audio thread reads it), the DAW has a slider beside the
      master effects, and the value is the song's `master_volume`.
- [ ] D9.S3 The playtest's sound is the gap behind the volume: the game
      plays the song through the same mixer the DAW previews with.
      `<editsweep-d9s1>`

## Acceptance

- All 13 audited defects fixed and each pinned by a test or an asserted UI
  invariant; ctest green; no file over 400 lines without a Bible baseline.
- Every editor page renders to a PNG at both estate sizes, the PNGs are
  opened, and the sweep's zoom is a control rather than an axis.

## Rollback

Phase commits; revert per `editsweep-d4` tag.

## Decisions and deviations

- Bug fixes land before features (audit's own order 1).
- `QUndoStack` is phase D4, not D1: the doubled-undo and stroke bugs are fixed
  within the existing stack first so behaviour is pinned by tests before the
  rewrite.
- A test fixture is part of the contract. When the fixture does not match the
  document, the test pins a fiction and the code under it is wrong.
- Entity strokes stay on the whole-array snapshot while layer strokes moved to
  per-cell diffs: an entity insert shifts every later index, so an
  index-addressed diff cannot restore the array.
- The world's margin colour is written by the terrain shader, so the shader
  and the CPU path have to agree on it. They now both use `#171b20`; the
  shader's own near-black void was the "black bars".
- The render harness drives the shipped executable through the app's own
  controls (`--tab`, `--zoom`, `--size`), so a sweep changes what a person
  changes. The zoom is a factor of the canvas's own fit, which makes 1.0 the
  fit view and 0.75 / 1.5 the zoom sweep.
- The harness sets per-monitor DPI awareness v2 before it launches anything,
  and the app inherits it. A capture is then 1.5x the logical size on this
  machine, and the window rect is read in physical pixels; reading it from a
  DPI-unaware process reports logical pixels and makes the same window look
  two different sizes.
