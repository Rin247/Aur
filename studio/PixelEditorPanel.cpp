#include "PixelEditorPanel.hpp"
#include "PixelEditSession.hpp"
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

void add_frame_actions(QHBoxLayout* toolbar, QWidget* scope, PixelCanvasWidget* canvas) {
    const std::pair<QString, std::function<void()>> buttons[] = {
        {"Undo", [canvas]() { canvas->undo(); }},
        {"Redo", [canvas]() { canvas->redo(); }},
        {"Copy", [canvas]() { canvas->copyFrame(); }},
        {"Cut", [canvas]() { canvas->cutFrame(); }},
        {"Paste", [canvas]() { canvas->pasteFrame(); }}
    };
    for (const auto& [label, action] : buttons) {
        auto* button = new QPushButton(label, scope);
        toolbar->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, scope, action);
    }

    const std::pair<QKeySequence, std::function<void()>> shortcuts[] = {
        {QKeySequence::Undo, [canvas]() { canvas->undo(); }},
        {QKeySequence::Redo, [canvas]() { canvas->redo(); }},
        {QKeySequence::Copy, [canvas]() { canvas->copyFrame(); }},
        {QKeySequence::Cut, [canvas]() { canvas->cutFrame(); }},
        {QKeySequence::Paste, [canvas]() { canvas->pasteFrame(); }}
    };
    for (const auto& [key, action] : shortcuts) {
        auto* shortcut = new QShortcut(key, scope);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, scope, action);
    }
}

int palette_index(char pixel) {
    if (pixel >= '0' && pixel <= '9') return pixel - '0';
    if (pixel >= 'a' && pixel <= 'f') return pixel - 'a' + 10;
    if (pixel >= 'A' && pixel <= 'F') return pixel - 'A' + 10;
    return 0;
}

} // namespace

void PixelCanvasWidget::setFrame(const std::string& frame) {
    stroke_pending = false;
    last_paint_pixel = {-1, -1};
    if (!project || !project->graphics.contains(graphic_id)) return;
    const Json& frames = project->graphics.at(graphic_id)["frames"];
    if (!frames.is_object() || !frames.contains(frame) || !frames[frame].is_array()) return;
    // History is per-frame now: switching frames (including the animation
    // preview's setFrame ticks) no longer wipes the undo stack.
    current_frame = frame;
    update();
}

PixelCanvasWidget::PixelCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 320);
    setMouseTracking(true);
    setBackgroundRole(QPalette::Dark);
}

void PixelCanvasWidget::setGraphic(Project* p, const std::string& id) {
    stroke_pending = false;
    last_paint_pixel = {-1, -1};
    if (project != p || graphic_id != id) {
        undo_frames.clear();
        redo_frames.clear();
    }
    project = p;
    graphic_id = id;
    current_frame = "0";
    if (project && project->graphics.contains(graphic_id)) {
        const Json& frames = project->graphics.at(graphic_id)["frames"];
        if (frames.is_object() && !frames.as_object().empty()) {
            current_frame = frames.contains(current_frame)
                ? current_frame : frames.as_object().begin()->first;
        }
    }
    resizeForGraphic();
    update();
}

void PixelCanvasWidget::pushUndo() {
    if (!project || !project->graphics.contains(graphic_id)) return;
    const Json& frames = project->graphics.at(graphic_id)["frames"];
    if (!frames.is_object() || !frames.contains(current_frame)) return;
    auto& stack = undo_frames[current_frame];
    stack.push_back(frames[current_frame]);
    if (stack.size() > 100) stack.erase(stack.begin());
    redo_frames[current_frame].clear();
}

void PixelCanvasWidget::restoreFrame(const Json& rows) {
    if (!project || !project->graphics.contains(graphic_id) || !rows.is_array()) return;
    project->graphics[graphic_id]["frames"][current_frame] = rows;
    update();
}

void PixelCanvasWidget::undo() {
    auto& stack = undo_frames[current_frame];
    if (stack.empty() || !project || !project->graphics.contains(graphic_id)) return;
    redo_frames[current_frame].push_back(project->graphics[graphic_id]["frames"][current_frame]);
    const Json rows = stack.back();
    stack.pop_back();
    restoreFrame(rows);
}

void PixelCanvasWidget::redo() {
    auto& stack = redo_frames[current_frame];
    if (stack.empty() || !project || !project->graphics.contains(graphic_id)) return;
    undo_frames[current_frame].push_back(project->graphics[graphic_id]["frames"][current_frame]);
    const Json rows = stack.back();
    stack.pop_back();
    restoreFrame(rows);
}

