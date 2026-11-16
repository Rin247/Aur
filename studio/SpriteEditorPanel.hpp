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

#include "pixel/SpriteCanvas.hpp"
#include "core/UndoCommands.hpp"

namespace aur {

class SpriteEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* modeCombo = nullptr;
    QLabel* graphicLabel = nullptr;
    QComboBox* graphicCombo = nullptr;
    QComboBox* directionCombo = nullptr;
    QComboBox* poseCombo = nullptr;
    QComboBox* brushCombo = nullptr;
    QComboBox* zoomCombo = nullptr;
    QComboBox* frameCombo = nullptr;
    QLabel* frameLabel = nullptr;
    QWidget* frameRow = nullptr;
    QComboBox* animationCombo = nullptr;
    QPushButton* playAnimationButton = nullptr;
    // D5.S1: the sprite-mode tools live in their own row, so the graphic
    // toolbar does not grow another five controls.
    QWidget* spriteTools = nullptr;
    QPushButton* onionButton = nullptr;
    QPushButton* mirrorButton = nullptr;
    QPushButton* addFrameButton = nullptr;
    QPushButton* duplicateFrameButton = nullptr;
    QPushButton* deleteFrameButton = nullptr;
    QPushButton* moveFrameLeftButton = nullptr;
    QPushButton* moveFrameRightButton = nullptr;
    QTimer* animationTimer = nullptr;
    std::vector<std::string> previewFrames;
    int previewFrameIndex = 0;
    int previewFps = 8;  // the sequence's own fps, or the default
    bool previewLoop = true;
    int graphicBrushIndex = 0;
    int graphicZoomIndex = 0;
    QListWidget* paletteWidget = nullptr;
    SpriteCanvasWidget* canvas = nullptr;
    QScrollArea* canvasScroll = nullptr;

public:
    explicit SpriteEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshGraphics();
    // D5.S1: sprite mode is the sprite editor's own mode; the render sweep
    // drives it the same way a person does, through the mode combo.
    void setSpriteMode(bool enabled);
    // Starts the preview of the current sequence.
    void startPreview();

private:
    void refreshPalette();
    void refreshAnimationData();
    void refreshSpriteFrames();
    void selectGraphic(const std::string& id);
    void updateEditorMode(int mode);
    // D5.S1.1: the frames the onion skin draws, from the frame list.
    void updateOnionSkin();
    // D5.S1.4: the frame tools over the sprite's sequence.
    void addFrame();
    void duplicateFrame();
    void deleteFrame();
    void moveFrame(int delta);
    // The graphic, the anim and the frame the sprite tools act on.
    std::string spriteGraphicId() const;
    std::string spriteAnimId() const;
    std::string spriteFrameId() const;
    void selectSpriteFrame(const std::string& frame_id);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace aur
