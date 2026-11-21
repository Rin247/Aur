#pragma once
#include "aur/json.hpp"
#include "aur/project.hpp"
#include "world/TerrainRhiWidget.hpp"
#include "world/WorldCommandHistory.hpp"
#include "world/WorldMapTools.hpp"
#include "world/WorldStrokeState.hpp"

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

class QComboBox;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class QResizeEvent;
class QEvent;

namespace aur
{

    using world::WorldTool;
    using world::WorldOverlay;

    class WorldMapOverlayWidget;

    class WorldMapCanvasWidget : public QWidget
    {
        Q_OBJECT

        Project *project = nullptr;
        std::string current_map;
        std::string selected_biome;
        WorldTool selected_tool = WorldTool::Inspect;
        int selected_tile = 0;
        int selected_node = -1;
        QPoint hover_tile{-1, -1};
        std::map<std::pair<std::string, int>, QImage> tile_cache;
        QImage surface_cache;
        QImage coarse_cache;
        QImage close_region_cache;
        std::string surface_cache_map;
        std::string coarse_cache_map;
        world::CloseRegionKey close_region_key;
        editor::WorldCommandHistory history;
        world::WorldStrokeState stroke;
        std::unique_ptr<editor::WorldModel> world_model;
        std::string world_model_map;
        std::uint64_t world_model_generation = 0;
        TerrainRhiWidget *terrain_renderer = nullptr;
        WorldMapOverlayWidget *overlay_widget = nullptr;
        int brush_size = 1;
        double height_strength_setting = 0.015;  // fraction of the R16 range
        qreal zoom_factor = 1.0;
        qreal user_zoom_multiplier = 1.0;
        QPointF pan_offset;
        QPointF pan_start_position;
        QPointF pan_start_offset;
        bool panning = false;
        bool temperature_overlay = false;
        bool wind_overlay = false;
        bool currents_overlay = false;
        bool repeat_world_view = false;  // WP1: off = show the wrapped world once
        bool weather_overlay = false;
        bool daynight_overlay = true;
        bool contours_enabled = true;
        bool globe_view = false;
        bool globe_drag = false;
        double globe_yaw = -0.6;
        double globe_pitch = -0.3;
        double globe_yaw_start = 0.0;
        double globe_pitch_start = 0.0;
        QPointF globe_drag_start;
        QImage globe_cache;
        QSize globe_cache_size;
        double globe_cache_yaw = 0.0;
        double globe_cache_pitch = 0.0;
        QImage globe_source;
        std::string globe_source_map;
        bool globe_rebuild_scheduled = false;
        int simulation_phase = 0;
        double preview_hour = 8.0;
        std::string preview_weather = "climate";
        QTimer simulation_timer;
        // The first fit waits for a real widget size, like the map editor's.
        bool fit_pending = false;
        qreal fitZoomForCurrentSize() const;
        void applyFit();
        void requestPendingFit();

        QRectF mapBounds() const;
        QPoint tileAt(const QPointF &position) const;
        void constrainWorldCamera();
        QImage tileImage(const std::string &tileset_id, int tile_index);
        TerrainView terrainView() const;
        void paintTile(const QPoint &tile);

    public:
        void paintCanvas(QPainter &painter);
        // The globe: the same surface, projected onto a sphere.
        void paintGlobe(QPainter &painter, const QImage &surface);
        // WP1: masks the left/right margins so the GPU terrain's Repeat-U
        // sampler shows only one world copy (called from the overlay widget).
        void paintRepeatMask(QPainter &painter);

    private:
        void ensureWorldModel();
        void refreshCanvas();

    public:
        explicit WorldMapCanvasWidget(QWidget *parent = nullptr);
        ~WorldMapCanvasWidget() override;
        void setProject(Project *p);
        void setMap(const std::string &map_id);
        void setTool(WorldTool tool);
        void setSelectedTile(int tile_index);
        void setBrushSize(int size);
        void setHeightStrength(double fraction) { height_strength_setting = fraction; }
        void setRepeatWorldView(bool enabled);
        void setSelectedBiome(const std::string &biome_id);
        void setSelectedNode(int node_index);
        void setOverlay(WorldOverlay overlay, bool enabled);
        void setWeatherType(const std::string &weather);
        void setSimulationRunning(bool enabled);
        void setContoursEnabled(bool enabled);
        void setGlobeView(bool enabled);
        bool globeView() const noexcept { return globe_view; }
        bool globeOnGpu() const;
        void setTimeOfDay(int hour, int minute);
        void setZoom(qreal zoom);
        qreal zoomFactor() const noexcept { return zoom_factor; }
        void fitToView();
        // The cell under the cursor, for the panel that picks a tile.
        QPoint hoveredTile() const { return hover_tile; }
        // The zoom control, as a factor of the fit.
        void zoomByFactor(qreal factor);
        void recordEntityEdit();
        void undo();
        void redo();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void showEvent(QShowEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        void leaveEvent(QEvent *event) override;

    signals:
        void zoomChanged(qreal zoom);
        void entityPlacementRequested(QPoint tile, aur::WorldTool tool);
        void entitySelected(int entity_index);
        void entitiesChanged();
        void timeOfDayChanged(int hour, int minute);
    };

} // namespace aur
