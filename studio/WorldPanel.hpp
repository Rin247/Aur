#pragma once

#include <QPoint>
#include <QWidget>

#include <string>

#include "aur/project.hpp"
#include "world/WorldGenerationJob.hpp"
#include "world/WorldMapCanvas.hpp"

namespace aur {
namespace world {
class TimeWeatherBar;
}
}

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimeEdit;

namespace aur {

using WorldTool = world::WorldTool;

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
    QSpinBox* zoomSpin = nullptr;
    QComboBox* weatherCombo = nullptr;
    QPushButton* newBiomeButton = nullptr;
    QPushButton* generateWorldButton = nullptr;
    world::TimeWeatherBar* timeWeather = nullptr;
    world::WorldGenerationJob* generation_job = nullptr;
    QPushButton* addLocationButton = nullptr;
    QPushButton* addExitButton = nullptr;
    QPushButton* editNodeButton = nullptr;
    QPushButton* deleteNodeButton = nullptr;

public:
    explicit WorldPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshGraph();
    // The zoom control, as a factor of the fit.
    void setEditorZoom(qreal factor);

private:
    void selectMap(const std::string& map_id);
    void refreshMapContents();

    void filterTerrainTiles();
    void createBiome();
    void createWorldEntity(const QPoint& tile, WorldTool tool);
    void editWorldEntity();
    void deleteWorldEntity();
};

} // namespace aur
