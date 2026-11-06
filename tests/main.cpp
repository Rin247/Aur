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

    std::cout << "\nResults: " << g_tests_passed << " passed, " << g_tests_failed << " failed.\n";
    return g_tests_failed > 0 ? 1 : 0;
}


