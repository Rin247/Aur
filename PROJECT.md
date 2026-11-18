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
  suites passed (75 checks), 2026-11-18.
- `py -3.15 C:\Projects\tools\conformance.py --project Aur` — all 9 checks clean, 2026-11-18.
- `py -3.15 tests\render_check.py` — 24 captures to `build\renders\`, 2026-11-18; the Forge PNGs
  were opened and show four labelled views with no smudge on the eye and no forehead or torso
  blocks. **A render asserts that a surface did not crash, not that it looks right: open the PNGs.**
- `cmake --build build\Release --target aur_studio aur_tests` — linked and green, 2026-11-18.

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
  (`studio/map/MapEditUndo.*`), the tileset and sprite editors snapshot the document they change, and
  the command core resolves a region against the live project (`studio/core/UndoCommands.*`).
- **Every module organizes its helpers in a nested folder under `studio/`.** `studio/` root holds
  only `main.cpp`, `MainWindow.*`, and the nine tab panel widgets (`GameView`, `MapEditorPanel`,
  `TilesetEditorPanel`, `WorldPanel`, `SpriteEditorPanel`, `CreatureForgePanel`, `DatabasePanel`,
  `EventsPanel`, `MusicPanel`), while all helpers live in `studio/core/`, `studio/theme/`,
  `studio/playtest/`, `studio/map/`, `studio/pixel/`, `studio/tileset/`, `studio/world/`,
  `studio/forge/`, `studio/bestiary/`, `studio/events/`, and `studio/music/`.
- **The sprite editor** has onion skinning (the neighbouring frames ghosted behind the current
  one), a live preview of the sprite's own sequence at the anim's fps, the mirror, and the frame
  tools (add, duplicate, delete, reorder). The tile editor has the same surface over a tile's
  rows, with the tile's own palette as the colour picker and PNG in and out.
- **One pixel surface, two editors.** `studio/pixel/PixelSurface` edits "a rows array with a palette",
  which is a graphic's frame and a tile; `studio/pixel/SpriteCanvas` binds it to a frame.
- **The chrome is one control cluster.** The file commands are behind a hamburger, the project's
  buttons are icon buttons beside it, and the tab bar is in the same row. `WM_NCCALCSIZE` gives
  the client area the whole window, so the chrome reaches the screen edge and there is no OS
  border to drag.
- **Every canvas and sidebar scales with the window.** `theme::bindResponsiveSplitter`
  (`studio/theme/ResponsiveSplitter.*`) redistributes splitter panes proportionally on resize across
  all nine panels while preserving manual splitter adjustments. `MapCanvas`, `WorldMapCanvas`,
  `PixelSurface`, `TilesetEditorPanel`'s tile preview, `TilePaletteWidget`, `CreatureForgePanel`,
  `RecordEditor`, `BestiaryEditor`'s portrait, and the DAW's `SessionGrid`, `ArrangementCanvas`,
  `StepSequencerCanvas` and `PianoRollCanvas` all scale dynamically with their viewport.
- **The app opens maximized** and remembers the page it was on through the schema-validated
  `AurSettings` store (`studio/core/AurSettings.*`, `studio/core/SettingsSchema.hpp`, carried over
  from Hydra) with the `settingsRead` startup guard. The data root is `~/.Aur`, with the leading dot,
  and `settings.ini` lives in it.
- **The DAW scrolls.** The arrangement timeline is sized to its content and sits in a scroll
  area, so the bars and the tracks beyond the window are reachable.
- **A tile carries its flags from the forge.** The Creature Forge's
  generator takes Solid and One-way, and the tileset editor can mark any
  tile one-way, so a platformer's floor and its platforms are tiles the
  owner makes.
- **The studio uses the estate design system and Hydra theme stack (`studio/theme/`, `C:\Projects\DESIGN.md` §1, §2, §6.3, §13; `C:\Projects\Hydra\`).**
  `ThemeManager`, `ThemeContract`, and `AurStyle` (`QProxyStyle` over Fusion) drive every surface,
  control, tab, scrollbar, splitter and item-view selection wash (`accentDim`) from `QPalette` and
  token roles, with zero stylesheets (`rg setStyleSheet studio` is 0). `ThemeIcons` paints vector
  glyphs in live token colours, and `AppearanceDialog` exposes all 13 Hydra modes (`system`,
  `brahmamuhurta` / `firstlight`, `dawn`, `sunrise`, `light`, `noon`, `sunset`, `bluehour`, `dusk`,
  `dark`, `midnight`, `amoled`, and accent-tinted `custom`), the 24×24 HSV + 23-grey + escape-hatch
  accent matrix, `#rrggbb` hex input, interface scale (`75–200%`), dyslexia-friendly font toggle, and
  colour-vision simulation (`none`, `protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia`).
- **Every tab of the Database page edits its records.** One generic
  editor (`records::RecordEditor`) and a table of fields per kind (`actors`, `classes`, `races`,
  `items`, `weapons`, `skills`), plus `Bestiary` for creatures. A field the editor does not show is
  carried through untouched.
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
- **The engine can read a world map's tile and grow it into a map.**
  `pick_terrain` names the cell's terrain and the side its land is on, and
  the map generator keeps that side: the beach on the west stays on the
  west. The World tab's "Expand this tile" sends the pick to the map editor;
  the field's own beach tile is still open (the owner chooses it).
