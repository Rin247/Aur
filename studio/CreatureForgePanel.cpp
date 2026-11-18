#include "CreatureForgePanel.hpp"
#include "forge/ForgeAssetOps.hpp"
#include "theme/LabelRoles.hpp"
#include "theme/StatusStrip.hpp"
#include "theme/ResponsiveSplitter.hpp"
#include "aur/gfx.hpp"

#include <QMessageBox>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>

namespace aur {

CreatureForgePanel::CreatureForgePanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* mainLayout = new QHBoxLayout(this);
    auto* leftPane = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(6, 6, 8, 6);
    leftLayout->setSpacing(6);

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel("Character & Creature Forge", this);
    title->setToolTip("One seed drives repeatable characters, creatures, gear and tiles.");
    theme::sectionTitle(title);
    seedSpin = new QSpinBox(this);
    seedSpin->setRange(-1000000000, 1000000000);
    seedSpin->setValue(1);
    seedSpin->setPrefix("Seed: ");
    auto* randomSeedButton = new QPushButton("Random", this);
    randomSeedButton->setToolTip("Roll a new seed and preview the character.");
    headerRow->addWidget(title);
    headerRow->addStretch();
    headerRow->addWidget(seedSpin);
    headerRow->addWidget(randomSeedButton);
    leftLayout->addLayout(headerRow);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildCharacterTab(), "Character");
    tabs->addTab(buildCreatureTab(), "Creatures");
    tabs->addTab(buildGearFxTab(), "Gear && FX");
    tabs->addTab(buildTileTab(), "Tiles");
    modeTabs = tabs;
    leftLayout->addWidget(tabs, 1);

    auto* rightPane = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 4, 0, 0);
    auto* previewBar = new QHBoxLayout();
    turnaroundCheck = new QCheckBox("4-Way Turnaround", this);
    turnaroundCheck->setChecked(true);
    turnaroundCheck->setToolTip("Show the front, right, back and left views together.");
    directionCombo = new QComboBox(this);
    for (const char* direction : {"down_0", "down_right_0", "right_0", "up_right_0",
                                  "up_0", "up_left_0", "left_0", "down_left_0"}) {
        const QString name = QString::fromLatin1(direction);
        directionCombo->addItem(name, name);
    }
    directionCombo->setEnabled(false);
    previewBar->addStretch();
    previewBar->addWidget(turnaroundCheck);
    previewBar->addWidget(directionCombo);
    rightLayout->addLayout(previewBar);

    previewWidget = new CharacterSheetWidget(this);
    rightLayout->addWidget(previewWidget, 1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    theme::bindResponsiveSplitter(splitter, {34, 66});
    mainLayout->addWidget(splitter, 1);

    connect(randomSeedButton, &QPushButton::clicked, this, [this]() {
        seedSpin->setValue(static_cast<int>(QRandomGenerator::global()->bounded(2000000000u)) - 1000000000);
    });
    connect(seedSpin, &QSpinBox::valueChanged, this, [this](int) { generateRandomNPC(); });
    connect(turnaroundCheck, &QCheckBox::toggled, this, [this](bool on) {
        directionCombo->setEnabled(!on);
        previewWidget->setTurnaroundMode(on);
    });
    connect(directionCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        previewWidget->setActiveFrame(directionCombo->currentData().toString().toStdString());
    });

    refreshTilesets();
    generateRandomNPC();
}

void CreatureForgePanel::setProject(Project* p) {
    project = p;
    refreshTilesets();
}

void CreatureForgePanel::selectPageTab(const QString& name) {
    if (!modeTabs) return;
    const auto plain = [](QString text) { return text.remove('&'); };
    for (int i = 0; i < modeTabs->count(); ++i) {
        if (plain(modeTabs->tabText(i)) == plain(name)) {
            modeTabs->setCurrentIndex(i);
            return;
        }
    }
}

void CreatureForgePanel::setStatus(const QString& text) {
    theme::postStatus(text);
}

