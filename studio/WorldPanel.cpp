#include "WorldPanel.hpp"
#include "RhiBackend.hpp"
#include "TileTexture.hpp"
#include "WorldSettingsPanel.hpp"
#include "world/HdSurface.hpp"
#include "world/WorldMapHelpers.hpp"
#include "world/MapView.hpp"
#include "world/TerrainRhiWidget.hpp"
#include "world/TimeWeatherBar.hpp"
#include "world/WorldWeatherOverlay.hpp"
#include "world/WorldClimateOverlays.hpp"
#include "world/WorldMarkerOverlays.hpp"
#include "world/WorldCamera.hpp"
#include "world/WorldEntityDialogs.hpp"
#include "aur/autotile.hpp"
#include "aur/clock.hpp"
#include "aur/game.hpp"
#include "aur/world_topology.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <queue>
#include <cstring>
#include <utility>

namespace aur {

WorldPanel::WorldPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel(this);
    title->setStyleSheet("font-size: 15px; font-weight: 700; color: #e8edf2; letter-spacing: 1px;");
    header->addWidget(title);
    header->addSpacing(14);
    header->addWidget(new QLabel("Map:", this));
    mapCombo = new QComboBox(this);
    mapCombo->setMinimumWidth(150);
    header->addWidget(mapCombo);
    header->addSpacing(12);
    header->addWidget(new QLabel("Tool:", this));
    toolCombo = new QComboBox(this);
    for (const std::string_view name : world::kWorldToolNames) {
        toolCombo->addItem(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    }
    header->addWidget(toolCombo);
    auto* undoButton = new QPushButton("Undo", this);
    auto* redoButton = new QPushButton("Redo", this);
    header->addWidget(undoButton);
    header->addWidget(redoButton);
    header->addSpacing(10);
    header->addWidget(new QLabel("Zoom:", this));
    zoomSpin = new QSpinBox(this);
    zoomSpin->setRange(5, 1600);
    zoomSpin->setSingleStep(25);
    zoomSpin->setValue(100);
    zoomSpin->setSuffix("%");
    zoomSpin->setMaximumWidth(96);
    header->addWidget(zoomSpin);
    auto* fitButton = new QPushButton("Fit", this);
    header->addWidget(fitButton);
    auto* overlayButton = new QToolButton(this);
    overlayButton->setText("Layers");
    overlayButton->setToolTip(
        "The layers to draw over the map: temperature, wind, currents, weather, day/night "
        "lighting and contours.");
    overlayButton->setPopupMode(QToolButton::InstantPopup);
    auto* overlayMenu = new QMenu(overlayButton);
    QAction* temperatureAction = overlayMenu->addAction("Temperature / equator / poles");
    temperatureAction->setCheckable(true);
    QAction* windAction = overlayMenu->addAction("Wind");
    windAction->setCheckable(true);
    QAction* currentsAction = overlayMenu->addAction("Ocean currents");
    currentsAction->setCheckable(true);
    QAction* weatherAction = overlayMenu->addAction("Weather");
    weatherAction->setCheckable(true);
    QAction* dayNightAction = overlayMenu->addAction("Day/night lighting");
    dayNightAction->setCheckable(true);
    dayNightAction->setChecked(true);
    QAction* contoursAction = overlayMenu->addAction("Terrain contours and pixel grid");
    contoursAction->setCheckable(true);
    contoursAction->setChecked(true);
    overlayMenu->addSeparator();
    // WP1: default OFF — the wrapped world shows once, camera clamped.
    QAction* repeatAction = overlayMenu->addAction("Repeat world horizontally (seam view)");
    repeatAction->setCheckable(true);
    repeatAction->setChecked(false);
    overlayMenu->addSeparator();
    QAction* simulationAction = overlayMenu->addAction("Run simulation");
    simulationAction->setCheckable(true);
    overlayButton->setMenu(overlayMenu);
    header->addWidget(overlayButton);
    header->addWidget(new QLabel("Weather:", this));
    timeWeather = new world::TimeWeatherBar(project, this);
    header->addWidget(timeWeather);
    header->addStretch(1);
    generateWorldButton = new QPushButton(QStringLiteral("New world"), this);
    generateWorldButton->setToolTip(
        "Generate a new world: a fresh seed, a new map, and the terrain to match.");
    header->addWidget(generateWorldButton);
    layout->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* sidebar = new QWidget(splitter);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(0, 0, 8, 0);
    sidebar_layout->setSpacing(6);

    auto* biome_title = new QLabel("BIOMES", sidebar);
    biome_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(biome_title);
    biomeList = new QListWidget(sidebar);
    biomeList->setMaximumHeight(190);
    sidebar_layout->addWidget(biomeList);
    newBiomeButton = new QPushButton(QStringLiteral("Create biome\u2026"), sidebar);
    sidebar_layout->addWidget(newBiomeButton);

    auto* terrain_title = new QLabel("TERRAIN", sidebar);
    terrain_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(terrain_title);
    auto* landformRow = new QHBoxLayout();
    landformCombo = new QComboBox(sidebar);
    landformCombo->clear();
    landformCombo->addItem("Selected tile");
    // WP2.S1: names resolve against the tileset at use time; missing tiles
    // fall back to the selected tile without a warning dialog.
    const std::array<std::pair<const char*, const char*>, 6> landforms{{
        {"River", "River"}, {"Lake", "Lake"}, {"Beach", "Beach"},
        {"Mountain range", "Mountain Range"}, {"Hills", "Hills"}, {"Cave", "Cave"}}};
    for (const auto& [label, tile_name] : landforms)
        landformCombo->addItem(QString::fromLatin1(label), QString::fromLatin1(tile_name));
    brushSizeSpin = new QSpinBox(sidebar);
    brushSizeSpin->setRange(1, 15);
    brushSizeSpin->setSingleStep(2);
    brushSizeSpin->setValue(1);
    brushSizeSpin->setToolTip("Odd brush width in map tiles; use a larger brush for lakes and ranges.");
    landformRow->addWidget(landformCombo, 1);
    landformRow->addWidget(brushSizeSpin);
    sidebar_layout->addLayout(landformRow);
    auto* tileToolsRow = new QHBoxLayout();
    tileCategoryCombo = new QComboBox(sidebar);
    tileCategoryCombo->addItem("All terrain", "all");
    // WP2.S1: the "Biome starters" category is removed with the starter set.
    tileCategoryCombo->addItem("Surface", "surface");
    tileCategoryCombo->addItem("Coasts", "coast");
    tileCategoryCombo->addItem("Underwater", "underwater");
    tileCategoryCombo->addItem("Elevation", "elevation");
    tileCategoryCombo->addItem("Structures", "structures");
    tileCategoryCombo->setToolTip("Filter the terrain tile palette by environment.");
    tileToolsRow->addWidget(tileCategoryCombo, 1);
    sidebar_layout->addLayout(tileToolsRow);
    tilePalette = new QListWidget(sidebar);
    tilePalette->setSelectionMode(QAbstractItemView::SingleSelection);
    tilePalette->setMaximumHeight(190);
    sidebar_layout->addWidget(tilePalette);
    auto* tileHelp = new QLabel(
        "Select a tile, then Paint terrain and click/drag. HD maps layer tiles over relief.", sidebar);
    tileHelp->setWordWrap(true);
    tileHelp->setStyleSheet("color: #8d98a3;");
    sidebar_layout->addWidget(tileHelp);

    auto* nodes_title = new QLabel("LOCATIONS & EXITS", sidebar);
    nodes_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(nodes_title);
    nodeList = new QListWidget(sidebar);
    sidebar_layout->addWidget(nodeList, 1);
    auto* node_buttons = new QHBoxLayout();
    addLocationButton = new QPushButton("Add Location", sidebar);
    addExitButton = new QPushButton("Add Exit", sidebar);
    node_buttons->addWidget(addLocationButton);
    node_buttons->addWidget(addExitButton);
    sidebar_layout->addLayout(node_buttons);
    auto* node_actions = new QHBoxLayout();
    editNodeButton = new QPushButton("Edit", sidebar);
    deleteNodeButton = new QPushButton("Delete", sidebar);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    node_actions->addWidget(editNodeButton);
    node_actions->addWidget(deleteNodeButton);
    sidebar_layout->addLayout(node_actions);
    splitter->addWidget(sidebar);

    mapCanvas = new WorldMapCanvasWidget(splitter);
    mapCanvas->setProject(project);
    splitter->addWidget(mapCanvas);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({235, 760});

    auto* footer = new QLabel(QStringLiteral("Wheel: zoom \u00b7 Ctrl+wheel: snap 1/2/4/8/16\u00d7 \u00b7 middle-drag: pan \u00b7 "
                              "Keys 1-7: tool \u00b7 Ctrl+Z/Y: undo/redo \u00b7 "
                              "Raise/Lower height or paint biome \u00b7 Add Location/Exit, then click \u00b7 "
                              "Fit: show the whole world \u00b7 Save with Ctrl+S."), this);
    footer->setStyleSheet("color: #8d98a3;");

    // Time & World settings are baked into the World editor as a second tab
    // (WANTED #3: the standalone tab is gone from MainWindow).
    auto* worldTabs = new QTabWidget(this);
    auto* mapTab = new QWidget(worldTabs);
    auto* mapTabLayout = new QVBoxLayout(mapTab);
    mapTabLayout->setContentsMargins(0, 0, 0, 0);
    mapTabLayout->setSpacing(8);
    mapTabLayout->addWidget(splitter, 1);
    mapTabLayout->addWidget(footer);
    worldTabs->addTab(mapTab, "World Map");
    worldTabs->addTab(new WorldSettingsPanel(project, worldTabs), "Settings");
    layout->addWidget(worldTabs, 1);

    generation_job = new world::WorldGenerationJob(this, this);
    connect(generation_job, &world::WorldGenerationJob::generationFinished, this,
            [this](const QString& map_id, const Json& map) {
        const std::string id = map_id.toStdString();
        if (project) project->maps[id] = map;
        current_map = id;
        refreshGraph();
    });
    connect(generation_job, &world::WorldGenerationJob::generationFailed, this,
            [this](const QString& message) {
        QMessageBox::warning(this, "World generation failed", message);
    });
    connect(undoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::undo);
    connect(redoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::redo);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::redo);

