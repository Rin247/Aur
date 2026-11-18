#include "CreatureForgePanel.hpp"
#include "theme/LabelRoles.hpp"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

namespace aur {

void CreatureForgePanel::populateRaceCombo() {
    raceCombo->clear();
    auto* model = qobject_cast<QStandardItemModel*>(raceCombo->model());
    const auto add_header = [&](const QString& text) {
        raceCombo->addItem(text, QString());
        if (model) {
            if (auto* item = model->item(raceCombo->count() - 1)) {
                item->setEnabled(false);
                item->setSelectable(false);
            }
        }
    };
    add_header("Mortal Peoples");
    for (const RaceInfo& info : character_race_catalog()) {
        if (info.group == RaceGroup::Mortal) {
            raceCombo->addItem(QString::fromStdString(info.label), QString::fromStdString(info.id));
        }
    }
    add_header("Supernatural & Anomalous");
    for (const RaceInfo& info : character_race_catalog()) {
        if (info.group == RaceGroup::Supernatural) {
            raceCombo->addItem(QString::fromStdString(info.label), QString::fromStdString(info.id));
        }
    }
    raceCombo->setCurrentIndex(raceCombo->findData("kaeli"));
}

void CreatureForgePanel::refreshVariantCombo() {
    const std::string race = raceCombo->currentData().toString().toStdString();
    variantCombo->blockSignals(true);
    variantCombo->clear();
    variantCombo->addItem("Random (by seed)", "random");
    const auto options = character_variant_options(race);
    for (const auto& [id, label] : options) {
        variantCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    variantCombo->setEnabled(options.size() > 1);
    variantCombo->blockSignals(false);
}

QWidget* CreatureForgePanel::buildCharacterTab() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget(scroll);
    auto* layout = new QVBoxLayout(body);
    layout->setSpacing(8);

    auto* identityGroup = new QGroupBox("Identity && Metadata", body);
    auto* identityForm = new QFormLayout(identityGroup);
    nameEdit = new QLineEdit("Wanderer", identityGroup);
    entityRoleCombo = new QComboBox(identityGroup);
    for (const auto& [id, label] : entity_role_options()) {
        entityRoleCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    entityRoleCombo->setCurrentIndex(entityRoleCombo->findData("npc"));
    ageCombo = new QComboBox(identityGroup);
    ageCombo->addItems({"child", "teen", "adult", "elder"});
    ageCombo->setCurrentText("adult");
    genderCombo = new QComboBox(identityGroup);
    genderCombo->addItems({"unspecified", "female", "male", "non-binary"});
    presentationCombo = new QComboBox(identityGroup);
    presentationCombo->addItems({"androgynous", "feminine", "masculine"});
    alignmentCombo = new QComboBox(identityGroup);
    for (const auto& [id, label] : alignment_options()) {
        alignmentCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    identityForm->addRow("Name", nameEdit);
    identityForm->addRow("Record Role", entityRoleCombo);
    identityForm->addRow("Age", ageCombo);
    identityForm->addRow("Gender", genderCombo);
    identityForm->addRow("Presentation", presentationCombo);
    identityForm->addRow("Alignment", alignmentCombo);
    layout->addWidget(identityGroup);

    auto* originGroup = new QGroupBox("People && Origin", body);
    auto* originForm = new QFormLayout(originGroup);
    raceCombo = new QComboBox(originGroup);
    populateRaceCombo();
    variantCombo = new QComboBox(originGroup);
    refreshVariantCombo();
    affinityCombo = new QComboBox(originGroup);
    for (const auto& [id, label] : affinity_options()) {
        affinityCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    originForm->addRow("People / Type", raceCombo);
    originForm->addRow("Variant", variantCombo);
    originForm->addRow("Affinity", affinityCombo);
    layout->addWidget(originGroup);

    auto* lookGroup = new QGroupBox("Appearance", body);
    auto* lookForm = new QFormLayout(lookGroup);
    hairStyleCombo = new QComboBox(lookGroup);
    hairStyleCombo->addItem("Seed Default", "seed");
    for (const auto& [id, label] : hair_style_options()) {
        hairStyleCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    bottomCombo = new QComboBox(lookGroup);
    bottomCombo->addItem("Seed Default", "seed");
    bottomCombo->addItems({"pants", "skirt", "shorts"});
    accessoryCombo = new QComboBox(lookGroup);
    for (const auto& [id, label] : accessory_options()) {
        accessoryCombo->addItem(QString::fromStdString(label), QString::fromStdString(id));
    }
    lookForm->addRow("Hair Style", hairStyleCombo);
    lookForm->addRow("Bottom", bottomCombo);
    lookForm->addRow("Accessory", accessoryCombo);
    layout->addWidget(lookGroup);

    buildColorSwatches(body, layout);

    auto* presetGrid = new QGridLayout();
    int presetIndex = 0;
    for (const std::string& preset : {"Adventurer", "Mage", "Knight", "Villager"}) {
        auto* button = new QPushButton(QString::fromStdString(preset), body);
        presetGrid->addWidget(button, presetIndex / 2, presetIndex % 2);
        ++presetIndex;
        connect(button, &QPushButton::clicked, this, [this, preset]() { applyPreset(preset); });
    }
    layout->addLayout(presetGrid);

    auto* actionRow = new QHBoxLayout();
    auto* previewButton = new QPushButton("Re-Roll Preview", body);
    auto* createButton = new QPushButton("Create Character", body);
    actionRow->addWidget(previewButton, 1);
    actionRow->addWidget(createButton, 1);
    layout->addLayout(actionRow);
    layout->addStretch();

    connect(raceCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshVariantCombo();
        generateRandomNPC();
    });
    for (QComboBox* combo : {entityRoleCombo, ageCombo, variantCombo, affinityCombo,
                             alignmentCombo, genderCombo, presentationCombo,
                             hairStyleCombo, bottomCombo, accessoryCombo}) {
        connect(combo, &QComboBox::currentIndexChanged, this, [this](int) { generateRandomNPC(); });
    }
    connect(nameEdit, &QLineEdit::editingFinished, this, [this]() { generateRandomNPC(); });
    connect(previewButton, &QPushButton::clicked, this, [this]() { generateRandomNPC(); });
    connect(createButton, &QPushButton::clicked, this, [this]() { createNpc(); });

    scroll->setWidget(body);
    return scroll;
}

QWidget* CreatureForgePanel::buildCreatureTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setSpacing(8);

    auto* group = new QGroupBox("Creatures & Pets", tab);
    auto* form = new QFormLayout(group);
    creatureCombo = new QComboBox(group);
    creatureCombo->addItems({"cat", "dog", "slime", "wolf", "bat", "golem"});
    auto* createButton = new QPushButton("+ Create Creature / Pet", group);
    form->addRow("Species", creatureCombo);
    form->addRow(createButton);
    layout->addWidget(group);
    layout->addStretch();

    connect(createButton, &QPushButton::clicked, this, [this]() { createCreature(); });
    return tab;
}

QWidget* CreatureForgePanel::buildGearFxTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setSpacing(8);

    auto* gearGroup = new QGroupBox("Clothing && Props", tab);
    auto* gearForm = new QFormLayout(gearGroup);
    clothingCombo = new QComboBox(gearGroup);
    clothingCombo->addItems({"shirt", "tunic", "pants", "shorts", "skirt", "dress", "cloak", "boots", "hat"});
    auto* clothingButton = new QPushButton("+ Create Clothing", gearGroup);
    propCombo = new QComboBox(gearGroup);
    propCombo->addItems({"sword", "staff", "shield", "bow", "lantern", "satchel", "potion"});
    auto* propButton = new QPushButton("+ Create Prop", gearGroup);
    gearForm->addRow("Clothing", clothingCombo);
    gearForm->addRow(clothingButton);
    gearForm->addRow("Prop", propCombo);
    gearForm->addRow(propButton);
    layout->addWidget(gearGroup);

    auto* effectGroup = new QGroupBox("Visual Effects", tab);
    auto* effectForm = new QFormLayout(effectGroup);
    effectCombo = new QComboBox(effectGroup);
    effectCombo->addItems({"spark", "fire", "water", "slash", "impact", "heal", "magic", "dust", "explosion"});
    auto* effectButton = new QPushButton("+ Create Effect", effectGroup);
    effectForm->addRow("Effect", effectCombo);
    effectForm->addRow(effectButton);
    layout->addWidget(effectGroup);
    layout->addStretch();

    connect(clothingButton, &QPushButton::clicked, this, [this]() { createClothing(); });
    connect(propButton, &QPushButton::clicked, this, [this]() { createProp(); });
    connect(effectButton, &QPushButton::clicked, this, [this]() { createEffect(); });
    return tab;
}

QWidget* CreatureForgePanel::buildTileTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setSpacing(8);

    auto* group = new QGroupBox("Procedural Tile Generator", tab);
    auto* form = new QFormLayout(group);
    tilesetCombo = new QComboBox(group);
    tileKindCombo = new QComboBox(group);
    tileKindCombo->addItems({"grass", "sand", "dirt", "stone", "brick", "planks",
                             "water", "snow", "lava", "crystal", "foliage", "noise",
                             "dunes", "marsh", "ruins", "sky"});
    auto* flagsRow = new QHBoxLayout();
    tileSolidCheck = new QCheckBox("Solid", group);
    tileOneWayCheck = new QCheckBox("One-way", group);
    flagsRow->addWidget(tileSolidCheck);
    flagsRow->addWidget(tileOneWayCheck);
    auto* tileButton = new QPushButton("+ Add Tile to Tileset", group);
    form->addRow("Tileset", tilesetCombo);
    form->addRow("Surface", tileKindCombo);
    form->addRow("Collision", flagsRow);
    form->addRow(tileButton);
    layout->addWidget(group);
    layout->addStretch();

    connect(tileButton, &QPushButton::clicked, this, [this]() { createTile(); });
    return tab;
}

} // namespace aur