void CreatureForgePanel::generateRandomNPC() {
    if (!raceCombo) return;
    const std::string race = raceCombo->currentData().toString().toStdString();
    if (race.empty()) return;
    const std::string name = nameEdit->text().trimmed().isEmpty()
        ? "Wanderer" : nameEdit->text().trimmed().toStdString();
    try {
        previewSpec = random_character_spec(
            seedSpin->value(), name,
            ageCombo->currentText().toStdString(),
            genderCombo->currentText().toStdString(),
            presentationCombo->currentText().toStdString(),
            race,
            affinityCombo->currentData().toString().toStdString(),
            alignmentCombo->currentData().toString().toStdString(),
            entityRoleCombo->currentData().toString().toStdString(),
            variantCombo->currentData().toString().toStdString());
    } catch (const std::exception& error) {
        setStatus(QString("Generation failed: %1").arg(error.what()));
        return;
    }
    previewSpec["parts"]["race"] = race;
    const std::string hair = hairStyleCombo->currentData().toString().toStdString();
    if (hair != "seed") previewSpec["parts"]["hair_style"] = hair;
    const std::string bottom = bottomCombo->currentData().toString().toStdString();
    if (bottom != "seed") previewSpec["parts"]["bottom"] = bottom;
    const std::string accessory = accessoryCombo->currentData().toString().toStdString();
    if (!accessory.empty() && accessory != "none") {
        previewSpec["parts"]["accessory"] = JsonArray{accessory};
    }
    for (const auto& [key, hex] : colorOverrides) previewSpec["colors"][key] = hex;
    recompilePreview();
}

void CreatureForgePanel::recompilePreview() {
    previewGraphic = compile_character(previewSpec, project ? Json(project->parts) : Json{});
    if (previewWidget) previewWidget->setGraphic(previewGraphic);
    refreshColorSwatches();
    setStatus(previewStatusText());
}

QString CreatureForgePanel::previewStatusText() const {
    return QString("Preview ready \u00b7 seed %1 \u00b7 %2 \u00b7 %3")
        .arg(seedSpin->value())
        .arg(raceCombo->currentText())
        .arg(ageCombo->currentText());
}

void CreatureForgePanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!previewSpec.is_null()) setStatus(previewStatusText());
}

void CreatureForgePanel::applyPreset(const std::string& preset_name) {
    generateRandomNPC();
    forge::apply_role_preset(previewSpec, preset_name);
    recompilePreview();
    setStatus(QString("%1 preview \u00b7 seed %2")
                             .arg(QString::fromStdString(preset_name))
                             .arg(seedSpin->value()));
}

void CreatureForgePanel::createNpc() {
    if (!project) return;
    if (previewSpec.is_null()) generateRandomNPC();
    const std::string base = forge::asset_id(nameEdit->text().trimmed().toStdString());
    if (base.empty()) {
        QMessageBox::warning(this, "Create Character", "Enter a name containing letters or numbers.");
        return;
    }
    const std::string id = forge::unique_id(project->characters, base);
    const Json& parts = previewSpec["parts"];
    project->characters[id] = Json(JsonObject{
        {"name", previewSpec["name"]},
        {"age_group", previewSpec["age_group"]},
        {"race", previewSpec["race"]},
        {"variant", parts.contains("variant") ? parts["variant"] : Json("standard")},
        {"affinity", previewSpec["affinity"]},
        {"alignment", previewSpec["alignment"]},
        {"entity_role", previewSpec["entity_role"]},
        {"gender", previewSpec["gender"]},
        {"graphic", id},
        {"spec", previewSpec}
    });
    project->graphics[id] = compile_character(previewSpec, project->parts);
    project->invalidate(id);
    setStatus(QString("Created %1 (%2) \u00b7 seed %3 \u00b7 Ctrl+S to save")
                             .arg(QString::fromStdString(id))
                             .arg(entityRoleCombo->currentText())
                             .arg(seedSpin->value()));
}

void CreatureForgePanel::refreshTilesets() {
    if (!tilesetCombo) return;
    const QString selected = tilesetCombo->currentData().toString();
    tilesetCombo->clear();
    if (project) {
        for (const auto& [id, tileset] : project->tilesets) {
            const std::string name = tileset.contains("name") ? tileset["name"].as_string() : id;
            tilesetCombo->addItem(QString::fromStdString(name), QString::fromStdString(id));
        }
    }
    const int index = tilesetCombo->findData(selected);
    if (index >= 0) tilesetCombo->setCurrentIndex(index);
}

void CreatureForgePanel::createCreature() {
    if (!project) return;
    const std::string species = creatureCombo->currentText().toStdString();
    const std::string label = nameEdit->text().trimmed().isEmpty()
        ? species : nameEdit->text().trimmed().toStdString();
    const std::string id = forge::unique_id(project->db["enemies"].as_object(), forge::asset_id(label));
    try {
        Json graphic = generate_creature_graphic(species, seedSpin->value());
        project->graphics[id] = graphic;
        project->db["enemies"][id] = Json(JsonObject{
            {"name", label}, {"species", species}, {"graphic", id},
            {"pet", species == "cat" || species == "dog"}, {"seed", seedSpin->value()}
        });
        project->invalidate(id);
        previewWidget->setGraphic(graphic);
        setStatus(QString("Created %1 \u00b7 seed %2 \u00b7 Ctrl+S to save")
                                 .arg(QString::fromStdString(label)).arg(seedSpin->value()));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Creature generation failed", QString::fromStdString(error.what()));
    }
}

