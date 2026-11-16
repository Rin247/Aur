#include "SpriteEditorPanel.hpp"

#include "pixel/SpriteCanvas.hpp"

#include "pixel/PixelFrameOps.hpp"
#include "pixel/PixelEditSession.hpp"
#include "theme/ResponsiveSplitter.hpp"
#include "aur/gfx.hpp"

#include <QIcon>
#include <QApplication>
#include <QClipboard>
#include <QKeySequence>
#include <QPixmap>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace aur {
namespace {

QColor palette_color(const Json& color) {
    const Rgb rgba = hex_to_rgba(color.as_string());
    return {rgba.r, rgba.g, rgba.b, rgba.a};
}

void add_frame_actions(QHBoxLayout* toolbar, QWidget* scope, SpriteCanvasWidget* canvas) {
    const std::pair<QString, std::function<void()>> buttons[] = {
        {"Undo", [canvas]() { canvas->undo(); }},
        {"Redo", [canvas]() { canvas->redo(); }},
        {"Copy", [canvas]() { canvas->copyRows(); }},
        {"Cut", [canvas]() { canvas->cutRows(); }},
        {"Paste", [canvas]() { canvas->pasteRows(); }},
        {"PNG", [canvas]() { canvas->exportPng(); }}
    };
    for (const auto& [label, action] : buttons) {
        auto* button = new QPushButton(label, scope);
        toolbar->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, scope, action);
    }

    const std::pair<QKeySequence, std::function<void()>> shortcuts[] = {
        {QKeySequence::Undo, [canvas]() { canvas->undo(); }},
        {QKeySequence::Redo, [canvas]() { canvas->redo(); }},
        {QKeySequence::Copy, [canvas]() { canvas->copyRows(); }},
        {QKeySequence::Cut, [canvas]() { canvas->cutRows(); }},
        {QKeySequence::Paste, [canvas]() { canvas->pasteRows(); }}
    };
    for (const auto& [key, action] : shortcuts) {
        auto* shortcut = new QShortcut(key, scope);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, scope, action);
    }
}

} // namespace

