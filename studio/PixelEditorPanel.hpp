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
    std::vector<Json> undo_frames;
    std::vector<Json> redo_frames;

    void paintAt(const QPoint& pixel);
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
    void leaveEvent(QEvent* event) override;

signals:
    void colorPicked(int index);
};

class PixelEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* graphicCombo = nullptr;
    QComboBox* brushCombo = nullptr;
    QComboBox* zoomCombo = nullptr;
    QComboBox* frameCombo = nullptr;
    QComboBox* animationCombo = nullptr;
    QPushButton* playAnimationButton = nullptr;
    QTimer* animationTimer = nullptr;
    std::vector<std::string> previewFrames;
    int previewFrameIndex = 0;
    bool previewLoop = true;
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

protected:
    void resizeEvent(QResizeEvent* event) override;
};

class SpriteEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* spriteCombo = nullptr;
    QComboBox* directionCombo = nullptr;
    QComboBox* poseCombo = nullptr;
    QComboBox* frameCombo = nullptr;
    QComboBox* brushCombo = nullptr;
    QComboBox* zoomCombo = nullptr;
    QListWidget* paletteWidget = nullptr;
    QScrollArea* canvasScroll = nullptr;
    PixelCanvasWidget* canvas = nullptr;

public:
    explicit SpriteEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);

private:
    void refreshSprites();
    void refreshFrames();
    void refreshPalette();

protected:
    void resizeEvent(QResizeEvent* event) override;
};

} // namespace aur