void CreatureForgePanel::createEffect() {
    if (!project) return;
    const std::string kind = effectCombo->currentText().toStdString();
    const std::string base = forge::asset_id(nameEdit->text().trimmed().toStdString() + "_" + kind);
    const std::string id = forge::unique_id(project->graphics, base);
    try {
        Json graphic = generate_effect_graphic(kind, seedSpin->value());
        graphic["seed"] = seedSpin->value();
        project->graphics[id] = graphic;
        project->invalidate(id);
        previewWidget->setGraphic(graphic);
        setStatus(QString("Created animated %1 \u00b7 seed %2 \u00b7 Ctrl+S to save")
                                 .arg(QString::fromStdString(kind)).arg(seedSpin->value()));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Effect generation failed", QString::fromStdString(error.what()));
    }
}

void CreatureForgePanel::createClothing() {
    if (!project) return;
    const std::string kind = clothingCombo->currentText().toStdString();
    const int seed = seedSpin->value();
    try {
        Json graphic = generate_clothing_graphic(kind, seed);
        std::string id = forge::unique_id(project->graphics, forge::asset_id(kind));
        while (project->db["items"].as_object().contains(id)) id += "_2";
        graphic["name"] = kind;
        project->graphics[id] = graphic;
        project->db["items"][id] = JsonObject{
            {"name", kind}, {"category", "clothing"}, {"slot", kind},
            {"graphic", id}, {"seed", seed}
        };
        project->invalidate(id);
        previewWidget->setGraphic(graphic);
        setStatus(QString("Drew %1 \u00b7 editable 32\u00d732 pixel graphic \u00b7 seed %2 \u00b7 Ctrl+S to save")
                                 .arg(QString::fromStdString(id)).arg(seed));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Clothing drawing failed", QString::fromStdString(error.what()));
    }
}

void CreatureForgePanel::createProp() {
    if (!project) return;
    const std::string kind = propCombo->currentText().toStdString();
    const int seed = seedSpin->value();
    try {
        Json graphic = generate_prop_graphic(kind, seed);
        std::string id = forge::unique_id(project->db["items"].as_object(), forge::asset_id(kind));
        while (project->graphics.contains(id)) id += "_2";
        graphic["name"] = kind;
        project->graphics[id] = std::move(graphic);
        project->db["items"][id] = JsonObject{
            {"name", kind}, {"category", "prop"}, {"graphic", id}, {"seed", seed}
        };
        project->invalidate(id);
        previewWidget->setGraphic(project->graphics[id]);
        setStatus(QString("Drew prop %1 \u00b7 seed %2 \u00b7 Ctrl+S to save")
                                 .arg(QString::fromStdString(id)).arg(seed));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Prop drawing failed", QString::fromStdString(error.what()));
    }
}

void CreatureForgePanel::createTile() {
    if (!project || tilesetCombo->currentIndex() < 0) return;
    const std::string tileset_id = tilesetCombo->currentData().toString().toStdString();
    auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end()) return;
    Json& tileset = tileset_it->second;
    if (!tileset["tiles"].is_array() || !tileset["palettes"].is_array() ||
        tileset["palettes"].as_array().empty()) {
        QMessageBox::warning(this, "Generate tile", "The selected tileset needs a palette bank.");
        return;
    }
    const int tile_size = tileset.contains("tile") ? tileset["tile"].as_int() : 16;
    const std::string kind = tileKindCombo->currentText().toStdString();
    const int seed = seedSpin->value();
    try {
        Json tile = generate_tile_asset(kind, seed, tile_size, tile_size);
        int flags = 0;
        if (tileSolidCheck && tileSolidCheck->isChecked()) flags |= SOLID;
        if (tileOneWayCheck && tileOneWayCheck->isChecked()) flags |= ONE_WAY;
        tile["flags"] = flags;
        auto& tiles = tileset["tiles"].as_array();
        const int index = static_cast<int>(tiles.size());
        tiles.push_back(tile);
        std::vector<std::string> palette;
        for (const Json& color : tileset["palettes"][0].as_array()) {
            palette.push_back(color.as_string());
        }
        Json graphic = new_graphic(tile_size, tile_size, palette, {0, 0});
        graphic["frames"]["0"] = tile["rows"];
        previewWidget->setGraphic(graphic);
        setStatus(QString("Added %1 tile #%2 \u00b7 seed %3 \u00b7 Ctrl+S to save")
                                 .arg(QString::fromStdString(kind)).arg(index).arg(seed));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Tile generation failed", QString::fromStdString(error.what()));
    }
}

} // namespace aur
