# Repaint and refactor: plan

Status: P6 closed (tag repaint-p6). Next step: P7.1-P7.3 (docs reconciliation, full render sweep in dark and light, conformance gate).

## Goal

Owner, 2026-11-16: "refactor the entire app and give it a fresh coat of paint". Two outcomes:

1. **Paint.** The studio draws from the estate's design system (`C:\Projects\DESIGN.md` §1, §2, §6.3,
   §13) instead of one hard-coded navy and gold stylesheet: tokens, a `QPalette`, a `QProxyStyle`,
   eleven modes, a user accent, icons drawn in theme colours.
2. **Refactor.** Every file over the 750-line soft limit is split, the chrome leaves `MainWindow.cpp`,
   the duplicated and stale pieces go, and the docs stop contradicting each other.

The owner's queue (W-01, W-02, platformer P4) is unchanged and resumes after P7. D-01 and Q-01 stay
owner-gated.

## Contracts (binding)

- Executable `Aur.exe`, data root `~/.Aur`, tab names, `--tab/--zoom/--size/--sub-tab/--no-state`
  flags and the Map tool indices (0 Select/Move, 1 Paint Tile, 2 Place Entity, 3 Erase Tile) do not
  change. The render harness depends on them.
- Settings keys for the theme: `theme/mode` (a picker id), `theme/accent` (`#rrggbb`). Both are
  validated on load; an unknown value falls back to the default (DESIGN §5.2).
- The engine (`engine/aur/`) never includes Qt. Colour math lives in `studio/theme/ThemeMath.*`
  with no Qt types so `aur_tests` can reach it.
- Frameless window hazards (`Bible/Operations/NOTES.md` L409-422): in `WM_NCCALCSIZE` never assign
  `rgrc[0] = rgrc[2]`; `WS_THICKFRAME` stays; one drag path.
- Pure-move commits change no behaviour; repaint commits change no layout logic.

## Current shape (verified 2026-11-16)

- Baseline: `AIO`-equivalent build green, ctest 7/7, `render_check.py` writes 24 captures and exits
  0; captures are 1920x1035 at 150% DPI (1280x690 logical).

## Hazards found

- Docs contradict each other (the audit list in Phase 7).
- A stylesheet wins over the palette for every property it names (DESIGN §6.3), so the app
  stylesheet was removed in the same step the palette landed.
- Light modes (`dawn`, `sunrise`, `light`, `noon`, `firstlight`) expose every hard-coded dark ink,
  so a light mode is the acceptance test for "nothing is hard-coded".
- `render_check.py` asserts non-flat capture, not rightness: open the PNGs.
- Shared tree: other harnesses push to `main`; pull before each push.

## Target layout

```
studio/theme/ThemeMath.{hpp,cpp}        Qt-free: luminance, isDark, onAccent, accentDim/Bright, contrast
studio/theme/ThemePalettes.{hpp,cpp}    one table row per mode (eleven + system), token structs
studio/theme/ThemeQt.{hpp,cpp}          QColor bridge and the QPalette mapping (Qt Gui only, unit-tested)
studio/theme/ThemeManager.{hpp,cpp}     current mode+accent, install, signals, persistence, validation
studio/theme/ThemeMetrics.hpp           the sizing tokens (28 control, radii 4/6/8, 16 glyph, 10 scrollbar)
studio/theme/AurStyle.{hpp,cpp}         QProxyStyle over Fusion: metrics, sizes, dispatch
studio/theme/AurStylePaint.{hpp,cpp}    the flat painters: button, edit, combo, spin, tab, scrollbar, splitter
studio/theme/ThemeIcons.{hpp,cpp}       glyphs drawn in token colours (hamburger, window, file, transport)
studio/theme/AppearanceDialog.{hpp,cpp} mode grid + accent matrix + hex
studio/StudioChrome.{hpp,cpp}           the title-row widget, out of MainWindow.cpp
studio/FramelessWindow.{hpp,cpp}        WM_NCCALCSIZE / WM_NCHITTEST, out of MainWindow.cpp
studio/music/MidiStrip.{hpp,cpp}        the DAW's MIDI device row, out of MusicPanel.cpp
studio/MusicPanelSession.cpp            session grid, clips, arrangement, preview and MIDI file actions
studio/WorldPanelActions.cpp            world map generator, tile expansion, biome and entity dialogs
studio/world/WorldMapCanvasInput.cpp    world canvas painting, undo/redo, mouse and wheel input
studio/music/PianoRollInput.cpp         piano roll mouse, wheel and keyboard input
tests/test_theme.cpp                    contrast, polarity, onAccent, validation, table completeness
```

