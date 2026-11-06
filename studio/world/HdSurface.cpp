#include "HdSurface.hpp"
#include "WorldMapHelpers.hpp"
#include "aur/world_topology.hpp"
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPen>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <vector>

namespace aur::world {

QImage build_hd_surface(const Json& map, const Project* project, const QRect& region) {
    if (!map.contains("layers") || !map["layers"].is_object() ||
        !map["layers"].contains("elevation") || !map["layers"]["elevation"].is_array()) {
        return {};
    }

    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {};
    constexpr int target_pixels_per_tile = 32;
    constexpr std::uint64_t max_surface_pixels = 16 * 1024 * 1024;
    const std::uint64_t tile_count =
        static_cast<std::uint64_t>(map_width) * static_cast<std::uint64_t>(map_height);
    int pixels_per_tile = target_pixels_per_tile;
    while (pixels_per_tile > 1 &&
           tile_count > max_surface_pixels /
               static_cast<std::uint64_t>(pixels_per_tile * pixels_per_tile)) {
        --pixels_per_tile;
    }
    const int image_width = map_width * pixels_per_tile;
    const int image_height = map_height * pixels_per_tile;
    QImage image(image_width, image_height, QImage::Format_RGB32);
    if (image.isNull()) return {};

    const auto& elevation_rows = map["layers"]["elevation"].as_array();
    const JsonArray* moisture_rows = map["layers"].contains("moisture") &&
        map["layers"]["moisture"].is_array() ? &map["layers"]["moisture"].as_array() : nullptr;
    const JsonArray* biome_rows = map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array() ? &map["layers"]["biomes"].as_array() : nullptr;
    const JsonArray* water_rows = map["layers"].contains("water") &&
        map["layers"]["water"].is_array() ? &map["layers"]["water"].as_array() : nullptr;
    const int sea_level = map.contains("worldgen") &&
        map["worldgen"].contains("sea_level") ? map["worldgen"]["sea_level"].as_int() : 500;
    const uint32_t seed = static_cast<uint32_t>(map.contains("worldgen") &&
        map["worldgen"].contains("seed") ? map["worldgen"]["seed"].as_int() : 1);
    const bool wraps_x = aur::wraps_world_x(map);
    const auto wrapped_x = [map_width, wraps_x](int x) {
        return wraps_x ? aur::wrapX(x, map_width) : x;
    };
    const int axial_tilt = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const double axis_radians = axial_tilt * 3.14159265358979323846 / 180.0;
    const double axis_sine = std::sin(axis_radians);
    const double axis_cosine = std::cos(axis_radians);
    const double latitude_scale = std::max(
        0.001, std::abs(axis_sine) + std::abs(axis_cosine));

    std::map<std::string, QColor> biome_colors{
        {"verdant_plains", QColor("#8eae65")},
        {"old_growth_forest", QColor("#426e50")},
        {"tropical_jungle", QColor("#39785b")},
        {"desert_wastes", QColor("#c6a066")},
        {"alpine_highlands", QColor("#858f8b")},
        {"frostfall_tundra", QColor("#c1d1d1")},
        {"mistfen_marsh", QColor("#668b78")},
        {"volcanic_badlands", QColor("#9c594a")},
        {"coast_and_islands", QColor("#d1bc83")},
        {"ancient_ruins", QColor("#92816b")},
        {"crystal_reaches", QColor("#8e80b4")},
        {"sky_isles", QColor("#8ab5c6")}
    };
    if (project) {
        for (const auto& [id, biome] : project->biomes) {
            if (!biome.contains("color") || !biome["color"].is_string()) continue;
            const QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) biome_colors[id] = color;
        }
    }

