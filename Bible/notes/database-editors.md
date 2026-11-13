# The Database's editors: plan

Status: P0 done (the audit). Next: P1, the record layer.

## Goal

Six of the Database page's seven tabs are a list and a form of empty
fields: the list is there, the fields are not, and nothing loads, saves
or follows the selection. Only "Races" is a real editor.

The Bestiary (Bible/notes/bestiary.md) is the seventh. This plan gives
the page one generic editor for the rest, so a record of any kind is
edited the same way.

## Contracts (binding)

- **The record shape is the project's.** `project.db[kind][id]` is what
  the editor writes; a field the editor does not show is carried through
  untouched, the same rule the Bestiary follows.
- **The field table is data.** One row per field: the key, the label,
  the kind and the range. Adding a field to a kind is a row, not code.
- **The layer is Qt-free.** The field tables, the defaults and the write
  are plain functions over `Json`, so the test needs no widget; the
  widget only draws what the layer says.
- **The page keeps its tab names.** Actors, Classes, Items, Weapons and
  Skills stay where they are; "Enemies" stays "Bestiary".

## Current shape (verified)

- `studio/DatabasePanel.cpp` (267 lines) builds the seven tabs. Six of
  them come from `createCategoryView(kind, fields)`: a `QListWidget`, a
  `QFormLayout` of `QLineEdit`s that are never filled, and no connect
  for the selection. The list is filled once, at construction, and never
  again.
- The shipped records' fields (measured):
  - `actors`: name, char, race, class, level, hp, mp, str, def, mag,
    spd, skill, weapon, and a nested `growth` of six numbers.
  - `classes`: name, move.
  - `items`: name, type, value, price.
  - `weapons`: name, kind, might, reach, arc, hit, a fractional `cd`, and
    a `range` of two numbers.
  - `skills`: name, kind, mp, a fractional `power`, radius.
- The Bestiary's layer (`studio/bestiary/BestiaryData.*`) is the model to
  follow: a field table, the defaults, and a write that carries the
  unknown fields through.

## Hazards found

- **The fields are not uniform.** A form built from a fixed list of
  fields drops whatever it does not show. The growth, the cooldown and
  the range are exactly the fields a fixed form loses.
- **Three kinds of number.** The counts, the fractions and the nested
  growth are three different widgets, and a record can hold all three.
- **The stubs look editable.** A list and a form that never fills are
  worse than an empty page: the person types into a field and the
  record does not move.

## Target layout

```
studio/bestiary/RecordFields.{hpp,cpp}   the field tables per kind (new)
studio/bestiary/RecordEditor.{hpp,cpp}   the generic editor (new)
studio/DatabasePanel.{hpp,cpp}           the page, with the generic editor
tests/test_record_fields.cpp             the tables and the write (new)
```

The record layer lives beside the Bestiary's: it is the same job, one
level up.

## Phases

### P0 Safety net

- [x] P0.1 The audit: what the seven tabs do, the fields each kind
  holds, and the three kinds of number. Above.

Acceptance: the audit names the file and the fields, and the plan is
committed on its own.

### P1 The field tables

- [x] P1.1 The tables: the fields of each kind, with their labels and
  their kinds (text, number, fraction, a nested group of numbers, a pair
  of numbers).
- [x] P1.2 The layer: the defaults for a new record, the read of a
  record into the fields, and the write that carries the fields the
  editor does not show through untouched.
- [x] P1.3 The test: a new record has every field, an edit keeps the
  fields the editor does not show, and the numbers are clamped.

Acceptance: ctest green; the layer needs no widget and the test says so.

### P2 The editor

- [x] P2.1 The generic editor: the list of the records, the form, and
  the portrait-free layout the Bestiary uses.
- [x] P2.2 The five tabs use it, and the page's own refresh follows the
  project's records.
- [x] P2.3 Add and remove, with the ids the project uses.

Acceptance: a record of any kind can be edited and comes back after a
reload.

### P3 The proof

- [x] P3.1 The Database page's tabs are captures of their own, and the
  capture shows a filled form.
- [x] P3.2 `PROJECT.md`, `WANTED.md` and `Bible/Operations/NOTES.md` carry
  the state, and the defect is closed.

Acceptance: the PNG is opened and shows the editor; the gate is clean.

## Acceptance

- Every tab of the Database page edits its records: the fields are
  filled, a change is written, and the record survives a reload.
- The fields the editor does not show are still there after an edit.
- Every rule above is pinned by a test that needs no widget.
- The capture is opened, and the gate is clean.

## Rollback

Phase commits; revert to the tag `db-editors-p2` for the editor and
`db-editors-p1` for the layer.

## Decisions and deviations

- **One editor, many kinds.** The kinds differ only in their fields, so
  the editor is generic and the fields are a table. Five copies of the
  same form would be five places to fix the same bug.
- **The field tables sit beside the Bestiary's layer.** The Bestiary
  already carries the rule "a field the editor does not show is carried
  through", and the two layers should not drift apart.
- **The record layer is Qt-free.** The tables and the write are plain
  functions over `Json`; the widget is the drawing.
- **A decimal is read with a comma as well as a point.** A locale that
  writes "0,5" must not be read as five, so the layer's parse takes
  both.