SpriteEditorPanel::SpriteEditorPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* mainLayout = new QVBoxLayout(this);
    auto* toolbar = new QHBoxLayout();

    toolbar->addWidget(new QLabel("Mode:", this));
    modeCombo = new QComboBox(this);
    modeCombo->addItems({"Graphic", "Sprite"});
    modeCombo->setToolTip("Switch between all graphics and character/enemy sprites");
    toolbar->addWidget(modeCombo);

    graphicLabel = new QLabel("Graphic:", this);
    toolbar->addWidget(graphicLabel);
    graphicCombo = new QComboBox(this);
    graphicCombo->setMinimumWidth(200);
    toolbar->addWidget(graphicCombo);

    toolbar->addSpacing(12);
    toolbar->addWidget(new QLabel("Brush:", this));
    brushCombo = new QComboBox(this);
    brushCombo->addItems({"8×8 Tile", "Pixel", "16×16 Stamp", "Color Picker", "Eraser"});
    brushCombo->setToolTip("Brush (keys 1-5)");
    for (int key = 1; key <= 5; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            brushCombo->setCurrentIndex(std::min(key - 1, brushCombo->count() - 1));
        });
    }
    toolbar->addWidget(brushCombo);
    toolbar->addSpacing(12);
    toolbar->addWidget(new QLabel("Zoom:", this));
    zoomCombo = new QComboBox(this);
    zoomCombo->addItem("Fit", 0);
    for (int scale : {1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64})
        zoomCombo->addItem(QString("%1×").arg(scale), scale);
    toolbar->addWidget(zoomCombo);
    toolbar->addSpacing(12);
    frameLabel = new QLabel("Frame:", this);
    toolbar->addWidget(frameLabel);
    frameCombo = new QComboBox(this);
    frameCombo->setMinimumWidth(110);
    toolbar->addWidget(frameCombo);
    toolbar->addStretch(1);
    mainLayout->addLayout(toolbar);

    // The frame and animation row: the toolbar is full, and a squeezed combo
    // is an empty one -- which looks like the editor has no frames.
    frameRow = new QWidget(this);
    auto* frameRowLayout = new QHBoxLayout(frameRow);
    frameRowLayout->setContentsMargins(6, 0, 6, 0);
    frameRowLayout->setSpacing(4);
    frameRowLayout->addWidget(new QLabel("Animation:", frameRow));
    animationCombo = new QComboBox(frameRow);
    animationCombo->setMinimumWidth(150);
    frameRowLayout->addWidget(animationCombo);
    playAnimationButton = new QPushButton("▶ Preview", frameRow);
    playAnimationButton->setCheckable(true);
    frameRowLayout->addWidget(playAnimationButton);
    frameRowLayout->addStretch(1);
    mainLayout->addWidget(frameRow);

    // D5.S1: the sprite tools get their own row; the graphic toolbar is
    // already full and these controls mean nothing outside sprite mode.
    spriteTools = new QWidget(this);
    auto* spriteToolsLayout = new QHBoxLayout(spriteTools);
    spriteToolsLayout->setContentsMargins(6, 0, 6, 0);
    spriteToolsLayout->setSpacing(4);
    spriteToolsLayout->addWidget(new QLabel("Direction:", spriteTools));
    directionCombo = new QComboBox(spriteTools);
    directionCombo->addItems({"down", "down_right", "right", "up_right",
                              "up", "up_left", "left", "down_left"});
    directionCombo->setMinimumWidth(130);
    spriteToolsLayout->addWidget(directionCombo);
    spriteToolsLayout->addWidget(new QLabel("Pose:", spriteTools));
    poseCombo = new QComboBox(spriteTools);
    poseCombo->addItems({"idle", "walk", "attack"});
    poseCombo->setMinimumWidth(120);
    spriteToolsLayout->addWidget(poseCombo);
    onionButton = new QPushButton("Onion skin", spriteTools);
    onionButton->setCheckable(true);
    onionButton->setToolTip("Draw the neighbouring frames ghosted behind this one");
    spriteToolsLayout->addWidget(onionButton);
    // D5.S1.3: a sprite is drawn facing one way and mirrored to the other.
    mirrorButton = new QPushButton("Mirror ⇄", spriteTools);
    mirrorButton->setToolTip("Copy the right half of the frame onto its left half");
    spriteToolsLayout->addWidget(mirrorButton);
    // D5.S1.4: the frame tools over the sprite's sequence.
    addFrameButton = new QPushButton("+ Frame", spriteTools);
    addFrameButton->setToolTip("Add an empty frame to the sprite");
    duplicateFrameButton = new QPushButton("Duplicate", spriteTools);
    duplicateFrameButton->setToolTip("Add a copy of the current frame");
    deleteFrameButton = new QPushButton("Delete", spriteTools);
    deleteFrameButton->setToolTip("Remove the current frame from the sprite");
    moveFrameLeftButton = new QPushButton("◀", spriteTools);
    moveFrameLeftButton->setToolTip("Move the frame earlier in the sequence");
    moveFrameRightButton = new QPushButton("▶", spriteTools);
    moveFrameRightButton->setToolTip("Move the frame later in the sequence");
    for (QPushButton *button : {addFrameButton, duplicateFrameButton, deleteFrameButton,
                                moveFrameLeftButton, moveFrameRightButton})
        spriteToolsLayout->addWidget(button);
    spriteToolsLayout->addStretch(1);
    mainLayout->addWidget(spriteTools);
    connect(onionButton, &QPushButton::toggled, this, [this](bool) { updateOnionSkin(); });
    connect(mirrorButton, &QPushButton::clicked, canvas, &SpriteCanvasWidget::mirrorRightToLeft);
    connect(addFrameButton, &QPushButton::clicked, this, &SpriteEditorPanel::addFrame);
    connect(duplicateFrameButton, &QPushButton::clicked, this, &SpriteEditorPanel::duplicateFrame);
    connect(deleteFrameButton, &QPushButton::clicked, this, &SpriteEditorPanel::deleteFrame);
    connect(moveFrameLeftButton, &QPushButton::clicked, this, [this] { moveFrame(-1); });
    connect(moveFrameRightButton, &QPushButton::clicked, this, [this] { moveFrame(1); });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* paletteDock = new QWidget(splitter);
    auto* paletteLayout = new QVBoxLayout(paletteDock);
    paletteLayout->setContentsMargins(0, 0, 8, 0);
    paletteLayout->addWidget(new QLabel("INDEXED PALETTE", paletteDock));
    paletteWidget = new QListWidget(paletteDock);
    paletteLayout->addWidget(paletteWidget);
    splitter->addWidget(paletteDock);

    canvasScroll = new QScrollArea(splitter);
    canvasScroll->setWidgetResizable(false);
    canvasScroll->setAlignment(Qt::AlignCenter);
    canvasScroll->setFrameShape(QFrame::NoFrame);
    canvas = new SpriteCanvasWidget();
    canvasScroll->setWidget(canvas);
    splitter->addWidget(canvasScroll);
    theme::bindResponsiveSplitter(splitter, {20, 80});
    mainLayout->addWidget(splitter, 1);

    connect(modeCombo, &QComboBox::currentIndexChanged, this,
            [this](int mode) { updateEditorMode(mode); });
    connect(graphicCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || !project) return;
        selectGraphic(graphicCombo->itemData(index).toString().toStdString());
    });
    connect(directionCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { if (modeCombo->currentIndex() == 1) refreshSpriteFrames(); });
    connect(poseCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { if (modeCombo->currentIndex() == 1) refreshSpriteFrames(); });
    connect(brushCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        if (modeCombo->currentIndex() == 1) {
            const int modes[] = {1, 0, 4};
            canvas->setBrushMode(modes[std::clamp(index, 0, 2)]);
        } else {
            canvas->setBrushMode(index);
        }
    });
    connect(zoomCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        canvas->setZoomScale(zoomCombo->itemData(index).toInt());
        canvasScroll->ensureVisible(canvas->width() / 2, canvas->height() / 2);
    });
    connect(canvas, &SpriteCanvasWidget::zoomStepRequested, this, [this](int step) {
        const int target = std::clamp(zoomCombo->currentIndex() + step, 1, zoomCombo->count() - 1);
        zoomCombo->setCurrentIndex(target);
    });
    connect(frameCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && !playAnimationButton->isChecked())
            canvas->setFrame(frameCombo->itemData(index).toString().toStdString());
        updateOnionSkin();
    });
    connect(animationCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || !project || graphicCombo->currentIndex() < 0) return;
        if (animationTimer) animationTimer->stop();
        playAnimationButton->setChecked(false);
        playAnimationButton->setText("▶ Preview");
        previewFrames.clear();
        const std::string graphic_id = graphicCombo->currentData().toString().toStdString();
        const std::string animation_id = animationCombo->itemData(index).toString().toStdString();
        const auto graphic_it = project->graphics.find(graphic_id);
        if (graphic_it == project->graphics.end() || animation_id.empty()) return;
        const Json& animations = graphic_it->second["anims"];
        if (!animations.is_object() || !animations.contains(animation_id)) return;
        const Json& frames = animations[animation_id]["frames"];
        if (!frames.is_array()) return;
        for (const Json& frame : frames.as_array()) {
            if (frame.is_string()) previewFrames.push_back(frame.as_string());
            else if (frame.is_number()) previewFrames.push_back(std::to_string(frame.as_int()));
        }
        previewFps = animations[animation_id].contains("fps")
                         ? std::clamp(animations[animation_id]["fps"].as_int(), 1, 60) : 8;
        previewLoop = !animations[animation_id].contains("loop") ||
                      !animations[animation_id]["loop"].is_bool() ||
                      animations[animation_id]["loop"].as_bool();
        previewFrameIndex = 0;
        if (!previewFrames.empty()) {
            const QSignalBlocker blocker(frameCombo);
            const int frame_index = frameCombo->findData(QString::fromStdString(previewFrames[0]));
            if (frame_index >= 0) frameCombo->setCurrentIndex(frame_index);
            canvas->setFrame(previewFrames[0]);
        }
    });
    animationTimer = new QTimer(this);
    connect(playAnimationButton, &QPushButton::toggled, this, [this](bool playing) {
        if (!playing) {
            animationTimer->stop();
            playAnimationButton->setText("▶ Preview");
            return;
        }
        if (previewFrames.size() < 2 || !project || graphicCombo->currentIndex() < 0) {
            playAnimationButton->setChecked(false);
            return;
        }
        previewFrameIndex = 0;
        playAnimationButton->setText("■ Stop");
        animationTimer->start(std::max(16, 1000 / std::max(1, previewFps)));
    });
    connect(animationTimer, &QTimer::timeout, this, [this]() {
        if (previewFrames.empty()) return;
        ++previewFrameIndex;
        if (previewFrameIndex >= static_cast<int>(previewFrames.size())) {
            if (previewLoop) previewFrameIndex = 0;
            else {
                previewFrameIndex = static_cast<int>(previewFrames.size()) - 1;
                playAnimationButton->setChecked(false);
                return;
            }
        }
        const std::string& frame = previewFrames[static_cast<size_t>(previewFrameIndex)];
        canvas->setFrame(frame);
        const QSignalBlocker blocker(frameCombo);
        const int frame_index = frameCombo->findData(QString::fromStdString(frame));
        if (frame_index >= 0) frameCombo->setCurrentIndex(frame_index);
    });
    connect(paletteWidget, &QListWidget::currentRowChanged, this,
            [this](int row) { if (row >= 0) canvas->setSelectedColor(row); });
    // The canvas fits the space the scroll area gives it, and that space is
    // only known after the layout runs.
    canvasScroll->viewport()->installEventFilter(this);
    connect(canvas, &SpriteCanvasWidget::colorPicked, this, [this](int index) {
        if (paletteWidget && index < paletteWidget->count()) paletteWidget->setCurrentRow(index);
    });

    add_frame_actions(toolbar, this, canvas);
    updateEditorMode(0);
    refreshGraphics();
}

