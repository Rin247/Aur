# Accessibility and Quality of Life

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 266-291). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 11. ACCESSIBILITY & QoL

- **Pause & Save Anywhere.** No exceptions. Combat, cutscenes, dialogue—all pausable.
- **UI Scaling:** Slider from 0.75x to 2.0x. Steam Deck native support.
- **Colorblind Modes:** Protanopia, Deuteranopia, Tritanopia shaders. Toggleable.
- **Controller Navigation:** Full focus-mode support. D-pad/stick navigates all menus.
- **Subtitle Options:** Size, background opacity, speaker nameplates.
- **Combat Speed:** 1x, 1.5x, 2x toggle for turn-based and tactical.
- **Clock Accessibility:** Optional "Extended Day" mode (96-minute cycle) for players who want slower pacing.
- **No Motion Sickness Triggers:** Camera swoops have adjustable speed. Screen shake toggleable.

---

## 12. MODDING PHILOSOPHY

**Core Rule:** Modders need a text editor and a folder. No Godot. No compiling.

- All game data (items, enemies, dialogue, quests) stored in external **JSON/CSV**.
- `user://mods/` folder scanned on boot. Valid `manifest.json` = mod loads.
- Dialogic timelines export as `.dtl` (JSON). Drop them in a mod folder.
- Steam Workshop integration via GodotSteam UGC.
- Mod-safe variables documented. Anti-tamper variables are **locked**.
- Community can create: balance patches, new items, custom dialogue, scavenger hunts, joke endings.

---

