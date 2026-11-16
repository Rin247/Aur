#include "MapEditorPanel.hpp"

#include "MapCanvas.hpp"

#include "EditorDefs.hpp"
#include "theme/ResponsiveSplitter.hpp"

#include <QKeySequence>

#include <QMessageBox>

#include <QOverload>

#include <QShortcut>

#include <QSignalBlocker>

#include <array>

namespace aur {
namespace {

void sync_mode_combo(QComboBox* modeCombo, const Json& m) {
    if (!modeCombo) return;
    const std::string mode = m.contains("mode") ? m["mode"].as_string() : "action";
    modeCombo->blockSignals(true);
    if (mode == "turn") modeCombo->setCurrentIndex(1);
    else if (mode == "tactics") modeCombo->setCurrentIndex(2);
    else modeCombo->setCurrentIndex(0);
    modeCombo->blockSignals(false);
    const char* h = "Smooth real-time exploration & combat on the field (Trials of Mana style).";
    if (mode == "turn") h = "Command-menu turn order on the exploration plane (Chrono Trigger style).";
    else if (mode == "tactics") h = "Grid-based positioning on the exploration plane (Fire Emblem / Triangle Strategy style).";
    modeCombo->setToolTip(h);
}

} // namespace

MapEditorPanel::MapEditorPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel("Map:", this));
    mapCombo = new QComboBox(this);
    mapCombo->setObjectName("mapSelector");
    toolbar->addWidget(mapCombo);

    toolbar->addWidget(new QLabel("Mode:", this));
    modeCombo = new QComboBox(this);
    modeCombo->addItems({"Real-Time Action", "Turn-Based", "Tactical"});
    modeCombo->setMinimumWidth(150);
    toolbar->addWidget(modeCombo);

    auto setModeHint = [this](int idx) {
        const char* h = nullptr;
        switch (idx) {
            case 0: h = "Smooth real-time exploration & combat on the field (Trials of Mana style)."; break;
            case 1: h = "Command-menu turn order on the exploration plane (Chrono Trigger style)."; break;
            case 2: h = "Grid-based positioning on the exploration plane (Fire Emblem / Triangle Strategy style)."; break;
            default: h = "";
        }
        modeCombo->setToolTip(h ? h : "");
    };
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, setModeHint](int idx) {
        if (!project || current_map.empty() || !project->maps.contains(current_map)) {
            setModeHint(idx);
            return;
        }
        const char* m = nullptr;
        switch (idx) {
            case 0: m = "action"; break;
            case 1: m = "turn"; break;
            case 2: m = "tactics"; break;
            default: m = "action";
        }
        auto& mapJson = project->maps[current_map];
        mapJson["mode"] = m;
        setModeHint(idx);
    });

    toolbar->addWidget(new QLabel("Layer:", this));
    layerCombo = new QComboBox(this);
    layerCombo->addItems({"Ground", "Objects", "Above", "Collision Overlay"});
    toolbar->addWidget(layerCombo);

    toolbar->addWidget(new QLabel("Tool:", this));
    toolCombo = new QComboBox(this);
    toolCombo->addItems({"Select / Move", "Paint Tile", "Place Entity / Area", "Erase Tile"});
    toolCombo->setToolTip("Tool (keys 1-4)");
    toolbar->addWidget(toolCombo);
    auto* undoButton = new QPushButton("Undo", this);
    auto* redoButton = new QPushButton("Redo", this);
    toolbar->addWidget(undoButton);
    toolbar->addWidget(redoButton);

    mainLayout->addLayout(toolbar);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    toolbar->addSpacing(8);
    zoomOutButton = new QPushButton("−", this);
    toolbar->addWidget(zoomOutButton);
    zoomLabel = new QLabel("100%", this);
    toolbar->addWidget(zoomLabel);
    zoomInButton = new QPushButton("+", this);
    toolbar->addWidget(zoomInButton);
    fitButton = new QPushButton("Fit", this);
    toolbar->addWidget(fitButton);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    QWidget* leftDock = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftDock);
    leftLayout->addWidget(new QLabel("Tile Palette:", this));
    tilePalette = new TilePaletteWidget(project, leftDock);
    leftLayout->addWidget(tilePalette);
    leftLayout->addWidget(new QLabel("Entity / Asset Palette:", this));

    entityTypeWidget = new QListWidget(this);
    entityTypeWidget->setObjectName("mapAssetPalette");
    const std::array<std::pair<const char*, const char*>, 8> entity_kinds{{
        {"spawn", "Spawn Point"}, {"exit", "Exit Door"}, {"npc", "NPC / Citizen"},
        {"chest", "Treasure Chest"}, {"sign", "Sign"}, {"trigger", "Trigger"},
        {"light", "Light"}, {"enemy", "Enemy"}}};
    for (const auto& [id, label] : entity_kinds) {
        auto* item = new QListWidgetItem(QString("[Entity] %1").arg(label), entityTypeWidget);
        item->setData(Qt::UserRole, QString::fromLatin1(id));
    }
    // Tile rows come from the shared TilePaletteWidget (D2.S1); the entity
    // list below keeps fixed indexes for the selected_entity_kind split.
    leftLayout->addWidget(entityTypeWidget);
    splitter->addWidget(leftDock);

    canvas = new MapCanvasWidget(this);
    canvas->setProject(project);
    splitter->addWidget(canvas);
    connect(toolCombo, &QComboBox::currentIndexChanged, canvas,
            &MapCanvasWidget::setTool);
    for (int key = 1; key <= 4; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            toolCombo->setCurrentIndex(key - 1);
        });
    }
    connect(undoButton, &QPushButton::clicked, canvas, &MapCanvasWidget::undo);
    connect(redoButton, &QPushButton::clicked, canvas, &MapCanvasWidget::redo);
    connect(undoShortcut, &QShortcut::activated, canvas, &MapCanvasWidget::undo);
    connect(redoShortcut, &QShortcut::activated, canvas, &MapCanvasWidget::redo);

    connect(tilePalette, &TilePaletteWidget::stampSelected, canvas,
            [this](const std::vector<int>& tiles) {
                canvas->setTool(1);  // choosing a tile implies the paint tool
                if (toolCombo->currentIndex() != 1) toolCombo->setCurrentIndex(1);
                if (tiles.size() == 1) canvas->setSelectedTile(tiles.front());
                else canvas->setSelectedStamp(tiles);
            });
    connect(tilePalette, &TilePaletteWidget::tilePicked, canvas,
            [this](int tile_index) {
                if (tile_index >= 0) {
                    canvas->setTool(1);
                    if (toolCombo->currentIndex() != 1) toolCombo->setCurrentIndex(1);
                    canvas->setSelectedTile(tile_index);
                }
            });

    auto* inspectorDock = new QWidget(this);
    QVBoxLayout* inspectorLayout = new QVBoxLayout(inspectorDock);
    QFormLayout* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    auto* deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, canvas,
            &MapCanvasWidget::deleteSelectedEntity);
    connect(entityDeleteButton, &QPushButton::clicked, canvas,
            &MapCanvasWidget::deleteSelectedEntity);
    selectedObjectLabel = new QLabel("Tile / Entity", inspectorDock);
    form->addRow("Selected Object:", selectedObjectLabel);
    idEdit = new QLineEdit(inspectorDock);
    idEdit->setReadOnly(true);
    form->addRow("Id:", idEdit);
    facingEdit = new QLineEdit("down", inspectorDock);
    form->addRow("Facing Dir:", facingEdit);
    eventCombo = new QComboBox(inspectorDock);
    eventCombo->setEditable(true);
    form->addRow("Event ID:", eventCombo);
    nameEdit = new QLineEdit(inspectorDock);
    form->addRow("Name:", nameEdit);
    textEdit = new QLineEdit(inspectorDock);
    form->addRow("Text:", textEdit);
    paramSpin = new QSpinBox(inspectorDock);
    paramSpin->setRange(-1000000, 1000000);
    form->addRow("Param:", paramSpin);
    inspectorLayout->addLayout(form);
    eventApplyButton = new QPushButton("Apply to Entity", inspectorDock);
    inspectorLayout->addWidget(eventApplyButton);
    entityDeleteButton = new QPushButton("Delete Entity (Del)", inspectorDock);
    inspectorLayout->addWidget(entityDeleteButton);
    inspectorLayout->addStretch();
    splitter->addWidget(inspectorDock);

    theme::bindResponsiveSplitter(splitter, {18, 60, 22});
    mainLayout->addWidget(splitter, 1);

    connect(mapCombo, &QComboBox::currentIndexChanged, [this](int idx) {
        if (!project) return;
        if (idx < 0 || idx >= static_cast<int>(project->maps.size())) return;
        auto it = std::next(project->maps.begin(), idx);
        current_map = it->first;
        canvas->setMap(current_map);
        canvas->fitToView();
        refreshTilePalette();
        // Sync mode selector to the loaded map's mode.
        sync_mode_combo(modeCombo, it->second);
    });

    connect(layerCombo, &QComboBox::currentIndexChanged, [this](int idx) {
        canvas->setLayer(idx);
    });
    connect(zoomInButton, &QPushButton::clicked, canvas, [this]() { canvas->zoomIn(); });
    connect(zoomOutButton, &QPushButton::clicked, canvas, [this]() { canvas->zoomOut(); });
    connect(fitButton, &QPushButton::clicked, canvas, [this]() { canvas->fitToView(); });
    connect(canvas, &MapCanvasWidget::zoomChanged, this, [this](qreal zoom) {
        zoomLabel->setText(QString("%1%").arg(qRound(zoom * 100)));
    });

    connect(entityTypeWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const QVariant data = entityTypeWidget->item(row)->data(Qt::UserRole);
        if (data.typeId() == QMetaType::Int) {
            canvas->setSelectedTile(data.toInt());
        } else {
            const QString type = data.toString();
            const auto found = std::find_if(editor::kEntityKinds.begin(), editor::kEntityKinds.end(),
                                            [&type](const char* kind) {
                                                return type == QLatin1String(kind);
                                            });
            if (found != editor::kEntityKinds.end()) {
                const int entity_kind = static_cast<int>(std::distance(editor::kEntityKinds.begin(), found));
                canvas->setSelectedEntityKind(entity_kind);
                emit canvas->entitySelectionRequested(entity_kind);
            }
        }
    });
    connect(canvas, &MapCanvasWidget::entitySelectionRequested, this, [this](int entity_kind) {
        if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
        const auto& map = project->maps.at(current_map);
        if (!map.contains("entities") || !map["entities"].is_array()) return;
        static const std::array<const char*, 8> kinds{
            "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"};
        if (entity_kind < 0 || entity_kind >= static_cast<int>(kinds.size())) return;
        const char* wanted = kinds[static_cast<size_t>(entity_kind)];
        const auto found = std::find_if(map["entities"].as_array().begin(),
            map["entities"].as_array().end(), [wanted](const Json& entity) {
                return entity.contains("type") && entity["type"].is_string() &&
                       entity["type"].as_string() == wanted;
            });
        if (found == map["entities"].as_array().end()) return;
        const QRectF bounds = canvas->mapBounds();
        const QPointF center = bounds.topLeft() + QPointF(
            (found->operator[]("x").as_int() + 0.5) * bounds.width() / map["w"].as_int(),
            (found->operator[]("y").as_int() + 0.5) * bounds.height() / map["h"].as_int());
        canvas->centerOn(center);
    });

    // Inspector: edits the clicked/selected entity instance (D3.S1), not
    // "first of kind". Kind selection in the palette only arms placement.
    syncInspectorFromInstance = [this](int instance) {
        const auto disable = [this]() {
            selectedObjectLabel->setText("Tile / Entity");
            idEdit->clear();
            facingEdit->clear();
            facingEdit->setEnabled(false);
            eventCombo->clear();
            eventCombo->setEnabled(false);
            nameEdit->clear();
            nameEdit->setEnabled(false);
            textEdit->clear();
            textEdit->setEnabled(false);
            paramSpin->setValue(0);
            paramSpin->setEnabled(false);
            eventApplyButton->setEnabled(false);
            entityDeleteButton->setEnabled(false);
        };
        const Json* entity = nullptr;
        if (project && !current_map.empty() &&
            project->maps.contains(current_map) && instance >= 0) {
            const auto& map = project->maps.at(current_map);
            if (map.contains("entities") && map["entities"].is_array() &&
                instance < static_cast<int>(map["entities"].as_array().size()))
                entity = &map["entities"].as_array()[static_cast<size_t>(instance)];
        }
        if (!entity) {
            disable();
            return;
        }
        const auto str_field = [&entity](const char* key) {
            return entity->contains(key) && (*entity)[key].is_string()
                ? QString::fromStdString((*entity)[key].as_string()) : QString();
        };
        selectedObjectLabel->setText(QString("Entity #%1: %2")
            .arg(instance)
            .arg(str_field("type")));
        idEdit->setText(str_field("id"));
        facingEdit->setText(entity->contains("facing") || entity->contains("dir")
            ? (entity->contains("facing") && (*entity)["facing"].is_string()
                   ? str_field("facing") : str_field("dir"))
            : "down");
        facingEdit->setEnabled(true);
        eventCombo->setCurrentText(str_field("event"));
        eventCombo->setEnabled(true);
        nameEdit->setText(str_field("name"));
        nameEdit->setEnabled(true);
        textEdit->setText(str_field("text"));
        textEdit->setEnabled(true);
        paramSpin->setValue(entity->contains("param") && (*entity)["param"].is_number()
            ? (*entity)["param"].as_int() : 0);
        paramSpin->setEnabled(true);
        eventApplyButton->setEnabled(true);
        entityDeleteButton->setEnabled(true);
    };
    connect(canvas, &MapCanvasWidget::entitySelected, this,
            [this](int instance) { syncInspectorFromInstance(instance); });
    connect(eventApplyButton, &QPushButton::clicked, this, [this]() {
        if (!project || current_map.empty() || !project->maps.contains(current_map))
            return;
        Json& map = project->maps[current_map];
        if (!map.contains("entities") || !map["entities"].is_array()) return;
        auto& entities = map["entities"].as_array();
        const int instance = canvas->selectedEntityInstance();
        if (instance < 0 || instance >= static_cast<int>(entities.size())) return;
        Json& entity = entities[static_cast<size_t>(instance)];
        const std::string kind = entity.contains("type") && entity["type"].is_string()
            ? entity["type"].as_string() : std::string();
        const std::string event_id = eventCombo->currentText().toStdString();
        if (!event_id.empty() && !project->events.contains(event_id)) {
            QMessageBox::warning(this, "Event ID",
                                 QString("Event '%1' is not in the project event registry.")
                                     .arg(QString::fromStdString(event_id)));
            return;
        }
        entity["facing"] = facingEdit->text().toStdString();
        if (kind == "spawn") entity["dir"] = facingEdit->text().toStdString();
        if (event_id.empty()) entity.as_object().erase("event");
        else entity["event"] = event_id;
        const std::string name = nameEdit->text().toStdString();
        if (name.empty()) entity.as_object().erase("name");
        else entity["name"] = name;
        const std::string text = textEdit->text().toStdString();
        if (text.empty()) entity.as_object().erase("text");
        else entity["text"] = text;
        if (paramSpin->value() != 0) entity["param"] = paramSpin->value();
        else entity.as_object().erase("param");
        selectedObjectLabel->setText(QString("Entity #%1 (%2 saved)")
                                         .arg(instance).arg(QString::fromStdString(kind)));
    });
    const auto refreshEventChoices = [this]() {
        if (!eventCombo) return;
        const QSignalBlocker blocker(eventCombo);
        eventCombo->clear();
        eventCombo->addItem(QString());
        if (project)
            for (const auto& [event_id, event] : project->events)
                eventCombo->addItem(QString::fromStdString(event_id));
    };
    refreshEventChoices();

    refreshMaps();
}