void SpriteEditorPanel::setProject(Project* p) {
    project = p;
    if (canvas) canvas->clearHistory();
    refreshGraphics();
}

void SpriteEditorPanel::setSpriteMode(bool enabled) {
    if (!modeCombo) return;
    const int index = enabled ? 1 : 0;
    if (modeCombo->currentIndex() == index) return;
    modeCombo->setCurrentIndex(index);
}

void SpriteEditorPanel::startPreview() {
    if (!playAnimationButton) return;
    if (previewFrames.size() < 2) return;
    playAnimationButton->setChecked(true);
}

void SpriteEditorPanel::refreshGraphics() {
    if (!graphicCombo) return;
    const QString previous = graphicCombo->currentData().toString();
    graphicCombo->blockSignals(true);
    graphicCombo->clear();
    if (project) {
        std::map<std::string, std::string> spriteNames;
        if (modeCombo->currentIndex() == 1) {
            const auto addRecord = [this, &spriteNames](const std::string& id, const Json& record) {
                const std::string graphicId = record.contains("graphic") && record["graphic"].is_string()
                                                  ? record["graphic"].as_string() : id;
                if (!project->graphics.contains(graphicId)) return;
                spriteNames.try_emplace(graphicId,
                    record.contains("name") && record["name"].is_string()
                        ? record["name"].as_string() : id);
            };
            for (const auto& [id, record] : project->characters) addRecord(id, record);
            if (project->db.contains("enemies") && project->db["enemies"].is_object())
                for (const auto& [id, record] : project->db["enemies"].as_object())
                    addRecord(id, record);
            for (const auto& [id, graphic] : project->graphics)
                if (graphic.contains("frames") && graphic.contains("anims"))
                    spriteNames.try_emplace(id, id);
        }
        for (const auto& [id, graphic] : project->graphics) {
            if (modeCombo->currentIndex() == 0 || spriteNames.contains(id)) {
                const QString label = modeCombo->currentIndex() == 1
                                          ? QString::fromStdString(spriteNames.at(id))
                                          : QString::fromStdString(id);
                graphicCombo->addItem(label, QString::fromStdString(id));
            }
        }
    }
    graphicCombo->blockSignals(false);

    int index = graphicCombo->findData(previous);
    if (index < 0 && graphicCombo->count() > 0) index = 0;
    graphicCombo->setCurrentIndex(index);
    if (index >= 0) selectGraphic(graphicCombo->itemData(index).toString().toStdString());
    else {
        canvas->setGraphic(nullptr, {});
        paletteWidget->clear();
        frameCombo->clear();
        animationCombo->clear();
    }
}

