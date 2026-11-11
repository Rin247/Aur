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
    engine/aur/audio/SynthCores.cpp
    engine/aur/audio/RealtimeMixer.cpp
    engine/aur/audio/SongDocument.cpp
    engine/aur/audio/OfflineRenderJob.cpp
    engine/aur/audio/YmFmCore.cpp
    engine/aur/audio/YmFmParams.cpp
    engine/aur/audio/YmFmPatches.cpp
    engine/aur/audio/SnesPcmCore.cpp
    engine/aur/audio/SnesSamples.cpp
    engine/aur/audio/BrrCodec.cpp
    engine/aur/audio/SnesEcho.cpp
    engine/aur/audio/FxRack.cpp
    engine/aur/audio/FxChorusDelay.cpp
    engine/aur/audio/FxReverb.cpp
    engine/aur/audio/EffectEq.cpp
    engine/aur/audio/EffectDrive.cpp
    engine/aur/audio/EffectPhaser.cpp
    engine/aur/audio/EffectEchoFir.cpp
    engine/aur/audio/EffectRegistry.cpp
    engine/aur/audio/EffectChain.cpp
    engine/aur/audio/StepSequencer.cpp
    engine/aur/audio/Arpeggiator.cpp
    engine/aur/audio/Sf2Core.cpp
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
    studio/music/ArrangementCanvas.cpp
    studio/music/ArrangementCanvas.hpp
    studio/music/ScaleSelector.cpp
    studio/music/ScaleSelector.hpp
    studio/music/ClipLauncherStates.cpp
    studio/music/ClipLauncherStates.hpp
    studio/music/SongLibrary.cpp
    studio/music/SongLibrary.hpp
    studio/music/QtAudioOutput.cpp
    studio/music/QtAudioOutput.hpp
    studio/music/FxChainStrip.cpp
    studio/music/FxChainStrip.hpp
    studio/music/StepSequencerCanvas.cpp
    studio/music/StepSequencerCanvas.hpp
    studio/music/GrooveBoxPanel.cpp
    studio/music/GrooveBoxPanel.hpp
    studio/SongHistory.cpp
    studio/MapEditorPanel.cpp
    studio/UndoCommands.cpp
    studio/DatabasePanel.cpp
    studio/ForgePanel.cpp
    studio/MusicPanel.cpp
    studio/WorldPanel.cpp
    studio/world/HdSurface.cpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/TerrainRhiWidget.cpp
    studio/world/WorldWeatherOverlay.cpp
    studio/world/WeatherRender.cpp
    studio/world/WeatherPreviewField.cpp
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
    studio/PixelEditSession.cpp
    studio/PixelEditSession.hpp
    studio/TilePaletteWidget.cpp
    studio/TilePaletteWidget.hpp
    studio/EventPanel.cpp
    studio/TilesetEditorPanel.cpp
    studio/WorldSettingsPanel.cpp
)
target_include_directories(aur_studio PRIVATE . studio)
target_link_libraries(aur_studio PRIVATE aur_engine Qt6::Widgets Qt6::Multimedia Qt6::GuiPrivate Qt6::Concurrent)
# The app's own icon, from the estate's M3 set (AssetLibrary/Icons/Aur).
# The PNGs are the window icon (":/icons/aur-icon-256.png"); the .rc carries
# the .ico for the PE resource section, which is what Explorer draws. Both
# are listed as sources so CMake compiles them with the right tool.
qt_add_resources(aur_studio aur_studio_icons
    PREFIX "/"
    BASE studio/resources
    FILES
        studio/resources/icons/aur-icon-16.png
        studio/resources/icons/aur-icon-24.png
        studio/resources/icons/aur-icon-32.png
        studio/resources/icons/aur-icon-48.png
        studio/resources/icons/aur-icon-64.png
        studio/resources/icons/aur-icon-128.png
        studio/resources/icons/aur-icon-256.png
)
if(WIN32)
    target_sources(aur_studio PRIVATE studio/resources/aur.rc)
endif()
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
    tests/test_synth_pitch.cpp
    tests/test_fx_chain.cpp
    tests/test_realtime_parity.cpp
    tests/test_offline_render.cpp
    tests/test_song_v1.cpp
    tests/test_song_v2.cpp
    tests/test_song_history.cpp
    tests/test_song_clips.cpp
    tests/test_automation_lanes.cpp
    tests/test_clip_launcher.cpp
    tests/test_arrangement.cpp
    tests/test_note_index.cpp
    tests/test_undo_commands.cpp
    tests/Sf2Fixture.cpp
    tests/test_sf2.cpp
    tests/test_pixel_session.cpp
    tests/test_weather_render.cpp
    tests/test_step_sequencer.cpp
    tests/test_arpeggiator.cpp
    studio/PixelEditSession.cpp
    studio/UndoCommands.cpp
    studio/world/WeatherRender.cpp
    studio/world/WeatherPreviewField.cpp
    studio/SongHistory.cpp
    studio/music/ClipLauncherStates.cpp
    studio/world/HdSurface.cpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/WorldCommandHistory.cpp
    studio/world/WorldStrokeState.cpp
    studio/WorldModel.cpp
)
target_include_directories(aur_tests PRIVATE studio)
target_link_libraries(aur_tests PRIVATE aur_engine Qt6::Gui Qt6::Widgets)
add_test(NAME EngineTests COMMAND aur_tests)
add_test(NAME TopologyTests COMMAND aur_tests --topology)
add_test(NAME WorldSimTests COMMAND aur_tests --worldsim)

# Synth-core suites: standalone mains that replace global operator new to
# count allocations so they cannot link into aur_tests.
add_executable(synth_fm_tests tests/test_synth_fm.cpp)
target_link_libraries(synth_fm_tests PRIVATE aur_engine)
add_test(NAME SynthFmTests COMMAND synth_fm_tests)
add_executable(synth_spc_tests tests/test_synth_spc.cpp)
target_link_libraries(synth_spc_tests PRIVATE aur_engine)
add_test(NAME SynthSpcTests COMMAND synth_spc_tests)
add_executable(synth_fx_tests tests/test_fx_rack.cpp)
target_link_libraries(synth_fx_tests PRIVATE aur_engine)
add_test(NAME SynthFxTests COMMAND synth_fx_tests)
add_executable(fx_effect_tests tests/test_fx_effects.cpp)
target_link_libraries(fx_effect_tests PRIVATE aur_engine)
add_test(NAME FxEffectTests COMMAND fx_effect_tests)