void PixelCanvasWidget::clearHistory() {
    undo_frames.clear();
    redo_frames.clear();
}

void PixelCanvasWidget::copyFrame() {
    if (!project || !project->graphics.contains(graphic_id)) return;
    const Json& graphic = project->graphics.at(graphic_id);
    if (!graphic["frames"].is_object() || !graphic["frames"].contains(current_frame)) return;
    QString text = QString("AUR_FRAME %1 %2\n").arg(graphic["w"].as_int()).arg(graphic["h"].as_int());
    for (const Json& row : graphic["frames"][current_frame].as_array()) {
        if (!row.is_string()) return;
        text += QString::fromStdString(row.as_string()) + '\n';
    }
    QApplication::clipboard()->setText(text);
}

void PixelCanvasWidget::cutFrame() {
    if (!project || !project->graphics.contains(graphic_id)) return;
    Json& graphic = project->graphics[graphic_id];
    if (!graphic["frames"].is_object() || !graphic["frames"].contains(current_frame)) return;
    copyFrame();
    pushUndo();
    JsonArray blank;
    for (int y = 0; y < graphic["h"].as_int(); ++y) {
        blank.emplace_back(std::string(static_cast<size_t>(graphic["w"].as_int()), '.'));
    }
    restoreFrame(blank);
}

void PixelCanvasWidget::pasteFrame() {
    if (!project || !project->graphics.contains(graphic_id)) return;
    const QStringList lines = QApplication::clipboard()->text().split('\n', Qt::KeepEmptyParts);
    if (lines.isEmpty()) return;
    std::istringstream header(lines[0].toStdString());
    std::string signature;
    int width = 0;
    int height = 0;
    std::string trailing;
    if (!(header >> signature >> width >> height) || (header >> trailing) ||
        signature != "AUR_FRAME") return;
    const Json& graphic = project->graphics.at(graphic_id);
    if (width != graphic["w"].as_int() || height != graphic["h"].as_int() ||
        lines.size() < height + 1) return;
    JsonArray rows;
    for (int y = 0; y < height; ++y) {
        QString row = lines[y + 1];
        if (row.endsWith('\r')) row.chop(1);
        if (row.size() != width) return;
        for (const QChar pixel : row) {
            if (pixel != '.' && !(pixel >= '0' && pixel <= '9') &&
                !(pixel >= 'A' && pixel <= 'F') && !(pixel >= 'a' && pixel <= 'f')) return;
        }
        rows.emplace_back(row.toStdString());
    }
    pushUndo();
    restoreFrame(rows);
}

void PixelCanvasWidget::resizeForGraphic() {
    if (!project || !project->graphics.contains(graphic_id)) return;
    const Json& graphic = project->graphics.at(graphic_id);
    const int image_width = graphic["w"].as_int();
    const int image_height = graphic["h"].as_int();
    if (image_width <= 0 || image_height <= 0) return;
    if (auto_zoom) {
        const int target_width = std::max(1280, available_viewport.width());
        const int target_height = std::max(720, available_viewport.height());
        const int height_scale = (target_height + image_height - 1) / image_height;
        const int width_scale = target_width / image_width;
        zoom_scale = std::clamp(std::min(height_scale, width_scale), 1, 64);
    }
    resize(std::max(420, image_width * zoom_scale + 48),
           std::max(320, image_height * zoom_scale + 48));
}

void PixelCanvasWidget::setSelectedColor(int idx) {
    selected_color_idx = std::clamp(idx, 0, 15);
    update();
}

void PixelCanvasWidget::setZoomScale(int scale) {
    auto_zoom = scale <= 0;
    if (!auto_zoom) zoom_scale = std::clamp(scale, 1, 64);
    resizeForGraphic();
    update();
}

void PixelCanvasWidget::setAvailableViewportSize(const QSize& size) {
    available_viewport = size;
    if (auto_zoom) {
        resizeForGraphic();
        update();
    }
}

void PixelCanvasWidget::setBrushMode(int mode) {
    brush_mode = mode;
    setCursor(brush_mode == 3 ? Qt::PointingHandCursor : Qt::CrossCursor);
    update();
}

