# AGENTS.md — Aur

> Guidelines for AI agents working within the Aur repository.

1. **Scope & Architecture**:
   - Aur's active product is the C++20 + Qt 6.12 Studio and engine. Godot references in older design documents are legacy context only; do not plan a Godot port or let it redirect current C++/Qt work.
   - The engine (`mana_engine`) must **never** include Qt headers. It relies solely on the abstract `Renderer` interface so it remains testable headless (`NullRenderer`).
2. **Design Bibles**:
   - Start at [`Bible/README.md`](../README.md) for the documentation index and source precedence.
   - The user's authoritative design source is `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt`. Focused documents under `Bible/World/`, `Bible/Game Design/`, `Bible/Systems/`, `Bible/Characters/`, `Bible/Time and Puzzles/`, and `Bible/Production/` organize its material; assistant proposals in the source are not binding unless confirmed.
   - `Bible/Archive/Legacy/` contains historical material and must not override current user-confirmed direction.
3. **Tooling & Safety**:
   - Use built-in IDE tools (`read_file`, `replace_file_content`, `grep`, `find_files`) rather than shell-based file edits.
   - Run builds and tests via `run_shell_command` with PowerShell syntax (`cmake --build build`, `ctest`).
4. **Testing**:
   - Ensure all changes compile cleanly with `-Wall -Wextra` and that `ctest` remains 100% green.
