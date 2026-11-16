#include "WorldPanel.hpp"
#include "theme/LabelRoles.hpp"
#include "RhiBackend.hpp"
#include "map/TileTexture.hpp"
#include "world/WorldSettingsPanel.hpp"
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
#include "aur/mapforge.hpp"
#include "MapEditorPanel.hpp"
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

void WorldPanel::generateMap() {
    if (!project)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Generate a map");
    QFormLayout form(&dialog);
    auto* kind = new QComboBox(&dialog);
    kind->addItem("Town", "town");
    kind->addItem("Cave", "cave");
    kind->addItem("Platformer room", "platformer");
    form.addRow("Kind:", kind);
    auto* seed = new QSpinBox(&dialog);
    seed->setRange(-1000000000, 1000000000);
    seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
    form.addRow("Seed:", seed);
    auto* width = new QSpinBox(&dialog);
    width->setRange(8, 128);
    width->setValue(40);
    form.addRow("Width (tiles):", width);
    auto* height = new QSpinBox(&dialog);
    height->setRange(8, 96);
    height->setValue(24);
    form.addRow("Height (tiles):", height);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;

    MapKind map_kind = MapKind::Town;
    mapKindFromName(kind->currentData().toString().toStdString(), map_kind);
    const int seed_value = seed->value();
    const std::string id = "map_" + std::to_string(seed_value);
    const std::string name = std::string(mapKindName(map_kind)) + " " + std::to_string(seed_value);
    try {
        // The tiles the map needs go into the tileset, and the map uses
        // them: the flags are per tile, so a platformer's floor cannot be
        // the field's grass.
        auto& tileset = project->tilesets["terra"];
        auto& tiles = tileset["tiles"].as_array();
        const int first_tile = static_cast<int>(tiles.size());
        const int tile_size = tileset.contains("tile") ? tileset["tile"].as_int() : 16;
        const GeneratedMap generated = generate_map_asset(map_kind, seed_value,
                                                         width->value(), height->value(),
                                                         tile_size, first_tile, id, name);
        for (const Json& tile : generated.tiles)
            tiles.push_back(tile);
        project->maps[id] = generated.map;
        project->invalidate();
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Generate map", QString::fromStdString(error.what()));
        return;
    }
    refreshGraph();
    selectMap(id);
}

void WorldPanel::expandHoveredTile() {
    if (!project || !mapCanvas)
        return;
    const QPoint cell = mapCanvas->hoveredTile();
    if (cell.x() < 0 || cell.y() < 0)
        return;
    const Json *map = project->maps.contains(current_map) ? &project->maps[current_map] : nullptr;
    if (!map)
        return;
    const std::string tileset_id = (*map)["tileset"].as_string();
    const auto tileset = project->tilesets.find(tileset_id);
    if (tileset == project->tilesets.end())
        return;
    const TerrainPick pick = pick_terrain(*map, tileset->second, "ground", cell.x(), cell.y());
    if (pick.group.empty())
        return;
    const int seed = QRandomGenerator::global()->bounded(-1000000000, 1000000000);
    const std::string id = "map_" + std::to_string(seed);
    const std::string name = pick.tile_name.empty() ? pick.group : pick.tile_name;
    try {
        auto& target = project->tilesets["terra"];
        auto& tiles = target["tiles"].as_array();
        const int first_tile = static_cast<int>(tiles.size());
        const int tile_size = target.contains("tile") ? target["tile"].as_int() : 16;
        const GeneratedMap generated = generate_map_asset(MapKind::Town, seed, 40, 24,
                                                         tile_size, first_tile, id, name, &pick);
        for (const Json& tile : generated.tiles)
            tiles.push_back(tile);
        project->maps[id] = generated.map;
        project->invalidate();
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Expand tile", QString::fromStdString(error.what()));
        return;
    }
    refreshGraph();
    selectMap(id);
    emit mapExpanded(QString::fromStdString(id));
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
