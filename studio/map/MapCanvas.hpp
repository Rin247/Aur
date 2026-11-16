#pragma once

// MapCanvas.hpp - the map editor's canvas: it draws a map, takes the mouse
// and owns the camera and the undo stack.

#include <QWidget>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QResizeEvent>
#include <QShowEvent>
#include <QUndoStack>
#include <QWheelEvent>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "map/MapEditUndo.hpp"
#include "core/UndoCommands.hpp"
#include "aur/project.hpp"
#include "aur/render.hpp"

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
    qreal user_zoom_multiplier = 1.0;
    QPointF pan_offset;
    QPointF pan_start_position;
    QPointF pan_start_offset;
    bool panning = false;
    int active_tool = 0;
    // The first fit has to wait for a real widget size: at construction the
    // canvas is still its minimum, so a fit then lands on the wrong zoom.
    bool fit_pending = false;
    bool stroke_active = false;
    void constrainCamera();

public:
    QRectF mapBounds() const;

private:
    std::unique_ptr<QUndoStack> undo_stack;
    editor::MapEditUndo undo_recorder;

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
    // The zoom control, absolute and relative to the fit.
    void setZoomFactor(qreal zoom);
    void zoomByFactor(qreal factor);
    void invalidateTileCache() { tile_cache.clear(); update(); }
    void zoomIn();
    void zoomOut();
    void undo();
    void redo();

private:
    // Fits the map into the canvas once the widget has a real size.
    qreal fitZoomForCurrentSize() const;
    void applyFit();
    void requestPendingFit();
    // Opens and closes an undoable stroke over a layer (-1 is entities).
    void beginStroke(int layer_index, const QString &label);
    void endStroke();

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
    void showEvent(QShowEvent* event) override;
    void leaveEvent(QEvent* event) override;
};

} // namespace aur
