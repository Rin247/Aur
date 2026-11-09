#include "WorldModel.hpp"
#include "aur/world_topology.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aur::editor {
namespace {

int layerValue(const aur::Json& map, const char* layer, int x, int y, int fallback) {
    if (!map.contains("layers") || !map["layers"].is_object() ||
        !map["layers"].contains(layer) || !map["layers"][layer].is_array()) {
        return fallback;
    }
    const auto& rows = map["layers"][layer].as_array();
    if (y < 0 || y >= static_cast<int>(rows.size()) || !rows[static_cast<std::size_t>(y)].is_array()) {
        return fallback;
    }
    const auto& row = rows[static_cast<std::size_t>(y)].as_array();
    if (x < 0 || x >= static_cast<int>(row.size())) return fallback;
    return row[static_cast<std::size_t>(x)].as_int();
}

std::string biomeValue(const aur::Json& map, int x, int y) {
    if (!map.contains("layers") || !map["layers"].is_object() ||
        !map["layers"].contains("biomes") || !map["layers"]["biomes"].is_array()) {
        return {};
    }
    const auto& rows = map["layers"]["biomes"].as_array();
    if (y < 0 || y >= static_cast<int>(rows.size()) ||
        !rows[static_cast<std::size_t>(y)].is_array()) {
        return {};
    }
    const auto& row = rows[static_cast<std::size_t>(y)].as_array();
    if (x < 0 || x >= static_cast<int>(row.size())) return {};
    return row[static_cast<std::size_t>(x)].as_string();
}

} // namespace

WorldModel WorldModel::fromMap(
    const aur::Json& map, const aur::JsonObject& projectBiomes) {
    WorldModel model;
    model.width_ = map.contains("w") ? map["w"].as_int() : 0;
    model.height_ = map.contains("h") ? map["h"].as_int() : 0;
    model.wraps_x_ = aur::wraps_world_x(map);
    if (model.width_ <= 0 || model.height_ <= 0 ||
        static_cast<std::uint64_t>(model.width_) * model.height_ > 64ull * 1024 * 1024) {
        throw std::invalid_argument("World dimensions are invalid or exceed the editor limit");
    }
    const std::size_t cellCount =
        static_cast<std::size_t>(model.width_) * static_cast<std::size_t>(model.height_);
    model.heights_.resize(cellCount);
    model.biomes_.resize(cellCount);
    model.water_types_.resize(cellCount);
    if (map.contains("worldgen") && map["worldgen"].is_object() &&
        map["worldgen"].contains("sea_level")) {
        const int seaLevel = std::clamp(map["worldgen"]["sea_level"].as_int(), 0, 1000);
        model.sea_level_ = (seaLevel * 65535 + 500) / 1000;
    }

    for (const auto& [id, _] : projectBiomes) {
        if (model.biome_names_.size() == 32) break;
        const auto numericId = static_cast<std::uint8_t>(model.biome_names_.size());
        model.biome_ids_.emplace(id, numericId);
        model.biome_names_.push_back(id);
    }
    for (int y = 0; y < model.height_; ++y) {
        for (int x = 0; x < model.width_; ++x) {
            const std::string biome = biomeValue(map, x, y);
            if (!biome.empty() && !model.biome_ids_.contains(biome) &&
                model.biome_names_.size() < 32) {
                const auto numericId = static_cast<std::uint8_t>(model.biome_names_.size());
                model.biome_ids_.emplace(biome, numericId);
                model.biome_names_.push_back(biome);
            }
        }
    }

    for (int y = 0; y < model.height_; ++y) {
        for (int x = 0; x < model.width_; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * model.width_ + static_cast<std::size_t>(x);
            const int height = std::clamp(layerValue(map, "elevation", x, y, 500), 0, 1000);
            model.heights_[index] = static_cast<std::uint16_t>((height * 65535 + 500) / 1000);
            model.water_types_[index] = static_cast<std::uint8_t>(
                std::clamp(layerValue(map, "water", x, y, 0), 0, 2));
            const auto biome = model.biome_ids_.find(biomeValue(map, x, y));
            if (biome != model.biome_ids_.end()) model.biomes_[index] = biome->second;
        }
    }
    return model;
}

void WorldModel::validateRect(Rect rect) const {
    if (rect.width <= 0 || rect.height <= 0 || rect.y < 0 ||
        rect.y > height_ - rect.height ||
        (wraps_x_ ? rect.width > width_
                  : rect.x < 0 || rect.width > width_ ||
                    rect.x > width_ - rect.width)) {
        throw std::out_of_range("World model rectangle is outside the layer bounds");
    }
}

WorldModel::RectSnapshot WorldModel::captureRect(Layer layer, Rect rect) const {
    validateRect(rect);
    RectSnapshot snapshot;
    snapshot.layer = layer;
    snapshot.rect = rect;
    const std::size_t count =
        static_cast<std::size_t>(rect.width) * static_cast<std::size_t>(rect.height);
    if (layer == Layer::Height) snapshot.heights.reserve(count);
    else snapshot.biomes.reserve(count);
    const int start_x = wraps_x_ ? aur::wrapX(rect.x, width_) : rect.x;
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(rect.y + y) * width_ +
                static_cast<std::size_t>(wraps_x_ ? aur::wrapX(start_x + x, width_)
                                                  : start_x + x);
            if (layer == Layer::Height) snapshot.heights.push_back(heights_[index]);
            else snapshot.biomes.push_back(biomes_[index]);
        }
    }
    return snapshot;
}