bool PixelCanvasWidget::paintAt(const QPoint& pixel) {
    if (!project || !project->graphics.contains(graphic_id) || pixel.x() < 0 || pixel.y() < 0) return false;

    Json& graphic = project->graphics[graphic_id];
    const int width = graphic["w"].as_int();
    const int height = graphic["h"].as_int();
    if (pixel.x() >= width || pixel.y() >= height) return false;

    Json& frames = graphic["frames"];
    if (!frames.contains(current_frame) || !frames[current_frame].is_array()) return false;
    auto& rows = frames[current_frame].as_array();
    if (pixel.y() >= static_cast<int>(rows.size()) ||
        !rows[static_cast<size_t>(pixel.y())].is_string()) return false;

    const char selected_pixel = "0123456789ABCDEF"[selected_color_idx];
    if (brush_mode == 3) {
        const std::string& row = rows[static_cast<size_t>(pixel.y())].as_string();
        if (pixel.x() < static_cast<int>(row.size())) {
            emit colorPicked(palette_index(row[static_cast<size_t>(pixel.x())]));
        }
        return false;
    }

    // Snapshot before the first pixel that actually changes (editsweep D1.S3):
    // clicking inside the image or re-painting the same colour no longer
    // creates empty undo entries.
    const auto would_change = [&]() {
        const int brush_size = (brush_mode == 0 || brush_mode == 4) ? 8 :
                               (brush_mode == 2 ? 16 : 1);
        const int start_x = (pixel.x() / brush_size) * brush_size;
        const int start_y = (pixel.y() / brush_size) * brush_size;
        const char value = brush_mode == 4 ? '.' : selected_pixel;
        for (int y = start_y; y < std::min(height, start_y + brush_size); ++y) {
            if (y >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(y)].is_string()) continue;
            const std::string& row = rows[static_cast<size_t>(y)].as_string();
            for (int x = start_x; x < std::min(width, start_x + brush_size); ++x) {
                if (x < static_cast<int>(row.size()) &&
                    row[static_cast<size_t>(x)] != value) return true;
            }
        }
        return false;
    };
    if (stroke_pending && would_change()) {
        pushUndo();
        stroke_pending = false;
    }
    if (!stroke_pending && !would_change()) return false;

    const int brush_size = (brush_mode == 0 || brush_mode == 4) ? 8 :
                           (brush_mode == 2 ? 16 : 1);
    const int start_x = (pixel.x() / brush_size) * brush_size;
    const int start_y = (pixel.y() / brush_size) * brush_size;
    const char value = brush_mode == 4 ? '.' : selected_pixel;
    for (int y = start_y; y < std::min(height, start_y + brush_size); ++y) {
        if (y >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(y)].is_string()) continue;
        std::string& row = rows[static_cast<size_t>(y)].s_val;
        for (int x = start_x; x < std::min(width, start_x + brush_size); ++x) {
            if (x < static_cast<int>(row.size())) row[static_cast<size_t>(x)] = value;
        }
    }
    update();
    return true;
}

void PixelCanvasWidget::paintStroke(const QPoint& from, const QPoint& to) {
    // Bresenham between the previous and current stroke pixel so fast drags
    // don't leave gaps (editsweep D1.S3).
    int x0 = from.x();
    int y0 = from.y();
    const int x1 = to.x();
    const int y1 = to.y();
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    int error = dx + dy;
    do {
        paintAt({x0, y0});
        if (x0 == x1 && y0 == y1) break;
        const int error2 = 2 * error;
        if (error2 >= dy) {
            error += dy;
            x0 += x0 < x1 ? 1 : -1;
        }
        if (error2 <= dx) {
            error += dx;
            y0 += y0 < y1 ? 1 : -1;
        }
    } while (true);
}

void PixelCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#323840"));

    if (!project || !project->graphics.contains(graphic_id)) {
        painter.setPen(QColor("#c1c8ce"));
        painter.drawText(rect(), Qt::AlignCenter, "Select a graphic to edit");
        return;
    }

    const Json& graphic = project->graphics.at(graphic_id);
    const int width = graphic["w"].as_int();
    const int height = graphic["h"].as_int();
    if (width <= 0 || height <= 0 || !graphic["frames"].contains(current_frame)) return;

    constexpr int origin = 24;
    const QRect image_rect(origin, origin, width * zoom_scale, height * zoom_scale);
    const QColor checker_light("#e8eaed");
    const QColor checker_dark("#cbd1d7");
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool alternate = (x + y) % 2 != 0;
            painter.fillRect(origin + x * zoom_scale, origin + y * zoom_scale,
                             zoom_scale, zoom_scale,
                             alternate ? checker_dark : checker_light);
        }
    }

    auto [frame_width, frame_height, data] = frame_rgba(graphic, current_frame);
    if (!data.empty() && frame_width > 0 && frame_height > 0) {
        QImage image(data.data(), frame_width, frame_height, QImage::Format_RGBA8888);
        painter.drawImage(image_rect, image);
    }

    for (int x = 0; x <= width; ++x) {
        const bool major = (x % 8) == 0;
        if (brush_mode == 1 && !major) continue;
        painter.setPen(QPen(major ? QColor(42, 52, 62, 210) : QColor(76, 88, 98, 120),
                            major ? 1.2 : 0.7));
        const int px = origin + x * zoom_scale;
        painter.drawLine(px, origin, px, origin + height * zoom_scale);
    }
    for (int y = 0; y <= height; ++y) {
        const bool major = (y % 8) == 0;
        if (brush_mode == 1 && !major) continue;
        painter.setPen(QPen(major ? QColor(42, 52, 62, 210) : QColor(76, 88, 98, 120),
                            major ? 1.2 : 0.7));
        const int py = origin + y * zoom_scale;
        painter.drawLine(origin, py, origin + width * zoom_scale, py);
    }

    if (hover_pixel.x() >= 0 && hover_pixel.x() < width &&
        hover_pixel.y() >= 0 && hover_pixel.y() < height) {
        const int brush_size = (brush_mode == 0 || brush_mode == 4) ? 8 :
                               (brush_mode == 2 ? 16 : 1);
        const int start_x = (hover_pixel.x() / brush_size) * brush_size;
        const int start_y = (hover_pixel.y() / brush_size) * brush_size;
        painter.setPen(QPen(QColor("#ffffff"), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(origin + start_x * zoom_scale, origin + start_y * zoom_scale,
                         std::min(brush_size, width - start_x) * zoom_scale,
                         std::min(brush_size, height - start_y) * zoom_scale);
    }

    painter.setPen(QColor("#c1c8ce"));
    painter.drawText(24, height * zoom_scale + 42,
                     QString("%1 × %2 px  ·  8×8 SNES tiles").arg(width).arg(height));
}

void PixelCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (!project || !project->graphics.contains(graphic_id)) return;
    const QPoint pixel(static_cast<int>(std::floor((event->position().x() - 24) / zoom_scale)),
                       static_cast<int>(std::floor((event->position().y() - 24) / zoom_scale)));
    if (event->button() == Qt::LeftButton) {
        if (brush_mode != 3) {
            stroke_pending = true;
            last_paint_pixel = pixel;
        }
        paintAt(pixel);
    }
}

void PixelCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pixel(static_cast<int>(std::floor((event->position().x() - 24) / zoom_scale)),
                       static_cast<int>(std::floor((event->position().y() - 24) / zoom_scale)));
    if (pixel != hover_pixel) {
        hover_pixel = pixel;
        update();
    }
    if (event->buttons().testFlag(Qt::LeftButton) && brush_mode != 3) {
        paintStroke(last_paint_pixel, pixel);
        last_paint_pixel = pixel;
    }
}

void PixelCanvasWidget::leaveEvent(QEvent* event) {
    hover_pixel = {-1, -1};
    update();
    QWidget::leaveEvent(event);
}

