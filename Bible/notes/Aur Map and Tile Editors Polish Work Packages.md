# Aur Map and Tile Editors: Polish Work Packages

Briefs for agents covering the Map Editor, the shared tile palette and the Tileset Editor. Based on a read of `MapEditorPanel.{hpp,cpp}`, `TilePaletteWidget.{hpp,cpp}`, `TilesetEditorPanel.{hpp,cpp}` and `TileTexture.hpp` (the versions with the D1/D2/D3 "editsweep" changes).

Everything below is from reading the code. Nothing was compiled or run, so each high-severity item has a 30-second manual check. Agents should confirm it, then fix it.

## 30-second checks (do these first)

| # | Check | Expected if the finding is right |
| --- | --- | --- |
| 1 | Open the Map Editor, choose **Paint Tile**, click the map | Nothing is painted (or an entity is selected) |
| 2 | Click a tile in the palette, then click the map | The tile under the cursor is **erased** |
| 3 | Pick Place Entity, pick NPC, click twice on two different tiles | The second click **moves** the first NPC; no second NPC appears |
| 4 | Ctrl-click three tiles in the palette, then paint | One tile paints, not a stamp (and the palette shows three selected) |
| 5 | Select an entity with the Select tool | No highlight ring on the selected entity |
| 6 | Select an entity, click **Delete Entity (Del)** | Nothing happens (the Del key works) |
| 7 | Start the editor / open a project | The map is far smaller than the canvas; label may show a low percentage |

## What the code says, ranked

### Blocking

1. **Tile painting is unreachable.** The tool combo is `{Paint Tile, Erase Tile, Place Entity / Area, Select / Move}` (indices 0–3), but the code treats them differently:
   - `mousePressEvent`: tool 0 = select/move entity.
   - `paintTile`: tools 0 and 3 return immediately (the comment calls them "Select/Move" and "Eyedropper"); tool 1 **erases**; tool 2 places entities.
   - The tile-painting code at the end of `paintTile` is only reachable in an odd state (tool 2 with `selecting_entity` cleared by `setLayer`).
   - The palette handlers call `setTool(1)` with the comment "choosing a tile implies the paint tool", which is Erase. Net effect: the combo labels, the shortcuts (1–4), the palette and the canvas disagree. The earlier fix for "Select/Move paints tiles" removed painting from the Paint tool instead.
2. **Stamp painting reads out of bounds.** `side = round(sqrt(n))` and then `selected_stamp[dy*side+dx]`. For n = 3 the index reaches 3 (past the end); n = 7 reaches 8. Any non-square selection is undefined behaviour (crash or garbage). n = 2 silently paints one tile.
3. **The palette never produces a stamp.** `TilePaletteWidget` uses `MultiSelection` (every click toggles), but `itemClicked` emits only the clicked tile. `stampTiles()` exists and is never used. The header comment says "Ctrl+click extends", which needs `ExtendedSelection`.
4. **Second placement moves the first entity.** In `paintTile`, tool 2 with `selected_entity_instance >= 0` moves that instance. After the first placement `selected_entity_instance` is set to the new entity, and clicking the same palette row does not fire `currentRowChanged`, so placing a second NPC is impossible without picking another row and back.

### Serious

