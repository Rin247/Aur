cmake_minimum_required(VERSION 3.21)
project(AurStudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep build products and deployed Qt runtime files together per preset.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets Multimedia ShaderTools GuiPrivate Concurrent)
qt_standard_project_setup(REQUIRES 6.12)

# Engine static library (zero Qt dependency, std only)
add_library(aur_engine STATIC
    engine/aur/gfx.cpp
    engine/aur/project.cpp
    engine/aur/autotile.cpp
    engine/aur/clock.cpp
    engine/aur/forge.cpp
    engine/aur/worldgen.cpp
    engine/aur/script.cpp
    engine/aur/game.cpp
    engine/aur/mcp.cpp
    engine/aur/audio/midi.cpp
    engine/aur/audio/synth.cpp
    engine/aur/audio/VoiceCore.cpp
    engine/aur/audio/RealtimeMixer.cpp
    engine/aur/audio/SongDocument.cpp
    engine/aur/audio/OfflineRenderJob.cpp
)
target_include_directories(aur_engine PUBLIC engine)

# Game runtime executable (Qt window player)
add_executable(aur_game
    game/main.cpp
    studio/GameView.cpp
)
target_include_directories(aur_game PRIVATE . studio)
target_link_libraries(aur_game PRIVATE aur_engine Qt6::Widgets Qt6::Multimedia)

# Local stdio MCP server for project-aware design tools.
add_executable(aur_mcp tools/mcp_main.cpp)
target_link_libraries(aur_mcp PRIVATE aur_engine)

# Aur Studio executable (IDE / Editors)
add_library(aur_editor_common STATIC
    studio/SongHistory.cpp
)
target_include_directories(aur_editor_common PUBLIC studio)
target_link_libraries(aur_editor_common PUBLIC aur_engine)

add_executable(aur_studio
    studio/main.cpp
    studio/MainWindow.cpp
    studio/GameView.cpp
    studio/MidiDeviceIo.cpp
    studio/MidiDeviceIo.hpp
    studio/music/MidiSession.cpp
    studio/music/MidiSession.hpp
    studio/music/PianoRollCanvas.cpp
    studio/music/PianoRollCanvas.hpp
    studio/music/VelocityLaneStrip.cpp
    studio/music/VelocityLaneStrip.hpp
    studio/music/SongLibrary.cpp
    studio/music/SongLibrary.hpp
    studio/music/QtAudioOutput.cpp
    studio/music/QtAudioOutput.hpp
    studio/SongHistory.cpp
    studio/MapEditorPanel.cpp
    studio/DatabasePanel.cpp
    studio/ForgePanel.cpp
    studio/MusicPanel.cpp
    studio/WorldPanel.cpp
    studio/world/HdSurface.cpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/TerrainRhiWidget.cpp
    studio/world/WorldWeatherOverlay.cpp
    studio/world/WorldClimateOverlays.cpp
    studio/world/WorldMarkerOverlays.cpp
    studio/world/WorldCommandHistory.cpp
    studio/world/WorldStrokeState.cpp
    studio/world/WorldGenerationJob.cpp
    studio/world/WorldEntityDialogs.cpp
    studio/world/WorldMapCanvas.cpp
    studio/WorldModel.cpp
    studio/WorldModel.hpp
    studio/RhiBackend.cpp
    studio/RhiBackend.hpp
    studio/PixelEditorPanel.cpp
    studio/EventPanel.cpp
    studio/TilesetEditorPanel.cpp
    studio/WorldSettingsPanel.cpp
)
target_include_directories(aur_studio PRIVATE . studio)
target_link_libraries(aur_studio PRIVATE aur_engine Qt6::Widgets Qt6::Multimedia Qt6::GuiPrivate Qt6::Concurrent)
qt_add_shaders(aur_studio aur_studio_sculpt_shaders
    PREFIX "/aur/shaders"
    BASE studio/shaders
    NOGLSL
    NOMSL
    HLSL 50
    PRECOMPILE
    FILES
        studio/shaders/world_sculpt.vert
        studio/shaders/world_sculpt.frag
)
if(WIN32)
    target_link_libraries(aur_studio PRIVATE winmm)
endif()
set_target_properties(aur_studio PROPERTIES OUTPUT_NAME "Aur Studio")

# Tests
enable_testing()
add_executable(aur_tests
    tests/main.cpp
    tests/test_json.cpp
    tests/test_autotile.cpp
    tests/test_mcp.cpp
    tests/test_forge.cpp
    tests/test_clock.cpp
    tests/test_game.cpp
    tests/test_topology_boundaries.cpp
    tests/test_midi.cpp
    tests/test_world_model.cpp
    tests/test_world_surface.cpp
    tests/test_map_view.cpp
    engine/aur/worldsim.cpp
    tests/test_world_command_history.cpp
    tests/test_worldsim.cpp
    tests/test_song_clamp.cpp
    tests/test_synth_golden.cpp
    tests/test_realtime_parity.cpp
    tests/test_offline_render.cpp
    tests/test_song_v1.cpp
    tests/test_song_v2.cpp
    tests/test_song_history.cpp
    tests/test_song_clips.cpp
    tests/test_automation_lanes.cpp
    studio/SongHistory.cpp
    studio/world/HdSurface.cpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/WorldCommandHistory.cpp
    studio/world/WorldStrokeState.cpp
    studio/WorldModel.cpp
)
target_include_directories(aur_tests PRIVATE studio)
target_link_libraries(aur_tests PRIVATE aur_engine Qt6::Gui)
add_test(NAME EngineTests COMMAND aur_tests)
add_test(NAME TopologyTests COMMAND aur_tests --topology)
add_test(NAME WorldSimTests COMMAND aur_tests --worldsim)
