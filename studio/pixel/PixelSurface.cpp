#include "pixel/PixelSurface.hpp"

#include "pixel/PixelFrameOps.hpp"
#include "aur/gfx.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace aur
{

    PixelSurface::PixelSurface(QWidget* parent) : QWidget(parent) {
        setMinimumSize(420, 320);
        setMouseTracking(true);
        setBackgroundRole(QPalette::Dark);
        undo_stack = std::make_unique<QUndoStack>(this);
    }

    int PixelSurface::palette_index(char pixel) {
        if (pixel >= '0' && pixel <= '9') return pixel - '0';
        if (pixel >= 'a' && pixel <= 'f') return pixel - 'a' + 10;
        if (pixel >= 'A' && pixel <= 'F') return pixel - 'A' + 10;
        return 0;
    }

    void PixelSurface::resetStroke() {
        stroke_pending = false;
        last_paint_pixel = {-1, -1};
    }

    void PixelSurface::setRowsTarget(Json* rows, const std::vector<std::string>& colors,
                                     int width, int height) {
        rows_target = rows;
        palette = colors;
        surface_width = width;
        surface_height = height;
        resetStroke();
        resizeForSurface();
        update();
    }

    void PixelSurface::setOnionSkin(const std::vector<const Json*>& frames, int current_index) {
        onion_rows = frames;
        onion_index = frames.empty() ? -1 : current_index;
        update();
    }

    void PixelSurface::pushUndo() {
        if (!rows_target || !rows_target->is_array()) return;
        auto command = std::make_unique<editor::SnapshotCommand>(QStringLiteral("paint pixels"));
        editor::RegionKey key;
        key.kind = editor::RegionKey::Direct;
        command->setRegion(key);
        command->setDirect(rows_target);
        command->setBefore(*rows_target);
        pending_open = true;
        pending_command = std::move(command);
    }

    void PixelSurface::finishUndo() {
        if (!pending_open || !pending_command || !rows_target) {
            pending_open = false;
            pending_command.reset();
            return;
        }
        pending_command->setAfter(*rows_target);
        undo_stack->push(pending_command.release());
        pending_open = false;
        pending_command.reset();
    }

    void PixelSurface::restoreRows(const Json& rows) {
        if (!rows_target) return;
        *rows_target = rows;
        update();
    }

    void PixelSurface::applyChange(Json* target, const QString& label,
                                  const std::function<void(Json&)>& change) {
        if (!target) return;
        auto command = std::make_unique<editor::SnapshotCommand>(label);
        editor::RegionKey key;
        key.kind = editor::RegionKey::Direct;
        command->setRegion(key);
        command->setDirect(target);
        command->setBefore(*target);
        change(*target);
        command->setAfter(*target);
        undo_stack->push(command.release());
        update();
    }

    void PixelSurface::undo() {
        if (undo_stack->canUndo()) {
            undo_stack->undo();
            update();
            emit painted();
        }
    }

    void PixelSurface::redo() {
        if (undo_stack->canRedo()) {
            undo_stack->redo();
            update();
            emit painted();
        }
    }

    void PixelSurface::clearHistory() {
        undo_stack->clear();
    }

    void PixelSurface::copyRows() {
        if (!rows_target || !rows_target->is_array()) return;
        QString text = QString("AUR_FRAME %1 %2\n").arg(surface_width).arg(surface_height);
        for (const Json& row : rows_target->as_array()) {
            if (!row.is_string()) return;
            text += QString::fromStdString(row.as_string()) + '\n';
        }
        QApplication::clipboard()->setText(text);
    }

    void PixelSurface::cutRows() {
        if (!rows_target || !rows_target->is_array()) return;
        copyRows();
        pushUndo();
        JsonArray blank;
        for (int y = 0; y < surface_height; ++y)
            blank.emplace_back(std::string(static_cast<size_t>(surface_width), '.'));
        restoreRows(blank);
        finishUndo();
    }

    void PixelSurface::pasteRows() {
        if (!rows_target) return;
        const QStringList lines = QApplication::clipboard()->text().split('\n', Qt::KeepEmptyParts);
        if (lines.isEmpty()) return;
        std::istringstream header(lines[0].toStdString());
        std::string signature;
        int width = 0;
        int height = 0;
        std::string trailing;
        if (!(header >> signature >> width >> height) || (header >> trailing) ||
            signature != "AUR_FRAME") return;
        if (width != surface_width || height != surface_height ||
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
        restoreRows(rows);
        finishUndo();
    }

    void PixelSurface::mirrorRightToLeft() {
        if (!rows_target || !rows_target->is_array()) return;
        pushUndo();
        *rows_target = editor::mirror_rows_right_to_left(rows_target->as_array());
        finishUndo();
        update();
    }

    QImage PixelSurface::toImage() const {
        if (!rows_target || !rows_target->is_array() || surface_width <= 0 ||
            surface_height <= 0)
            return {};
        QImage image(surface_width, surface_height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        const auto &rows = rows_target->as_array();
        for (int y = 0; y < surface_height && y < static_cast<int>(rows.size()); ++y) {
            const Json &row = rows[static_cast<size_t>(y)];
            if (!row.is_string()) continue;
            const std::string &text = row.as_string();
            for (int x = 0; x < surface_width && x < static_cast<int>(text.size()); ++x) {
                const int color_index = palette_index(text[static_cast<size_t>(x)]);
                if (color_index <= 0 || color_index >= static_cast<int>(palette.size())) continue;
                const Rgb color = hex_to_rgba(palette[static_cast<size_t>(color_index)]);
                image.setPixelColor(x, y, QColor(color.r, color.g, color.b, color.a));
            }
        }
        return image;
    }

    void PixelSurface::exportPng() {
        const QImage image = toImage();
        if (image.isNull())
            return;
        const QString path = QFileDialog::getSaveFileName(
            this, "Export as PNG", QString("frame.png"), "PNG images (*.png)");
        if (path.isEmpty())
            return;
        if (!image.save(path, "PNG"))
            QMessageBox::warning(this, "Export failed",
                                 QString("Could not write %1").arg(path));
    }

    void PixelSurface::resizeForSurface() {
        if (surface_width <= 0 || surface_height <= 0) return;
        // The margin around the image: the readout sits under it.
        constexpr int margin = 24;
        if (auto_zoom) {
            // Fit: the content fills the space the canvas was given, and the
            // canvas is the size of that space, so nothing is cut off and
            // nothing is empty. The margin is the space around the image.
            const int target_width = std::max(1, available_viewport.width() - margin * 2);
            const int target_height = std::max(1, available_viewport.height() - margin * 2);
            const int height_scale = target_height / surface_height;
            const int width_scale = target_width / surface_width;
            zoom_scale = std::clamp(std::min(height_scale, width_scale), 1, 64);
        }
        resize(std::max(420, surface_width * zoom_scale + margin * 2),
               std::max(320, surface_height * zoom_scale + margin * 2));
    }

    void PixelSurface::setSelectedColor(int idx) {
        selected_color_idx = std::clamp(idx, 0, 15);
        update();
    }

    void PixelSurface::setZoomScale(int scale) {
        auto_zoom = scale <= 0;
        if (!auto_zoom) zoom_scale = std::clamp(scale, 1, 64);
        resizeForSurface();
        update();
    }

    void PixelSurface::setAvailableViewportSize(const QSize& size) {
        available_viewport = size;
        if (auto_zoom) {
            resizeForSurface();
            update();
        }
    }

    void PixelSurface::setBrushMode(int mode) {
        brush_mode = mode;
        setCursor(brush_mode == 3 ? Qt::PointingHandCursor : Qt::CrossCursor);
        update();
    }

    bool PixelSurface::paintAt(const QPoint& pixel) {
        if (!rows_target || !rows_target->is_array() || pixel.x() < 0 || pixel.y() < 0)
            return false;
        if (pixel.x() >= surface_width || pixel.y() >= surface_height) return false;
        auto& rows = rows_target->as_array();
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
            for (int y = start_y; y < std::min(surface_height, start_y + brush_size); ++y) {
                if (y >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(y)].is_string())
                    continue;
                const std::string& row = rows[static_cast<size_t>(y)].as_string();
                for (int x = start_x; x < std::min(surface_width, start_x + brush_size); ++x) {
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
        for (int y = start_y; y < std::min(surface_height, start_y + brush_size); ++y) {
            if (y >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(y)].is_string())
                continue;
            std::string& row = rows[static_cast<size_t>(y)].s_val;
            for (int x = start_x; x < std::min(surface_width, start_x + brush_size); ++x) {
                if (x < static_cast<int>(row.size())) row[static_cast<size_t>(x)] = value;
            }
        }
        update();
        emit painted();
        return true;
    }

    void PixelSurface::paintStroke(const QPoint& from, const QPoint& to) {
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

    void PixelSurface::paintEvent(QPaintEvent*) {
        using theme::qt;
        const theme::Palette& pal = theme::tokens().palette;
        QPainter painter(this);
        painter.fillRect(rect(), qt(pal.surface_variant));

        if (!rows_target || !rows_target->is_array() || surface_width <= 0 || surface_height <= 0) {
            painter.setPen(qt(pal.text_dim));
            painter.drawText(rect(), Qt::AlignCenter, "Select something to edit");
            return;
        }

        constexpr int origin = 24;  // the readout sits under the image
        const QRect image_rect(origin, origin, surface_width * zoom_scale, surface_height * zoom_scale);
        const QColor checker_light("#e8eaed");
        const QColor checker_dark("#cbd1d7");
        for (int y = 0; y < surface_height; ++y) {
            for (int x = 0; x < surface_width; ++x) {
                const bool alternate = (x + y) % 2 != 0;
                painter.fillRect(origin + x * zoom_scale, origin + y * zoom_scale,
                                 zoom_scale, zoom_scale,
                                 alternate ? checker_dark : checker_light);
            }
        }

        // D5.S1.1: previous frame warm, next frame cool, ghosted behind the current one.
        if (onion_index >= 0 && onion_index < static_cast<int>(onion_rows.size())) {
            const auto ghost = [&](int index, const QColor& tint) {
                if (index < 0 || index >= static_cast<int>(onion_rows.size())) return;
                const Json *rows = onion_rows[static_cast<size_t>(index)];
                if (!rows || !rows->is_array()) return;
                painter.setOpacity(0.32);
                for (int y = 0; y < surface_height && y < static_cast<int>(rows->as_array().size()); ++y) {
                    const Json &row = rows->as_array()[static_cast<size_t>(y)];
                    if (!row.is_string()) continue;
                    const std::string &text = row.as_string();
                    for (int x = 0; x < surface_width && x < static_cast<int>(text.size()); ++x) {
                        const int color_index = palette_index(text[static_cast<size_t>(x)]);
                        if (color_index <= 0 || color_index >= static_cast<int>(palette.size())) continue;
                        const Rgb color = hex_to_rgba(palette[static_cast<size_t>(color_index)]);
                        painter.fillRect(origin + x * zoom_scale, origin + y * zoom_scale,
                                         zoom_scale, zoom_scale,
                                         QColor(color.r, color.g, color.b, color.a));
                    }
                }
                painter.setOpacity(1.0);
            };
            ghost(onion_index - 1, QColor("#e0685f"));
            ghost(onion_index + 1, QColor("#5f9fe0"));
        }

        const auto &rows = rows_target->as_array();
        for (int y = 0; y < surface_height && y < static_cast<int>(rows.size()); ++y) {
            const Json &row = rows[static_cast<size_t>(y)];
            if (!row.is_string()) continue;
            const std::string &text = row.as_string();
            for (int x = 0; x < surface_width && x < static_cast<int>(text.size()); ++x) {
                const int color_index = palette_index(text[static_cast<size_t>(x)]);
                if (color_index <= 0 || color_index >= static_cast<int>(palette.size())) continue;
                const Rgb color = hex_to_rgba(palette[static_cast<size_t>(color_index)]);
                painter.fillRect(origin + x * zoom_scale, origin + y * zoom_scale,
                                 zoom_scale, zoom_scale,
                                 QColor(color.r, color.g, color.b, color.a));
            }
        }

        for (int x = 0; x <= surface_width; ++x) {
            const bool major = (x % 8) == 0;
            if (brush_mode == 1 && !major) continue;
            painter.setPen(QPen(major ? QColor(42, 52, 62, 210) : QColor(76, 88, 98, 120),
                                major ? 1.2 : 0.7));
            const int px = origin + x * zoom_scale;
            painter.drawLine(px, origin, px, origin + surface_height * zoom_scale);
        }
        for (int y = 0; y <= surface_height; ++y) {
            const bool major = (y % 8) == 0;
            if (brush_mode == 1 && !major) continue;
            painter.setPen(QPen(major ? QColor(42, 52, 62, 210) : QColor(76, 88, 98, 120),
                                major ? 1.2 : 0.7));
            const int py = origin + y * zoom_scale;
            painter.drawLine(origin, py, origin + surface_width * zoom_scale, py);
        }

        if (hover_pixel.x() >= 0 && hover_pixel.x() < surface_width &&
            hover_pixel.y() >= 0 && hover_pixel.y() < surface_height) {
            const int brush_size = (brush_mode == 0 || brush_mode == 4) ? 8 :
                                   (brush_mode == 2 ? 16 : 1);
            const int start_x = (hover_pixel.x() / brush_size) * brush_size;
            const int start_y = (hover_pixel.y() / brush_size) * brush_size;
            painter.setPen(QPen(QColor("#ffffff"), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(origin + start_x * zoom_scale, origin + start_y * zoom_scale,
                             std::min(brush_size, surface_width - start_x) * zoom_scale,
                             std::min(brush_size, surface_height - start_y) * zoom_scale);
        }

        painter.setPen(qt(pal.text_dim));
        painter.drawText(24, surface_height * zoom_scale + 42,
                         QString("%1 × %2 px  ·  8×8 SNES tiles").arg(surface_width).arg(surface_height));
    }

    void PixelSurface::mousePressEvent(QMouseEvent* event) {
        if (!rows_target) return;
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

    void PixelSurface::mouseMoveEvent(QMouseEvent* event) {
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

    void PixelSurface::mouseReleaseEvent(QMouseEvent* event) {
        if (event->button() == Qt::LeftButton && pending_open) finishUndo();
    }

    void PixelSurface::resizeEvent(QResizeEvent* event) {
        QWidget::resizeEvent(event);
    }

    void PixelSurface::leaveEvent(QEvent* event) {
        hover_pixel = {-1, -1};
        update();
        QWidget::leaveEvent(event);
    }

} // namespace aur
