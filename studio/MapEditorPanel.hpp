#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>

#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>

#include <QFormLayout>
#include <functional>

#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QSplitter>
#include <QLabel>

#include <QPainter>
#include <QMouseEvent>

#include <QEvent>
#include <QRectF>

#include <QWheelEvent>
#include <QResizeEvent>

#include <QPointF>
#include <QUndoStack>
#include <memory>

#include "UndoCommands.hpp"

#include <map>

#include <vector>
#include "aur/project.hpp"
#include "aur/render.hpp"
#include "TilePaletteWidget.hpp"

namespace aur {

class MapCanvasWidget : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    std::string current_map;
    int current_layer = 0; // 0=ground, 1=objects, 2=above, 3=collision overlay
    int selected_tile = 0;
    std::vector<int> selected_stamp;  // D2.S1 multi-tile stamp (ascending)
    int hover_x = -1, hover_y = -1;
    bool selecting_entity = false;
    int selected_entity_index = -1;
    int selected_entity_instance = -1;  // D3.S1: index into entities[], -1 = none
    int drag_entity_index = -1;         // D3.S1: entity being drag-moved
    std::map<std::pair<std::string, int>, QImage> tile_cache;
    qreal zoom_factor = 1.0;
    QPointF pan_offset;
    QPointF pan_start_position;
    QPointF pan_start_offset;
    bool panning = false;
    int active_tool = 0;
    void constrainCamera();

public:
    QRectF mapBounds() const;

private:
    std::unique_ptr<QUndoStack> undo_stack;
    // Regions pushed since the last command was finalized; D4.S1b2 collects
    // per-cell diffs from these instead of storing whole-layer copies.
    Json region_before;   // snapshot taken at begin time
    editor::RegionKey region_key; // where it came from
    QString region_label; // command text
    bool region_open = false;
    bool stroke_active = false;

    QPoint tileAt(const QPointF& position) const;
    QImage tileImage(const std::string& tileset_id, int tile_index);
    void paintTile(const QPoint& tile);
    Json* editableLayer(int layer_index);
    int entityAt(const QPoint& tile);           // D3.S1: hit-test, -1 = none
    void moveSelectedEntity(const QPoint& tile);
    void uniqueEntityId(Json& entity) const;    // D3.S1: id = kind_n

public:
    explicit MapCanvasWidget(QWidget* parent = nullptr);
    void setProject(Project* p);
    void setMap(const std::string& mid);
    void setLayer(int l);
    void setTool(int tool_index);
    void setSelectedTile(int t);
    void setSelectedStamp(std::vector<int> tiles);
    void setSelectedEntityKind(int entity_kind);
    void deleteSelectedEntity();
    int selectedEntityInstance() const noexcept { return selected_entity_instance; }
    qreal zoomFactor() const noexcept { return zoom_factor; }
    QPointF panOffset() const noexcept { return pan_offset; }
    void fitToView();
    void centerOn(const QPointF& position);
    void invalidateTileCache() { tile_cache.clear(); update(); }
    void zoomIn();
    void zoomOut();
    void undo();
    void redo();

private:
    // Snapshot undo over QUndoStack (D4.S1b): begin before a region mutation,
    // finish after it, so the command captures both states.
    void beginRegionUndo(int layer_index, const QString& label);
    void finishRegionUndo();

public:

signals:
    void zoomChanged(qreal zoom);
    void entitySelectionRequested(int entity_index);
    void entitySelected(int instance);  // D3.S1: clicked/dragged entity changed


protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
};

class MapEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* mapCombo = nullptr;
    QComboBox* modeCombo = nullptr;   // Real-Time Action / Turn-Based / Tactical
    QComboBox* layerCombo = nullptr;
    QComboBox* toolCombo = nullptr;
    QListWidget* entityTypeWidget = nullptr;
    TilePaletteWidget* tilePalette = nullptr;
    MapCanvasWidget* canvas = nullptr;
    QPushButton* fitButton = nullptr;
    QPushButton* zoomInButton = nullptr;
    QPushButton* zoomOutButton = nullptr;
    QLabel* zoomLabel = nullptr;
    QLabel* modeHint = nullptr;
    QLabel* selectedObjectLabel = nullptr;
    QLineEdit* facingEdit = nullptr;
    QComboBox* eventCombo = nullptr;
    QPushButton* eventApplyButton = nullptr;
    QLineEdit* idEdit = nullptr;        // D3.S1: selected entity's id
    QLineEdit* nameEdit = nullptr;      // D3.S1: display name / text field
    QLineEdit* textEdit = nullptr;      // D3.S1: sign/dialogue text
    QSpinBox* paramSpin = nullptr;      // D3.S1: generic integer payload
    QPushButton* entityDeleteButton = nullptr;
    std::function<void(int)> syncInspectorFromInstance;
    std::string current_map;


public:
    explicit MapEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshMaps();
    void refreshTilePalette();
    void onTilesetChanged();  // flag edits elsewhere invalidate the tile cache
};

} // namespace aur