void WorldModel::setRect(Layer layer, Rect rect, std::span<const std::uint16_t> values) {
    validateRect(rect);
    const std::size_t expected =
        static_cast<std::size_t>(rect.width) * static_cast<std::size_t>(rect.height);
    if (layer != Layer::Height || values.size() != expected) {
        throw std::invalid_argument("Height edits require one R16 value per cell");
    }
    bool changed = false;
    const int start_x = wraps_x_ ? aur::wrapX(rect.x, width_) : rect.x;
    for (int y = 0; y < rect.height; ++y) {
        const std::size_t source = static_cast<std::size_t>(y) * rect.width;
        for (int x = 0; x < rect.width; ++x) {
            const auto value = values[source + static_cast<std::size_t>(x)];
            const std::size_t index =
                static_cast<std::size_t>(rect.y + y) * width_ +
                static_cast<std::size_t>(wraps_x_ ? aur::wrapX(start_x + x, width_)
                                                  : start_x + x);
            changed |= heights_[index] != value;
            heights_[index] = value;
        }
    }
    if (changed) markDirty(layer, rect);
}

void WorldModel::setRect(Layer layer, Rect rect, std::span<const std::uint8_t> values) {
    validateRect(rect);
    const std::size_t expected =
        static_cast<std::size_t>(rect.width) * static_cast<std::size_t>(rect.height);
    if (layer != Layer::Biome || values.size() != expected) {
        throw std::invalid_argument("Biome edits require one R8 value per cell");
    }
    bool changed = false;
    const int start_x = wraps_x_ ? aur::wrapX(rect.x, width_) : rect.x;
    for (int y = 0; y < rect.height; ++y) {
        const std::size_t source = static_cast<std::size_t>(y) * rect.width;
        for (int x = 0; x < rect.width; ++x) {
            const auto value = values[source + static_cast<std::size_t>(x)];
            const std::size_t index =
                static_cast<std::size_t>(rect.y + y) * width_ +
                static_cast<std::size_t>(wraps_x_ ? aur::wrapX(start_x + x, width_)
                                                  : start_x + x);
            changed |= biomes_[index] != value;
            biomes_[index] = value;
        }
    }
    if (changed) markDirty(layer, rect);
}

void WorldModel::restore(const RectSnapshot& snapshot) {
    if (snapshot.layer == Layer::Height) setRect(snapshot.layer, snapshot.rect, snapshot.heights);
    else setRect(snapshot.layer, snapshot.rect, snapshot.biomes);
}

std::optional<std::uint8_t> WorldModel::biomeId(const std::string& name) const {
    const auto it = biome_ids_.find(name);
    if (it == biome_ids_.end()) return std::nullopt;
    return it->second;
}

void WorldModel::writeRect(aur::Json& map, Layer layer, Rect rect) const {
    validateRect(rect);
    if (!map.contains("layers") || !map["layers"].is_object()) {
        map["layers"] = aur::JsonObject{};
    }
    const char* layerName = layer == Layer::Height ? "elevation" : "biomes";
    if (!map["layers"].contains(layerName) || !map["layers"][layerName].is_array()) {
        map["layers"][layerName] = aur::JsonArray{};
    }
    auto& rows = map["layers"][layerName].as_array();
    while (static_cast<int>(rows.size()) < height_) rows.emplace_back(aur::JsonArray{});
    const int start_x = wraps_x_ ? aur::wrapX(rect.x, width_) : rect.x;
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        if (!rows[static_cast<std::size_t>(y)].is_array()) {
            rows[static_cast<std::size_t>(y)] = aur::JsonArray{};
        }
        auto& row = rows[static_cast<std::size_t>(y)].as_array();
        while (static_cast<int>(row.size()) < width_) {
            row.emplace_back(layer == Layer::Height ? 500 : 0);
        }
        for (int x = 0; x < rect.width; ++x) {
            const int column = wraps_x_ ? aur::wrapX(start_x + x, width_) : rect.x + x;
            const std::size_t index =
                static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(column);
            if (layer == Layer::Height) {
                const int value =
                    (static_cast<int>(heights_[index]) * 1000 + 32767) / 65535;
                row[static_cast<std::size_t>(column)] = value;
            } else {
                const std::uint8_t id = biomes_[index];
                row[static_cast<std::size_t>(column)] =
                    id < biome_names_.size() ? biome_names_[id] : std::string{};
            }
        }
    }
}

std::vector<WorldModel::DirtyRect> WorldModel::takeDirtyRects() {
    std::vector<DirtyRect> dirty;
    dirty.swap(dirty_rects_);
    return dirty;
}

void WorldModel::markDirty(Layer layer, Rect rect) {
    if (!wraps_x_) {
        dirty_rects_.push_back({layer, rect});
    } else {
        const int start_x = aur::wrapX(rect.x, width_);
        const int first_width = std::min(rect.width, width_ - start_x);
        dirty_rects_.push_back({layer, {start_x, rect.y, first_width, rect.height}});
        if (first_width < rect.width) {
            dirty_rects_.push_back(
                {layer, {0, rect.y, rect.width - first_width, rect.height}});
        }
    }
    ++revision_;
}

} // namespace aur::editor
