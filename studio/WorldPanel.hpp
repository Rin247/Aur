#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "WorldModel.hpp"
#include "aur/project.hpp"
#include "world/TerrainRhiWidget.hpp"
#include "world/WorldMapTools.hpp"

class QComboBox;
class QLabel;
class QListWidget;
class QPainter;
class QProgressDialog;
class QPushButton;
class QSpinBox;
class QTimeEdit;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class QResizeEvent;
class QEvent;

namespace aur {

using WorldTool = world::WorldTool;

class WorldMapOverlayWidget;

class WorldMapCanvasWidget : public QWidget {
    Q_OBJECT

    Project* project = nullptr;
    std::string current_map;
    std::string selected_biome;
    WorldTool selected_tool = WorldTool::Inspect;
    int selected_tile = 0;
    int selected_node = -1;
    QPoint hover_tile{-1, -1};
    std::map<std::pair<std::string, int>, QImage> tile_cache;
    QImage surface_cache;
    std::string surface_cache_map;
    struct UndoAction {
        bool entities = false;
        Json state;
        std::vector<editor::WorldModel::RectSnapshot> model_rects;
    };
    std::vector<UndoAction> undo_actions;
    std::vector<UndoAction> redo_actions;
    std::vector<editor::WorldModel::RectSnapshot> stroke_model_rects;
    std::set<int> stroke_captured_cells;
    std::unique_ptr<editor::WorldModel> world_model;
    std::string world_model_map;
    std::uint64_t world_model_generation = 0;
    TerrainRhiWidget* terrain_renderer = nullptr;
    WorldMapOverlayWidget* overlay_widget = nullptr;
    bool stroke_active = false;
    QPoint stroke_last_tile{-1, -1};
    int brush_size = 1;
    qreal zoom_factor = 1.0;
    QPointF pan_offset;
    QPointF pan_start_position;
    QPointF pan_start_offset;
    bool panning = false;
    bool temperature_overlay = false;
    bool wind_overlay = false;
    bool currents_overlay = false;
    bool weather_overlay = false;
    bool daynight_overlay = true;
    bool contours_enabled = true;
    int simulation_phase = 0;
    double preview_hour = 8.0;
    std::string preview_weather = "climate";
    QTimer simulation_timer;

    QRectF mapBounds() const;
    QPoint tileAt(const QPointF& position) const;
    void constrainWorldCamera();
    QImage tileImage(const std::string& tileset_id, int tile_index);
    TerrainView terrainView() const;
    void paintTile(const QPoint& tile);
public:
    void paintCanvas(QPainter& painter);
private:
    void ensureWorldModel();
    void refreshCanvas();

public:
    explicit WorldMapCanvasWidget(QWidget* parent = nullptr);
    ~WorldMapCanvasWidget() override;
    void setProject(Project* p);
    void setMap(const std::string& map_id);
    void setTool(WorldTool tool);
    void setSelectedTile(int tile_index);
    void setBrushSize(int size);
    void setSelectedBiome(const std::string& biome_id);
    void setSelectedNode(int node_index);
    void setTemperatureOverlay(bool enabled);
    void setWindOverlay(bool enabled);
    void setCurrentsOverlay(bool enabled);
    void setWeatherOverlay(bool enabled);
    void setWeatherType(const std::string& weather);
    void setSimulationRunning(bool enabled);
    void setDayNightOverlay(bool enabled);
    void setContoursEnabled(bool enabled);
    void setTimeOfDay(int hour, int minute);
    void setZoom(qreal zoom);
    qreal zoomFactor() const noexcept { return zoom_factor; }
    void fitToView();
    void recordEntityEdit();
    void undo();
    void redo();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

signals:
    void zoomChanged(qreal zoom);
    void entityPlacementRequested(QPoint tile, aur::WorldTool tool);
    void entitySelected(int entity_index);
    void entitiesChanged();
    void timeOfDayChanged(int hour, int minute);
};

class WorldPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    std::string current_map;
    std::string selected_biome;
    QComboBox* mapCombo = nullptr;
    QComboBox* toolCombo = nullptr;
    QComboBox* landformCombo = nullptr;
    QComboBox* tileCategoryCombo = nullptr;
    QSpinBox* brushSizeSpin = nullptr;
    QListWidget* tilePalette = nullptr;
    QListWidget* biomeList = nullptr;
    QListWidget* nodeList = nullptr;
    WorldMapCanvasWidget* mapCanvas = nullptr;
    QLabel* mapInfo = nullptr;
    QSpinBox* zoomSpin = nullptr;
    QComboBox* weatherCombo = nullptr;
    QPushButton* newBiomeButton = nullptr;
    QPushButton* generateWorldButton = nullptr;
    QTimeEdit* timeOfDayEdit = nullptr;
    QProgressDialog* generationProgress = nullptr;
    std::shared_ptr<std::atomic_bool> generationCancelled;
    std::uint64_t generationRequest = 0;
    QPushButton* addLocationButton = nullptr;
    QPushButton* addExitButton = nullptr;
    QPushButton* editNodeButton = nullptr;
    QPushButton* deleteNodeButton = nullptr;

public:
    explicit WorldPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshGraph();

private:
    void selectMap(const std::string& map_id);
    void refreshMapContents();
    void filterTerrainTiles();
    void addStarterTerrainTiles();
    void createBiome();
    void createWorldEntity(const QPoint& tile, WorldTool tool);
    void editWorldEntity();
    void deleteWorldEntity();
};

} // namespace aur