    connect(mapCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) selectMap(mapCombo->itemData(index).toString().toStdString());
    });
    connect(toolCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const int bounded = std::clamp(index, 0, world::kWorldToolCount - 1);
                mapCanvas->setTool(static_cast<WorldTool>(bounded));
            });
    for (int key = 1; key <= 7; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            toolCombo->setCurrentIndex(key - 1);
        });
    }
    toolCombo->setToolTip("Tool (keys 1-7: Inspect, Paint terrain, Paint biome, "
                          "Place location, Place exit, Raise height, Lower height)");
    connect(brushSizeSpin, &QSpinBox::valueChanged, mapCanvas,
            &WorldMapCanvasWidget::setBrushSize);
    connect(tileCategoryCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { filterTerrainTiles(); });
    connect(landformCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        // WP2.S1: landform entries resolve tiles by NAME (starter indices are
        // not stable); entries whose tile is missing are hidden instead of a
        // warning dialog. data key: name of the target terrain tile.
        static const std::array<int, 7> sizes = {1, 1, 5, 3, 5, 3, 1};
        if (index <= 0) return;
        if (!project || !project->maps.contains(current_map)) return;
        const QVariant data = landformCombo->itemData(index);
        if (!data.isValid()) return;
        const std::string wanted = data.toString().toStdString();
        const auto tileset_it = project->tilesets.find(
            project->maps[current_map]["tileset"].as_string());
        if (tileset_it == project->tilesets.end() ||
            !tileset_it->second.contains("tiles") ||
            !tileset_it->second["tiles"].is_array()) {
            landformCombo->setCurrentIndex(0);
            return;
        }
        const auto& tiles = tileset_it->second["tiles"].as_array();
        int found = -1;
        for (size_t i = 0; i < tiles.size(); ++i) {
            const Json& tile = tiles[i];
            if (tile.is_object() && tile.contains("name") &&
                tile["name"].as_string() == wanted) {
                found = static_cast<int>(i);
                break;
            }
        }
        if (found < 0) {
            landformCombo->setCurrentIndex(0);
            return;
        }
        mapCanvas->setSelectedTile(found);
        brushSizeSpin->setValue(sizes[static_cast<size_t>(index)]);
        toolCombo->setCurrentIndex(1);
        for (int row = 0; row < tilePalette->count(); ++row) {
            if (tilePalette->item(row)->data(Qt::UserRole).toInt() == found) {
                tilePalette->setCurrentRow(row);
                break;
            }
        }
    });
    connect(zoomSpin, &QSpinBox::valueChanged, this, [this](int value) {
        mapCanvas->setZoom(value / 100.0);
    });
    connect(fitButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::fitToView);
    connect(temperatureAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Temperature, enabled); });
    connect(windAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Wind, enabled); });
    connect(currentsAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Currents, enabled); });
    connect(weatherAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Weather, enabled); });
    connect(timeWeather, &world::TimeWeatherBar::weatherChanged, this,
            [this, weatherAction](const QString& kind) {
                mapCanvas->setWeatherType(kind.toStdString());
                if (!weatherAction->isChecked()) {
                    weatherAction->setChecked(true);
                }
            });
    connect(dayNightAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::DayNight, enabled); });
    connect(contoursAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setContoursEnabled);
    connect(repeatAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setRepeatWorldView);
    connect(simulationAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setSimulationRunning);
    connect(timeWeather, &world::TimeWeatherBar::timeChanged, this, [this](int hour, int minute) {
        mapCanvas->setTimeOfDay(hour, minute);
    });
    connect(mapCanvas, &WorldMapCanvasWidget::timeOfDayChanged, this,
            [this](int hour, int minute) {
                timeWeather->setTime(hour, minute);
            });
    connect(mapCanvas, &WorldMapCanvasWidget::zoomChanged, this, [this](qreal zoom) {
        const QSignalBlocker blocker(zoomSpin);
        zoomSpin->setValue(qRound(zoom * 100.0));
    });
    connect(tilePalette, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const int tile = tilePalette->item(row)->data(Qt::UserRole).toInt();
        mapCanvas->setSelectedTile(tile);
        if (landformCombo->currentIndex() != 0) landformCombo->setCurrentIndex(0);
    });
    connect(biomeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        selected_biome = biomeList->item(row)->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    });
    connect(nodeList, &QListWidget::currentRowChanged, this, [this](int row) {
        const QListWidgetItem* item = row >= 0 ? nodeList->item(row) : nullptr;
        const int entity_index = item ? item->data(Qt::UserRole).toInt() : -1;
        mapCanvas->setSelectedNode(entity_index);
        editNodeButton->setEnabled(entity_index >= 0);
        deleteNodeButton->setEnabled(entity_index >= 0);
    });
    connect(nodeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { editWorldEntity(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entitySelected, this, [this](int entity_index) {
        int row = -1;
        for (int index = 0; index < nodeList->count(); ++index) {
            if (nodeList->item(index)->data(Qt::UserRole).toInt() == entity_index) {
                row = index;
                break;
            }
        }
        nodeList->setCurrentRow(row);
    });
    connect(mapCanvas, &WorldMapCanvasWidget::entitiesChanged, this,
            [this]() { refreshMapContents(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entityPlacementRequested, this,
            [this](const QPoint& tile, WorldTool tool) { createWorldEntity(tile, tool); });
    connect(addLocationButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(3);
    });
    connect(addExitButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(4);
    });
    connect(editNodeButton, &QPushButton::clicked, this, [this]() { editWorldEntity(); });
    connect(deleteNodeButton, &QPushButton::clicked, this, [this]() { deleteWorldEntity(); });
    connect(newBiomeButton, &QPushButton::clicked, this, [this]() { createBiome(); });
    connect(generateWorldButton, &QPushButton::clicked, this, [this]() {
        if (!project || !project->tilesets.contains("world") ||
            !project->tilesets["world"].contains("tiles") ||
            project->tilesets["world"]["tiles"].as_array().size() < 8) {
            QMessageBox::warning(this, "Generate world", "World generation needs a world tileset with at least 8 terrain tiles.");
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle("Generate a new world");
        QFormLayout form(&dialog);
        auto* namingHint = new QLabel(
            "Create an original spherical world with editable terrain and a shaded, high-detail surface. "
            "The seed reproduces the same landforms; repeated seeds get a numbered copy.",
            &dialog);
        namingHint->setWordWrap(true);
        form.addRow(namingHint);
        auto* topology = new QComboBox(&dialog);
        topology->addItem("Natural continents", "continents");
        topology->addItem("Island chains", "archipelago");
        topology->addItem("One vast supercontinent", "supercontinent");
        topology->addItem("Fractured / mountainous", "fractured");
        topology->setToolTip("Choose the broad landform style. A new seed changes the actual geography.");
        form.addRow("World topology:", topology);
        auto* width = new QSpinBox(&dialog);
        width->setRange(24, 2560);
        width->setValue(256);
        auto* height = new QSpinBox(&dialog);
        height->setRange(16, 1280);
        height->setValue(128);
        form.addRow("Width (tiles):", width);
        form.addRow("Height (tiles):", height);
        auto* resolutionHint = new QLabel(
            "Default is 256 \u00d7 128 tiles (the Sylvi diorama scale). Larger maps remain available "
            "up to 2560 \u00d7 1280, but can use several GB of editor memory.",
            &dialog);
        resolutionHint->setWordWrap(true);
        form.addRow(resolutionHint);
        auto* seed = new QSpinBox(&dialog);
        seed->setRange(-1000000000, 1000000000);
        seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        auto* randomSeedButton = new QPushButton("Randomize", &dialog);
        auto* seedRow = new QHBoxLayout();
        seedRow->addWidget(seed);
        seedRow->addWidget(randomSeedButton);
        form.addRow("Seed:", seedRow);
        connect(randomSeedButton, &QPushButton::clicked, &dialog, [seed]() {
            seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        });
        auto* water = new QSpinBox(&dialog);
        water->setRange(0, 100);
        water->setValue(50);
        water->setSuffix("%");
        form.addRow("Water level:", water);
        auto* vegetation = new QSpinBox(&dialog);
        vegetation->setRange(0, 100);
        vegetation->setValue(65);
        vegetation->setSuffix("%");
        form.addRow("Vegetation:", vegetation);
        auto* decay = new QSpinBox(&dialog);
        decay->setRange(0, 100);
        decay->setValue(15);
        decay->setSuffix("%");
        form.addRow("Decay / ruins:", decay);
        auto* axialTilt = new QSpinBox(&dialog);
        axialTilt->setRange(0, 359);
        axialTilt->setValue(0);
        axialTilt->setSuffix(QStringLiteral("\u00b0"));
        axialTilt->setToolTip(QStringLiteral("0\u00b0 places the equator horizontally; this rotates the equator and pole axis on the map."));
        form.addRow("Axial tilt / equator angle:", axialTilt);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;

        const int seed_value = seed->value();
        const std::string seed_text = std::to_string(seed_value);
        const std::string base_id = world::make_biome_id("new_world_seed_" + seed_text);
        const std::string base_name = "New World " + seed_text;
        std::string id = base_id;
        std::string name = base_name;
        int copy_number = 2;
        while (project->maps.contains(id)) {
            id = base_id + "_" + std::to_string(copy_number);
            name = base_name + " (" + std::to_string(copy_number) + ")";
            ++copy_number;
        }
        world::WorldGenerationParams params;
        params.map_id = id;
        params.map_name = name;
        params.width = width->value();
        params.height = height->value();
        params.seed = seed_value;
        params.topology = topology->currentData().toString().toStdString();
        params.water = water->value();
        params.vegetation = vegetation->value();
        params.decay = decay->value();
        params.axial_tilt = axialTilt->value();
        generation_job->start(std::move(params), project);
    });

    refreshGraph();
}

void WorldPanel::setProject(Project* p) {
    project = p;
    mapCanvas->setProject(p);
    refreshGraph();
}

void WorldPanel::setEditorZoom(qreal factor) {
    if (mapCanvas) mapCanvas->zoomByFactor(factor);
}

void WorldPanel::refreshGraph() {
    if (!mapCombo) return;
    mapCombo->blockSignals(true);
    mapCombo->clear();

    if (project) {
        for (const auto& [map_id, map] : project->maps) {
            if (map.contains("kind") && map["kind"].as_string() == "world") {
                const std::string name = map.contains("name") ? map["name"].as_string() : map_id;
                mapCombo->addItem(QString::fromStdString(name), QString::fromStdString(map_id));
            }
        }
    }

    int selected_index = -1;
    for (int index = 0; index < mapCombo->count(); ++index) {
        if (mapCombo->itemData(index).toString().toStdString() == current_map) {
            selected_index = index;
            break;
        }
    }
    if (selected_index < 0 && mapCombo->count() > 0) selected_index = 0;
    mapCombo->setCurrentIndex(selected_index);
    mapCombo->blockSignals(false);

    if (selected_index >= 0) selectMap(mapCombo->itemData(selected_index).toString().toStdString());
    else {
        current_map.clear();
        mapCanvas->setMap({});
        tilePalette->clear();
        biomeList->clear();
        nodeList->clear();
        mapCombo->setToolTip("No world maps in project");
    }
}

void WorldPanel::selectMap(const std::string& map_id) {
    const bool first_show = current_map.empty();
    current_map = map_id;
    mapCanvas->setMap(map_id);
    refreshMapContents();
    // First boot / first selection shows the whole world (WANTED: the editor
    // used to open zoomed to 100% on a corner of the 256×128 map).
    if (first_show)
        mapCanvas->fitToView();
}

void WorldPanel::refreshMapContents() {
    tilePalette->clear();
    biomeList->clear();
    nodeList->clear();
    mapCanvas->setSelectedNode(-1);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        mapCombo->setToolTip("");
        return;
    }

    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    QString info = QString(QStringLiteral("%1 \u00d7 %2 \u00b7 %3"))
        .arg(map["w"].as_int()).arg(map["h"].as_int())
        .arg(QString::fromStdString(tileset_id));
    if (map.contains("worldgen") && map["worldgen"].contains("style") &&
        map["worldgen"]["style"].as_string() == "hd2d") {
        info += QString(QStringLiteral(" \u00b7 HD \u00b7 %1")).arg(QString::fromStdString(
            map["worldgen"]["topology"].as_string()));
    }
    mapCombo->setToolTip(info);

    const auto tileset = project->tilesets.find(tileset_id);
    if (tileset != project->tilesets.end()) {
        const auto& tiles = tileset->second["tiles"].as_array();
        tilePalette->setIconSize(QSize(40, 40));
        for (size_t index = 0; index < tiles.size(); ++index) {
            const Json& tile = tiles[index];
            const std::string name = tile.contains("name")
                ? tile["name"].as_string() : "Tile " + std::to_string(index);
            auto* item = new QListWidgetItem(
                QString("%1  %2").arg(index, 2, 10, QChar('0')).arg(QString::fromStdString(name)),
                tilePalette);
            const QImage preview = make_tileset_tile(*project, tileset_id, static_cast<int>(index));
            if (!preview.isNull()) {
                item->setIcon(QIcon(QPixmap::fromImage(preview).scaled(
                    40, 40, Qt::KeepAspectRatio, Qt::FastTransformation)));
            }
            item->setData(Qt::UserRole, static_cast<int>(index));
            item->setData(Qt::UserRole + 1,
                          QString::fromStdString(world::terrain_tile_category(name)));
        }
    }
    if (tilePalette->count() > 0) tilePalette->setCurrentRow(0);
    filterTerrainTiles();

    for (const auto& [biome_id, biome] : project->biomes) {
        if (!biome.contains("tileset") || biome["tileset"].as_string() != tileset_id) continue;
        const std::string name = biome.contains("name") ? biome["name"].as_string() : biome_id;
        auto* item = new QListWidgetItem(QString::fromStdString(name), biomeList);
        item->setData(Qt::UserRole, QString::fromStdString(biome_id));
        if (biome.contains("color")) {
            QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) item->setForeground(color);
        }
        if (biome_id == selected_biome) biomeList->setCurrentItem(item);
    }
    if (biomeList->count() > 0 && biomeList->currentRow() < 0) biomeList->setCurrentRow(0);
    if (biomeList->currentItem()) {
        selected_biome = biomeList->currentItem()->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    }

    if (map.contains("entities") && map["entities"].is_array()) {
        const auto& entities = map["entities"].as_array();
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& entity = entities[index];
            const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
            if (type != "location" && type != "exit") continue;
            std::string label = type == "exit" ? "Exit" : "Location";
            if (entity.contains("target") && entity["target"].contains("map")) {
                label += " \u2192 " + entity["target"]["map"].as_string();
                if (entity["target"].contains("spawn") && entity["target"]["spawn"].is_string() &&
                    !entity["target"]["spawn"].as_string().empty()) {
                    label += " / " + entity["target"]["spawn"].as_string();
                }
            } else if (entity.contains("id")) {
                label += " \u00b7 " + entity["id"].as_string();
            }
            label += "  (" + std::to_string(entity["x"].as_int()) + ", " +
                     std::to_string(entity["y"].as_int()) + ")";
            auto* item = new QListWidgetItem(QString::fromStdString(label), nodeList);
            item->setData(Qt::UserRole, static_cast<int>(index));
        }
    }
}