void SpriteEditorPanel::selectGraphic(const std::string& id) {
    if (!project || !project->graphics.contains(id)) return;
    if (animationTimer) animationTimer->stop();
    if (playAnimationButton) {
        playAnimationButton->setChecked(false);
        playAnimationButton->setText("▶ Preview");
    }
    canvas->setGraphic(project, id);
    refreshPalette();
    if (modeCombo->currentIndex() == 1) refreshSpriteFrames();
    else refreshAnimationData();
}

void SpriteEditorPanel::updateEditorMode(int mode) {
    const bool spriteMode = mode == 1;
    if (spriteMode) {
        graphicBrushIndex = brushCombo->currentIndex();
        graphicZoomIndex = zoomCombo->currentIndex();
    }
    graphicLabel->setText(spriteMode ? "Sprite:" : "Graphic:");
    spriteTools->setVisible(spriteMode);
    frameRow->setVisible(!spriteMode);
    frameCombo->setVisible(spriteMode);
    frameLabel->setVisible(spriteMode);
    animationCombo->setVisible(!spriteMode);
    // D5.S1.2: the preview plays in both modes -- a sprite's frames are
    // never seen moving otherwise.
    playAnimationButton->setVisible(true);
    if (spriteMode) {
        brushCombo->clear();
        brushCombo->addItems({"Pixel", "8×8 cluster", "Erase 8×8"});
        brushCombo->setToolTip("Brush (keys 1-3)");
        brushCombo->setCurrentIndex(0);
        zoomCombo->setCurrentIndex(0);  // fit: a sprite is small next to a sheet
        // The onion skin is on by default in sprite mode: a pose is drawn
        // against its neighbours, not in the dark.
        onionButton->setChecked(true);
        updateOnionSkin();
    } else {
        brushCombo->clear();
        brushCombo->addItems({"8×8 Tile", "Pixel", "16×16 Stamp", "Color Picker", "Eraser"});
        brushCombo->setToolTip("Brush (keys 1-5)");
        brushCombo->setCurrentIndex(std::clamp(graphicBrushIndex, 0, brushCombo->count() - 1));
        zoomCombo->setCurrentIndex(std::clamp(graphicZoomIndex, 0, zoomCombo->count() - 1));
    }
    refreshGraphics();
}