void MapEditorPanel::setProject(Project* p) {
    project = p;
    if (canvas) canvas->setProject(p);
    refreshMaps();
}

void MapEditorPanel::setEditorZoom(qreal factor) {
    if (canvas) canvas->zoomByFactor(factor);
}

void MapEditorPanel::openMap(const QString& id) {
    if (!mapCombo) return;
    refreshMaps();
    for (int index = 0; index < mapCombo->count(); ++index) {
        if (mapCombo->itemData(index).toString() == id) {
            mapCombo->setCurrentIndex(index);
            return;
        }
    }
}

void MapEditorPanel::refreshMaps() {
    if (!mapCombo) return;
    mapCombo->blockSignals(true);
    mapCombo->clear();
    if (project) {
        for (const auto& [mid, m] : project->maps) {
            std::string label = mid;
            if (m.contains("name")) label += " - " + m["name"].as_string();
            mapCombo->addItem(QString::fromStdString(label));
        }
        if (!project->maps.empty()) {
            canvas->setMap(project->maps.begin()->first);
            canvas->fitToView();
            current_map = project->maps.begin()->first;
            refreshTilePalette();
            sync_mode_combo(modeCombo, project->maps.begin()->second);
        }
    }
    mapCombo->blockSignals(false);
    if (mapCombo->count() > 0) mapCombo->setCurrentIndex(0);
    else {
        current_map.clear();
        canvas->setMap({});
        refreshTilePalette();
    }
}

void MapEditorPanel::refreshTilePalette() {
    if (!tilePalette) return;
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        tilePalette->setTileset({});
        return;
    }
    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map.contains("tileset") && map["tileset"].is_string()
        ? map["tileset"].as_string() : "";
    tilePalette->setTileset(tileset_id);
    tilePalette->refresh();
}

void MapEditorPanel::onTilesetChanged() {
    if (canvas) canvas->invalidateTileCache();
}

} // namespace aur
