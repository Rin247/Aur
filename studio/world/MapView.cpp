#include "MapView.hpp"
#include "WorldMapHelpers.hpp"
#include "aur/world_topology.hpp"

namespace aur::world
{

    std::optional<MapView> MapView::current(Project *project, const std::string &map_id)
    {
        if (!project || map_id.empty() || project->maps.find(map_id) == project->maps.end())
            return std::nullopt;
        return MapView(project, &project->maps[map_id]);
    }

    int MapView::width() const
    {
        return (*map_)["w"].as_int();
    }

    int MapView::height() const
    {
        return (*map_)["h"].as_int();
    }

    std::string MapView::tileset() const
    {
        return (*map_)["tileset"].as_string();
    }

    bool MapView::wrapsX() const
    {
        return aur::wraps_world_x(*map_);
    }

    bool MapView::hasHdSurface() const
    {
        return has_hd_surface(*map_);
    }

    int MapView::seaLevel() const
    {
        if (!map_->contains("worldgen")) return 500;
        const Json& worldgen = (*map_)["worldgen"];
        return worldgen.contains("sea_level") ? worldgen["sea_level"].as_int() : 500;
    }

    int MapView::seed() const
    {
        if (map_->contains("worldgen")) {
            const Json& worldgen = (*map_)["worldgen"];
            if (worldgen.contains("seed")) return worldgen["seed"].as_int();
        }
        return map_->contains("seed") ? (*map_)["seed"].as_int() : 1;
    }

    int MapView::axialTilt() const
    {
        if (!map_->contains("worldgen")) return 0;
        const Json& worldgen = (*map_)["worldgen"];
        return worldgen.contains("axial_tilt") ? worldgen["axial_tilt"].as_int() : 0;
    }

    JsonArray *MapView::entities()
    {
        if (!map_->contains("entities")) return nullptr;
        Json& entities = (*map_)["entities"];
        if (!entities.is_array()) return nullptr;
        return &entities.as_array();
    }

    const JsonArray *MapView::entities() const
    {
        if (!map_->contains("entities")) return nullptr;
        const Json& entities = (*map_)["entities"];
        if (!entities.is_array()) return nullptr;
        return &entities.as_array();
    }

    JsonArray *MapView::layer(const char *name)
    {
        if (!map_->contains("layers")) return nullptr;
        Json &layers = (*map_)["layers"];
        if (!layers.is_object() || !layers.contains(name) || !layers[name].is_array())
            return nullptr;
        return &layers[name].as_array();
    }

    const JsonArray *MapView::layer(const char *name) const
    {
        if (!map_->contains("layers")) return nullptr;
        const Json &layers = (*map_)["layers"];
        if (!layers.is_object() || !layers.contains(name) || !layers[name].is_array())
            return nullptr;
        return &layers[name].as_array();
    }

} // namespace aur::world

namespace aur::world {

int MapView::elevationAt(int x, int y, int fallback) const
{
    const JsonArray* rows = layer("elevation");
    if (!rows || y < 0 || y >= static_cast<int>(rows->size())) return fallback;
    const Json& row = (*rows)[static_cast<std::size_t>(y)];
    if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return fallback;
    return row[static_cast<std::size_t>(x)].as_int();
}

std::string MapView::biomeAt(int x, int y) const
{
    const JsonArray* rows = layer("biomes");
    if (!rows || y < 0 || y >= static_cast<int>(rows->size())) return {};
    const Json& row = (*rows)[static_cast<std::size_t>(y)];
    if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return {};
    return row[static_cast<std::size_t>(x)].as_string();
}

}
