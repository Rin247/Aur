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
#include <map>
#include <vector>
#include "aur/project.hpp"

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
    // Per-frame undo history (editsweep D1.S3): history survives frame
    // switches and animation preview, keyed by frame id. The previous
    // whole-stack vectors were wiped by setFrame, destroying undo on every
    // animation tick.
    std::map<std::string, std::vector<Json>> undo_frames;
    std::map<std::string, std::vector<Json>> redo_frames;
    QPoint last_paint_pixel{-1, -1};  // previous stroke pixel, for interpolation
    bool stroke_pending = false;      // stroke began; push undo on first real change

    bool paintAt(const QPoint& pixel);  // returns true when pixels changed
    void resizeForGraphic();
    void pushUndo();
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
