#include "TilesetEditorPanel.hpp"

#include "PixelFrameOps.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include "TileTexture.hpp"
#include <QFormLayout>
#include <QKeySequence>
#include <QShortcut>

namespace aur
{

    namespace
    {
        struct FlagDef
        {
            int bit;
            const char *label;
        };
        constexpr std::array<FlagDef, 7> kFlagDefs{{
            {SOLID, "Solid Wall"},
            {WATER, "Water / Swim"},
            {SLOW, "Slow Terrain"},
            {HURT, "Hurt / Lava"},
            {ENCOUNTER, "Random Encounters"},
            {BRIDGE, "Bridge Overpass"},
            {ONE_WAY, "One-way Platform"},
        }};
    } // namespace

    TilesetEditorPanel::TilesetEditorPanel(Project *p, QWidget *parent)
        : QWidget(parent), project(p)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QHBoxLayout *toolbar = new QHBoxLayout();
        toolbar->addWidget(new QLabel("Tileset:", this));
        tsCombo = new QComboBox(this);
        toolbar->addWidget(tsCombo);
        colorCombo = new QComboBox(this);
        colorCombo->setMinimumWidth(140);
        colorCombo->setToolTip("The colour the brush paints with");
        toolbar->addWidget(new QLabel("Colour:", this));
        toolbar->addWidget(colorCombo);
        // D6.S1.3: a tile in and out of a PNG.
        exportButton = new QPushButton("Export PNG", this);
        toolbar->addWidget(exportButton);
        importButton = new QPushButton("Import PNG", this);
        toolbar->addWidget(importButton);
        mainLayout->addLayout(toolbar);

        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

        tileList = new QListWidget(this);
        splitter->addWidget(tileList);

        // D6.S1.2: the tile's pixels, on the surface the pixel editor uses.
        // The canvas fits the space it is given, so a tile is never cut off.
        tileCanvas = new PixelSurface(this);
        tileCanvas->setBrushMode(1);  // one pixel at a time
        canvasScroll = new QScrollArea(splitter);
        canvasScroll->setWidgetResizable(false);
        canvasScroll->setAlignment(Qt::AlignCenter);
        canvasScroll->setFrameShape(QFrame::NoFrame);
        canvasScroll->setWidget(tileCanvas);
        canvasScroll->viewport()->installEventFilter(this);
        splitter->addWidget(canvasScroll);

        QWidget *inspector = new QWidget(this);
        QVBoxLayout *inspectorLayout = new QVBoxLayout(inspector);
        tilePreview = new QLabel(inspector);
        tilePreview->setMinimumSize(128, 128);
        tilePreview->setAlignment(Qt::AlignCenter);
        tilePreview->setStyleSheet("background:#20222c; border:1px solid #34384a;");
        inspectorLayout->addWidget(tilePreview);
        tileNameLabel = new QLabel(inspector);
        inspectorLayout->addWidget(tileNameLabel);

        QFormLayout *form = new QFormLayout();
        form->addRow(new QLabel("Tile Gameplay Flags (collision mask)", inspector));
        for (int i = 0; i < 6; ++i)
        {
            QCheckBox *box = new QCheckBox(kFlagDefs[i].label, inspector);
            flagChecks[i] = box;
            const int bit = kFlagDefs[i].bit;
            connect(box, &QCheckBox::toggled, this, [this, bit](bool checked)
                    { applyFlagChange(bit, checked); });
            form->addRow(box);
        }
        inspectorLayout->addLayout(form);
        inspectorLayout->addStretch();
        splitter->addWidget(inspector);

        splitter->setSizes({250, 500, 350});
        mainLayout->addWidget(splitter, 1);