## Phases

### P0 Safety net
- [x] P0.1 Reproduce the blank Map Editor capture (`python tests\render_check.py --quick`), find
  whether the harness or the page is wrong, fix the cause. Record in `NOTES.md`.
- [x] P0.2 Archive the baseline captures to `build/renders/baseline/` (untracked) for before/after
  comparison; record the line-count baseline of the four offenders in `Bible/`.
- [x] P0.3 Run `python C:\Projects\tools\conformance.py --project Aur` once and record its result.
Acceptance: harness exits 0 with 24 captures; ctest 7/7; baseline recorded. Tag `repaint-p0`.

### P1 Theme core (no visual change yet)
- [x] P1.1 `ThemeMath` and `ThemePalettes` with the DESIGN §2.2 table, `test_theme.cpp`.
- [x] P1.2 `ThemeManager` with palette mapping per §6.3, settings validation, a `changed` signal.
- [x] P1.3 `AurStyle` (`QProxyStyle`): `controlHeight` 28, radii 4/6/8, hit target floor 28, focus and
  hover from accent, scrollbars in `divider`, no stylesheet.
Acceptance: unit tests green; app still looks as today (not yet installed). Tag `repaint-p1`.

### P2 Install it, delete the stylesheet
- [x] P2.1 `main.cpp` installs `ThemeManager` + `AurStyle` before the window and sets the colour scheme
  before first paint (no white flash, DESIGN §12.2b); the `main.cpp` stylesheet is deleted in the same
  commit.
- [x] P2.2 Replace the 21 `setStyleSheet` sites with palette roles, object names handled in
  `AurStyle`, or `theme::tokens()` reads; `rg setStyleSheet studio` returns nothing.
- [x] P2.3 Default mode `dark`, default accent per DESIGN (`#3b82f6`). Record the decision.
Acceptance: all pages render in `dark`; no navy or gold literal left in `studio/` outside data
colours; ctest 7/7. Tag `repaint-p2`.

### P3 Chrome
- [x] P3.1 Pure move: `StudioChrome` and the native-event code leave `MainWindow.cpp`.
- [x] P3.2 `ThemeIcons`: hamburger, minimise, maximise/restore, close, file actions drawn in token
  colours at 16 px glyph, 28 px target; close hovers in `bad`. Delete `SP_TitleBar*` use.
- [x] P3.3 Title row follows §13.6 (labels drop to icon-only, tabs ellipsise, overflow opens) at the
  1280x690 floor. Hotkeys stay (F5, F11, Ctrl+Tab, Ctrl+S, Ctrl+O); add `Alt+Enter` for the window
  mode and the Appearance entry in the hamburger.
Acceptance: renders at both sizes. Tag `repaint-p3`.

### P4 Canvases and painters read tokens
- [x] P4.1 `PixelSurface`, `MapCanvas`, `EditorDefs.hpp` (grid, selection, void margin).
- [x] P4.2 DAW canvases: `PianoRollCanvas`, `ArrangementCanvas`, `StepSequencerCanvas`,
  `VelocityLaneStrip`, session grid.
