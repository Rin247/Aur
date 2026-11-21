#pragma once

#include "aur/json.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aur::editor {

class WorldModel {
public:
    enum class Layer {
        Height,
        Biome,
        Water
    };

    struct Rect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    struct RectSnapshot {
        Layer layer = Layer::Height;
        Rect rect;
        std::vector<std::uint16_t> heights;
        std::vector<std::uint8_t> biomes;
    };

    struct DirtyRect {
        Layer layer = Layer::Height;
        Rect rect;
    };

    static WorldModel fromMap(const aur::Json& map,
                              const aur::JsonObject& projectBiomes);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    int seaLevel() const noexcept { return sea_level_; }
    std::uint64_t revision() const noexcept { return revision_; }
    bool wrapsX() const noexcept { return wraps_x_; }
    const std::vector<std::uint16_t>& heights() const noexcept { return heights_; }
    const std::vector<std::uint8_t>& biomes() const noexcept { return biomes_; }
    const std::vector<std::uint8_t>& waterTypes() const noexcept { return water_types_; }
    const std::vector<std::string>& biomeNames() const noexcept { return biome_names_; }
    std::optional<std::uint8_t> biomeId(const std::string& name) const;

    RectSnapshot captureRect(Layer layer, Rect rect) const;
    void setRect(Layer layer, Rect rect, std::span<const std::uint16_t> values);
    void setRect(Layer layer, Rect rect, std::span<const std::uint8_t> values);
    void restore(const RectSnapshot& snapshot);
    void writeRect(aur::Json& map, Layer layer, Rect rect) const;
    std::vector<DirtyRect> takeDirtyRects();

private:
    int width_ = 0;
    int height_ = 0;
    int sea_level_ = 32768;
    bool wraps_x_ = false;
    std::uint64_t revision_ = 0;
    std::vector<std::uint16_t> heights_;
    std::vector<std::uint8_t> biomes_;
    std::vector<std::uint8_t> water_types_;
    std::vector<std::string> biome_names_;
    std::map<std::string, std::uint8_t> biome_ids_;
    std::vector<DirtyRect> dirty_rects_;

    void validateRect(Rect rect) const;
    void markDirty(Layer layer, Rect rect);
};

} // namespace aur::editor
