#pragma once
#include "aur/json.hpp"
#include "aur/project.hpp"

#include <optional>
#include <string>

namespace aur::world {

class MapView {
public:
    static std::optional<MapView> current(Project* project, const std::string& map_id);

    Json& json() const { return *map_; }
    int width() const;
    int height() const;
    std::string tileset() const;
    bool wrapsX() const;
    bool hasHdSurface() const;
    int seaLevel() const;
    int seed() const;
    int axialTilt() const;
    int elevationAt(int x, int y, int fallback = 0) const;
    std::string biomeAt(int x, int y) const;

    JsonArray* entities();
    const JsonArray* entities() const;
    JsonArray* layer(const char* name);
    const JsonArray* layer(const char* name) const;

private:
    MapView(Project* project, Json* map) : project_(project), map_(map) {}

    Project* project_;
    Json* map_;
};

}