- [x] P4.3 World: `WorldMapCanvas`, `WorldMarkerOverlays`, `WorldClimateOverlays`; the world margin
  stays `#171b20` in the shader and the CPU path (PROJECT.md decision).
- [x] P4.4 `CreatureForgePanel`, `GameView` overlay, `bestiary/CreaturePortrait` backgrounds.
Acceptance: `dark` and `light` captures of every page opened and read. Tag `repaint-p4`.

### P5 Page polish
- [x] P5.1 DAW header: merge the song, MIDI and master rows so the track list is not clipped at
  1280x690 (`MidiStrip` extracted first, P6.1).
- [x] P5.2 Padding budget and one hint style across pages (DESIGN §1.7.1); selected rows use the accent
  wash (`accentDim`), not the brown fill.
- [x] P5.3 Appearance dialog: mode grid, accent hex field, live apply, persisted.
Acceptance: the 24-capture sweep at floor sizes; a person opens every PNG. Tag `repaint-p5`.

### P6 Refactor splits (pure moves, one commit each, build and ctest between)
- [x] P6.1 `MusicPanel.cpp` (1,268 -> 628): `MidiStrip`, `GmPatchList`, and `MusicPanelSession.cpp`.
- [x] P6.2 `WorldPanel.cpp` (968 -> 660): `WorldPanelActions.cpp`.
- [x] P6.3 `world/WorldMapCanvas.cpp` (825 -> 465): `world/WorldMapCanvasInput.cpp`.
- [x] P6.4 `music/PianoRollCanvas.cpp` (788 -> 536): `music/PianoRollInput.cpp`.
- [x] P6.5 `MainWindow.cpp` (575 -> 377) after P3.1: page registry stays, nothing else.
- [x] P6.6 Races tab onto the generic editor (E-01), a behaviour commit with a test.
- [x] P6.7 `CMakeLists.txt`: remove the duplicate `TransportIcon` lines and trim over-long comments.
Acceptance: no studio file over 750 without a baseline entry; ctest 7/7. Tag `repaint-p6`.

### P7 Docs and close
- [ ] P7.1 Fix the contradictions: `Bible/Operations/Project Overview.md` ("Mana Studio", "Aur Studio",
  `.\build.bat`), `MCP Server.md` and `worldsim-p31.md` build commands, `PROJECT.md` conformance path
  (`C:\Projects\tools\conformance.py`) and line counts, `WANTED.md` bestiary line, `editsweep.md`
  status line, `NOTES.md` L18 broken ref, `daw/plan.md` L8 broken ref. Mark `Operations/Handover.md`
  and `Operations/Roadmap.md` historical or prune them.
- [ ] P7.2 Update `PROJECT.md`, `HANDOVER.md`, `WANTED.md`, `ROADMAP.md`, `IMPLEMENTATION.md`,
  `Bible/Operations/NOTES.md`; add the design-system adoption to `PROJECT.md`.
- [ ] P7.3 Delete this plan's closed phases; remove it from `WANTED.md` when nothing is open.
Acceptance: full gate (build, ctest, render sweep opened, conformance). Tag `repaint-p7`.

## Acceptance

Build green; ctest 7/7 plus `test_theme`; render sweep at both floor sizes in `dark` and `light`
opened and read; `rg setStyleSheet studio` empty; no studio file over 750 lines; contradictions
listed in P7.1 resolved; the owner queue is intact.

## Rollback

Revert to the last `repaint-pN` tag, fix this plan, retry.

## Decisions and deviations

- 2026-11-16: the request supersedes queue order for this initiative; the queue resumes after P7.
- 2026-11-16: the studio adopts `DESIGN.md` §6.3 (palette plus `QProxyStyle`, no stylesheet).
- 2026-11-16: DESIGN.md 2.2's `textMute` row has 11 cells for 12 columns; Pitch Black has none. The
  table uses `#9494a0` (Dark and Midnight's value); the test pins the document's 6.14:1 against it.
