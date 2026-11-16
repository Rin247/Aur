#include "WorldTerrainPaint.hpp"

#include "aur/autotile.hpp"
#include "aur/world_topology.hpp"

#include <cmath>

namespace aur
{
    namespace world
    {
        namespace
        {

            void draw_surface(TerrainPaintContext &context)
            {
                if (context.surface.isNull()) return;
                QPainter &painter = context.painter;
                const QRectF bounds = context.bounds;
                painter.save();
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                if (context.repeat_x) {
                    const int first_copy = static_cast<int>(
                        std::floor((0.0 - bounds.left()) / bounds.width()));
                    const int last_copy = static_cast<int>(
                        std::ceil((context.visible.width() - bounds.left()) / bounds.width()));
                    for (int copy = first_copy; copy < last_copy; ++copy) {
                        QRectF repeated_bounds = bounds;
                        repeated_bounds.moveLeft(bounds.left() + copy * bounds.width());
                        painter.drawImage(repeated_bounds, context.surface);
                    }
                } else {
                    painter.drawImage(bounds, context.surface);
                }
                painter.restore();
            }

            void draw_overrides(TerrainPaintContext &context)
            {
                const Json *map = context.map;
                if (!map) return;
                const JsonArray *overrides = map->contains("layers") &&
                    (*map)["layers"].is_object() &&
                    (*map)["layers"].contains("terrain_overrides") &&
                    (*map)["layers"]["terrain_overrides"].is_array()
                    ? &(*map)["layers"]["terrain_overrides"].as_array() : nullptr;
                if (!overrides) return;
                QPainter &painter = context.painter;
                for (int y = context.first_y;
                     y < context.last_y && y < static_cast<int>(overrides->size()); ++y) {
                    const Json &row = (*overrides)[static_cast<size_t>(y)];
                    if (!row.is_array()) continue;
                    for (int x = context.first_x; x < context.last_x; ++x) {
                        const int source_x = context.wraps_x ? aur::wrapX(x, context.map_width) : x;
                        if (source_x < 0 ||
                            source_x >= static_cast<int>(row.as_array().size())) continue;
                        const int tile_index = row[static_cast<size_t>(source_x)].as_int();
                        if (tile_index < 0) continue;
                        const QRectF tile_rect(context.bounds.left() + x * context.cell,
                                               context.bounds.top() + y * context.cell,
                                               context.cell, context.cell);
                        const QImage tile = context.tile_image(tile_index);
                        if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                    }
                }
            }

            void draw_ground_tiles(TerrainPaintContext &context)
            {
                const JsonArray *ground = context.ground;
                if (!ground) return;
                QPainter &painter = context.painter;
                for (int y = context.first_y;
                     y < context.last_y && y < static_cast<int>(ground->size()); ++y) {
                    const auto &row = (*ground)[static_cast<size_t>(y)].as_array();
                    for (int x = context.first_x; x < context.last_x; ++x) {
                        const int source_x = context.wraps_x ? aur::wrapX(x, context.map_width) : x;
                        if (source_x < 0 || source_x >= static_cast<int>(row.size())) continue;
                        int tile_index = row[static_cast<size_t>(source_x)].as_int();
                        if (context.groups && context.tileset &&
                            context.groups->contains(tile_index) &&
                            context.groups->at(tile_index) == "ocean") {
                            const int mask = mask_at(*context.map, *context.tileset, "ground",
                                                     source_x, y, "ocean", *context.groups);
                            const auto &ocean_tiles =
                                (*context.tileset)["autotiles"]["ocean"]["tiles"].as_array();
                            if (mask >= 0 && static_cast<size_t>(mask) < ocean_tiles.size()) {
                                tile_index = ocean_tiles[static_cast<size_t>(mask)].as_int();
                            }
                        }
                        const QRectF tile_rect(context.bounds.left() + x * context.cell,
                                               context.bounds.top() + y * context.cell,
                                               context.cell, context.cell);
                        const QImage tile = context.tile_image(tile_index);
                        if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                        else painter.fillRect(tile_rect, QColor("#34404a"));
                    }
                }
            }

        } // namespace

        void draw_terrain(TerrainPaintContext &context)
        {
            if (!context.surface.isNull()) {
                draw_surface(context);
                draw_overrides(context);
                return;
            }
            draw_ground_tiles(context);
        }

    } // namespace world
} // namespace aur
