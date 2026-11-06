#pragma once
#include <array>
#include <string_view>

namespace aur::world {

enum class WorldTool {
    Inspect = 0,
    PaintTerrain,
    PaintBiome,
    PlaceLocation,
    PlaceExit,
    RaiseHeight,
    LowerHeight,
};

inline constexpr std::array<std::string_view, 7> kWorldToolNames{
    "Inspect", "Paint terrain", "Paint biome",
    "Place location", "Place exit", "Raise height", "Lower height"};

inline constexpr int kWorldToolCount = static_cast<int>(kWorldToolNames.size());

inline constexpr int world_tool_index(WorldTool tool) {
    return static_cast<int>(tool);
}

inline constexpr bool is_entity_placement(WorldTool tool) {
    return tool == WorldTool::PlaceLocation || tool == WorldTool::PlaceExit;
}

inline constexpr bool is_height_tool(WorldTool tool) {
    return tool == WorldTool::RaiseHeight || tool == WorldTool::LowerHeight;
}

inline constexpr bool is_paint_tool(WorldTool tool) {
    return tool == WorldTool::PaintTerrain || tool == WorldTool::PaintBiome;
}

}
