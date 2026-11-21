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
namespace {

struct SurfaceContext {
    const Json& map;
    int map_width = 0;
    int map_height = 0;
    QRect tile_bounds;
    int pixels_per_tile = 1;
    bool full_map = true;
    bool wraps_x = false;
    int sea_level = 500;
    uint32_t seed = 1;
    double axis_sine = 0.0;
    double axis_cosine = 1.0;
    double latitude_scale = 1.0;
    const JsonArray* elevation_rows = nullptr;
    const JsonArray* moisture_rows = nullptr;
    const JsonArray* biome_rows = nullptr;
    const JsonArray* water_rows = nullptr;
    std::map<std::string, QColor> biome_colors;
    std::vector<int> shore_distance;

    int wrappedX(int x) const { return wraps_x ? aur::wrapX(x, map_width) : x; }

    int fieldValue(const JsonArray* rows, int x, int y, int fallback) const {
        if (!rows || y < 0 || y >= static_cast<int>(rows->size())) return fallback;
        const Json& row = (*rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return fallback;
        return row[static_cast<size_t>(x)].as_int();
    }

    std::string biomeAt(int x, int y) const {
        if (!biome_rows || y < 0 || y >= map_height) return {};
        const Json& row = (*biome_rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return {};
        return row[static_cast<size_t>(x)].as_string();
    }

    int elevationAt(int x, int y) const { return fieldValue(elevation_rows, x, y, 0); }
    int waterAt(int x, int y) const { return fieldValue(water_rows, x, y, 0); }
    bool isWaterAt(int x, int y) const { return elevationAt(x, y) <= sea_level || waterAt(x, y) == 2; }

    double sampleField(const JsonArray* rows, double x, double y, int fallback) const {
        const int x0_unwrapped = static_cast<int>(std::floor(x));
        const int y0_unclamped = static_cast<int>(std::floor(y));
        const double fx = x - x0_unwrapped;
        const double fy = y - y0_unclamped;
        const int x0 = aur::wrapX(x0_unwrapped, map_width);
        const int x1 = aur::wrapX(x0_unwrapped + 1, map_width);
        const int y0 = std::clamp(y0_unclamped, 0, map_height - 1);
        const int y1 = std::clamp(y0_unclamped + 1, 0, map_height - 1);
        const double top = fieldValue(rows, x0, y0, fallback) * (1.0 - fx) +
                           fieldValue(rows, x1, y0, fallback) * fx;
        const double bottom = fieldValue(rows, x0, y1, fallback) * (1.0 - fx) +
                              fieldValue(rows, x1, y1, fallback) * fx;
        return top * (1.0 - fy) + bottom * fy;
    }

    double climateLatitudeAt(int x, int y) const {
        const double centered_y = map_height > 1 ? 2.0 * y / (map_height - 1) - 1.0 : 0.0;
        if (wraps_x) return std::clamp(std::abs(centered_y), 0.0, 1.0);
        const double centered_x = map_width > 1 ? 2.0 * x / (map_width - 1) - 1.0 : 0.0;
        return std::clamp(std::abs((centered_x * axis_sine + centered_y * axis_cosine) / latitude_scale),
                          0.0, 1.0);
    }
};

QColor mixColor(const QColor& a, const QColor& b, double amount) {
    const double t = std::clamp(amount, 0.0, 1.0);
    return QColor(qRound(a.red() + (b.red() - a.red()) * t),
                  qRound(a.green() + (b.green() - a.green()) * t),
                  qRound(a.blue() + (b.blue() - a.blue()) * t));
}

double smoothStep(double low, double high, double value) {
    const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

QColor depthColor(double depth) {
    constexpr std::array<double, 6> depths{0.0, 45.0, 140.0, 320.0, 620.0, 1000.0};
    constexpr std::array<const char*, 6> colors{
        "#68b9ad", "#4c9eaa", "#347d98", "#285f80", "#1c4264", "#132d49"
    };
    size_t band = 0;
    while (band + 1 < depths.size() && depth > depths[band + 1]) ++band;
    if (band + 1 == depths.size()) return QColor(colors.back());
    const double blend = (depth - depths[band]) / (depths[band + 1] - depths[band]);
    return mixColor(QColor(colors[band]), QColor(colors[band + 1]), blend);
}

void buildShoreDistance(SurfaceContext& ctx) {
    ctx.shore_distance.assign(
        static_cast<size_t>(ctx.map_width) * ctx.map_height, ctx.map_width + ctx.map_height);
    std::queue<int> frontier;
    for (int y = 0; y < ctx.map_height; ++y) {
        for (int x = 0; x < ctx.map_width; ++x) {
            const bool water = ctx.isWaterAt(x, y);
            const bool coast = ctx.isWaterAt(aur::wrapX(x - 1, ctx.map_width), y) != water ||
                ctx.isWaterAt(aur::wrapX(x + 1, ctx.map_width), y) != water ||
                (y > 0 && ctx.isWaterAt(x, y - 1) != water) ||
                (y + 1 < ctx.map_height && ctx.isWaterAt(x, y + 1) != water);
            if (!coast) continue;
            const int index = y * ctx.map_width + x;
            ctx.shore_distance[static_cast<size_t>(index)] = 1;
            frontier.push(index);
        }
    }
    constexpr std::array<std::pair<int, int>, 8> neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    const int max_dist = std::min(6, std::max(ctx.map_width, ctx.map_height));
    while (!frontier.empty()) {
        const int index = frontier.front();
        frontier.pop();
        const int x = index % ctx.map_width;
        const int y = index / ctx.map_width;
        const int next_dist = ctx.shore_distance[static_cast<size_t>(index)] + 1;
        if (next_dist > max_dist) continue;
        const bool water = ctx.isWaterAt(x, y);
        for (const auto& [dx, dy] : neighbors) {
            const int ny = y + dy;
            if (ny < 0 || ny >= ctx.map_height) continue;
            const int nx = aur::wrapX(x + dx, ctx.map_width);
            if (ctx.isWaterAt(nx, ny) != water) continue;
            const int next = ny * ctx.map_width + nx;
            if (ctx.shore_distance[static_cast<size_t>(next)] <= next_dist) continue;
            ctx.shore_distance[static_cast<size_t>(next)] = next_dist;
            frontier.push(next);
        }
    }
}

void paintBasePixels(const SurfaceContext& ctx, QImage& image) {
    const QColor shallow_ocean("#327b88");
    const QColor beach("#d9c58f");
    const auto biome_color = [&ctx, &shallow_ocean](const std::string& id) {
        if (id.empty()) return shallow_ocean;
        if (id == "coast_and_islands") return QColor("#7d9862");
        const auto it = ctx.biome_colors.find(id);
        return it == ctx.biome_colors.end() ? QColor("#8eae65") : it->second;
    };
    const int ppt = ctx.pixels_per_tile;
    const int offset_px = ctx.tile_bounds.left() * ppt;
    const int offset_py = ctx.tile_bounds.top() * ppt;

    for (int py = 0; py < image.height(); ++py) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(py));
        const int global_py = offset_py + py;
        const int y = std::clamp(ctx.tile_bounds.top() + py / ppt, 0, ctx.map_height - 1);
        const double world_y = (global_py + 0.5) / ppt - 0.5;
        for (int px = 0; px < image.width(); ++px) {
            const int global_px = offset_px + px;
            const int raw_x = ctx.tile_bounds.left() + px / ppt;
            const int x = std::clamp(ctx.wrappedX(raw_x), 0, ctx.map_width - 1);
            const double world_x = (global_px + 0.5) / ppt - 0.5;
            const double height = ctx.sampleField(ctx.elevation_rows, world_x, world_y, 0);
            const double water_depth = std::max(0.0, ctx.sea_level - height);
            const std::string biome_id = ctx.biomeAt(x, y);
            const double moisture = ctx.sampleField(ctx.moisture_rows, world_x, world_y, 50);
            const int inland_water = ctx.fieldValue(ctx.water_rows, x, y, 0);
            const int shore = ctx.shore_distance[static_cast<size_t>(y * ctx.map_width + x)];
            const double climate_lat = ctx.climateLatitudeAt(x, y);
            const double polar_ice = smoothStep(0.78, 0.97, climate_lat);
            QColor color;
            if (height <= ctx.sea_level) {
                color = depthColor(water_depth);
                if (shore <= 2 && water_depth < 110.0) {
                    color = mixColor(color, QColor("#79c3b2"),
                                     (3.0 - shore) * (110.0 - water_depth) / 440.0);
                }
                color = mixColor(color, QColor("#91b9bd"), polar_ice * 0.78);
            } else if (inland_water > 0) {
                color = inland_water == 2 ? QColor("#347f91") : QColor("#286f87");
            } else {
                color = biome_color(biome_id);
                const double altitude = std::clamp((height - ctx.sea_level) / 340.0, 0.0, 1.0);
                if (shore == 1 && height - ctx.sea_level < 100) {
                    color = mixColor(color, beach,
                        std::clamp((100.0 - (height - ctx.sea_level)) / 100.0, 0.0, 0.92));
                } else if (shore == 2 && height - ctx.sea_level < 48) {
                    color = mixColor(color, beach, 0.16);
                } else if (altitude > 0.78) {
                    color = mixColor(color, QColor("#dce4dc"),
                                     std::clamp((altitude - 0.66) * 1.9, 0.0, 0.68));
                } else if (moisture < 20) {
                    color = mixColor(color, QColor("#c2a16d"),
                                     std::clamp((20.0 - moisture) / 55.0, 0.0, 0.24));
                }
                color = mixColor(color, QColor("#c8d8d5"), polar_ice * 0.92);
            }

            const double west = ctx.sampleField(ctx.elevation_rows, world_x - 0.35, world_y, height);
            const double east = ctx.sampleField(ctx.elevation_rows, world_x + 0.35, world_y, height);
            const double north = ctx.sampleField(ctx.elevation_rows, world_x, world_y - 0.35, height);
            const double south = ctx.sampleField(ctx.elevation_rows, world_x, world_y + 0.35, height);
            const bool ocean = height <= ctx.sea_level;
            const double relief = ocean
                ? std::clamp(1.0 + (west - east) * 0.00055 + (north - south) * 0.00045, 0.93, 1.07)
                : std::clamp(1.0 + (west - east) * 0.0018 + (north - south) * 0.0015, 0.72, 1.26);
            const uint32_t texture_hash = surface_hash(ctx.seed, x / 2, y / 2, 0x68bc21ebu);
            const double texture = ocean || polar_ice > 0.4
                ? 0.0 : static_cast<double>(texture_hash & 7u) - 3.0;
            const double light = std::clamp(relief + texture / 100.0, 0.68, 1.28);
            line[px] = qRgb(std::clamp(qRound(color.red() * light), 0, 255),
                            std::clamp(qRound(color.green() * light), 0, 255),
                            std::clamp(qRound(color.blue() * light), 0, 255));
        }
    }
}

void paintWaterDetails(const SurfaceContext& ctx, QPainter& details,
                       int x0, int x1, int y0, int y1) {
    const QColor wave_color(176, 222, 225, 34);
    details.setPen(QPen(wave_color, 0.035, Qt::SolidLine, Qt::RoundCap));
    for (int y = y0; y < y1; ++y) {
        if ((y - 1) % 3 != 0) continue;
        for (int tx = x0; tx < x1; ++tx) {
            const int sx = ctx.wrappedX(tx);
            if (sx < 0 || sx >= ctx.map_width || sx % 3 != 0) continue;
            if (ctx.elevationAt(sx, y) > ctx.sea_level && ctx.waterAt(sx, y) == 0) continue;
            const uint32_t hash = surface_hash(ctx.seed, sx, y, 0x02e5be93u);
            const qreal left = tx + 0.12 + ((hash >> 3) & 7u) * 0.045;
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
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    for (int y = y0; y < y1; ++y) {
        for (int tx = x0; tx < x1; ++tx) {
            const int sx = ctx.wrappedX(tx);
            if (sx < 0 || sx >= ctx.map_width || ctx.waterAt(sx, y) != 1) continue;
            const int source_height = ctx.elevationAt(sx, y);
            int downstream_x = sx;
            int downstream_y = y;
            int best_dx = 0;
            int lowest_height = source_height;
            for (const auto& [dx, dy] : flow_neighbors) {
                const int next_y = y + dy;
                if (next_y < 0 || next_y >= ctx.map_height) continue;
                const int next_x = ctx.wrappedX(sx + dx);
                if (next_x < 0 || next_x >= ctx.map_width) continue;
                const int next_height = ctx.elevationAt(next_x, next_y) <= ctx.sea_level
                    ? ctx.sea_level : ctx.elevationAt(next_x, next_y);
                if (next_height >= lowest_height) continue;
                lowest_height = next_height;
                downstream_x = next_x;
                downstream_y = next_y;
                best_dx = dx;
            }
            if (downstream_x == sx && downstream_y == y) continue;
            if (ctx.waterAt(downstream_x, downstream_y) != 1 &&
                ctx.waterAt(downstream_x, downstream_y) != 2 &&
                ctx.elevationAt(downstream_x, downstream_y) > ctx.sea_level) continue;

            qreal end_x = ctx.full_map ? (downstream_x + 0.5) : (tx + best_dx + 0.5);
            if (ctx.full_map) {
                if (downstream_x - sx > 1) end_x -= ctx.map_width;
                else if (sx - downstream_x > 1) end_x += ctx.map_width;
            }
            river_segments.emplace_back(QPointF(tx + 0.5, y + 0.5),
                                        QPointF(end_x, downstream_y + 0.5));
        }
    }
    for (const QPen& pen : {
             QPen(QColor(14, 48, 62, 65), 0.28, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
             QPen(QColor("#347f91"), 0.17, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
             QPen(QColor(123, 205, 200, 125), 0.035, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)}) {
        details.setPen(pen);
        for (const auto& [source, destination] : river_segments) details.drawLine(source, destination);
    }

    details.setPen(QPen(QColor(132, 202, 205, 145), 0.08, Qt::SolidLine, Qt::RoundCap));
    for (int y = y0; y < y1; ++y) {
        for (int tx = x0; tx < x1; ++tx) {
            const int sx = ctx.wrappedX(tx);
            if (sx < 0 || sx >= ctx.map_width || ctx.waterAt(sx, y) != 2) continue;
            const uint32_t hash = surface_hash(ctx.seed, sx, y, 0x6c616b65u);
            if ((hash & 3u) != 0u) continue;
            const qreal left = tx + 0.2 + ((hash >> 3) & 7u) * 0.045;
            const qreal top = y + 0.25 + ((hash >> 8) & 7u) * 0.04;
            details.drawLine(QPointF(left, top), QPointF(left + 0.22, top + 0.025));
        }
    }
}

void paintMountainPeak(QPainter& details, int tx, int y, int height, int sea_level,
                       int west, int east, int north, int south, uint32_t hash) {
    details.save();
    details.translate(tx + 0.5, y + 0.5);
    if (std::abs(east - west) > std::abs(south - north)) details.rotate(90.0);
    const qreal ridge_shift = ((hash >> 10) & 7u) * 0.018;
    const qreal main_peak = -0.57 - ((hash >> 18) & 3u) * 0.045;
    QPolygonF mountain_range{
        QPointF(-0.78, 0.43), QPointF(-0.57, 0.05), QPointF(-0.42, -0.29),
        QPointF(-0.23, 0.04), QPointF(-0.02, main_peak), QPointF(0.17, 0.01),
        QPointF(0.39, -0.27), QPointF(0.54, 0.04), QPointF(0.78, 0.43)
    };
    for (QPointF& point : mountain_range) point.rx() += ridge_shift;
    details.setPen(QPen(QColor("#514b45"), 0.035));
    details.setBrush(QColor("#827665"));
    details.drawPolygon(mountain_range);

    details.setPen(Qt::NoPen);
    details.setBrush(QColor("#b7a58a"));
    details.drawPolygon(QPolygonF{
        QPointF(-0.57 + ridge_shift, 0.05), QPointF(-0.42 + ridge_shift, -0.29),
        QPointF(-0.30 + ridge_shift, 0.17), QPointF(-0.45 + ridge_shift, 0.12)
    });
    details.drawPolygon(QPolygonF{
        QPointF(-0.02 + ridge_shift, main_peak),
        QPointF(0.14 + ridge_shift, 0.13), QPointF(-0.12 + ridge_shift, 0.17)
    });
    details.drawPolygon(QPolygonF{
        QPointF(0.39 + ridge_shift, -0.27),
        QPointF(0.54 + ridge_shift, 0.04), QPointF(0.27 + ridge_shift, 0.16)
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
            QPointF(0.39 + ridge_shift, -0.27), QPointF(0.46 + ridge_shift, -0.09)
        });
    }
    details.restore();
}

void paintLandDetails(const SurfaceContext& ctx, QPainter& details,
                      int x0, int x1, int y0, int y1) {
    const QColor foam_color(230, 243, 224, 96);
    const QColor forest_shadow(32, 62, 53, 112);
    const QColor forest_mid(49, 93, 66, 182);
    const QColor forest_light(98, 131, 77, 145);

    for (int y = y0; y < y1; ++y) {
        for (int tx = x0; tx < x1; ++tx) {
            const int sx = ctx.wrappedX(tx);
            if (sx < 0 || sx >= ctx.map_width) continue;
            const int height = ctx.elevationAt(sx, y);
            if (height <= ctx.sea_level || ctx.waterAt(sx, y) > 0) {
                if (height <= ctx.sea_level) {
                    const bool land_north = ctx.elevationAt(sx, y - 1) > ctx.sea_level &&
                        ctx.waterAt(sx, y - 1) == 0;
                    const bool land_east = ctx.elevationAt(ctx.wrappedX(sx + 1), y) > ctx.sea_level &&
                        ctx.waterAt(ctx.wrappedX(sx + 1), y) == 0;
                    const bool land_south = ctx.elevationAt(sx, y + 1) > ctx.sea_level &&
                        ctx.waterAt(sx, y + 1) == 0;
                    const bool land_west = ctx.elevationAt(ctx.wrappedX(sx - 1), y) > ctx.sea_level &&
                        ctx.waterAt(ctx.wrappedX(sx - 1), y) == 0;
                    const uint32_t foam_hash = surface_hash(ctx.seed, sx, y, 0x9d39247eu);
                    details.setPen(QPen(foam_color, 0.055, Qt::SolidLine, Qt::RoundCap));
                    const qreal inset = 0.08 + ((foam_hash >> 4) & 3u) * 0.025;
                    const qreal span = 0.54 + ((foam_hash >> 9) & 3u) * 0.08;
                    if (land_north) details.drawLine(QPointF(tx + inset, y + 0.07), QPointF(tx + inset + span, y + 0.07));
                    if (land_east) details.drawLine(QPointF(tx + 0.93, y + inset), QPointF(tx + 0.93, y + inset + span));
                    if (land_south) details.drawLine(QPointF(tx + inset, y + 0.93), QPointF(tx + inset + span, y + 0.93));
                    if (land_west) details.drawLine(QPointF(tx + 0.07, y + inset), QPointF(tx + 0.07, y + inset + span));
                }
                continue;
            }

            const std::string biome_id = ctx.biomeAt(sx, y);
            const uint32_t hash = surface_hash(ctx.seed, sx, y, 0xa24baed5u);
            const bool forest_biome = biome_id.find("forest") != std::string::npos ||
                biome_id.find("jungle") != std::string::npos;
            const int moisture = ctx.fieldValue(ctx.moisture_rows, sx, y, 50);
            if ((forest_biome || moisture > 82) && ctx.climateLatitudeAt(sx, y) < 0.78 &&
                height < ctx.sea_level + 330 &&
                hash % (forest_biome ? 13u : 17u) < (forest_biome ? 4u : 2u)) {
                const qreal center_x = tx + 0.18 + ((hash >> 4) & 7u) * 0.09;
                const qreal center_y = y + 0.18 + ((hash >> 9) & 7u) * 0.09;
                const qreal size = 0.55 + ((hash >> 14) & 7u) * 0.075;
                details.setPen(Qt::NoPen);
                details.setBrush(forest_shadow);
                details.drawEllipse(QPointF(center_x + 0.04, center_y + 0.16), size * 1.02, size * 0.48);
                details.setBrush(forest_mid);
                details.drawEllipse(QPointF(center_x - size * 0.42, center_y), size * 0.58, size * 0.62);
                details.drawEllipse(QPointF(center_x + size * 0.02, center_y - size * 0.10), size * 0.62, size * 0.72);
                details.drawEllipse(QPointF(center_x + size * 0.45, center_y + size * 0.01), size * 0.55, size * 0.60);
                details.setBrush(forest_light);
                details.drawEllipse(QPointF(center_x - size * 0.20, center_y - size * 0.31), size * 0.29, size * 0.20);
            }

            if (height < ctx.sea_level + 190 ||
                surface_hash(ctx.seed, sx, y, 0x3c6ef372u) % 9u != 0u) continue;
            const int west = ctx.elevationAt(ctx.wrappedX(sx - 1), y);
            const int east = ctx.elevationAt(ctx.wrappedX(sx + 1), y);
            const int north = ctx.elevationAt(sx, y - 1);
            const int south = ctx.elevationAt(sx, y + 1);
            if (height < west || height < east || height < north || height < south) continue;
            paintMountainPeak(details, tx, y, height, ctx.sea_level, west, east, north, south, hash);
        }
    }
}

} // namespace

QImage build_hd_surface(const Json& map, const Project* project, const QRect& region,
                        int density) {
    if (!map.contains("layers") || !map["layers"].is_object() ||
        !map["layers"].contains("elevation") || !map["layers"]["elevation"].is_array()) {
        return {};
    }

    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {};

    const bool wraps_x = aur::wraps_world_x(map);
    const bool full_map = region.isNull();
    QRect tile_bounds(0, 0, map_width, map_height);
    if (!full_map) {
        if (region.width() <= 0 || region.height() <= 0) return {};
        const int y0 = std::clamp(region.top(), 0, map_height);
        const int y1 = std::clamp(region.top() + region.height(), y0, map_height);
        const int x0 = wraps_x ? region.left() : std::clamp(region.left(), 0, map_width);
        const int x1 = wraps_x
            ? (region.left() + std::clamp(region.width(), 0, map_width))
            : std::clamp(region.left() + region.width(), x0, map_width);
        tile_bounds = QRect(x0, y0, x1 - x0, y1 - y0);
        if (tile_bounds.isEmpty()) return {};
    }

    constexpr int target_pixels_per_tile = 32;
    constexpr std::uint64_t max_surface_pixels = 16 * 1024 * 1024;
    const std::uint64_t tile_count =
        static_cast<std::uint64_t>(tile_bounds.width()) *
        static_cast<std::uint64_t>(tile_bounds.height());
    int pixels_per_tile = density > 0 ? density : target_pixels_per_tile;
    while (pixels_per_tile > 1 &&
           tile_count > max_surface_pixels /
               static_cast<std::uint64_t>(pixels_per_tile * pixels_per_tile)) {
        --pixels_per_tile;
    }

    QImage image(tile_bounds.width() * pixels_per_tile,
                 tile_bounds.height() * pixels_per_tile,
                 QImage::Format_RGB32);
    if (image.isNull()) return {};

    const int axial_tilt = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const double axis_radians = axial_tilt * 3.14159265358979323846 / 180.0;
    const double axis_sine = std::sin(axis_radians);
    const double axis_cosine = std::cos(axis_radians);

    SurfaceContext ctx{
        map,
        map_width,
        map_height,
        tile_bounds,
        pixels_per_tile,
        full_map,
        wraps_x,
        map.contains("worldgen") && map["worldgen"].contains("sea_level")
            ? map["worldgen"]["sea_level"].as_int() : 500,
        static_cast<uint32_t>(map.contains("worldgen") && map["worldgen"].contains("seed")
            ? map["worldgen"]["seed"].as_int() : 1),
        axis_sine,
        axis_cosine,
        std::max(0.001, std::abs(axis_sine) + std::abs(axis_cosine)),
        &map["layers"]["elevation"].as_array(),
        map["layers"].contains("moisture") && map["layers"]["moisture"].is_array()
            ? &map["layers"]["moisture"].as_array() : nullptr,
        map["layers"].contains("biomes") && map["layers"]["biomes"].is_array()
            ? &map["layers"]["biomes"].as_array() : nullptr,
        map["layers"].contains("water") && map["layers"]["water"].is_array()
            ? &map["layers"]["water"].as_array() : nullptr,
        {
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
        },
        {}
    };
    if (project) {
        for (const auto& [id, biome] : project->biomes) {
            if (!biome.contains("color") || !biome["color"].is_string()) continue;
            const QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) ctx.biome_colors[id] = color;
        }
    }

    buildShoreDistance(ctx);
    paintBasePixels(ctx, image);

    if (pixels_per_tile >= 4) {
        QPainter details(&image);
        details.setRenderHint(QPainter::Antialiasing);
        details.scale(pixels_per_tile, pixels_per_tile);
        details.translate(-tile_bounds.left(), -tile_bounds.top());
        const int detail_x0 = full_map ? 0 : (tile_bounds.left() - 2);
        const int detail_x1 = full_map ? map_width : (tile_bounds.left() + tile_bounds.width() + 2);
        const int detail_y0 = std::max(1, full_map ? 1 : (tile_bounds.top() - 2));
        const int detail_y1 = std::min(map_height - 1,
                                       full_map ? (map_height - 1)
                                                : (tile_bounds.top() + tile_bounds.height() + 2));
        paintWaterDetails(ctx, details, detail_x0, detail_x1, detail_y0, detail_y1);
        paintLandDetails(ctx, details, detail_x0, detail_x1, detail_y0, detail_y1);
    }
    return image;
}

} // namespace aur::world