        connect(tsCombo, &QComboBox::currentIndexChanged, this, [this](int index)
                { selectTileset(index); });
        connect(tileList, &QListWidget::currentRowChanged, this, [this](int row)
                { selectTile(row); });
        connect(colorCombo, &QComboBox::currentIndexChanged, this, [this](int index)
                { if (index >= 0) tileCanvas->setSelectedColor(index); });
        connect(exportButton, &QPushButton::clicked, this, &TilesetEditorPanel::exportTilePng);
        connect(importButton, &QPushButton::clicked, this, &TilesetEditorPanel::importTilePng);
        connect(tileCanvas, &PixelSurface::colorPicked, this, [this](int index)
                { if (index >= 0 && index < colorCombo->count()) colorCombo->setCurrentIndex(index); });
        connect(tileCanvas, &PixelSurface::painted, this, [this] { refreshTileDetail(); });
        auto *flagUndoShortcut = new QShortcut(QKeySequence::Undo, this);
        flagUndoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(flagUndoShortcut, &QShortcut::activated, this,
                &TilesetEditorPanel::undoFlagChange);

        refreshTilesets();
    }

    void TilesetEditorPanel::setProject(Project *p)
    {
        project = p;
        refreshTilesets();
    }

    void TilesetEditorPanel::refreshTilesets()
    {
        if (!tsCombo)
            return;
        updatingFlags = true;
        const QSignalBlocker comboBlocker(tsCombo);
        tsCombo->clear();
        if (tileList)
        {
            const QSignalBlocker listBlocker(tileList);
            tileList->clear();
        }
        if (project)
        {
            for (const auto &[tsid, ts] : project->tilesets)
                tsCombo->addItem(QString::fromStdString(tsid));
        }
        updatingFlags = false;
        if (tsCombo->count() > 0)
            selectTileset(tsCombo->currentIndex());
        else
            refreshTileDetail();
    }

    std::string TilesetEditorPanel::currentTilesetId() const
    {
        if (!tsCombo || tsCombo->currentIndex() < 0)
            return {};
        return tsCombo->currentText().toStdString();
    }

    int TilesetEditorPanel::currentTilesetTileCount() const
    {
        if (!project)
            return 0;
        const auto it = project->tilesets.find(currentTilesetId());
        if (it == project->tilesets.end() || !it->second.contains("tiles") ||
            !it->second["tiles"].is_array())
            return 0;
        return static_cast<int>(it->second["tiles"].as_array().size());
    }

    int TilesetEditorPanel::flagsOfCurrentTile() const
    {
        if (!project || currentTile < 0)
            return 0;
        const auto it = project->tilesets.find(currentTilesetId());
        if (it == project->tilesets.end() || !it->second.contains("tiles") ||
            !it->second["tiles"].is_array())
            return 0;
        const auto &tiles = it->second["tiles"].as_array();
        if (currentTile >= static_cast<int>(tiles.size()))
            return 0;
        const auto &tile = tiles[static_cast<size_t>(currentTile)];
        return tile.contains("flags") && tile["flags"].is_number()
                   ? tile["flags"].as_int()
                   : 0;
    }

    void TilesetEditorPanel::selectTileset(int index)
    {
        Q_UNUSED(index);
        currentTile = -1;
        if (tileList)
        {
            const QSignalBlocker listBlocker(tileList);
            tileList->clear();
            const int count = currentTilesetTileCount();
            for (int i = 0; i < count; ++i)
            {
                const auto &tiles = project->tilesets[currentTilesetId()]["tiles"].as_array();
                const auto &tile = tiles[static_cast<size_t>(i)];
                const std::string name =
                    tile.contains("name") && tile["name"].is_string()
                        ? tile["name"].as_string()
                        : "Tile " + std::to_string(i);
                tileList->addItem(QString("%1  [%2]").arg(QString::fromStdString(name), QString::number(i)));
            }
            // D6.S1.2: the canvas has something to show when the editor opens.
            if (tileList->count() > 0)
            {
                tileList->setCurrentRow(0);
                selectTile(0);
            }
        }
        refreshTileDetail();
    }

    void TilesetEditorPanel::selectTile(int row)
    {
        currentTile = row;
        refreshTileDetail();
    }

    void TilesetEditorPanel::refreshTileDetail()
    {
        if (!tilePreview)
            return;
        const int count = currentTilesetTileCount();
        const bool valid = project && currentTile >= 0 && currentTile < count;

        updatingFlags = true;
        const int flags = valid ? flagsOfCurrentTile() : 0;
        for (int i = 0; i < 6; ++i)
            flagChecks[i]->setChecked(valid && (flags & kFlagDefs[i].bit) != 0);
        for (int i = 0; i < 6; ++i)
            flagChecks[i]->setEnabled(valid);
        updatingFlags = false;

        // D6.S1.2: the surface edits the current tile's rows, drawn in the
        // palette that tile names.
        Json *rows = nullptr;
        std::vector<std::string> palette;
        int width = 0;
        int height = 0;
        if (valid)
        {
            Json &tileset = project->tilesets[currentTilesetId()];
            Json &tile = tileset["tiles"].as_array()[static_cast<size_t>(currentTile)];
            if (tile.contains("rows") && tile["rows"].is_array())
            {
                rows = &tile["rows"];
                height = static_cast<int>(rows->as_array().size());
                if (height > 0 && rows->as_array()[0].is_string())
                    width = static_cast<int>(rows->as_array()[0].as_string().size());
            }
            const int palette_index = tile.contains("pal") ? tile["pal"].as_int() : 0;
            if (tileset.contains("palettes") && tileset["palettes"].is_array())
            {
                const auto &palettes = tileset["palettes"].as_array();
                if (palette_index >= 0 && palette_index < static_cast<int>(palettes.size()) &&
                    palettes[static_cast<size_t>(palette_index)].is_array())
                    for (const Json &color : palettes[static_cast<size_t>(palette_index)].as_array())
                        palette.push_back(color.is_string() ? color.as_string() : std::string());
            }
        }
        tileCanvas->setRowsTarget(rows, palette, width, height);

        const QSignalBlocker blocker(colorCombo);
        colorCombo->clear();
        for (std::size_t index = 0; index < palette.size(); ++index)
        {
            const QString label = QString("%1 %2").arg(static_cast<int>(index), 2, 10, QChar('0'))
                                      .arg(QString::fromStdString(palette[index]));
            colorCombo->addItem(label);
        }
        if (colorCombo->count() > 1) colorCombo->setCurrentIndex(1);
        tileCanvas->setSelectedColor(std::max(1, colorCombo->currentIndex()));

        if (!valid)
        {
            tilePreview->setText("No tile selected");
            tileNameLabel->clear();
            tilePreview->setPixmap(QPixmap());
            return;
        }

        const auto &tile = project->tilesets[currentTilesetId()]["tiles"]
                               .as_array()[static_cast<size_t>(currentTile)];
        tileNameLabel->setText(QString("Index %1 · %2 rows")
                                   .arg(currentTile)
                                   .arg(tile.contains("rows") && tile["rows"].is_array()
                                            ? static_cast<int>(tile["rows"].as_array().size())
                                            : 0));
        const QString tile_name =
            tile.contains("name") && tile["name"].is_string()
                ? QString::fromStdString(tile["name"].as_string()) : QString();
        if (!tile_name.isEmpty()) tileNameLabel->setText(tile_name);

        const QImage source = make_tileset_tile(*project, currentTilesetId(), currentTile);
        if (source.isNull())
        {
            tilePreview->setText("Tile has no pixel data");
            tilePreview->setPixmap(QPixmap());
            return;
        }
        const int previewScale = std::max(1, 128 / std::max(1, std::max(source.width(), source.height())));
        tilePreview->setPixmap(QPixmap::fromImage(
            source.scaled(source.width() * previewScale, source.height() * previewScale,
                          Qt::KeepAspectRatio, Qt::FastTransformation)));
    }

    void TilesetEditorPanel::applyFlagChange(int bit, bool checked)
    {
        if (updatingFlags || !project || currentTile < 0)
            return;
        const std::string tsid = currentTilesetId();
        const auto it = project->tilesets.find(tsid);
        if (it == project->tilesets.end() || !it->second.contains("tiles") ||
            !it->second["tiles"].is_array())
            return;
        auto &tiles = it->second["tiles"].as_array();
        if (currentTile >= static_cast<int>(tiles.size()))
            return;
        auto &tile = tiles[static_cast<size_t>(currentTile)];
        const int old_flags = tile.contains("flags") && tile["flags"].is_number()
                                  ? tile["flags"].as_int()
                                  : 0;
        const int new_flags = checked ? (old_flags | bit) : (old_flags & ~bit);
        if (new_flags == old_flags)
            return;
        // Undoable + dirty (editsweep D1.S5 → D4.S1b → D6.S1.2): a flag
        // toggle is one change to the tile, and the surface's QUndoStack owns
        // the history, so one Ctrl+Z undoes the last edit whatever it was.
        // Consumers (map collision overlay) repaint via tilesetFlagsChanged.
        tileCanvas->applyChange(&tile, QStringLiteral("toggle flag"), [&tile, new_flags](Json &changed) {
            changed["flags"] = new_flags;
        });
        emit tilesetFlagsChanged(QString::fromStdString(tsid));
    }

    void TilesetEditorPanel::exportTilePng()
    {
        if (!project || currentTile < 0 || !tileCanvas)
            return;
        const QImage image = tileCanvas->toImage();
        if (image.isNull())
            return;
        const QString path = QFileDialog::getSaveFileName(
            this, "Export tile as PNG", QString(),
            "PNG images (*.png)");
        if (path.isEmpty())
            return;
        if (!image.save(path, "PNG"))
            QMessageBox::warning(this, "Export failed",
                                 QString("Could not write %1").arg(path));
    }

    void TilesetEditorPanel::importTilePng()
    {
        if (!project || currentTile < 0 || !tileCanvas)
            return;
        const QString path = QFileDialog::getOpenFileName(
            this, "Import a PNG into the tile", QString(),
            "PNG images (*.png)");
        if (path.isEmpty())
            return;
        QImage source(path);
        if (source.isNull())
        {
            QMessageBox::warning(this, "Import failed",
                                 QString("Could not read %1").arg(path));
            return;
        }
        const int size = tileCanvas->surfaceSize();
        if (size <= 0)
            return;
        const QImage scaled = source.scaled(size, size, Qt::IgnoreAspectRatio,
                                             Qt::FastTransformation);
        const QImage rgba = scaled.convertToFormat(QImage::Format_RGBA8888);
        const std::size_t byte_count = static_cast<std::size_t>(rgba.width()) *
                                       static_cast<std::size_t>(rgba.height()) * 4;
        const std::vector<unsigned char> bytes(rgba.constBits(),
                                               rgba.constBits() + byte_count);
        const JsonArray rows = editor::snap_rgba_to_rows(bytes, rgba.width(),
                                                         rgba.height(),
                                                         tileCanvas->paletteColors());
        if (rows.empty())
            return;
        Json &tile = project->tilesets[currentTilesetId()]["tiles"]
                         .as_array()[static_cast<size_t>(currentTile)];
        tileCanvas->applyChange(&tile, QStringLiteral("import png"),
                                [&rows](Json &changed) { changed["rows"] = rows; });
        refreshTileDetail();
    }

    void TilesetEditorPanel::undoFlagChange()
    {
        if (!tileCanvas)
            return;
        if (!tileCanvas->undoStack()->canUndo())
            return;
        tileCanvas->undoStack()->undo();
        refreshTileDetail();
        const std::string tsid = currentTilesetId();
        emit tilesetFlagsChanged(QString::fromStdString(tsid));
    }

    bool TilesetEditorPanel::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == canvasScroll->viewport() && event->type() == QEvent::Resize)
            tileCanvas->setAvailableViewportSize(canvasScroll->viewport()->size());
        return QWidget::eventFilter(watched, event);
    }

    void TilesetEditorPanel::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        if (canvasScroll && tileCanvas)
            tileCanvas->setAvailableViewportSize(canvasScroll->viewport()->size());
    }

} // namespace aur