5. **Startup zoom.** `fitToView()` runs inside `refreshMaps()` in the panel constructor, when the canvas is still at its 512×512 minimum (not laid out). Nothing re-fits when it gets its real size (`resizeEvent` only re-constrains the camera). If the canvas is smaller than 44 px in either direction the fit goes negative and clamps to 5%. This is why the map starts zoomed out.
6. **`entityDeleteButton` is connected before it exists.** `connect(entityDeleteButton, ...)` runs while the pointer is still `nullptr` (the button is created later in the inspector block), so the button never works.
7. **No selection highlight for the Select tool.** `selected` in `paintEvent` requires `selecting_entity`, which is only true for tool 2.
8. **Delete and undo leave the inspector stale.** `deleteSelectedEntity()` clears the selection without `emit entitySelected(-1)`; `undo()`/`redo()` can leave `selected_entity_instance` pointing at a different or missing entity.
9. **`tilePalette->setProject()` is never called** from `MapEditorPanel::setProject`. After a project reload the palette keeps the old `Project*` (dangling if the project object was replaced).
10. **Inspector edits are not undoable,** and `Apply to Entity` is required; the event-ID list is built once in the constructor (`refreshEventChoices` is a local lambda that is never called again) so it goes stale after project or event edits.
11. **Exits cannot be configured in the Map Editor.** The inspector has no target map / spawn fields, although the World Map reads `entity["target"]["map"]`.
12. **Outdated tactical hint.** The Tactical mode hint says "Fire Emblem / Triangle Strategy style"; the roadmap says Fire Emblem is not the active tactical reference (XCOM / Wasteland 3 / Jagged Alliance 3 / Desperados 3).
13. **`make_tileset_tile` is fragile and slow.** `tileset["tiles"].as_array()` and `tileset["palettes"].as_array()` have no `contains` guard (a tileset missing either key can throw), and `hex_to_rgba` is parsed again for every pixel (1,024 parses per 32×32 tile; about a million for a 1,000-tile palette refresh). Resolve the 16 palette colours once per tile.

### Visual and feel

14. Tiles are drawn with antialiasing on and fractional rectangles, so seams and shimmer appear at non-integer zoom (the wheel steps zoom by 15%).
15. The grid draws at every zoom, so zoomed-out maps turn into a dark grid.
16. Layer opacity is hard-coded (ground 1.0, objects 0.95, above 0.82); there is no way to dim the inactive layers.
17. The wheel handler ignores the delta magnitude and the trackpad (`pixelDelta`), so smooth wheels and trackpads zoom too fast. The canvas has no focus policy, so no keyboard panning or shortcuts.
18. The palette icons are 32 px with smooth scaling, so 16 px tiles look blurry; tooltips show only `[Tile n]` even though tiles have names.

---

## EP0: Blocking bugs (do first, one commit each)

1. **Define the tools once.** Add `enum class MapTool { Paint, Erase, Fill, Rect, PlaceEntity, Select, Eyedropper }` in `EditorDefs.hpp` with a name table used to build the combo (like `kWorldToolNames` in the World editor). Replace every raw `0/1/2/3` in `MapCanvasWidget` and `MapEditorPanel`, including `setTool(1)` in the palette handlers (it must select Paint). Shortcuts: `B` paint, `E` erase, `G` fill, `R` rectangle, `P` place entity, `V` select, `I` eyedropper (keep 1–4 only if the order matches the combo).
2. **Restore painting** and keep Select/Move from painting. Painting uses the stroke path (one undo step per stroke), with line interpolation between mouse events (reuse `aur::world::stroke_line_tiles` from `WorldStrokeState` or move it to a shared header).
3. **Stamps.** The palette emits a `Stamp { int w, h; std::vector<int> tiles; }` computed from the selection's bounding box in grid row/column order (give the grid a fixed column count, or lay tiles out by the tileset's own width), not a sorted index list. Painting reads `tiles[dy*w+dx]` with bounds checks; `-1` entries are transparent. If the selection is not a filled rectangle, treat it as a **random brush**: each painted cell picks one of the selected tiles (deterministic from the cell coordinates).
4. **Palette selection:** `ExtendedSelection` (Ctrl and Shift), emit `stampSelected` from the full selection on `itemSelectionChanged`, right-click picks `grid->itemAt(pos)`.
5. **Placement always places.** Tool PlaceEntity creates a new instance on every click (optionally refusing a second entity of the same kind on the same tile); moving belongs to Select. After placing, select the new instance and emit `entitySelected`.
6. **Create `entityDeleteButton` before connecting it,** and call `syncInspectorFromInstance(-1)` at the end of the constructor so the inspector starts disabled.
7. **Selection highlight** whenever `index == selected_entity_instance`, regardless of tool.
8. **After delete/undo/redo/map change**, clamp or clear `selected_entity_instance` and `emit entitySelected(...)` (and clear `drag_entity_index`).
9. **Call `tilePalette->setProject(p)`** from `MapEditorPanel::setProject`, and refresh the event combo there too.
10. Select tool: push the undo snapshot on the first actual move, not on press (a click currently creates an empty undo step).