void SpriteEditorPanel::refreshSpriteFrames() {
    const QString previous = frameCombo->currentData().toString();
    frameCombo->blockSignals(true);
    frameCombo->clear();
    if (!project || graphicCombo->currentIndex() < 0) {
        frameCombo->blockSignals(false);
        return;
    }
    const std::string id = graphicCombo->currentData().toString().toStdString();
    const auto frames = PixelEditSession::resolve_anim_frames(
        *project, id, directionCombo->currentText().toStdString(),
        poseCombo->currentText().toStdString());
    for (const std::string& frame : frames)
        frameCombo->addItem(QString::fromStdString(frame), QString::fromStdString(frame));
    // D5.S1.2: the sprite's own sequence is what the preview plays, at the
    // anim's fps.
    previewFrames = frames;
    const auto anim = PixelEditSession::resolve_anim(
        *project, id, directionCombo->currentText().toStdString(),
        poseCombo->currentText().toStdString());
    previewFps = anim.fps > 0 ? anim.fps : 8;
    previewLoop = anim.loop;
    frameCombo->blockSignals(false);
    // D5.S1.4: a frame that is still in the sequence keeps the selection, so
    // adding or duplicating a frame does not jump back to the first.
    const int previous_index = frameCombo->findData(previous);
    if (previous_index >= 0) {
        frameCombo->setCurrentIndex(previous_index);
        canvas->setFrame(previous.toStdString());
    } else if (frameCombo->count() > 0) {
        frameCombo->setCurrentIndex(0);
        canvas->setFrame(frameCombo->itemData(0).toString().toStdString());
    }
    updateOnionSkin();
}

void SpriteEditorPanel::updateOnionSkin() {
    std::vector<std::string> frames;
    int index = -1;
    if (modeCombo->currentIndex() == 1 && onionButton && onionButton->isChecked() &&
        frameCombo) {
        for (int i = 0; i < frameCombo->count(); ++i)
            frames.push_back(frameCombo->itemData(i).toString().toStdString());
        index = frameCombo->currentIndex();
    }
    canvas->setOnionSkin(frames, index);
}

std::string SpriteEditorPanel::spriteGraphicId() const {
    if (!graphicCombo || graphicCombo->currentIndex() < 0) return {};
    return graphicCombo->currentData().toString().toStdString();
}

std::string SpriteEditorPanel::spriteAnimId() const {
    if (!project) return {};
    const auto anim = PixelEditSession::resolve_anim(
        *project, spriteGraphicId(), directionCombo->currentText().toStdString(),
        poseCombo->currentText().toStdString());
    return anim.id;
}

std::string SpriteEditorPanel::spriteFrameId() const {
    if (!frameCombo || frameCombo->currentIndex() < 0) return {};
    return frameCombo->currentData().toString().toStdString();
}

void SpriteEditorPanel::selectSpriteFrame(const std::string& frame_id) {
    const int index = frameCombo->findData(QString::fromStdString(frame_id));
    if (index < 0) return;
    frameCombo->setCurrentIndex(index);
    canvas->setFrame(frame_id);
}

void SpriteEditorPanel::addFrame() {
    if (!project) return;
    const std::string anim = spriteAnimId();
    const std::string base = spriteFrameId();
    std::string new_id;
    canvas->applyGraphicChange(QStringLiteral("add frame"), [&](Json &graphic) {
        editor::add_frame(graphic, anim, base, new_id);
    });
    refreshSpriteFrames();
    if (!new_id.empty()) selectSpriteFrame(new_id);
}

