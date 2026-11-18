#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QTabWidget>
#include <map>
#include <string>
#include "aur/project.hpp"
#include "aur/forge.hpp"
#include "forge/CharacterSheetWidget.hpp"

class QVBoxLayout;

namespace aur {

class CreatureForgePanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    CharacterSheetWidget* previewWidget = nullptr;

    QSpinBox* seedSpin = nullptr;
    QTabWidget* modeTabs = nullptr;

    QLineEdit* nameEdit = nullptr;
    QComboBox* entityRoleCombo = nullptr;
    QComboBox* ageCombo = nullptr;
    QComboBox* raceCombo = nullptr;
    QComboBox* variantCombo = nullptr;
    QComboBox* affinityCombo = nullptr;
    QComboBox* alignmentCombo = nullptr;
    QComboBox* genderCombo = nullptr;
    QComboBox* presentationCombo = nullptr;
    QComboBox* hairStyleCombo = nullptr;
    QComboBox* bottomCombo = nullptr;
    QComboBox* accessoryCombo = nullptr;
    QCheckBox* turnaroundCheck = nullptr;
    QComboBox* directionCombo = nullptr;

    QComboBox* creatureCombo = nullptr;
    QComboBox* effectCombo = nullptr;
    QComboBox* clothingCombo = nullptr;
    QComboBox* propCombo = nullptr;
    QComboBox* tileKindCombo = nullptr;
    QCheckBox* tileSolidCheck = nullptr;
    QCheckBox* tileOneWayCheck = nullptr;
    QComboBox* tilesetCombo = nullptr;

    std::map<std::string, QPushButton*> colorButtons;
    std::map<std::string, std::string> colorOverrides;

    Json previewSpec;
    Json previewGraphic;

public:
    explicit CreatureForgePanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void generateRandomNPC();
    void applyPreset(const std::string& preset_name);
    Q_INVOKABLE void selectPageTab(const QString& name);

private:
    QWidget* buildCharacterTab();
    QWidget* buildCreatureTab();
    QWidget* buildGearFxTab();
    QWidget* buildTileTab();
    void buildColorSwatches(QWidget* body, QVBoxLayout* layout);
    void refreshColorSwatches();
    void setStatus(const QString& text);
    void populateRaceCombo();
    void refreshVariantCombo();
    void recompilePreview();
    void createNpc();
    void createCreature();
    void createEffect();
    void createClothing();
    void createProp();
    void createTile();
    void refreshTilesets();
    QString previewStatusText() const;

protected:
    void showEvent(QShowEvent* event) override;
};

} // namespace aur