**Acceptance:** the seven 30-second checks above behave correctly; unit tests for tool mapping, stamp painting (including n = 3, 7), and entity placement count; CTest green.

---

## EP1: View and camera polish

1. **Startup and fit.** Add `fit_pending = true` on `setMap`; perform the fit in `showEvent` and `resizeEvent` while `fit_pending` is set and the user has not zoomed or panned. Guard `fitToView` against a viewport smaller than the padding.
2. **Default view (decision below).** Recommended: open maps at 100% centred on the spawn point (or the map centre); use Fit only if the whole map is smaller than the viewport. Make `F` = Fit, `Ctrl+0` = 100%.
3. **Remember the camera per map** (zoom and pan) for the session, and the last open map across sessions (editor settings), instead of always opening the first map alphabetically.
4. **Crisp tiles.** Disable antialiasing for tiles and the grid, snap each tile rectangle to integer pixels (floor left/top, ceil right/bottom), and use zoom steps (25, 50, 75, 100, 150, 200, 300, 400, 800%). The wheel zooms by `angleDelta/120` per step toward the cursor; trackpad `pixelDelta` pans; Ctrl+wheel zooms; Shift+wheel pans horizontally.
5. **Grid level of detail.** Hide the grid below about 8 px per cell, fade between 8 and 16 px, add a `G` toggle, and an optional major line every 8 or 16 tiles.
6. **Pan:** Space+drag and middle-drag; arrow keys pan when the canvas has focus (`setFocusPolicy(Qt::StrongFocus)`); Home = Fit.
7. **Reuse camera maths.** `studio::WorldCamera` already has pure `mapBounds/constrain/fitZoom/tileAt`; use it (non-wrapping) instead of duplicating the logic in `MapCanvasWidget`, so both editors share tests.
8. **Layers.** Replace the "Collision Overlay" pseudo-layer with a checkbox plus a legend (`editor::kFlagStyles` already defines colours), per-layer visibility toggles, and "dim inactive layers" (default on). Remove the hard-coded 0.95/0.82 opacities.
9. **Feedback on the canvas.** Brush ghost at the hover tile (single tile, stamp footprint, or entity ghost), tool-specific cursors, and a footer readout: `x, y · ground N · objects N · above N · flags` plus the selected tool and layer. The current top-left label stays for map size and tileset.
10. Use the application font for entity markers (the code constructs `QFont("Segoe UI", 9)` per entity per paint) and show entity names when zoomed in.

**Acceptance:** live capture at fit, 100% and 400%: map opens at the intended zoom, no tile seams, grid fades out when zoomed out, per-map camera restored after switching maps.

---

## EP2: Tools and palette

1. Tools: Fill (4-connected, per layer, with undo), Rectangle (filled; Shift = outline), Line (Shift-click from the last point), Eyedropper (`I` or right-click on the map picks the tile under the cursor from the active layer and selects it in the palette).
2. Palette widget (`TilePaletteWidget`): thumbnails scaled with nearest-neighbour at integer multiples, a size control (24/32/48/64), tile name and index in the tooltip, a search box filtering by tile name, optional category sections (`terrain_tile_category` in `WorldMapHelpers` already classifies names), flag badges on thumbnails (solid, water, hurt).
3. Build the thumbnails once per tileset and refresh on `tilesetChanged` (generalise `tilesetFlagsChanged` so pixel edits, renames and flag edits all refresh both the palette and the map cache). `refreshTilePalette` currently builds the grid twice (`setTileset` then `refresh`).
4. Keep Escape = clear stamp, `[` and `]` = change the active layer, `Alt` = temporary eyedropper.

