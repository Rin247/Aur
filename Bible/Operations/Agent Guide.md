# Agent Guide — Aur

> Guidelines for AI agents working within the Aur repository.

1. **Scope & Architecture**:
   - Aur's active product is the C++20 + Qt 6.12 Studio and engine. Godot references in older design documents are legacy context only; do not plan a Godot port or let it redirect current C++/Qt work.
   - The engine lives in `engine/aur/`, uses the `aur` namespace, and builds as `aur_engine`. It must **never** include Qt headers; it relies solely on the abstract `Renderer` interface so it remains testable headless (`NullRenderer`).
2. **Design Bibles**:
   - Start at [`Bible/README.md`](../README.md) for the documentation index and source precedence.
   - The user's authoritative design source is `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt`. Focused documents under `Bible/World/`, `Bible/Game Design/`, `Bible/Systems/`, `Bible/Characters/`, `Bible/Time and Puzzles/`, and `Bible/Production/` organize its material; assistant proposals in the source are not binding unless confirmed.
   - `Bible/Archive/Legacy/` contains historical material and must not override current user-confirmed direction.
3. **Tooling & Safety**:
   - PowerShell is permitted across the full workspace, all projects, and the wider project estate for inspection, search, builds, tests, and carefully scoped file operations.
   - Be cautious with mass edits: broad replacements and bulk line rewriting can eat or corrupt lines. Prefer precise, per-file changes; inspect the affected files and review the diff immediately afterward. Never overwrite unrelated work.
   - Run builds and tests with PowerShell (`cmake --build build`, `ctest`) or the project's documented equivalent.
4. **Testing**:
   - Ensure all changes compile cleanly with `-Wall -Wextra` and that `ctest` remains 100% green.
