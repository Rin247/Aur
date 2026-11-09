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
#include <vector>
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
    struct FlagSnapshot { std::string tileset_id; int tile; int flags; };
    std::vector<FlagSnapshot> flag_undo;

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
    void undoFlagChange();

signals:
    // A tile flag changed; consumers (map collision overlay) repaint.
    void tilesetFlagsChanged(const QString& tileset_id);
};

} // namespace aur
