#include "mini_test.hpp"

void test_json_basic();
void test_json_roundtrip();
void test_autotile_basic();
void test_mcp_server();
void test_forge_generators();
void test_project_calendar_and_weather();
void test_game_updates_field_scene();
void test_topology_paint_boundaries();
void test_topology_field_climate_and_camera();
void test_midi_import_export();
void test_world_model_rect_edits();
void test_world_model_wrap_edits();
void test_world_surface_checksum();
void test_map_view_layers();
void test_command_history_roundtrip();
void test_command_history_limit();
void test_stroke_line_tiles();
void test_camera_repeat_view();
void test_worldsim_shell();
void test_worldsim_pins_and_values();
void test_worldsim_json_roundtrip();
void test_worldsim_kernel_determinism();
void test_worldsim_pin_immutable_through_kernels();
void test_worldsim_wrap_continuity();
void test_worldsim_golden_hash();
void test_worldsim_save_load_roundtrip();
void test_worldsim_tick_cost();
void test_song_clamp_regression();
void test_synth_golden_hash();
void test_synth_pitch_all_programs();
void test_synth_pitch_follows_note();
void test_fx_chain_absent_key_unchanged();
void test_fx_chain_empty_is_noop();
void test_fx_chain_changes_audio_and_skips_unknown();
void test_fx_chain_bypass_slot_is_transparent();
void test_fx_chain_realtime_parity();
void test_realtime_parity_core_synths();
void test_offline_render_job();
void test_song_v1_contract();
void test_song_v2_migration();
void test_midi_fidelity();
void test_song_history();
void test_note_props();
void test_note_index();
void test_undo_commands();
void test_sf2();
void test_song_clips();
void test_automation_lanes();
void test_clip_launcher();
void test_arrangement();
void test_pixel_session();
void test_direction_token_match();
void test_sprite_anim_resolution();
void test_sprite_frame_mirror();
void test_sprite_frame_tools();
void test_sprite_png_snap();
void test_weather_render();
void test_weather_preview_field();
void test_step_sequencer();
void test_arpeggiator();

int main(int argc, char** argv) {
    // Allow a focused run without the memory-heavy full generator sweep.
    const bool focused_mode = argc > 1 && (std::string(argv[1]) == "--topology" ||
                                           std::string(argv[1]) == "--worldsim");
    const bool focused_worldsim = argc > 1 && std::string(argv[1]) == "--worldsim";
    std::cout << (focused_mode ? (focused_worldsim ? "=== Running Aur WorldSim Tests ===\n"
                                                   : "=== Running Aur Topology Tests ===\n")
                               : "=== Running Aur Engine Tests ===\n");
    if (focused_worldsim) {
        RUN_TEST(test_worldsim_shell);
        RUN_TEST(test_worldsim_pins_and_values);
        RUN_TEST(test_worldsim_json_roundtrip);
        RUN_TEST(test_worldsim_kernel_determinism);
        RUN_TEST(test_worldsim_pin_immutable_through_kernels);
        RUN_TEST(test_worldsim_wrap_continuity);
        RUN_TEST(test_worldsim_golden_hash);
        RUN_TEST(test_worldsim_save_load_roundtrip);
        RUN_TEST(test_worldsim_tick_cost);
        std::cout << "\nResults: " << g_tests_passed << " passed, "
                  << g_tests_failed << " failed.\n";
        return g_tests_failed > 0 ? 1 : 0;
    }
    if (!focused_worldsim && focused_mode) {
        RUN_TEST(test_autotile_basic);
        RUN_TEST(test_game_updates_field_scene);
        RUN_TEST(test_topology_paint_boundaries);
        RUN_TEST(test_topology_field_climate_and_camera);
        RUN_TEST(test_world_model_wrap_edits);
        RUN_TEST(test_world_surface_checksum);
        RUN_TEST(test_map_view_layers);
        RUN_TEST(test_command_history_roundtrip);
        std::cout << "\nResults: " << g_tests_passed << " passed, "
                  << g_tests_failed << " failed.\n";
        return g_tests_failed > 0 ? 1 : 0;
    }
    RUN_TEST(test_json_basic);
    RUN_TEST(test_json_roundtrip);
    RUN_TEST(test_autotile_basic);
    RUN_TEST(test_mcp_server);
    RUN_TEST(test_forge_generators);
    RUN_TEST(test_project_calendar_and_weather);
    RUN_TEST(test_game_updates_field_scene);
    RUN_TEST(test_topology_paint_boundaries);
    RUN_TEST(test_topology_field_climate_and_camera);
    RUN_TEST(test_midi_import_export);
    RUN_TEST(test_world_model_rect_edits);
    RUN_TEST(test_world_model_wrap_edits);
    RUN_TEST(test_world_surface_checksum);
    RUN_TEST(test_map_view_layers);
    RUN_TEST(test_command_history_roundtrip);
    RUN_TEST(test_command_history_limit);
    RUN_TEST(test_stroke_line_tiles);
    RUN_TEST(test_camera_repeat_view);
    RUN_TEST(test_worldsim_shell);
    RUN_TEST(test_worldsim_pins_and_values);
    RUN_TEST(test_worldsim_json_roundtrip);
    RUN_TEST(test_worldsim_kernel_determinism);
    RUN_TEST(test_worldsim_pin_immutable_through_kernels);
    RUN_TEST(test_worldsim_wrap_continuity);
    RUN_TEST(test_worldsim_golden_hash);
    RUN_TEST(test_worldsim_save_load_roundtrip);
    RUN_TEST(test_worldsim_tick_cost);
    RUN_TEST(test_song_clamp_regression);
    RUN_TEST(test_synth_golden_hash);
    RUN_TEST(test_synth_pitch_all_programs);
    RUN_TEST(test_synth_pitch_follows_note);
    RUN_TEST(test_fx_chain_absent_key_unchanged);
    RUN_TEST(test_fx_chain_empty_is_noop);
    RUN_TEST(test_fx_chain_changes_audio_and_skips_unknown);
    RUN_TEST(test_fx_chain_bypass_slot_is_transparent);
    RUN_TEST(test_fx_chain_realtime_parity);
    RUN_TEST(test_realtime_parity_core_synths);
    RUN_TEST(test_offline_render_job);
    RUN_TEST(test_song_v1_contract);
    RUN_TEST(test_song_v2_migration);
    RUN_TEST(test_midi_fidelity);
    RUN_TEST(test_song_history);
    RUN_TEST(test_note_props);
    RUN_TEST(test_note_index);
    RUN_TEST(test_undo_commands);
    RUN_TEST(test_sf2);
    RUN_TEST(test_song_clips);
    RUN_TEST(test_automation_lanes);
    RUN_TEST(test_clip_launcher);
    RUN_TEST(test_arrangement);
    RUN_TEST(test_pixel_session);
    RUN_TEST(test_direction_token_match);
    RUN_TEST(test_sprite_anim_resolution);
    RUN_TEST(test_sprite_frame_mirror);
    RUN_TEST(test_sprite_frame_tools);
    RUN_TEST(test_sprite_png_snap);
    RUN_TEST(test_weather_render);
    RUN_TEST(test_weather_preview_field);
    RUN_TEST(test_step_sequencer);
    RUN_TEST(test_arpeggiator);

    std::cout << "\nResults: " << g_tests_passed << " passed, " << g_tests_failed << " failed.\n";
    return g_tests_failed > 0 ? 1 : 0;
}


