# Operations Notes
## Project tidy audit (2026-11-06)

- Moved the original Sea of Stars map-size conversation into `Bible/Archive/Reference/`; updated its topology-handover link. The Bible root now contains only its index.
- Removed the empty duplicate `Bible/Puzzle & Time/` directory; `Bible/Time and Puzzles/` is the populated topic directory.
- Confirmed tests are grouped under root `tests/`; no `work/` or `imagegen/` directories exist in the inspected project tree.
- Kept ignored `build/` output in place; it contains active build products and diagnostic images/logs and is not source content.
- Tidy ask #50 remains open: the broader Operations/root audit is incomplete. The root handover still carries the active graphics-backend policy and should not be dropped as stale.


## WorldPanel refactor audit (2026-11-06)

- `WorldPanel.cpp` starts at 3,100 lines; the header has a duplicate `<memory>` include and relied on transitive Qt widget includes. Header cleanup now keeps value-type Qt includes, forward-declares pointer/member-event types, and includes the standard containers/types it names.
- Baseline `cmake --build build/Release --parallel 8` and `ctest --test-dir build/Release --output-on-failure` passed before edits. Use `cmd /c .\\build.bat` under this shell to load Visual Studio's compiler environment; direct cmake build fails because the shell lacks MSVC standard include paths.
- After the header cleanup, the project build exposed missing direct implementation includes (`QPainter`, widget definitions, and `QFutureWatcher`); adding those to `WorldPanel.cpp` restores a successful full build.
- Existing `studio/world/MapView.*` / `WorldMapHelpers.*` files were already untracked at task start, so keep them out of unrelated commits while integrating them intentionally in the refactor.
- Runtime `WeatherSystem` is not a direct replacement for the editor's deterministic `climate_weather(project, climate, roll)` preview selection; preserve the editor behavior unless a matching shared selector is added and tested.
- The persistent plan is `Bible/Operations/WorldPanel Refactor Plan.md`; the first implementation checkpoint is header/include hygiene. The required `buildHdSurface` checksum still needs a testable Qt image-builder seam before completing step 1.
