#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSize>
#include <QTimer>

#include <QUndoStack>

#include <memory>

#include "aur/project.hpp"

#include "UndoCommands.hpp"

namespace aur {

class PixelCanvasWidget : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    std::string graphic_id;
    std::string current_frame = "0";
    int selected_color_idx = 1;
    int brush_mode = 0;
    int zoom_scale = 16;
    bool auto_zoom = true;
    QSize available_viewport{1280, 720};
    QPoint hover_pixel{-1, -1};
    // D4.S1b: one QUndoStack whose commands target the live frame array
    // (RegionKey::Direct); history survives frame switches and animation
    // preview, since setFrame no longer touches it.
    std::unique_ptr<QUndoStack> undo_stack;
    std::unique_ptr<editor::SnapshotCommand> pending_command;
    QPoint last_paint_pixel{-1, -1};  // previous stroke pixel, for interpolation
    bool stroke_pending = false;      // stroke began; push undo on first real change
    bool pending_open = false;        // a stroke's snapshot waits for finishUndo

    bool paintAt(const QPoint& pixel);  // returns true when pixels changed
    void resizeForGraphic();
    void pushUndo();
    void finishUndo();
    void restoreFrame(const Json& rows);

public:
    explicit PixelCanvasWidget(QWidget* parent = nullptr);
    void setGraphic(Project* p, const std::string& id);
    void setSelectedColor(int idx);
    void setBrushMode(int mode);
    void setZoomScale(int scale);
    void setAvailableViewportSize(const QSize& size);
    void setFrame(const std::string& frame);
    void undo();
    void redo();
    void copyFrame();
    void cutFrame();
    void pasteFrame();
    void clearHistory();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintStroke(const QPoint& from, const QPoint& to);
    void leaveEvent(QEvent* event) override;

signals:
    void colorPicked(int index);
};

class PixelEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* modeCombo = nullptr;
    QLabel* graphicLabel = nullptr;
    QComboBox* graphicCombo = nullptr;
    QComboBox* directionCombo = nullptr;
    QComboBox* poseCombo = nullptr;
    QWidget* spriteControls = nullptr;
    QComboBox* brushCombo = nullptr;
    QComboBox* zoomCombo = nullptr;
    QComboBox* frameCombo = nullptr;
    QComboBox* animationCombo = nullptr;
    QPushButton* playAnimationButton = nullptr;
    QTimer* animationTimer = nullptr;
    std::vector<std::string> previewFrames;
    int previewFrameIndex = 0;
    bool previewLoop = true;
    int graphicBrushIndex = 0;
    int graphicZoomIndex = 0;
    QListWidget* paletteWidget = nullptr;
    PixelCanvasWidget* canvas = nullptr;
    QScrollArea* canvasScroll = nullptr;

public:
    explicit PixelEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshGraphics();

private:
    void refreshPalette();
    void refreshAnimationData();
    void refreshSpriteFrames();
    void selectGraphic(const std::string& id);
    void updateEditorMode(int mode);

protected:
    void resizeEvent(QResizeEvent* event) override;
};

} // namespace aur