void WorldPanel::filterTerrainTiles() {
    if (!tilePalette || !tileCategoryCombo) return;
    const QString category = tileCategoryCombo->currentData().toString();
    int first_visible = -1;
    for (int row = 0; row < tilePalette->count(); ++row) {
        QListWidgetItem* item = tilePalette->item(row);
        const bool visible = category == "all" ||
            item->data(Qt::UserRole + 1).toString() == category;
        item->setHidden(!visible);
        if (visible && first_visible < 0) first_visible = row;
    }
    const int selected_row = tilePalette->currentRow();
    if (selected_row < 0 || tilePalette->item(selected_row)->isHidden()) {
        tilePalette->setCurrentRow(first_visible);
    }
}

void WorldPanel::createBiome() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end() ||
        !tileset_it->second.contains("tiles") || tileset_it->second["tiles"].as_array().empty()) {
        QMessageBox::warning(this, "Create biome", "The selected map has no usable tileset.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Create biome");
    QFormLayout form(&dialog);
    QLineEdit nameEdit(&dialog);
    nameEdit.setPlaceholderText("e.g. Moonlit Coast");
    form.addRow("Name:", &nameEdit);

    QComboBox climateCombo(&dialog);
    climateCombo.addItems({"temperate", "tropical", "arid", "arctic", "humid", "volcanic", "arcane"});
    form.addRow("Climate:", &climateCombo);

    QComboBox baseTileCombo(&dialog);
    const auto& tiles = tileset_it->second["tiles"].as_array();
    for (size_t index = 0; index < tiles.size(); ++index) {
        const std::string name = tiles[index].contains("name")
            ? tiles[index]["name"].as_string() : "Tile " + std::to_string(index);
        baseTileCombo.addItem(QString(QStringLiteral("%1 \u00b7 %2")).arg(index).arg(QString::fromStdString(name)),
                              static_cast<int>(index));
    }
    if (tilePalette->currentRow() >= 0) baseTileCombo.setCurrentIndex(tilePalette->currentRow());
    form.addRow("Base terrain:", &baseTileCombo);

    auto* colorEdit = new QLineEdit("#75b96b", &dialog);
    auto* colorButton = new QPushButton(QStringLiteral("Choose\u2026"), &dialog);
    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(colorEdit);
    colorRow->addWidget(colorButton);
    form.addRow("Map tint:", colorRow);
    connect(colorButton, &QPushButton::clicked, &dialog, [&dialog, colorEdit]() {
        const QColor selected = QColorDialog::getColor(QColor(colorEdit->text()), &dialog,
                                                       "Biome overlay color");
        if (selected.isValid()) colorEdit->setText(selected.name());
    });

    QLineEdit descriptionEdit(&dialog);
    descriptionEdit.setPlaceholderText("Optional notes about this region");
    form.addRow("Description:", &descriptionEdit);
    auto add_percent_field = [&form, &dialog](const QString& label, int value) {
        auto* field = new QSpinBox(&dialog);
        field->setRange(0, 100);
        field->setValue(value);
        field->setSuffix("%");
        form.addRow(label, field);
        return field;
    };
    QSpinBox* vegetation = add_percent_field("Vegetation:", 60);
    QSpinBox* decay = add_percent_field("Decay:", 10);
    QSpinBox* waterLevel = add_percent_field("Water level:", 50);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const std::string name = nameEdit.text().trimmed().toStdString();
    const QColor color(colorEdit->text());
    if (name.empty() || !color.isValid()) {
        QMessageBox::warning(this, "Create biome", "Enter a biome name and a valid color.");
        return;
    }

    std::string id = world::make_biome_id(name);
    if (id.empty()) {
        QMessageBox::warning(this, "Create biome", "The name must contain at least one letter or number.");
        return;
    }
    const std::string base_id = id;
    int suffix = 2;
    while (project->biomes.contains(id)) id = base_id + "_" + std::to_string(suffix++);

    Json biome;
    biome["name"] = name;
    biome["climate"] = climateCombo.currentText().toStdString();
    biome["tileset"] = tileset_id;
    biome["base_tile"] = baseTileCombo.currentData().toInt();
    biome["color"] = color.name().toStdString();
    biome["description"] = descriptionEdit.text().trimmed().toStdString();
    biome["vegetation"] = vegetation->value();
    biome["decay"] = decay->value();
    biome["water_level"] = waterLevel->value();
    project->biomes[id] = biome;
    selected_biome = id;
    refreshMapContents();
}

