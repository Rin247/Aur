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
#include <QScrollArea>

#include <QUndoStack>

#include <memory>

#include <vector>
#include <array>

#include "aur/project.hpp"

#include "PixelSurface.hpp"
#include "UndoCommands.hpp"

namespace aur {

class TilesetEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* tsCombo = nullptr;
    QListWidget* tileList = nullptr;
    QLabel* tilePreview = nullptr;
    QLabel* tileNameLabel = nullptr;
    // D6.S1.2: the tile's pixels are edited on the shared surface, and the
    // colour picker is the tile's own palette.
    PixelSurface* tileCanvas = nullptr;
    QScrollArea* canvasScroll = nullptr;
    QComboBox* colorCombo = nullptr;
    QPushButton* exportButton = nullptr;
    QPushButton* importButton = nullptr;
    std::array<QCheckBox*, 7> flagChecks {};
    int currentTile = -1;
    bool updatingFlags = false;

    void selectTileset(int index);
    void selectTile(int row);
    void refreshTileDetail();
    void applyFlagChange(int bit, bool checked);
    void exportTilePng();
    void importTilePng();
    int currentTilesetTileCount() const;
    std::string currentTilesetId() const;
    int flagsOfCurrentTile() const;

public:
    explicit TilesetEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshTilesets();
    void undoFlagChange();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    // A tile flag changed; consumers (map collision overlay) repaint.
    void tilesetFlagsChanged(const QString& tileset_id);
};

} // namespace aur