- **A map is seeded too.** `engine/aur/mapforge.*` makes a town, a cave or a
  platformer room from a seed and brings the tiles it needs with their flags,
  so a room's floor is solid without making the field's grass solid. The World
  tab asks for one and the MCP's `map_generate` writes one into the project;
  `seeded_platformer` is the shipped example.
- **A scene can start on a named map.** `Game::startAt` begins on that map's own
  spawn instead of the project's start map — how the playtest plays a generated
  room (`Aur --map <id>`), and how the sweep captures it.
- **The world map answers the game's weather.** The pick belongs to the
  engine (`weather_for_slot`): the runtime rolls it and so does the editor,
  so the day, the hour and the map's climate in the World header answer
  "what does the game show then?". A forced kind is the runtime's manual
  override.
- **The world map has no starter locations.** The world map shipped three
  markers -- western_harbor, eastern_capital and southern_ruins -- that
  pointed at places on an old seed. A new world seed has no entities at
  all, so they meant nothing; the spawn stays.
- **The window has a bottom context strip** (`studio/theme/StatusStrip.{hpp,cpp}`): the page
  name on the left, the last line a panel posted, and on the right the project and whether it
  is saved, the RHI backend in use, the theme mode and the interface scale. It is read-only
  context — no buttons — and it is how a page says what it just did (`theme::postStatus`).
- **The Character Forge draws at native size and shows a turnaround.** The renderer
  writes a real 48×64 pixel grid — no 3/2 or 4/3 coordinate squeeze — and the preview
  shows the front, right, back and left views side by side, or one chosen direction,
  fitted to the pane up to 16×.
- **The forge is three engine files and three studio files.** `engine/aur/forge/` holds
  `SpeciesCatalog` (race catalog, variants, option lists, seeded spec), `CharacterRenderer`
  (the 48×64 canvas and its layers) and `AssetGenerators` (creatures, effects, clothing,
  props, tiles); `studio/forge/` holds the preview, the tab builders and the colour
  swatches. `engine/aur/forge.hpp` is still the one public header.
- **The Forge has four tabs** (Character, Creatures, Gear & FX, Tiles), the race combo is
  grouped Mortal / Supernatural, and a created character record carries its `variant`,
  `affinity`, `alignment` and `entity_role` — not only its name, age and race.
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
| `F11` / `Alt+Enter` | swap between maximized and windowed |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | the next page / the previous one |
| `Ctrl+S` / `Ctrl+O` | save the project / open another one |
| `F2` | the in-game debug menu (the playtest) |

## Open defects

1. **Three of the seven creatures have no artwork.** `bandit`,
   `bandit_archer` and `bandit_boss` name graphics the project does not
   have, so the Bestiary's portrait says so for them. The game's own
   renderer draws a default graphic instead, which is worse: the creature
   in the game is not the creature the record names.

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
- **The studio adopts `DESIGN.md` §6.3 (`QPalette` + `QProxyStyle`, no stylesheet)** (2026-11-16).
  Default mode is `dark` and default accent is `#3b82f6`, persisted under `theme/mode` and
  `theme/accent` in `~/.Aur` and validated on load.
- **Every `studio/` source file sits under the 750-line soft limit** (2026-11-16): `StudioChrome`
  and `FramelessWindow` left `MainWindow.cpp` (377 lines); `MidiStrip`, `GmPatchList`, and
  `MusicPanelSession.cpp` (567 lines) split `MusicPanel.cpp` (628 lines); `WorldPanelActions.cpp`
  (395 lines) split `WorldPanel.cpp` (660 lines); `WorldMapCanvasInput.cpp` (412 lines) split
  `WorldMapCanvas.cpp` (465 lines); and `PianoRollInput.cpp` (289 lines) split `PianoRollCanvas.cpp`
  (536 lines).
- **The character renderer is native 48×64** (owner brief `Bible/notes/forge-brief.md`, 2026-11-18).
  The old `character_frame` was authored in 32×48 and silently multiplied by 3/2 and 4/3, which
  strewed artefacts over every sprite (a dark ellipse across one eye, a metal pixel on the
  forehead, a gold block on the torso). The canvas and the rig are now real pixels and each layer
  paints a named palette index; `tests/test_forge.cpp` pins the contract.
- **The race catalog is data, not an if-chain** (`RaceInfo` + `character_race_catalog()`): the
  six Mortal and nine Supernatural peoples, their labels, and whether an affinity is forced. The
  combo, the validator and the seeded spec all read the same table.
- **The bottom strip is context, not a second control cluster** (2026-11-18). It shows the page,
  the last posted line, the project's saved state, the backend, the theme mode and the scale;
  anything interactive would break `DESIGN.md` §13's one-cluster rule. A page change clears the
  line so a stale message never describes the wrong page; a panel reposts on `showEvent`.

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

**All MDs/docs in this project are living documents** — maintained in place with finished items pruned; git holds the history (estate root AGENTS.md).
