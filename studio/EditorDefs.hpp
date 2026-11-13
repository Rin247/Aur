// EditorDefs.hpp - single definitions shared by the map, pixel and tileset
// editor panels: the entity-kind list, flag definitions and colors, and the
// palette swatch helper. Previously each panel held its own copy.
#pragma once

#include <QColor>
#include <array>
#include <string>
#include "aur/gfx.hpp"
#include "aur/project.hpp"

namespace aur::editor {

// Entity kinds in palette order; the map canvas indexes entities by this list.
inline constexpr std::array<const char*, 8> kEntityKinds{
    "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"};

// Flag colors used by both the tileset editor and the map collision overlay.
struct FlagStyle { int flag; const char* name; QColor color; };
inline constexpr std::array<FlagStyle, 6> kFlagStyles{{
    {SOLID, "Solid", QColor(229, 57, 53, 105)},
    {WATER, "Water", QColor(30, 136, 229, 115)},
    {HURT, "Hurt", QColor(239, 83, 80, 120)},
    {SLOW, "Slow", QColor(255, 202, 40, 100)},
    {ENCOUNTER, "Encounter", QColor(171, 71, 188, 95)},
    {ONE_WAY, "One-way", QColor(120, 200, 190, 110)},
}};

// Palette swatch color for a palette JSON string ("#rrggbbaa").
inline QColor palette_color(const Json& color) {
    const Rgb rgba = hex_to_rgba(color.as_string());
    return {rgba.r, rgba.g, rgba.b, rgba.a};
}

} // namespace aur::editor
