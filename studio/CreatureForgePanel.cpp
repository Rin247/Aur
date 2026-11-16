#include "CreatureForgePanel.hpp"
#include "forge/ForgeAssetOps.hpp"
#include "theme/LabelRoles.hpp"
#include "theme/ResponsiveSplitter.hpp"
#include "aur/gfx.hpp"

#include <QGridLayout>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>

namespace aur {

CreatureForgePanel::CreatureForgePanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* mainLayout = new QHBoxLayout(this);
    auto* controlsPanel = new QWidget(this);
    auto* controlsLayout = new QVBoxLayout(controlsPanel);
    auto* title = new QLabel("Creature Forge", this);
    theme::sectionTitle(title);
    controlsLayout->addWidget(title);
    auto* subtitle = new QLabel("One seed drives repeatable character, creature, tile, and effect generation.", this);
    subtitle->setWordWrap(true);
    theme::dimLabel(subtitle);
    controlsLayout->addWidget(subtitle);

    nameEdit = new QLineEdit("Wanderer", this);
    controlsLayout->addWidget(nameEdit);
    ageCombo = new QComboBox(this);
    ageCombo->addItems({"child", "teen", "adult", "elder"});
    controlsLayout->addWidget(ageCombo);
    controlsLayout->addWidget(new QLabel("Race", this));
    raceCombo = new QComboBox(this);
    for (const auto& [id, label] : character_race_options()) {
        raceCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    raceCombo->setCurrentIndex(raceCombo->findData("sylvani"));
    raceCombo->setToolTip("Choose a people or supernatural category defined in the World Bible.");
    controlsLayout->addWidget(raceCombo);
    genderCombo = new QComboBox(this);
    genderCombo->addItems({"unspecified", "female", "male", "non-binary"});
    genderCombo->setToolTip("Character gender identity; appearance is controlled separately.");
    controlsLayout->addWidget(genderCombo);
    presentationCombo = new QComboBox(this);
    presentationCombo->addItems({"androgynous", "feminine", "masculine"});
    presentationCombo->setToolTip("Visual style for the generated character sprite.");
    controlsLayout->addWidget(presentationCombo);
    seedSpin = new QSpinBox(this);
    seedSpin->setRange(-1000000000, 1000000000);
    seedSpin->setValue(1);
    seedSpin->setPrefix("Seed: ");
    controlsLayout->addWidget(seedSpin);

    auto* previewButton = new QPushButton("Preview NPC", this);
    auto* createNpcButton = new QPushButton("Create NPC sprite + record", this);
    auto* randomSeedButton = new QPushButton("Randomize seed", this);
    controlsLayout->addWidget(previewButton);
    controlsLayout->addWidget(createNpcButton);
    controlsLayout->addWidget(randomSeedButton);

    controlsLayout->addWidget(new QLabel("Creature / pet", this));
    creatureCombo = new QComboBox(this);
    creatureCombo->addItems({"cat", "dog", "slime", "wolf", "bat", "golem"});
    controlsLayout->addWidget(creatureCombo);
    auto* creatureButton = new QPushButton("Generate creature / pet", this);
    controlsLayout->addWidget(creatureButton);

    controlsLayout->addWidget(new QLabel("Animated effect", this));
    effectCombo = new QComboBox(this);
    effectCombo->addItems({"spark", "fire", "water", "slash", "impact", "heal", "magic", "dust", "explosion"});
    controlsLayout->addWidget(effectCombo);
    auto* effectButton = new QPushButton("Generate effect", this);
    controlsLayout->addWidget(effectButton);

    controlsLayout->addWidget(new QLabel("Clothing · drawn as editable pixel art", this));
    clothingCombo = new QComboBox(this);
    clothingCombo->addItems({"shirt", "tunic", "pants", "shorts", "skirt", "dress",
                             "cloak", "boots", "hat"});
    controlsLayout->addWidget(clothingCombo);
    auto* clothingButton = new QPushButton("Draw & add clothing", this);
    controlsLayout->addWidget(clothingButton);

    controlsLayout->addWidget(new QLabel("Props · drawn as editable pixel art", this));
    propCombo = new QComboBox(this);
    propCombo->addItems({"sword", "staff", "shield", "bow", "lantern", "satchel", "potion"});
    controlsLayout->addWidget(propCombo);
    auto* propButton = new QPushButton("Draw & add prop", this);
    controlsLayout->addWidget(propButton);

    controlsLayout->addWidget(new QLabel("Terrain tile", this));
    tilesetCombo = new QComboBox(this);
    controlsLayout->addWidget(tilesetCombo);
    tileKindCombo = new QComboBox(this);
    tileKindCombo->addItems({"grass", "sand", "dirt", "stone", "brick", "planks",
                             "water", "snow", "lava", "crystal", "foliage", "noise"});
    controlsLayout->addWidget(tileKindCombo);
    tileSolidCheck = new QCheckBox("Solid", this);
    tileSolidCheck->setToolTip("The tile blocks movement: the floor of a platformer.");
    controlsLayout->addWidget(tileSolidCheck);
    tileOneWayCheck = new QCheckBox("One-way", this);
    tileOneWayCheck->setToolTip("The tile holds from above and is open from below.");
    controlsLayout->addWidget(tileOneWayCheck);
    auto* tileButton = new QPushButton("Generate tile", this);
    controlsLayout->addWidget(tileButton);

    controlsLayout->addWidget(new QLabel("Outfit previews", this));
    auto* presetLayout = new QGridLayout();
    int presetIdx = 0;
    for (const std::string& preset : {"Adventurer", "Mage", "Knight", "Villager"}) {
        auto* button = new QPushButton(QString::fromStdString(preset), this);
        presetLayout->addWidget(button, presetIdx / 2, presetIdx % 2);
        ++presetIdx;
        connect(button, &QPushButton::clicked, this, [this, preset]() { applyPreset(preset); });
    }
    controlsLayout->addLayout(presetLayout);
    controlsLayout->addStretch();
    statusLabel = new QLabel("Generated assets are project-backed; use Ctrl+S to save.", this);
    statusLabel->setWordWrap(true);
    theme::dimLabel(statusLabel);
    controlsLayout->addWidget(statusLabel);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* controlsScroll = new QScrollArea(splitter);
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setWidget(controlsPanel);
    splitter->addWidget(controlsScroll);

    previewWidget = new CharacterSheetWidget(splitter);
    splitter->addWidget(previewWidget);
    theme::bindResponsiveSplitter(splitter, {28, 72});
    mainLayout->addWidget(splitter, 1);

    connect(previewButton, &QPushButton::clicked, this, [this]() { generateRandomNPC(); });
    connect(createNpcButton, &QPushButton::clicked, this, [this]() { createNpc(); });
    connect(randomSeedButton, &QPushButton::clicked, this, [this]() {
        seedSpin->setValue(static_cast<int>(QRandomGenerator::global()->bounded(2000000000u)) - 1000000000);
        generateRandomNPC();
    });
    connect(ageCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { generateRandomNPC(); });
    connect(raceCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { generateRandomNPC(); });
    connect(genderCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { generateRandomNPC(); });
    connect(presentationCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { generateRandomNPC(); });
    connect(seedSpin, &QSpinBox::valueChanged, this, [this](int) { generateRandomNPC(); });
    connect(nameEdit, &QLineEdit::editingFinished, this, [this]() { generateRandomNPC(); });
    connect(creatureButton, &QPushButton::clicked, this, [this]() { createCreature(); });
    connect(effectButton, &QPushButton::clicked, this, [this]() { createEffect(); });
    connect(clothingButton, &QPushButton::clicked, this, [this]() { createClothing(); });
    connect(propButton, &QPushButton::clicked, this, [this]() { createProp(); });
    connect(tileButton, &QPushButton::clicked, this, [this]() { createTile(); });

    refreshTilesets();
    generateRandomNPC();
}

void CreatureForgePanel::setProject(Project* p) {
    project = p;
    refreshTilesets();
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

void CreatureForgePanel::generateRandomNPC() {
    const std::string name = nameEdit->text().trimmed().isEmpty()
        ? "Wanderer" : nameEdit->text().trimmed().toStdString();
    previewSpec = random_character_spec(seedSpin->value(), name, ageCombo->currentText().toStdString(),
                                        genderCombo->currentText().toStdString(),
                                        presentationCombo->currentText().toStdString(),
                                        raceCombo->currentData().toString().toStdString());
    previewGraphic = compile_character(previewSpec, project ? Json(project->parts) : Json{});
    if (previewWidget) previewWidget->setGraphic(previewGraphic);
    statusLabel->setText(QString("Preview ready · seed %1 · %2")
                             .arg(seedSpin->value()).arg(ageCombo->currentText()));
}

void CreatureForgePanel::applyPreset(const std::string& preset_name) {
    Json spec = random_character_spec(seedSpin->value(), nameEdit->text().toStdString(),
                                      ageCombo->currentText().toStdString(),
                                      genderCombo->currentText().toStdString(),
                                      presentationCombo->currentText().toStdString(),
                                      raceCombo->currentData().toString().toStdString());
    forge::apply_role_preset(spec, preset_name);
    previewSpec = spec;
    previewGraphic = compile_character(previewSpec, project ? Json(project->parts) : Json{});
    if (previewWidget) previewWidget->setGraphic(previewGraphic);
    statusLabel->setText(QString("%1 preview · seed %2").arg(QString::fromStdString(preset_name))
                             .arg(seedSpin->value()));
}

void CreatureForgePanel::createNpc() {
    if (!project) return;
    if (previewSpec.is_null()) generateRandomNPC();
    const std::string base = forge::asset_id(nameEdit->text().trimmed().toStdString());
    if (base.empty()) {
        QMessageBox::warning(this, "Create NPC", "Enter a name containing letters or numbers.");
        return;
    }
    const std::string id = forge::unique_id(project->characters, base);
    previewSpec["name"] = nameEdit->text().trimmed().toStdString();
    previewSpec["age_group"] = ageCombo->currentText().toStdString();
    previewSpec["race"] = raceCombo->currentData().toString().toStdString();
    previewSpec["parts"]["age_group"] = ageCombo->currentText().toStdString();
    previewSpec["parts"]["race"] = raceCombo->currentData().toString().toStdString();
    previewSpec["gender"] = genderCombo->currentText().toStdString();
    previewSpec["parts"]["presentation"] = presentationCombo->currentText().toStdString();
    project->characters[id] = Json(JsonObject{
        {"name", previewSpec["name"]}, {"age_group", previewSpec["age_group"]},
        {"race", previewSpec["race"]}, {"graphic", id}, {"spec", previewSpec}
    });
    project->graphics[id] = compile_character(previewSpec, project->parts);
    project->invalidate(id);
    statusLabel->setText(QString("Created %1 · seed %2 · 8-way walk cycles · Ctrl+S to save")
                             .arg(QString::fromStdString(id)).arg(seedSpin->value()));
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
        statusLabel->setText(QString("Created %1 · seed %2 · Ctrl+S to save")
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
        statusLabel->setText(QString("Created animated %1 · seed %2 · Ctrl+S to save")
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
        statusLabel->setText(QString("Drew %1 · editable 32×32 pixel graphic · seed %2 · Ctrl+S to save")
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
        statusLabel->setText(QString("Drew prop %1 · seed %2 · Ctrl+S to save")
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
        // The flags are what make the tile a floor or a platform.
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
        statusLabel->setText(QString("Added %1 tile #%2 · seed %3 · Ctrl+S to save")
                                 .arg(QString::fromStdString(kind)).arg(index).arg(seed));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Tile generation failed", QString::fromStdString(error.what()));
    }
}

} // namespace aur
