#include "TilePaletteWidget.hpp"
#include "TileTexture.hpp"

#include <QSignalBlocker>
#include <algorithm>

namespace aur {

TilePaletteWidget::TilePaletteWidget(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tilesetCombo = new QComboBox(this);
    tilesetCombo->setObjectName("tilePaletteTileset");
    layout->addWidget(tilesetCombo);
    grid = new QListWidget(this);
    grid->setObjectName("tilePaletteGrid");
    grid->setViewMode(QListView::IconMode);
    grid->setIconSize(QSize(32, 32));
    grid->setGridSize(QSize(40, 40));
    grid->setResizeMode(QListView::Adjust);
    grid->setMovement(QListView::Static);
    grid->setSelectionMode(QAbstractItemView::MultiSelection);
    grid->setUniformItemSizes(true);
    layout->addWidget(grid);

    connect(tilesetCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { refresh(); });
    connect(grid, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        emit stampSelected({item->data(Qt::UserRole).toInt()});
    });
    connect(grid, &QListWidget::customContextMenuRequested, this, [this](const QPoint&) {
        const QList<QListWidgetItem*> selected = grid->selectedItems();
        if (selected.isEmpty()) return;
        emit tilePicked(selected.first()->data(Qt::UserRole).toInt());
    });
    grid->setContextMenuPolicy(Qt::CustomContextMenu);
}

void TilePaletteWidget::setProject(Project* p) {
    project = p;
    tilesetCombo->blockSignals(true);
    tilesetCombo->clear();
    if (project)
        for (const auto& [tsid, ts] : project->tilesets)
            tilesetCombo->addItem(QString::fromStdString(tsid));
    tilesetCombo->blockSignals(false);
    if (!forced_tileset.empty()) {
        tilesetCombo->setVisible(false);
        tilesetCombo->setCurrentText(QString::fromStdString(forced_tileset));
    } else if (tilesetCombo->count() > 0) {
        tilesetCombo->setCurrentIndex(0);
    }
    refresh();
}

void TilePaletteWidget::setTileset(const std::string& tileset_id) {
    forced_tileset = tileset_id;
    tilesetCombo->setVisible(tileset_id.empty());
    tilesetCombo->setCurrentText(QString::fromStdString(tileset_id));
    refresh();
}

std::string TilePaletteWidget::tilesetId() const {
    return activeTilesetId();
}

std::string TilePaletteWidget::activeTilesetId() const {
    if (!forced_tileset.empty()) return forced_tileset;
    return tilesetCombo->currentText().toStdString();
}

void TilePaletteWidget::refresh() {
    const QSignalBlocker blocker(grid);
    grid->clear();
    if (!project || !project->tilesets.contains(activeTilesetId())) return;
    const Json& tileset = project->tilesets.at(activeTilesetId());
    if (!tileset.contains("tiles") || !tileset["tiles"].is_array()) return;
    const auto& tiles = tileset["tiles"].as_array();
    for (size_t index = 0; index < tiles.size(); ++index) {
        const QImage image = make_tileset_tile(*project, activeTilesetId(),
                                               static_cast<int>(index));
        auto* item = new QListWidgetItem(grid);
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setToolTip(QString("[Tile %1]").arg(static_cast<int>(index)));
        if (!image.isNull()) item->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

std::vector<int> TilePaletteWidget::stampTiles() const {
    std::vector<int> indices;
    const QList<QListWidgetItem*> selection = grid->selectedItems();
    indices.reserve(static_cast<size_t>(selection.size()));
    for (const QListWidgetItem* item : selection)
        indices.push_back(item->data(Qt::UserRole).toInt());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

} // namespace aur
