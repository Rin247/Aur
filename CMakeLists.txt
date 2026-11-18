cmake_minimum_required(VERSION 3.21)
project(Aur VERSION 1.0.0 LANGUAGES CXX)

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
    engine/aur/movement.cpp
    engine/aur/movement.hpp
    engine/aur/camera.cpp
    engine/aur/camera.hpp
    engine/aur/clock.cpp
    engine/aur/forge/AssetGenerators.cpp
    engine/aur/forge/CharacterRenderer.cpp
    engine/aur/forge/SpeciesCatalog.cpp
    engine/aur/mapforge.cpp
    engine/aur/mapforge.hpp
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
    studio/playtest/QtRenderer.cpp
    studio/playtest/PlaytestDebugDialog.cpp
    studio/music/SharedAudio.cpp
    studio/core/AurPaths.cpp
    studio/core/AurSettings.cpp
    studio/core/AurSettings.hpp
    studio/music/QtAudioOutput.cpp
    studio/music/MasterVolumeStrip.cpp
    studio/theme/CardFrame.cpp
)
target_include_directories(aur_game PRIVATE . studio)
target_link_libraries(aur_game PRIVATE aur_engine Qt6::Widgets Qt6::Multimedia)

# Local stdio MCP server for project-aware design tools.
add_executable(aur_mcp tools/mcp_main.cpp)
target_link_libraries(aur_mcp PRIVATE aur_engine)

# Aur executable (IDE / Editors)
add_library(aur_editor_common STATIC
    studio/music/SongHistory.cpp
)
target_include_directories(aur_editor_common PUBLIC studio)
target_link_libraries(aur_editor_common PUBLIC aur_engine)