    const auto field_value = [](const JsonArray* rows, int x, int y, int fallback) {
        if (!rows || y < 0 || y >= static_cast<int>(rows->size())) return fallback;
        const Json& row = (*rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return fallback;
        return row[static_cast<size_t>(x)].as_int();
    };
    const auto biome_at = [biome_rows, map_height](int x, int y) {
        if (!biome_rows || y < 0 || y >= map_height) return std::string{};
        const Json& row = (*biome_rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) {
            return std::string{};
        }
        return row[static_cast<size_t>(x)].as_string();
    };
    const auto elevation_at = [&field_value, &elevation_rows](int x, int y) {
        return field_value(&elevation_rows, x, y, 0);
    };
    const auto water_at = [&field_value, water_rows](int x, int y) {
        return field_value(water_rows, x, y, 0);
    };
    const auto is_water_at = [&elevation_at, &water_at, sea_level](int x, int y) {
        return elevation_at(x, y) <= sea_level || water_at(x, y) == 2;
    };
    std::vector<int> shore_distance(
        static_cast<size_t>(map_width) * map_height, map_width + map_height);
    std::queue<int> shore_frontier;
    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            const bool water = is_water_at(x, y);
            const bool coast = is_water_at(aur::wrapX(x - 1, map_width), y) != water ||
                is_water_at(aur::wrapX(x + 1, map_width), y) != water ||
                (y > 0 && is_water_at(x, y - 1) != water) ||
                (y + 1 < map_height && is_water_at(x, y + 1) != water);
            if (!coast) continue;
            const int index = y * map_width + x;
            shore_distance[static_cast<size_t>(index)] = 1;
            shore_frontier.push(index);
        }
    }
    constexpr std::array<std::pair<int, int>, 8> shore_neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    const int maximum_shore_distance = std::min(6, std::max(map_width, map_height));
    while (!shore_frontier.empty()) {
        const int index = shore_frontier.front();
        shore_frontier.pop();
        const int x = index % map_width;
        const int y = index / map_width;
        const int next_distance = shore_distance[static_cast<size_t>(index)] + 1;
        if (next_distance > maximum_shore_distance) continue;
        const bool water = is_water_at(x, y);
        for (const auto& [dx, dy] : shore_neighbors) {
            const int next_y = y + dy;
            if (next_y < 0 || next_y >= map_height) continue;
            const int next_x = aur::wrapX(x + dx, map_width);
            if (is_water_at(next_x, next_y) != water) continue;
            const int next = next_y * map_width + next_x;
            if (shore_distance[static_cast<size_t>(next)] <= next_distance) continue;
            shore_distance[static_cast<size_t>(next)] = next_distance;
            shore_frontier.push(next);
        }
    }
    const auto sample_field = [&field_value, map_width, map_height](
                                  const JsonArray* rows, double x, double y, int fallback) {
        const int x0_unwrapped = static_cast<int>(std::floor(x));
        const int y0_unclamped = static_cast<int>(std::floor(y));
        const double fx = x - x0_unwrapped;
        const double fy = y - y0_unclamped;
        const int x0 = aur::wrapX(x0_unwrapped, map_width);
        const int x1 = aur::wrapX(x0_unwrapped + 1, map_width);
        const int y0 = std::clamp(y0_unclamped, 0, map_height - 1);
        const int y1 = std::clamp(y0_unclamped + 1, 0, map_height - 1);
        const double top = field_value(rows, x0, y0, fallback) * (1.0 - fx) +
                           field_value(rows, x1, y0, fallback) * fx;
        const double bottom = field_value(rows, x0, y1, fallback) * (1.0 - fx) +
                              field_value(rows, x1, y1, fallback) * fx;
        return top * (1.0 - fy) + bottom * fy;
    };
    const auto mix_color = [](const QColor& a, const QColor& b, double amount) {
        const double t = std::clamp(amount, 0.0, 1.0);
        return QColor(
            qRound(a.red() + (b.red() - a.red()) * t),
            qRound(a.green() + (b.green() - a.green()) * t),
            qRound(a.blue() + (b.blue() - a.blue()) * t));
    };
    const auto smoothstep = [](double low, double high, double value) {
        const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    };
    const auto climate_latitude_at = [map_width, map_height, wraps_x, axis_sine,
                                      axis_cosine, latitude_scale](int x, int y) {
        const double centered_y = map_height > 1
            ? 2.0 * y / (map_height - 1) - 1.0 : 0.0;
        if (wraps_x) return std::clamp(std::abs(centered_y), 0.0, 1.0);
        const double centered_x = map_width > 1
            ? 2.0 * x / (map_width - 1) - 1.0 : 0.0;
        return std::clamp(std::abs(
            (centered_x * axis_sine + centered_y * axis_cosine) / latitude_scale),
            0.0, 1.0);
    };
    const QColor shallow_ocean("#327b88");
    const QColor beach("#d9c58f");
    const auto biome_color = [&biome_colors, &shallow_ocean](const std::string& id) {
        if (id.empty()) return shallow_ocean;
        if (id == "coast_and_islands") return QColor("#7d9862");
        const auto biome = biome_colors.find(id);
        return biome == biome_colors.end() ? QColor("#8eae65") : biome->second;
    };
    const auto depth_color = [&mix_color](double depth) {
        constexpr std::array<double, 6> depths{0.0, 45.0, 140.0, 320.0, 620.0, 1000.0};
        constexpr std::array<const char*, 6> colors{
            "#68b9ad", "#4c9eaa", "#347d98", "#285f80", "#1c4264", "#132d49"
        };
        size_t band = 0;
        while (band + 1 < depths.size() && depth > depths[band + 1]) ++band;
        if (band + 1 == depths.size()) return QColor(colors.back());
        const double blend = (depth - depths[band]) / (depths[band + 1] - depths[band]);
        return mix_color(QColor(colors[band]), QColor(colors[band + 1]), blend);
    };

    for (int py = 0; py < image_height; ++py) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(py));
        const int y = py / pixels_per_tile;
        const double world_y = (py + 0.5) / pixels_per_tile - 0.5;
        for (int px = 0; px < image_width; ++px) {
            const int x = px / pixels_per_tile;
            const double world_x = (px + 0.5) / pixels_per_tile - 0.5;
            const double height = sample_field(&elevation_rows, world_x, world_y, 0);
            const double water_depth = std::max(0.0, sea_level - height);
            const std::string biome_id = biome_at(x, y);
            const double moisture = sample_field(moisture_rows, world_x, world_y, 50);
            const int inland_water = field_value(water_rows, x, y, 0);
            const int shore = shore_distance[static_cast<size_t>(y * map_width + x)];
            const double climate_latitude = climate_latitude_at(x, y);
            const double polar_ice = smoothstep(0.78, 0.97, climate_latitude);
            QColor color;
            if (height <= sea_level) {
                color = depth_color(water_depth);
                if (shore <= 2 && water_depth < 110.0) {
                    color = mix_color(color, QColor("#79c3b2"),
                                      (3.0 - shore) * (110.0 - water_depth) / 440.0);
                }
                color = mix_color(color, QColor("#91b9bd"), polar_ice * 0.78);
            } else if (inland_water > 0) {
                if (inland_water == 2) {
                    color = QColor("#347f91");
                } else {
                    color = QColor("#286f87");
                }
            } else {
                color = biome_color(biome_id);
                const double altitude = std::clamp((height - sea_level) / 340.0, 0.0, 1.0);
                if (shore == 1 && height - sea_level < 100) {
                    color = mix_color(color, beach,
                        std::clamp((100.0 - (height - sea_level)) / 100.0, 0.0, 0.92));
                } else if (shore == 2 && height - sea_level < 48) {
                    color = mix_color(color, beach, 0.16);
                } else if (altitude > 0.78) {
                    color = mix_color(color, QColor("#dce4dc"),
                                      std::clamp((altitude - 0.66) * 1.9, 0.0, 0.68));
                } else if (moisture < 20) {
                    color = mix_color(color, QColor("#c2a16d"),
                                      std::clamp((20.0 - moisture) / 55.0, 0.0, 0.24));
                }
                color = mix_color(color, QColor("#c8d8d5"), polar_ice * 0.92);
            }

            const double west = sample_field(&elevation_rows, world_x - 0.35, world_y, height);
            const double east = sample_field(&elevation_rows, world_x + 0.35, world_y, height);
            const double north = sample_field(&elevation_rows, world_x, world_y - 0.35, height);
            const double south = sample_field(&elevation_rows, world_x, world_y + 0.35, height);
            const bool ocean = height <= sea_level;
            const double relief = ocean
                ? std::clamp(1.0 + (west - east) * 0.00055 +
                             (north - south) * 0.00045, 0.93, 1.07)
                : std::clamp(1.0 + (west - east) * 0.0018 +
                             (north - south) * 0.0015, 0.72, 1.26);
            const uint32_t texture_hash = surface_hash(seed, x / 2, y / 2, 0x68bc21ebu);
            const double texture = ocean || polar_ice > 0.4
                ? 0.0 : static_cast<double>(texture_hash & 7u) - 3.0;
            const double light = std::clamp(relief + texture / 100.0, 0.68, 1.28);
            line[px] = qRgb(
                std::clamp(qRound(color.red() * light), 0, 255),
                std::clamp(qRound(color.green() * light), 0, 255),
                std::clamp(qRound(color.blue() * light), 0, 255));
        }
    }

    QPainter details(&image);
    details.setRenderHint(QPainter::Antialiasing);
    details.scale(pixels_per_tile, pixels_per_tile);
    const QColor wave_color(176, 222, 225, 34);
    const QColor foam_color(230, 243, 224, 96);
    const QColor forest_shadow(32, 62, 53, 112);
    const QColor forest_mid(49, 93, 66, 182);
    const QColor forest_light(98, 131, 77, 145);
    details.setPen(QPen(wave_color, 0.035, Qt::SolidLine, Qt::RoundCap));
    for (int y = 1; y < map_height - 1; y += 3) {
        for (int x = 0; x < map_width; x += 3) {
            if (elevation_at(x, y) > sea_level && water_at(x, y) == 0) continue;
            const uint32_t hash = surface_hash(seed, x, y, 0x02e5be93u);
            const qreal left = x + 0.12 + ((hash >> 3) & 7u) * 0.045;
            const qreal top = y + 0.18 + ((hash >> 8) & 7u) * 0.055;
            const qreal length = 0.35 + ((hash >> 13) & 7u) * 0.045;
            QPainterPath wave;
            wave.moveTo(left, top);
            wave.quadTo(left + length * 0.5, top - 0.09, left + length, top);
            details.drawPath(wave);
        }
    }

    std::vector<std::pair<QPointF, QPointF>> river_segments;
    constexpr std::array<std::pair<int, int>, 8> flow_neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            if (water_at(x, y) != 1) continue;
            const int source_height = elevation_at(x, y);
            int downstream_x = x;
            int downstream_y = y;
            int lowest_height = source_height;
            for (const auto& [dx, dy] : flow_neighbors) {
                const int next_y = y + dy;
                if (next_y < 0 || next_y >= map_height) continue;
                const int next_x = wrapped_x(x + dx);
                const int next_height = elevation_at(next_x, next_y) <= sea_level
                    ? sea_level : elevation_at(next_x, next_y);
                if (next_height >= lowest_height) continue;
                lowest_height = next_height;
                downstream_x = next_x;
                downstream_y = next_y;
            }
            if (downstream_x == x && downstream_y == y) continue;
            if (water_at(downstream_x, downstream_y) != 1 &&
                water_at(downstream_x, downstream_y) != 2 &&
                elevation_at(downstream_x, downstream_y) > sea_level) continue;

            qreal end_x = downstream_x + 0.5;
            if (downstream_x - x > 1) end_x -= map_width;
            else if (x - downstream_x > 1) end_x += map_width;
            river_segments.emplace_back(QPointF(x + 0.5, y + 0.5),
                                        QPointF(end_x, downstream_y + 0.5));
        }
    }
    details.setPen(QPen(QColor(14, 48, 62, 65), 0.28,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }
    details.setPen(QPen(QColor("#347f91"), 0.17,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }
    details.setPen(QPen(QColor(123, 205, 200, 125), 0.035,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }

    details.setPen(QPen(QColor(132, 202, 205, 145), 0.08, Qt::SolidLine, Qt::RoundCap));
    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            if (water_at(x, y) != 2) continue;
            const uint32_t hash = surface_hash(seed, x, y, 0x6c616b65u);
            if ((hash & 3u) != 0u) continue;
            const qreal left = x + 0.2 + ((hash >> 3) & 7u) * 0.045;
            const qreal top = y + 0.25 + ((hash >> 8) & 7u) * 0.04;
            details.drawLine(QPointF(left, top),
                             QPointF(left + 0.22, top + 0.025));
        }
    }

    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            const int height = elevation_at(x, y);
            if (height <= sea_level || water_at(x, y) > 0) {
                if (height <= sea_level) {
                    const bool land_north = elevation_at(x, y - 1) > sea_level &&
                        water_at(x, y - 1) == 0;
                    const bool land_east = elevation_at(wrapped_x(x + 1), y) > sea_level &&
                        water_at(wrapped_x(x + 1), y) == 0;
                    const bool land_south = elevation_at(x, y + 1) > sea_level &&
                        water_at(x, y + 1) == 0;
                    const bool land_west = elevation_at(wrapped_x(x - 1), y) > sea_level &&
                        water_at(wrapped_x(x - 1), y) == 0;
                    const uint32_t foam_hash = surface_hash(seed, x, y, 0x9d39247eu);
                    details.setPen(QPen(foam_color, 0.055, Qt::SolidLine, Qt::RoundCap));
                    const qreal inset = 0.08 + ((foam_hash >> 4) & 3u) * 0.025;
                    const qreal span = 0.54 + ((foam_hash >> 9) & 3u) * 0.08;
                    if (land_north) {
                        details.drawLine(QPointF(x + inset, y + 0.07),
                                         QPointF(x + inset + span, y + 0.07));
                    }
                    if (land_east) {
                        details.drawLine(QPointF(x + 0.93, y + inset),
                                         QPointF(x + 0.93, y + inset + span));
                    }
                    if (land_south) {
                        details.drawLine(QPointF(x + inset, y + 0.93),
                                         QPointF(x + inset + span, y + 0.93));
                    }
                    if (land_west) {
                        details.drawLine(QPointF(x + 0.07, y + inset),
                                         QPointF(x + 0.07, y + inset + span));
                    }
                }
                continue;
            }

            const std::string biome_id = biome_at(x, y);
            const uint32_t hash = surface_hash(seed, x, y, 0xa24baed5u);
            const bool forest_biome = biome_id.find("forest") != std::string::npos ||
                biome_id.find("jungle") != std::string::npos;
            const int moisture = field_value(moisture_rows, x, y, 50);
            if ((forest_biome || moisture > 82) && climate_latitude_at(x, y) < 0.78 &&
                height < sea_level + 330 &&
                hash % (forest_biome ? 13u : 17u) < (forest_biome ? 4u : 2u)) {
                const qreal center_x = x + 0.18 + ((hash >> 4) & 7u) * 0.09;
                const qreal center_y = y + 0.18 + ((hash >> 9) & 7u) * 0.09;
                const qreal size = 0.55 + ((hash >> 14) & 7u) * 0.075;
                details.setPen(Qt::NoPen);
                details.setBrush(forest_shadow);
                details.drawEllipse(QPointF(center_x + 0.04, center_y + 0.16),
                                    size * 1.02, size * 0.48);
                details.setBrush(forest_mid);
                details.drawEllipse(QPointF(center_x - size * 0.42, center_y),
                                    size * 0.58, size * 0.62);
                details.drawEllipse(QPointF(center_x + size * 0.02, center_y - size * 0.10),
                                    size * 0.62, size * 0.72);
                details.drawEllipse(QPointF(center_x + size * 0.45, center_y + size * 0.01),
                                    size * 0.55, size * 0.60);
                details.setBrush(forest_light);
                details.drawEllipse(QPointF(center_x - size * 0.20, center_y - size * 0.31),
                                    size * 0.29, size * 0.20);
            }

            if (height < sea_level + 190 ||
                surface_hash(seed, x, y, 0x3c6ef372u) % 9u != 0u) continue;
            const int west = elevation_at(wrapped_x(x - 1), y);
            const int east = elevation_at(wrapped_x(x + 1), y);
            const int north = elevation_at(x, y - 1);
            const int south = elevation_at(x, y + 1);
            if (height < west || height < east || height < north || height < south) continue;

            details.save();
            details.translate(x + 0.5, y + 0.5);
            if (std::abs(east - west) > std::abs(south - north)) details.rotate(90.0);
            const qreal ridge_shift = ((hash >> 10) & 7u) * 0.018;
            const qreal main_peak = -0.57 - ((hash >> 18) & 3u) * 0.045;
            QPolygonF mountain_range{
                QPointF(-0.78, 0.43), QPointF(-0.57, 0.05),
                QPointF(-0.42, -0.29), QPointF(-0.23, 0.04),
                QPointF(-0.02, main_peak), QPointF(0.17, 0.01),
                QPointF(0.39, -0.27), QPointF(0.54, 0.04),
                QPointF(0.78, 0.43)
            };
            for (QPointF& point : mountain_range) point.rx() += ridge_shift;
            details.setPen(QPen(QColor("#514b45"), 0.035));
            details.setBrush(QColor("#827665"));
            details.drawPolygon(mountain_range);

            details.setPen(Qt::NoPen);
            details.setBrush(QColor("#b7a58a"));
            details.drawPolygon(QPolygonF{
                QPointF(-0.57 + ridge_shift, 0.05),
                QPointF(-0.42 + ridge_shift, -0.29),
                QPointF(-0.30 + ridge_shift, 0.17),
                QPointF(-0.45 + ridge_shift, 0.12)
            });
            details.drawPolygon(QPolygonF{
                QPointF(-0.02 + ridge_shift, main_peak),
                QPointF(0.14 + ridge_shift, 0.13),
                QPointF(-0.12 + ridge_shift, 0.17)
            });
            details.drawPolygon(QPolygonF{
                QPointF(0.39 + ridge_shift, -0.27),
                QPointF(0.54 + ridge_shift, 0.04),
                QPointF(0.27 + ridge_shift, 0.16)
            });
            if (height > sea_level + 470) {
                details.setBrush(QColor("#e4e6dc"));
                details.drawPolygon(QPolygonF{
                    QPointF(-0.09 + ridge_shift, main_peak + 0.15),
                    QPointF(-0.02 + ridge_shift, main_peak),
                    QPointF(0.06 + ridge_shift, main_peak + 0.17),
                    QPointF(0.02 + ridge_shift, main_peak + 0.14)
                });
                details.drawPolygon(QPolygonF{
                    QPointF(0.34 + ridge_shift, -0.12),
                    QPointF(0.39 + ridge_shift, -0.27),
                    QPointF(0.46 + ridge_shift, -0.09)
                });
            }
            details.restore();
        }
    }
    if (region.isNull()) return image;
    const QRect pixel_region(region.x() * pixels_per_tile, region.y() * pixels_per_tile,
                             region.width() * pixels_per_tile,
                             region.height() * pixels_per_tile);
    return image.copy(pixel_region.intersected(image.rect()));


}

}
