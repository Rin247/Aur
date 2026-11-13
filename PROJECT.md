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
  suites passed (67 checks), 2026-11-13.
- `python tools\conformance.py --project Aur` — all 9 checks clean, 2026-11-13.
- `python tests\render_check.py` — 24 captures written to `build/renders/`, 2026-11-13. The pages
  are Map Editor, World, Sprite Editor, Tileset Editor, DAW, Creature Forge, Database and
  Playtest, at both estate sizes, plus the zoom sweep, the sprite preview, the DAW's
  arrangement, the Bestiary and the Database's actors and weapons. **A render asserts that a
  surface did not crash, not that it looks right: open the PNGs.**
- `cmd /c "AIO.bat build windows Release"` — built and deployed, 2026-11-13.

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
twenty-two PNGs.

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
- **The DAW scrolls.** The arrangement timeline is sized to its content and sits in a scroll
  area, so the bars and the tracks beyond the window are reachable.
- **Every tab of the Database page edits its records.** One generic
  editor and a table of fields per kind: a character's nested growth, a
  weapon's fraction and range, and the rest. A field the editor does
  not show is carried through untouched.
- **The Database page has a Bestiary.** The creatures are edited where
  their records live: the list, the record, the artwork, and the fields
  the editor does not show carried through. The other tabs of that page
  are still stubs, and say so.
- **The pages are named for what they are.** The tab bar reads Playtest,
  Map Editor, Tileset Editor, World, Sprite Editor, Character Forge,
  Database, Events and DAW -- one name per page, and the class and its
  files carry the same name.
- **The platformer's rules are in the engine.** Gravity, a jump with a
  variable height, a body swept per axis, and a one-way platform that
  catches a fall from above and lets a hero up through it.
- **The cameras are the mode's.** The field's is the hero's place,
  clamped and wrap-aware; the platformer's scrolls sideways and holds its
  height until the hero leaves a dead zone.
- **The engine has movement modes.** The top-down field is one of them
  and a platformer is another, chosen by the map's `mode`. The rules are
  a pure function of the hero's state and the tile flags, so they are
  tested without a widget -- and the shipped field's rule is unchanged.
- **The world map answers the game's weather.** The pick belongs to the
  engine (`weather_for_slot`): the runtime rolls it and so does the editor,
  so the day, the hour and the map's climate in the World header answer
  "what does the game show then?". A forced kind is the runtime's manual
  override.
- **The world map has no starter locations.** The world map shipped three
  markers -- western_harbor, eastern_capital and southern_ruins -- that
  pointed at places on an old seed. A new world seed has no entities at
  all, so they meant nothing; the spawn stays.
- **The Character Forge's preview fits its pane**, like the map, the world
  and the pixel surface. It scaled its sprite to at most 8x, so a 48x64
  character sat in the middle of a large box.
- **The DAW's transport is play/pause and stop, as icons.** The play button
  holds the playback still and picks it up where it was; the stop rewinds it.
  The glyphs are drawn in the app's own colour, because the style's media
  icons are black on a dark button.
- **The studio has one master volume.** The mixer's master gain is the app's, saved in `~/.Aur`,
  and the DAW has a slider for it beside the master effects.
- **The playtest plays the map's song, on request.** The map's `music` names a song in the
  project, and the playtest plays it through the same device the DAW previews with. The sound is
  opt-in -- a "Play sound" button in the corner -- and leaving the tab stops it.
- **The app has an icon.** Aur's mark is a pixel globe from the estate's single generator
  (`AssetLibrary/Icons`), installed as `studio/resources/icons/` with a PE resource so Explorer
  draws it.
- **The window's minimum is the estate's floor** (1280×690, `DESIGN.md` §12). It was 720, which
  is larger than the floor, so the app could not be shown at it, and every capture was 45 px
  short at the bottom.

## Keybinds

| key | what it does |
|---|---|
| `F5` | play the project |
| `F11` | swap between maximized and windowed |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | the next page / the previous one |
| `Ctrl+S` / `Ctrl+O` | save the project / open another one |
| `F2` | the in-game debug menu (the playtest) |

## Open defects

1. **`studio/MusicPanel.cpp` is 1,311 lines**, over the owner's hard ceiling of ~1,250 and
   pre-existing. It needs splitting; the MIDI file transfer moved out in this pass.
2. **The Races tab is still a bespoke editor.** It writes a dozen lore
   fields with a text box each, where the other tabs use the generic
   editor. It works, and it is the one tab the generic editor does not
   hold.
3. **Three of the seven creatures have no artwork.** `bandit`,
   `bandit_archer` and `bandit_boss` name graphics the project does not
   have, so the Bestiary's portrait says so for them. The game's own
   renderer draws a default graphic instead, which is worse: the creature
   in the game is not the creature the record names.
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
- **One audio device for the app** (owner, 2026-11-12: "if you swap tabs the other stops"). The
  DAW's preview and the playtest's music share one `QtAudioOutput` (`studio/SharedAudio`), so the
  two cannot play over each other: the page on screen holds the device, and the other falls silent.
- **The playtest's sound is opt-in.** A studio that makes noise on its own is a studio nobody
  wants to open, so the game plays silent until the corner's button is pressed. The chrome's play
  button asks for the sound with the game.
- **The master volume is the studio's, not a song's** (owner, 2026-11-12). One number in the
  data root, moved from the DAW or from the playtest, and it moves both: the two pages play
  different songs through one device, so a per-song fader would fight the shared mixer.
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
