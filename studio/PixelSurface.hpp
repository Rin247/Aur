#pragma once

// PixelSurface.hpp - the pixel-editing surface: it edits "a rows array with
// a palette", which is what a graphic's frame is and what a tile is.
//
// The surface owns the brush, the zoom, the undo and the painting. It does
// not know which document it is editing: the owner hands it the rows and
// the palette, and takes them back when the target changes.

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

#include <memory>
#include <string>
#include <vector>

#include "UndoCommands.hpp"
#include "aur/json.hpp"

namespace aur {

class PixelSurface : public QWidget {
    Q_OBJECT
    // The rows the surface edits. Borrowed: the owner must outlive it.
    Json* rows_target = nullptr;
    std::vector<std::string> palette;
    int surface_width = 0;
    int surface_height = 0;
    int selected_color_idx = 1;
    int brush_mode = 0;
    int zoom_scale = 16;
    bool auto_zoom = true;
    QSize available_viewport{1280, 720};
    QPoint hover_pixel{-1, -1};
    // D5.S1.1: the rows drawn ghosted behind the target, and where the
    // target sits in them. An empty list turns the skin off.
    std::vector<const Json*> onion_rows;
    int onion_index = -1;
    std::unique_ptr<QUndoStack> undo_stack;
    std::unique_ptr<editor::SnapshotCommand> pending_command;
    QPoint last_paint_pixel{-1, -1};  // previous stroke pixel, for interpolation
    bool stroke_pending = false;      // stroke began; push undo on first real change
    bool pending_open = false;        // a stroke's snapshot waits for finishUndo

    bool paintAt(const QPoint& pixel);  // returns true when pixels changed
    void resizeForSurface();
    void pushUndo();
    void finishUndo();
    void restoreRows(const Json& rows);
    static int palette_index(char pixel);

public:
    explicit PixelSurface(QWidget* parent = nullptr);
    // The rows and the palette the surface draws and edits. The rows pointer
    // is borrowed; the owner must outlive the surface.
    void setRowsTarget(Json* rows, const std::vector<std::string>& palette,
                       int width, int height);
    void setSelectedColor(int idx);
    void setBrushMode(int mode);
    void setZoomScale(int scale);
    void setAvailableViewportSize(const QSize& size);
    void setOnionSkin(const std::vector<const Json*>& frames, int current_index);
    // A change to another target in the same document (a whole graphic, for
    // the frame tools), recorded as one undoable snapshot of it.
    void applyChange(Json* target, const QString& label,
                     const std::function<void(Json&)>& change);
    void undo();
    void redo();
    void clearHistory();
    // The stack, for an owner that shares it (the tileset editor's flag
    // undo shares the pixels' history).
    QUndoStack* undoStack() { return undo_stack.get(); }
    void copyRows();
    void cutRows();
    void pasteRows();
    // D5.S1.3: mirrors the right half of the target onto its left half.
    void mirrorRightToLeft();

signals:
    void colorPicked(int index);
    void painted();

protected:
    // Resets the stroke bookkeeping, for a change of target.
    void resetStroke();
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintStroke(const QPoint& from, const QPoint& to);
    void leaveEvent(QEvent* event) override;
};

} // namespace aur
