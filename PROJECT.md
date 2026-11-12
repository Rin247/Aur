# PROJECT.md — Aur

> Aur is an AGPL desktop RPG studio: a 2D-HD pixel-art world, a map editor, a sprite
> editor, a tileset editor and a DAW, over an engine that plays the game.
>
> This file is the verified state, maintained in place. The owner's asks live in
> `WANTED.md`; the plan in flight is `Bible/notes/editsweep.md`; the older history is in
> `Bible/Operations/NOTES.md`.

## Verified

<!-- The last thing that was actually run, with its result. Delete a line when it stops being true. -->

- `C:\Dev\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build\Release --timeout 300` — 7 of 7
  suites passed, 2026-11-12.
- `python tools\conformance.py --project Aur` — all 9 checks clean, 2026-11-12.
- `python tests\render_check.py` — 12 captures written to `build/renders/`, 2026-11-12. The pages
  are Map Editor, World, Sprite Editor and Tileset Editor, at both estate sizes, plus the zoom
  sweep and the sprite preview. **A render asserts that a surface did not crash, not that it looks
  right: open the PNGs.**
- `cmd /c "AIO.bat build windows Release"` — built and deployed, 2026-11-12.

## What this is

Aur is a studio for a Chrono-Trigger-shaped 2D-HD RPG: the world is a pixel-art diorama, the
editors write the project's JSON, and the same engine plays it. The editors are the product:
the map editor, the sprite editor, the tileset editor and the DAW. What the estate does not have
is a studio where a pixel-art sprite and its walk cycle are authored, previewed and exported
from one place.

## The documents

| document | holds |
|---|---|
| `AGENTS.md` | durable operating rules for this project |
| `HANDOVER.md` | the pointer: where things stand and the next action |
| `WANTED.md` | the owner's asks, ordered, with open and closed |
| `PROJECT.md` | this file: the design, the decisions, the open defects |
| `Bible/notes/editsweep.md` | the editor defect sweep, the plan in flight |
| `Bible/Operations/NOTES.md` | the dated history of the studio |

## Build targets

**Desktop.** `cmd /c "AIO.bat build windows Release"` builds `build\Release\Aur.exe`. The Qt
runtime is deployed beside the exe by the build; without it the exe exits
`0xC0000135 STATUS_DLL_NOT_FOUND` and every runtime check fails on a build that passed. The
estate's `C:\Dev\build.bat build Aur` does the same thing and finds the same exe.

**Desktop, viable.** Yes: it built, it launched, the suite passed, and the render sweep wrote
twelve PNGs.

## What works

- **The editors undo over `QUndoStack`.** The map editor records per-cell diffs
  (`studio/MapEditUndo.*`), the tileset and sprite editors snapshot the document they change, and
  the command core resolves a region against the live project (`studio/UndoCommands.*`).
- **The sprite editor** has onion skinning (the neighbouring frames ghosted behind the current
  one), a live preview of the sprite's own sequence at the anim's fps, the mirror, and the frame
  tools (add, duplicate, delete, reorder). The tile editor has the same surface over a tile's
  rows, with the tile's own palette as the colour picker and PNG in and out.
- **One pixel surface, two editors.** `studio/PixelSurface` edits "a rows array with a palette",
  which is a graphic's frame and a tile; `studio/SpriteCanvas` binds it to a frame.
- **The chrome is one control cluster.** The file commands are behind a hamburger, the project's
  buttons are icon buttons beside it, and the tab bar is in the same row. `WM_NCCALCSIZE` gives
  the client area the whole window, so the chrome reaches the screen edge and there is no OS
  border to drag.
- **Every canvas fits the space it is given.** The map, the world and the pixel surface scale to
  their pane with a small margin; the pixel surface learns its room from the scroll area's
  viewport, which is the only place that knows it.
- **The app opens maximized** and remembers the page it was on. The data root is `~/.Aur`, with
  the leading dot, and the settings file lives in it.
- **The app has an icon.** Aur's mark is a pixel globe from the estate's single generator
  (`AssetLibrary/Icons`), installed as `studio/resources/icons/` with a PE resource so Explorer
  draws it.

## Open defects

1. **The DAW does not scroll.** The session grid, the arrangement and the piano roll are painted
   canvases with no scroll offset, so a song longer than the window cannot be reached. (Owner
   report, 2026-11-12.)
2. **`studio/MusicPanel.cpp` is 1,348 lines**, over the owner's hard ceiling of ~1,250 and
   pre-existing. It needs splitting, and the split is where the DAW's scrolling belongs.
3. **The world map's time and weather preview does not share the runtime's clock.** The editor
   keeps its own hour and its own forced weather, while the game rolls the project's calendar and
   the climate profile. What the editor shows is not always what the game will show.
   (`Bible/notes/editsweep.md`, the next phase.)
4. **`studio/world/WorldMapCanvas.cpp` (934) and `studio/WorldPanel.cpp` (928)** are over the
   750-line soft limit and pre-existing. The map editor and the pixel editor were split for this
   reason; these two are not.

## Decisions

- **The app is `Aur`**, not "Aur Studio" (owner, 2026-11-12). The executable is `Aur.exe`, the
  `project()` is `Aur`, and the AIO scripts name `Aur.exe`.
- **The data root is `~/.Aur`** with the leading dot, and the settings file lives in it. A size
  or a page asked for on the command line wins over the saved one, and `--no-state` turns the
  save off so a capture run cannot overwrite the state the person left behind.
- **One pixel surface** (`studio/PixelSurface`) rather than a canvas per editor: a graphic's frame
  and a tile are the same shape, and a second copy of that drawing code would rot.
- **The undo base resolves regions through one function.** Three of its regions once resolved
  shapes the project does not have (a layer's row instead of the layer, `flags` as an array, and
  `graphics[i].pixels`, which does not exist), and the tests agreed with the wrong shapes. The
  lesson is in `Bible/notes/editsweep.md`.
- **The pixel editor is the sprite editor** (owner, 2026-11-12): `studio/SpriteEditorPanel.*` and
  `studio/SpriteCanvas.*`. The pixel-level modules keep their names, because they are about
  pixels.
- **The world's margin colour is written by the terrain shader**, so the shader and the CPU path
  have to agree on it; both are `#171b20`.

## Durable findings

- `Bible/notes/editsweep.md` — the editor defect sweep: what landed, what is open, and the
  fixture lesson.
- `Bible/Operations/NOTES.md` — the dated history of the studio, including the undo base's three
  defects and the sprite studio's state.

## Build and test

```powershell
# the one command, from the project root
cmd /c "AIO.bat build windows Release"

# the suite
C:\Dev\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build\Release --timeout 300

# the render sweep, and the PNGs it writes
python tests\render_check.py
```

- **Compiles run at `-j 8`**, never two at once.
- **A render test asserts that a surface did not crash, not that it looks right: open the PNGs.**
- **A build is a gate, not proof.** Launch the exe and confirm it boots and stays up.
