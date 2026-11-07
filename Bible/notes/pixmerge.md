# Pixel/Sprite editor merge

Status: plan committed; not started.

## Goal

Close WANTED item "Refactor and merge the pixel and sprite editors": one
editor, one set of selection/palette/undo state, sprite selection
(direction/pose/frame) becomes a mode of the pixel editor rather than a
duplicate panel.

## Contracts (binding)

- `PixelCanvasWidget` (studio/PixelEditorPanel.hpp) is the single edit
  surface; its public API (setGraphic/setFrame/undo/redo/copy-cut-paste/
  colorPicked) must not change shape — both modes target it.
- `Project::graphics` JSON shape is untouched. Frame ids, anims, palette
  records stay exactly as serialized today.
- MainWindow keeps one "Pixel & Sprite Editor" tab; the separate Sprite tab
  is removed. `findChildren<PixelCanvasWidget*>` and the
  `reloadProject`/`setProject` fan-out must still reach every panel.

## Current shape (verified 2026-11-07)

- `PixelEditorPanel.hpp` (122 lines) declares three Q_OBJECT classes:
  PixelCanvasWidget (the real canvas: undo/redo frames, brush modes, zoom,
  auto-fit), PixelEditorPanel (graphic/frame/anim/palette/brush/zoom UI),
  SpriteEditorPanel (sprite/direction/pose/frame + duplicated brush/zoom/
  palette UI).
- `PixelEditorPanel.cpp` is ~37 KB; both panels implement their own
  `refreshPalette()`, brush/zoom combo wiring, and `resizeEvent` auto-fit
  logic; both own a `PixelCanvasWidget*`.
- The A0 audit (Bible/Operations/Handover.md) already recorded the shared
  finding: selection/history duplicated across Sprite and Pixel.
- MainWindow holds both as separate tabs; `MainWindow.cpp` fans `setProject`
  via recursive `findChildren`, so consolidation is transparent to it.

## Hazards found

- The cpp is a single 37 KB file; the estate file-size rule (750 soft) means
  the merge must not grow it. Plan: extract `PixelEditSession` (shared
  selection/undo/palette state) into `studio/PixelEditSession.{hpp,cpp}` and
  shrink both panels into it.
- Both `resizeEvent` implementations differ slightly (sprite panel sizes
  against a different viewport source); the merge must preserve each
  behaviour for its mode or the auto-fit regresses silently.
- Frame-id semantics differ: pixel mode uses raw frame ids; sprite mode
  derives `direction_pose_frame` ids from anims. One session must accept
  both without re-deriving in two places.

## Target layout

- `studio/PixelEditSession.{hpp,cpp}` — owns: current graphic, frame id,
  colour index, palette snapshot, undo/redo stacks, clipboard frame; exposes
  the same operations the canvas needs. One definition of
  `refreshPaletteData` and `resolveFrameIds`.
- `PixelEditorPanel` becomes the single panel: a mode toggle
  (Graphic / Sprite). Sprite mode adds the direction/pose/frame selector row
  (populated from graphic `anims`); graphic mode hides it.
- `SpriteEditorPanel` class deleted; MainWindow tab removed; tests for the
  session state added in `tests/` (session undo/redo round-trip, frame-id
  resolution for an anim-backed graphic vs a plain graphic).

## Phases

### Phase 0 — pin current behaviour

- [ ] Add `tests/test_pixel_session.cpp` covering: frame copy/paste
      round-trip through the canvas API, undo/redo restoring exact frame
      rows, palette read for anim-backed and plain graphics.
      Acceptance: suite green with the new file; tag `pixmerge-p0`.

### Phase 1 — extract the session

- [ ] `PixelEditSession` extraction; both panels consume it; behaviour
      unchanged (pure move, per the refactor-commit rule).
      Acceptance: build + suite green; UI behaves identically; `pixmerge-p1`.

### Phase 2 — merge the panels

- [ ] One panel with the mode toggle; SpriteEditorPanel removed from
      MainWindow; sprite selectors drive the same session.
      Acceptance: build + suite green; one tab in Studio; `pixmerge-p2`.

### Phase 3 — docs

- [ ] WANTED closed with evidence; Operations Handover A0 "Sprite/Pixel
      selection/history duplicated" annotated; plan deleted (history in git).

## Acceptance

- One editor tab; no duplicated palette/undo/selection state; suite green at
  every phase; A0 shared-state finding resolved.

## Rollback

Revert to the last `pixmerge-pN` tag; the estate rule for pure-move commits
makes any phase independently revertible.

## Decisions and deviations

- 2026-11-07: merge is a panel consolidation, not a canvas rewrite — the
  canvas API is already shared and healthy; the duplication lives in the two
  panel shells.