void SpriteEditorPanel::duplicateFrame() {
    if (!project) return;
    const std::string anim = spriteAnimId();
    const std::string source = spriteFrameId();
    if (source.empty()) return;
    const int index = frameCombo->currentIndex();
    std::string new_id;
    canvas->applyGraphicChange(QStringLiteral("duplicate frame"), [&](Json &graphic) {
        editor::duplicate_frame(graphic, anim, source, index + 1, new_id);
    });
    refreshSpriteFrames();
    if (!new_id.empty()) selectSpriteFrame(new_id);
}

void SpriteEditorPanel::deleteFrame() {
    if (!project) return;
    const std::string anim = spriteAnimId();
    const std::string frame = spriteFrameId();
    if (frame.empty()) return;
    canvas->applyGraphicChange(QStringLiteral("delete frame"), [&](Json &graphic) {
        editor::remove_frame(graphic, anim, frame);
    });
    refreshSpriteFrames();
}

void SpriteEditorPanel::moveFrame(int delta) {
    if (!project) return;
    const std::string anim = spriteAnimId();
    const int index = frameCombo->currentIndex();
    if (anim.empty() || index < 0) return;
    bool moved = false;
    canvas->applyGraphicChange(QStringLiteral("move frame"), [&](Json &graphic) {
        moved = editor::move_frame(graphic, anim, index, delta);
    });
    if (!moved) return;
    refreshSpriteFrames();
    const int moved_index = index + delta;
    if (moved_index >= 0 && moved_index < frameCombo->count())
        frameCombo->setCurrentIndex(moved_index);
}

void SpriteEditorPanel::refreshAnimationData() {
    if (!frameCombo || !animationCombo) return;
    if (animationTimer) animationTimer->stop();
    if (playAnimationButton) {
        playAnimationButton->setChecked(false);
        playAnimationButton->setText("▶ Preview");
    }
    previewFrames.clear();
    frameCombo->clear();
    animationCombo->clear();
    if (!project || graphicCombo->currentIndex() < 0) return;
    const std::string id = graphicCombo->currentData().toString().toStdString();
    const auto graphic_it = project->graphics.find(id);
    if (graphic_it == project->graphics.end()) return;

    const Json& frames = graphic_it->second["frames"];
    if (frames.is_object()) {
        for (const auto& [frame_id, frame] : frames.as_object()) {
            if (!frame.is_array()) continue;
            frameCombo->addItem(QString::fromStdString(frame_id), QString::fromStdString(frame_id));
        }
    }
    const Json& animations = graphic_it->second["anims"];
    std::string initial_animation;
    if (animations.is_object()) {
        for (const auto& [animation_id, animation] : animations.as_object()) {
            if (!animation.contains("frames") || !animation["frames"].is_array() ||
                animation["frames"].as_array().empty()) continue;
            animationCombo->addItem(QString::fromStdString(animation_id), QString::fromStdString(animation_id));
            if (initial_animation.empty()) initial_animation = animation_id;
        }
    }
    for (const std::string preferred : {"idle_down", "idle", "play"}) {
        const int index = animationCombo->findData(QString::fromStdString(preferred));
        if (index >= 0) {
            initial_animation = preferred;
            break;
        }
    }
    if (!initial_animation.empty()) {
        const int index = animationCombo->findData(QString::fromStdString(initial_animation));
        if (index >= 0) animationCombo->setCurrentIndex(index);
    }
    if (frameCombo->count() > 0) {
        const std::string current = frameCombo->currentData().toString().toStdString();
        canvas->setFrame(current);
    }
}

void SpriteEditorPanel::refreshPalette() {
    if (!project) {
        paletteWidget->clear();
        return;
    }
    const std::string id = graphicCombo->currentIndex() >= 0
                               ? graphicCombo->currentData().toString().toStdString()
                               : std::string();
    PixelEditSession::fill_palette(paletteWidget, *project, id);
}

bool SpriteEditorPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvasScroll->viewport() && event->type() == QEvent::Resize)
        canvas->setAvailableViewportSize(canvasScroll->viewport()->size());
    return QWidget::eventFilter(watched, event);
}

void SpriteEditorPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (canvas && canvasScroll) {
        canvas->setAvailableViewportSize(canvasScroll->viewport()->size());
    }
}



} // namespace aur