void WorldPanel::createWorldEntity(const QPoint& tile, WorldTool tool) {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !world::is_entity_placement(tool)) return;
    Json& map = project->maps[current_map];
    if (tile.x() < 0 || tile.y() < 0 || tile.x() >= map["w"].as_int() ||
        tile.y() >= map["h"].as_int()) return;
    const JsonArray no_entities;
    const JsonArray& existing_entities = map.contains("entities") && map["entities"].is_array()
        ? map["entities"].as_array() : no_entities;
    int added_index = -1;

    if (tool == WorldTool::PlaceLocation) {
        bool accepted = false;
        const QString entered_id = QInputDialog::getText(
            this, "Add world location", "Location ID:", QLineEdit::Normal,
            "new_location", &accepted).trimmed();
        if (!accepted) return;
        const std::string id = world::make_biome_id(entered_id.toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Add world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (const Json& entity : existing_entities) {
            if (entity.contains("id") && entity["id"].is_string() &&
                entity["id"].as_string() == id) {
                QMessageBox::warning(this, "Add world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(Json(JsonObject{
            {"id", id}, {"type", "location"}, {"x", tile.x()}, {"y", tile.y()}
        }));
        added_index = static_cast<int>(entities.size()) - 1;
    } else {
        const std::optional<world::WorldExitTarget> target =
            world::run_add_exit_dialog(this, *project);
        if (!target) return;
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(world::make_exit_entity(
            tile.x(), tile.y(), target->map, target->spawn));
        added_index = static_cast<int>(entities.size()) - 1;
    }

    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == added_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::editWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    Json& entity = entities[static_cast<size_t>(entity_index)];
    const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
    if (type != "location" && type != "exit") return;

    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (type == "location") {
        const std::optional<world::WorldLocationEdits> edits =
            world::run_edit_location_dialog(this, map_width, map_height, entity);
        if (!edits) return;
        const std::string id = world::make_biome_id(edits->id_text.toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Edit world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& other = entities[index];
            if (static_cast<int>(index) != entity_index && other.contains("id") &&
                other["id"].is_string() && other["id"].as_string() == id) {
                QMessageBox::warning(this, "Edit world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
        mapCanvas->recordEntityEdit();
        entity["id"] = id;
        entity["x"] = edits->x;
        entity["y"] = edits->y;
    } else {
        const std::optional<world::WorldExitEdits> edits =
            world::run_edit_exit_dialog(this, *project, map_width, map_height, entity);
        if (!edits) return;
        if (edits->target_map.empty()) {
            QMessageBox::warning(this, "Edit world exit", "The project has no target maps.");
            return;
        }
        mapCanvas->recordEntityEdit();
        entity["target"]["map"] = edits->target_map;
        entity["target"]["spawn"] = edits->target_spawn;
        entity["x"] = edits->x;
        entity["y"] = edits->y;
    }
    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == entity_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::deleteWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    const auto answer = QMessageBox::question(
        this, "Delete world marker", "Delete the selected location or exit?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    mapCanvas->recordEntityEdit();
    entities.erase(entities.begin() + entity_index);
    refreshMapContents();
}

} // namespace aur