PixelEditorPanel::PixelEditorPanel(Project* p, QWidget* parent)
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

    spriteControls = new QWidget(this);
    auto* spriteLayout = new QHBoxLayout(spriteControls);
    spriteLayout->setContentsMargins(0, 0, 0, 0);
    spriteLayout->setSpacing(4);
    spriteLayout->addWidget(new QLabel("Direction:", spriteControls));
    directionCombo = new QComboBox(spriteControls);
    directionCombo->addItems({"down", "down_right", "right", "up_right",
                              "up", "up_left", "left", "down_left"});
    spriteLayout->addWidget(directionCombo);
    spriteLayout->addWidget(new QLabel("Pose:", spriteControls));
    poseCombo = new QComboBox(spriteControls);
    poseCombo->addItems({"idle", "walk", "attack"});
    spriteLayout->addWidget(poseCombo);
    toolbar->addWidget(spriteControls);

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
    zoomCombo->addItem("Fit 720p", 0);
    zoomCombo->addItem("1×", 1);
    zoomCombo->addItem("2×", 2);
    zoomCombo->addItem("4×", 4);
    zoomCombo->addItem("8×", 8);
    zoomCombo->addItem("16×", 16);
    zoomCombo->addItem("32×", 32);
    zoomCombo->addItem("48×", 48);
    zoomCombo->addItem("64×", 64);
    toolbar->addWidget(zoomCombo);
    toolbar->addSpacing(12);
    toolbar->addWidget(new QLabel("Frame:", this));
    frameCombo = new QComboBox(this);
    frameCombo->setMinimumWidth(110);
    toolbar->addWidget(frameCombo);
    toolbar->addWidget(new QLabel("Animation:", this));
    animationCombo = new QComboBox(this);
    animationCombo->setMinimumWidth(130);
    toolbar->addWidget(animationCombo);
    playAnimationButton = new QPushButton("▶ Preview", this);
    playAnimationButton->setCheckable(true);
    toolbar->addWidget(playAnimationButton);
    toolbar->addStretch(1);
    mainLayout->addLayout(toolbar);

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
    canvas = new PixelCanvasWidget();
    canvasScroll->setWidget(canvas);
    splitter->addWidget(canvasScroll);
    splitter->setSizes({250, 950});
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
    connect(frameCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && !playAnimationButton->isChecked())
            canvas->setFrame(frameCombo->itemData(index).toString().toStdString());
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
        const std::string graphic_id = graphicCombo->currentData().toString().toStdString();
        const std::string animation_id = animationCombo->currentData().toString().toStdString();
        const Json& animation = project->graphics.at(graphic_id)["anims"][animation_id];
        const int fps = animation.contains("fps") ? std::clamp(animation["fps"].as_int(), 1, 60) : 8;
        previewLoop = !animation.contains("loop") || !animation["loop"].is_bool() ||
                      animation["loop"].as_bool();
        previewFrameIndex = 0;
        playAnimationButton->setText("■ Stop");
        animationTimer->start(std::max(16, 1000 / fps));
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
    connect(canvas, &PixelCanvasWidget::colorPicked, this, [this](int index) {
        if (paletteWidget && index < paletteWidget->count()) paletteWidget->setCurrentRow(index);
    });

    add_frame_actions(toolbar, this, canvas);
    updateEditorMode(0);
    refreshGraphics();
}

void PixelEditorPanel::setProject(Project* p) {
    project = p;
    if (canvas) canvas->clearHistory();
    refreshGraphics();
}

void PixelEditorPanel::refreshGraphics() {
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

void PixelEditorPanel::selectGraphic(const std::string& id) {
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

void PixelEditorPanel::updateEditorMode(int mode) {
    const bool spriteMode = mode == 1;
    if (spriteMode) {
        graphicBrushIndex = brushCombo->currentIndex();
        graphicZoomIndex = zoomCombo->currentIndex();
    }
    graphicLabel->setText(spriteMode ? "Sprite:" : "Graphic:");
    spriteControls->setVisible(spriteMode);
    frameCombo->setVisible(spriteMode);
    animationCombo->setVisible(!spriteMode);
    playAnimationButton->setVisible(!spriteMode);
    if (spriteMode) {
        brushCombo->clear();
        brushCombo->addItems({"Pixel", "8×8 cluster", "Erase 8×8"});
        brushCombo->setToolTip("Brush (keys 1-3)");
        brushCombo->setCurrentIndex(0);
        zoomCombo->setCurrentIndex(std::max(0, zoomCombo->findData(2)));
    } else {
        brushCombo->clear();
        brushCombo->addItems({"8×8 Tile", "Pixel", "16×16 Stamp", "Color Picker", "Eraser"});
        brushCombo->setToolTip("Brush (keys 1-5)");
        brushCombo->setCurrentIndex(std::clamp(graphicBrushIndex, 0, brushCombo->count() - 1));
        zoomCombo->setCurrentIndex(std::clamp(graphicZoomIndex, 0, zoomCombo->count() - 1));
    }
    refreshGraphics();
}

void PixelEditorPanel::refreshSpriteFrames() {
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
    frameCombo->blockSignals(false);
    if (frameCombo->count() > 0) {
        frameCombo->setCurrentIndex(0);
        canvas->setFrame(frameCombo->itemData(0).toString().toStdString());
    }
}

void PixelEditorPanel::refreshAnimationData() {
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

void PixelEditorPanel::refreshPalette() {
    if (!project) {
        paletteWidget->clear();
        return;
    }
    const std::string id = graphicCombo->currentIndex() >= 0
                               ? graphicCombo->currentData().toString().toStdString()
                               : std::string();
    PixelEditSession::fill_palette(paletteWidget, *project, id);
}

void PixelEditorPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (canvas && canvasScroll) {
        canvas->setAvailableViewportSize(canvasScroll->viewport()->size());
    }
}



} // namespace aur
