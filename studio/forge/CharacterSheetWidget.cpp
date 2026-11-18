#include "forge/CharacterSheetWidget.hpp"
#include "aur/gfx.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QImage>
#include <QPainter>
#include <algorithm>
#include <utility>
#include <vector>

namespace aur {

CharacterSheetWidget::CharacterSheetWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 280);
}

void CharacterSheetWidget::setGraphic(const Json& g) {
    graphic = g;
    update();
}

void CharacterSheetWidget::setActiveFrame(const std::string& frameName) {
    activeFrame = frameName;
    update();
}

void CharacterSheetWidget::setTurnaroundMode(bool enabled) {
    turnaroundMode = enabled;
    update();
}

void CharacterSheetWidget::paintEvent(QPaintEvent*) {
    using theme::qt;
    const theme::Palette& pal = theme::tokens().palette;
    QPainter painter(this);
    painter.fillRect(rect(), qt(pal.surface_variant));

    if (!graphic.contains("frames") || !graphic["frames"].is_object() ||
        graphic["frames"].as_object().empty()) {
        painter.setPen(qt(pal.text_dim));
        painter.drawText(rect(), Qt::AlignCenter, "Generate a seeded asset to preview");
        return;
    }

    const JsonObject& frames = graphic["frames"].as_object();
    std::vector<std::pair<std::string, std::string>> views;
    if (turnaroundMode && frames.contains("down_0") && frames.contains("right_0") &&
        frames.contains("up_0") && frames.contains("left_0")) {
        views = {{"down_0", "Front"}, {"right_0", "Right"}, {"up_0", "Back"}, {"left_0", "Left"}};
    } else {
        std::string frame = frames.contains(activeFrame) ? activeFrame
                          : frames.contains("down_0") ? "down_0"
                          : frames.begin()->first;
        views = {{frame, frame}};
    }

    auto [image_width, image_height, data] = frame_rgba(graphic, views[0].first);
    if (image_width <= 0 || image_height <= 0 || data.empty()) return;

    const int count = static_cast<int>(views.size());
    const int gap = 16;
    const int avail_width = std::max(64, width() - 32 - gap * (count - 1));
    const int avail_height = std::max(64, height() - 76);
    const int scale = std::clamp(
        std::min((avail_width / count) / image_width, avail_height / image_height), 1, 16);

    const int draw_width = image_width * scale;
    const int draw_height = image_height * scale;
    const int total_width = draw_width * count + gap * (count - 1);
    const int start_x = (width() - total_width) / 2;
    const int start_y = (height() - draw_height) / 2 - 4;

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    constexpr int checker = 4;

    for (int i = 0; i < count; ++i) {
        const QRect target(start_x + i * (draw_width + gap), start_y, draw_width, draw_height);
        for (int y = 0; y < image_height; y += checker) {
            for (int x = 0; x < image_width; x += checker) {
                const bool alternate = ((x / checker) + (y / checker)) % 2 != 0;
                painter.fillRect(
                    QRect(target.left() + x * scale, target.top() + y * scale,
                          std::min(checker, image_width - x) * scale,
                          std::min(checker, image_height - y) * scale),
                    alternate ? QColor("#cbd1d7") : QColor("#eef0f2"));
            }
        }
        auto [view_width, view_height, view_data] =
            frame_rgba(graphic, views[static_cast<size_t>(i)].first);
        if (view_width > 0 && view_height > 0 && !view_data.empty()) {
            QImage image(view_data.data(), view_width, view_height, QImage::Format_RGBA8888);
            painter.drawImage(target, image);
        }
        painter.setPen(qt(pal.text_dim));
        painter.drawText(QRect(target.left(), target.bottom() + 6, target.width(), 18),
                         Qt::AlignCenter,
                         QString::fromStdString(views[static_cast<size_t>(i)].second));
    }

    painter.setPen(qt(pal.text_dim));
    painter.drawText(QRect(8, height() - 24, width() - 16, 18), Qt::AlignCenter,
                     QStringLiteral("%1 \u00d7 %2 px \u00b7 %3x zoom")
                         .arg(image_width).arg(image_height).arg(scale));
}

} // namespace aur