add_executable(aur_studio
    studio/main.cpp
    studio/MainWindow.cpp
    studio/theme/StudioChrome.cpp
    studio/theme/StudioChrome.hpp
    studio/theme/FramelessWindow.cpp
    studio/theme/FramelessWindow.hpp
    studio/core/AurPaths.cpp
    studio/core/AurSettings.cpp
    studio/core/AurSettings.hpp
    studio/GameView.cpp
    studio/playtest/QtRenderer.cpp
    studio/playtest/PlaytestDebugDialog.cpp
    studio/music/SharedAudio.cpp
    studio/music/SharedAudio.hpp
    studio/music/MidiDeviceIo.cpp
    studio/music/MidiDeviceIo.hpp
    studio/music/MidiSession.cpp
    studio/music/MidiSession.hpp
    studio/music/PianoRollCanvas.cpp
    studio/music/PianoRollInput.cpp
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
    studio/music/MasterVolumeStrip.cpp
    studio/music/MasterVolumeStrip.hpp
    studio/music/SongTransfer.cpp
    studio/music/SongTransfer.hpp
    studio/music/MidiStrip.cpp
    studio/music/MidiStrip.hpp
    studio/music/TransportIcon.cpp
    studio/music/TransportIcon.hpp
    studio/music/SongHistory.cpp
    studio/MapEditorPanel.cpp
    studio/map/MapCanvas.cpp
    studio/map/MapCanvas.hpp
    studio/map/MapEditUndo.cpp
    studio/core/UndoCommands.cpp
    studio/DatabasePanel.cpp
    studio/bestiary/BestiaryData.cpp
    studio/bestiary/BestiaryData.hpp
    studio/bestiary/RecordFields.cpp
    studio/bestiary/RecordFields.hpp
    studio/bestiary/RecordEditor.cpp
    studio/bestiary/RecordEditor.hpp
    studio/bestiary/BestiaryEditor.cpp
    studio/bestiary/BestiaryEditor.hpp
    studio/bestiary/CreaturePortrait.cpp
    studio/bestiary/CreaturePortrait.hpp
    studio/CreatureForgePanel.cpp
    studio/forge/CharacterSheetWidget.cpp
    studio/forge/CharacterSheetWidget.hpp
    studio/forge/ForgeAssetOps.cpp
    studio/forge/ForgePanelTabs.cpp
    studio/forge/ForgeColorSwatches.cpp
    studio/MusicPanel.cpp
    studio/music/MusicPanelSession.cpp
    studio/WorldPanel.cpp
    studio/world/WorldPanelActions.cpp
    studio/world/HdSurface.cpp
    studio/world/TerrainLod.cpp
    studio/world/TerrainLod.hpp
    studio/world/WorldTerrainPaint.cpp
    studio/world/WorldTerrainPaint.hpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/TerrainRhiWidget.cpp
    studio/world/TimeWeatherBar.cpp
    studio/world/TimeWeatherBar.hpp
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
    studio/world/WorldMapCanvasInput.cpp
    studio/world/WorldModel.cpp
    studio/world/WorldModel.hpp
    studio/core/RhiBackend.cpp
    studio/core/RhiBackend.hpp
    studio/SpriteEditorPanel.cpp
    studio/pixel/PixelEditSession.cpp
    studio/pixel/PixelFrameOps.cpp
    studio/pixel/PixelSurface.cpp
    studio/pixel/PixelSurface.hpp
    studio/pixel/SpriteCanvas.cpp
    studio/pixel/SpriteCanvas.hpp
    studio/pixel/PixelEditSession.hpp
    studio/map/TilePaletteWidget.cpp
    studio/map/TilePaletteWidget.hpp
    studio/EventsPanel.cpp
    studio/events/EventCommandOps.cpp
    studio/TilesetEditorPanel.cpp
    studio/tileset/TilePngIo.cpp
    studio/world/WorldSettingsPanel.cpp
    studio/world/WorldSettingsPanel.hpp
    studio/theme/ThemeMath.cpp
    studio/theme/ThemePalettes.cpp
    studio/theme/ThemeQt.cpp
    studio/theme/ThemeManager.cpp
    studio/theme/ThemeManager.hpp
    studio/theme/AurStyle.cpp
    studio/theme/AurStylePaint.cpp
    studio/theme/ThemeIcons.cpp
    studio/theme/AppearanceDialog.cpp
    studio/theme/AppearanceDialog.hpp
    studio/theme/ResponsiveSplitter.cpp
    studio/theme/CardFrame.cpp
    studio/music/ClipButton.cpp
)
target_include_directories(aur_studio PRIVATE . studio)
target_link_libraries(aur_studio PRIVATE aur_engine Qt6::Widgets Qt6::Multimedia Qt6::GuiPrivate Qt6::Concurrent)
# The app's own icon from AssetLibrary/Icons/Aur (qrc PNGs + Windows .rc/.ico).
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
set_target_properties(aur_studio PROPERTIES OUTPUT_NAME "Aur")

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
    tests/test_bestiary_data.cpp
    tests/test_movement.cpp
    tests/test_record_fields.cpp
    tests/test_mapforge.cpp
    tests/test_terrain_lod.cpp
    tests/test_weather_render.cpp
    tests/test_step_sequencer.cpp
    tests/test_arpeggiator.cpp
    tests/test_theme.cpp
    tests/test_settings.cpp
    studio/core/AurPaths.cpp
    studio/core/AurSettings.cpp
    studio/core/AurSettings.hpp
    studio/pixel/PixelEditSession.cpp
    studio/bestiary/BestiaryData.cpp
    studio/bestiary/RecordFields.cpp
    studio/pixel/PixelFrameOps.cpp
    studio/core/UndoCommands.cpp
    studio/map/MapEditUndo.cpp
    studio/world/WeatherRender.cpp
    studio/world/WeatherPreviewField.cpp
    studio/music/SongHistory.cpp
    studio/music/ClipLauncherStates.cpp
    studio/world/HdSurface.cpp
    studio/world/TerrainLod.cpp
    studio/world/WorldMapHelpers.cpp
    studio/world/MapView.cpp
    studio/world/WorldCommandHistory.cpp
    studio/world/WorldStrokeState.cpp
    studio/world/WorldModel.cpp
    studio/theme/ThemeMath.cpp
    studio/theme/ThemePalettes.cpp
    studio/theme/ThemeQt.cpp
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

if(WIN32)
    add_custom_command(TARGET aur_studio PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -D EXE_NAME=$<TARGET_FILE_NAME:aur_studio>
            -P ${CMAKE_SOURCE_DIR}/cmake/KillRunningExeForBuild.cmake
        COMMENT "Stopping running aur_studio if open, so the link can replace it"
        VERBATIM
    )
    add_custom_command(TARGET aur_game PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -D EXE_NAME=$<TARGET_FILE_NAME:aur_game>
            -P ${CMAKE_SOURCE_DIR}/cmake/KillRunningExeForBuild.cmake
        COMMENT "Stopping running aur_game if open, so the link can replace it"
        VERBATIM
    )
endif()
