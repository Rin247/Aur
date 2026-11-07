#include "TilesetEditorPanel.hpp"
#include "TileTexture.hpp"
#include <QFormLayout>

namespace aur
{

    namespace
    {
        struct FlagDef
        {
            int bit;
            const char *label;
        };
        constexpr std::array<FlagDef, 6> kFlagDefs{{
            {SOLID, "Solid Wall"},
            {WATER, "Water / Swim"},
            {SLOW, "Slow Terrain"},
            {HURT, "Hurt / Lava"},
            {ENCOUNTER, "Random Encounters"},
            {BRIDGE, "Bridge Overpass"},
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
        mainLayout->addLayout(toolbar);

        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

        tileList = new QListWidget(this);
        splitter->addWidget(tileList);

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

        splitter->setSizes({250, 750});
        mainLayout->addWidget(splitter, 1);

        connect(tsCombo, &QComboBox::currentIndexChanged, this, [this](int index)
                { selectTileset(index); });
        connect(tileList, &QListWidget::currentRowChanged, this, [this](int row)
                { selectTile(row); });

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

        if (!valid)
        {
            tilePreview->setText("No tile selected");
            tileNameLabel->clear();
            tilePreview->setPixmap(QPixmap());
            return;
        }

        tileNameLabel->setText(QString("%1  (index %2)")
                                   .arg(tileList->currentItem()
                                            ? tileList->currentItem()->text()
                                            : QString())
                                   .arg(currentTile));

        const auto &tile = project->tilesets[currentTilesetId()]["tiles"]
                               .as_array()[static_cast<size_t>(currentTile)];
        tileNameLabel->setText(QString("Index %1 · %2 rows")
                                   .arg(currentTile)
                                   .arg(tile.contains("rows") && tile["rows"].is_array()
                                            ? static_cast<int>(tile["rows"].as_array().size())
                                            : 0));

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
        const int flags = tile.contains("flags") && tile["flags"].is_number()
                              ? tile["flags"].as_int()
                              : 0;
        tile["flags"] = checked ? (flags | bit) : (flags & ~bit);
    }

} // namespace aur