---

## EP3: Entity workflow

1. **Schema-driven inspector.** Add a field table per kind to `EditorDefs.hpp` (`{key, label, type, choices, default}`) and generate the form from it, replacing the shared Facing/Event/Name/Text/Param rows. Suggested fields: NPC (name, dialogue/text, event, facing, movement), chest (item, count/gold, flag), sign (text), trigger (event, width, height, once), light (radius, colour, intensity), enemy (encounter group, patrol), spawn (facing), **exit (target map, target spawn, event)**.
2. Facing becomes a direction combo (down, up, left, right and diagonals if the movement system uses them).
3. **Apply on edit** (`editingFinished`/`activated`) instead of the Apply button, each as one undoable step (coalesce typing); keep validation for event IDs and show unknown IDs inline.
4. Rebuild the event and map choices on `setProject` and when the event registry changes.
5. **Areas.** The tool is labelled "Place Entity / Area" but triggers are single tiles: allow dragging a rectangle for trigger/light areas (store `w`,`h`), drawn as a translucent rectangle with resize handles.
6. Multi-select (marquee) with group move and delete; copy/paste of tiles and entities inside and across maps.
7. Entity list panel (all entities, click to select and centre), with search.

---

## EP4: Tileset Editor

Today: a text list, a preview, six flag checkboxes, flag undo via a private stack.

1. Replace the text list with the shared `TilePaletteWidget` (its header already says it is meant for this), with multi-select; flag edits then apply to the whole selection in one undo step.
2. Show the flag colours next to the checkboxes using the same `kFlagStyles` table as the collision overlay (the panel still has its own `kFlagDefs`).
3. Rename tiles (name field), show the palette index, tile size and "used in N maps" (scan `project->maps`).
4. Add / duplicate / delete tile (guard deletes: refuse if used, or remap).
5. "Edit pixels" button: open the tile in the Pixel Editor. Tiles are `rows` plus a tileset palette, so introduce a small `PixelSurface` adapter so `PixelCanvasWidget` can edit graphics and tiles. On change, emit `tilesetChanged` so the map canvas cache and palette thumbnails refresh.
6. Replace the private `flag_undo` vector with the shared undo mechanism (below).

---

## EP5: Structure

- **Consolidate constants in `EditorDefs.hpp`:** entity kinds (the list is repeated three times in `MapEditorPanel.cpp`: the palette array, the local `kinds` in the lambda, and `kEntityKinds`), flag definitions, mode hints (the hint text is duplicated in three places, and should move into one table).
- **One undo mechanism.** The map editor still snapshots whole layer JSON, the tileset editor has its own vector, and the inspector has none. Move to `QUndoStack` (or the existing `CommandStack<Action>` that the World editor uses) with per-cell diffs for tile strokes, and show "Undo Paint Stroke" in the menu. Mark the project dirty on every edit (the roadmap still lists consistent dirty-state reporting as missing; I could not see how the Map Editor reports changes to the main window, so verify).
- **Tests.** Extract tool state, stamp painting, flood fill and camera maths into Qt-light functions with unit tests (the repo already tests `stroke_line_tiles` and the world camera helpers this way).
- **Size.** `MapEditorPanel.cpp` is 1,047 lines; split the inspector and the toolbar into their own files as part of EP3 so new code does not push it past the cap.
- Update the Tactical mode hint (item 12) and remove any remaining mention of Fire Emblem as the tactical reference in the UI.

---

## Decisions the owner still has to make

1. **Default zoom:** 100% centred on the spawn point (recommended), or always Fit?
2. **Shortcut scheme:** letter keys (B/E/G/R/P/V/I, as in most map editors) or keep 1–4?
3. **Stamp semantics:** is a non-rectangular palette selection a random brush (recommended) or an error?
4. **Exit fields:** are `target.map` and `target.spawn` the right keys (the World Map already reads `target.map`)? Confirm the schema before EP3.
