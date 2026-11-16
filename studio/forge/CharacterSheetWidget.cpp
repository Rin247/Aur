#include "forge/CharacterSheetWidget.hpp"
#include "aur/gfx.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QImage>
#include <QPainter>
#include <algorithm>

namespace aur {

CharacterSheetWidget::CharacterSheetWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(256, 256);
}

void CharacterSheetWidget::setGraphic(const Json& g) {
    graphic = g;
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

    std::string frame = "down_0";
    if (!graphic["frames"].contains(frame)) {
        frame = graphic["frames"].as_object().begin()->first;
    }
    auto [image_width, image_height, data] = frame_rgba(graphic, frame);
    if (image_width <= 0 || image_height <= 0 || data.empty()) return;

    // Fit the preview to the available pane (up to 32x).
    const int scale = std::clamp(std::min((width() - 48) / image_width,
                                          (height() - 64) / image_height), 1, 32);
    const int draw_width = image_width * scale;
    const int draw_height = image_height * scale;
    const QRect target((width() - draw_width) / 2, (height() - draw_height) / 2,
                       draw_width, draw_height);
    constexpr int checker_pixels = 4;
    for (int y = 0; y < image_height; y += checker_pixels) {
        for (int x = 0; x < image_width; x += checker_pixels) {
            const bool alternate = ((x / checker_pixels) + (y / checker_pixels)) % 2 != 0;
            painter.fillRect(QRect(target.left() + x * scale, target.top() + y * scale,
                                   std::min(checker_pixels, image_width - x) * scale,
                                   std::min(checker_pixels, image_height - y) * scale),
                             alternate ? QColor("#cbd1d7") : QColor("#eef0f2"));
        }
    }
    QImage image(data.data(), image_width, image_height, QImage::Format_RGBA8888);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, image);

    painter.setPen(qt(pal.text_dim));
    painter.drawText(QRect(8, height() - 30, width() - 16, 20), Qt::AlignCenter,
                     QStringLiteral("%1 \u00d7 %2 px \u00b7 seed preview").arg(image_width).arg(image_height));
}

} // namespace aur
