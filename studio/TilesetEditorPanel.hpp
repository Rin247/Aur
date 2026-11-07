#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <array>
#include "aur/project.hpp"

namespace aur {

class TilesetEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* tsCombo = nullptr;
    QListWidget* tileList = nullptr;
    QLabel* tilePreview = nullptr;
    QLabel* tileNameLabel = nullptr;
    std::array<QCheckBox*, 6> flagChecks {};
    int currentTile = -1;
    bool updatingFlags = false;

    void selectTileset(int index);
    void selectTile(int row);
    void refreshTileDetail();
    void applyFlagChange(int bit, bool checked);
    int currentTilesetTileCount() const;
    std::string currentTilesetId() const;
    int flagsOfCurrentTile() const;

public:
    explicit TilesetEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshTilesets();
};

} // namespace aur
