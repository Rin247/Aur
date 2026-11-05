#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPainter>
#include "aur/project.hpp"
#include "aur/forge.hpp"

namespace aur {

class CharacterSheetWidget : public QWidget {
    Q_OBJECT
    Json graphic;

public:
    explicit CharacterSheetWidget(QWidget* parent = nullptr);
    void setGraphic(const Json& g);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class ForgePanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    CharacterSheetWidget* previewWidget = nullptr;
    QLineEdit* nameEdit = nullptr;
    QComboBox* ageCombo = nullptr;
    QComboBox* raceCombo = nullptr;
    QComboBox* genderCombo = nullptr;
    QComboBox* presentationCombo = nullptr;
    QComboBox* creatureCombo = nullptr;
    QComboBox* effectCombo = nullptr;
    QComboBox* clothingCombo = nullptr;
    QComboBox* propCombo = nullptr;
    QComboBox* tileKindCombo = nullptr;
    QComboBox* tilesetCombo = nullptr;
    QSpinBox* seedSpin = nullptr;
    QLabel* statusLabel = nullptr;
    Json previewSpec;
    Json previewGraphic;

public:
    explicit ForgePanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void generateRandomNPC();
    void applyPreset(const std::string& preset_name);

private:
    void createNpc();
    void createCreature();
    void createEffect();
    void createClothing();
    void createProp();
    void createTile();
    void refreshTilesets();
};

} // namespace aur
