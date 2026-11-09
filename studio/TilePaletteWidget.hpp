#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QVBoxLayout>
#include <string>
#include <vector>
#include "aur/project.hpp"

namespace aur {

// Thumbnail grid of a tileset's tiles (editsweep D2.S1): one picker shared by
// the map editor (and later the tileset editor). Clicking selects a single
// tile; Ctrl+click extends a row-major multi-tile stamp. Right-click picks a
// tile as an eyedropper action.
class TilePaletteWidget : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* tilesetCombo = nullptr;
    QListWidget* grid = nullptr;
    std::string forced_tileset;  // when set, the dropdown is hidden

    std::string activeTilesetId() const;

public:
    explicit TilePaletteWidget(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refresh();
    // Pin the tileset (map editor: the map's tileset) and hide the dropdown.
    void setTileset(const std::string& tileset_id);
    std::string tilesetId() const;
    // Current stamp: sorted tile indices of the selection (1..n entries).
    std::vector<int> stampTiles() const;

signals:
    // A stamp (one or more tiles, ascending) was chosen with left-click.
    void stampSelected(const std::vector<int>& tiles);
    // A tile was eyedropped with right-click; -1 when empty space.
    void tilePicked(int tile_index);
};

} // namespace aur
