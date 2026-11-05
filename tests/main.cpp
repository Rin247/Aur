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

int main(int argc, char** argv) {
    // Allow a focused topology run without the memory-heavy full generator sweep.
    const bool focused_topology = argc > 1 && std::string(argv[1]) == "--topology";
    std::cout << (focused_topology ? "=== Running Aur Topology Tests ===\n"
                                   : "=== Running Aur Engine Tests ===\n");
    if (focused_topology) {
        RUN_TEST(test_autotile_basic);
        RUN_TEST(test_game_updates_field_scene);
        RUN_TEST(test_topology_paint_boundaries);
        RUN_TEST(test_topology_field_climate_and_camera);
        RUN_TEST(test_world_model_wrap_edits);
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

    std::cout << "\nResults: " << g_tests_passed << " passed, " << g_tests_failed << " failed.\n";
    return g_tests_failed > 0 ? 1 : 0;
}


