#include "TilesetEditorPanel.hpp"
#include <QFormLayout>

namespace aur {

TilesetEditorPanel::TilesetEditorPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel("Tileset:", this));
    tsCombo = new QComboBox(this);
    toolbar->addWidget(tsCombo);
    mainLayout->addLayout(toolbar);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    tileList = new QListWidget(this);
    splitter->addWidget(tileList);

    QWidget* inspector = new QWidget(this);
    QFormLayout* form = new QFormLayout(inspector);
    form->addRow("Tile Flags:", new QLabel("SNES Tile Attributes", inspector));
    form->addRow("SOLID:", new QCheckBox("Solid Wall", inspector));
    form->addRow("WATER:", new QCheckBox("Water / Swim", inspector));
    form->addRow("SLOW:", new QCheckBox("Slow Terrain", inspector));
    form->addRow("HURT:", new QCheckBox("Hurt / Lava", inspector));
    form->addRow("ENCOUNTER:", new QCheckBox("Random Encounters", inspector));
    form->addRow("BRIDGE:", new QCheckBox("Bridge Overpass", inspector));
    splitter->addWidget(inspector);

    splitter->setSizes({250, 750});
    mainLayout->addWidget(splitter, 1);

    refreshTilesets();
}

void TilesetEditorPanel::setProject(Project* p) {
    project = p;
    refreshTilesets();
}

void TilesetEditorPanel::refreshTilesets() {
    if (!tsCombo) return;
    tsCombo->clear();
    if (tileList) tileList->clear();
    if (project) {
        for (const auto& [tsid, ts] : project->tilesets) {
            tsCombo->addItem(QString::fromStdString(tsid));
        }
    }
}

} // namespace aur
