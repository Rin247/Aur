# The Bestiary: plan

Status: P1, P2 and P3 done (2026-11-12). The Bestiary edits the
creatures and shows their artwork.

## Goal

The creatures need an editor. The Database page lists the enemies and
shows an empty form -- the list is there, the fields are not -- so the
creatures are the one kind of record in the project that cannot be
edited at all. The Bestiary is that editor: the Database page's own tab,
where a creature's name, its artwork and its numbers are read, written
and previewed.

The owner's words (2026-11-12): "we need a Bestiary, I guess we could
expand on the database."

## Contracts (binding)

- **The record shape is the project's.** `project.db["enemies"][id]` is
  `{name, gfx, look, hp, atk, def, spd, mov, skl, tspd, xp, ai, aggro, r,
  gold, drops, weapon}` in the shipped project. The Bestiary edits that
  shape and does not invent a second one; a field it does not show is
  still carried through untouched.
- **The artwork is the project's graphics.** The creature's `gfx` names a
  graphic in `project.graphics`, and the portrait is that graphic's
  frame, drawn by the engine's `frame_rgba` -- the same pixels the game
  draws.
- **The engine stays Qt-free.** The data layer (the field list, the
  defaults, the add and the remove) is plain functions over `Json`, so
  the test does not need a widget.
- **The tab is part of the Database page.** The Bestiary is the Database
  page's "Bestiary" tab, not a new page: it is a kind of record, and the
  page is the records.

## Current shape (verified)

- `studio/DatabasePanel.cpp` (267 lines) builds seven tabs. Six of them
  are `createCategoryView(...)`: a list on the left, a form of empty
  `QLineEdit`s on the right, with **no** load, no save and no selection
  handling. Only "Races" is a real editor (`createRaceEditor`,
  `loadSelectedRace`, `applyRaceChanges`, `deleteSelectedRace`).
- The shipped project has six enemies: bandit, bandit_archer,
  bandit_boss, bat, slime, slime_blue, slime_king. Their fields are not
  uniform: `aggro`, `r`, `gold` and `drops` appear on some, `mov`, `skl`,
  `tspd`, `weapon` and `look` on others.
- The `gfx` of some enemies names a graphic the project does not have
  (`bandit`, `bandit_archer`, `bandit_boss`): `graphics.json` holds `bat`,
  `slime`, `slime_blue`, `slime_king` and the player characters. A
  portrait for a creature whose graphic is missing must say so, not show
  a broken image.
- The game's `ai` values in the shipped project: `chaser`, `flyer`,
  `hopper`. The engine's creature code (engine/aur/forge.cpp,
  engine/aur/game.cpp) is where the kinds are defined.

## Hazards found

- **The empty forms look like editors.** A person opening "Enemies" sees
  a list and a set of fields and reasonably believes they can type in
  them. Six tabs of that is a lie; the Bestiary replaces one of them
  with a real editor, and the rest stay as they are until they are
  built (and are recorded as such).
- **The field set is not uniform.** A form built from a fixed list of
  fields would drop `aggro`, `r`, `gold` and `drops` on save. The data
  layer carries unknown fields through.
- **The missing graphics.** Four of the seven creatures name a graphic
  the project does not have. The portrait has to handle that honestly.
- **The Database page is the page the owner named.** The Bestiary is not
  a new page in the tab bar: a "Bestiary" tab next to "Enemies" would be
  two names for one thing.

## Target layout

```
studio/DatabasePanel.{hpp,cpp}     the page and its tabs
studio/bestiary/BestiaryData.{hpp,cpp}   the Qt-free record layer (new)
studio/bestiary/BestiaryEditor.{hpp,cpp} the list, the form, the portrait (new)
studio/bestiary/CreaturePortrait.{hpp,cpp} the picture of a creature (new)
engine/aur/...                      unchanged: the shape and the pixels are the engine's
```

## Phases

### P0 Safety net

- [x] P0.1 The audit: what the Database page does, what the record shape
  is, what the missing graphics are, and which parts of the page are
  still stubs. Above.

Acceptance: the audit names the file and the field, and the plan is
committed on its own.

### P1 The record layer (Qt-free)

- [x] P1.1 `BestiaryData`: the fields a creature shows (the ones the
  editor owns), the defaults for a new one, and the add and the remove.
  Unknown fields are carried through untouched.
- [x] P1.2 The test: a new creature has a name and a full set of
  numbers; a creature that is edited keeps the fields the editor does
  not show; a removed creature is gone.

Acceptance: ctest green; the layer needs no widget and the test says so.

### P2 The editor

- [x] P2.1 The Bestiary tab: the list of the creatures on the left, the
  form on the right, and the portrait above it.
- [x] P2.2 The form writes: the name, the artwork, the numbers and the
  AI, and the record is saved when a field changes.
- [x] P2.3 Add and remove, with the ids the project uses.

Acceptance: the tab replaces the "Enemies" stub; a creature's numbers can
be edited and come back after a reload.

### P3 The proof

- [x] P3.1 The Database page joins the render matrix, and the capture
  shows the Bestiary with a creature and its portrait.
- [x] P3.2 `PROJECT.md`, `WANTED.md` and `Bible/Operations/NOTES.md` carry
  the state, and the queue's row is closed.

Acceptance: the PNG is opened and shows the editor; the gate is clean.

**Landed 2026-11-12.** `studio/bestiary/` holds the three modules: the
record layer (Qt-free, with the test), the editor and the portrait. The
Database page's "Enemies" stub is the "Bestiary" tab now. The capture
shows the editor with a creature; the bandit has no artwork in the
project, so the portrait says so, and the artwork path itself is pinned
by the test.

## Acceptance

- The Bestiary edits a creature's record, and the record survives a
  reload.
- The portrait is the creature's own sprite, or an honest "no artwork"
  when the project has none.
- The fields the editor does not show are still there after an edit.
- ctest green, the gate clean, and the capture opened.

## Rollback

Phase commits; revert to the tag `bestiary-p2` for the editor and
`bestiary-p1` for the data layer.

## Decisions and deviations

- **The Bestiary is a tab of the Database page**, not a page of its own
  (owner: "we could expand on the database"). A page in the tab bar for
  one kind of record would make the bar longer for no reason.
- **The data layer is Qt-free** and tested without a widget: the
  record shape is the engine's, and a test that needs a widget is a test
  that cannot be run in the suite.
- **The artwork lookup asks whether the project has the graphic**, not
  what the project returns for that id: `Project::graphic` hands back a
  default graphic for an id it does not have, and a default is not the
  creature's picture.
- **The page's own tabs are reachable from the command line** (`--sub-tab`),
  so the sweep can capture one: the Bestiary is a capture of its own.
- The other six tabs stay as they are. They are stubs, and the plan
  records that; building five more editors is a bigger piece of work
  than the Bestiary, and pretending they work would be worse than
  saying so.
