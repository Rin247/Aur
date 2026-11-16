#include "tileset/TilePngIo.hpp"
#include "pixel/PixelFrameOps.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include <cstddef>

namespace aur::tileset {

void export_surface_png(QWidget* parent, const QImage& image, const QString& title) {
    if (image.isNull()) return;
    const QString path = QFileDialog::getSaveFileName(parent, title, QString(), "PNG images (*.png)");
    if (path.isEmpty()) return;
    if (!image.save(path, "PNG")) {
        QMessageBox::warning(parent, "Export failed", QString("Could not write %1").arg(path));
    }
}

JsonArray import_surface_png_rows(QWidget* parent, int target_size,
                                  const std::vector<std::string>& palette, const QString& title) {
    if (target_size <= 0) return {};
    const QString path = QFileDialog::getOpenFileName(parent, title, QString(), "PNG images (*.png)");
    if (path.isEmpty()) return {};
    const QImage source(path);
    if (source.isNull()) {
        QMessageBox::warning(parent, "Import failed", QString("Could not read %1").arg(path));
        return {};
    }
    const QImage scaled = source.scaled(target_size, target_size, Qt::IgnoreAspectRatio,
                                        Qt::FastTransformation);
    const QImage rgba = scaled.convertToFormat(QImage::Format_RGBA8888);
    const std::size_t byte_count = static_cast<std::size_t>(rgba.width()) *
                                   static_cast<std::size_t>(rgba.height()) * 4;
    const std::vector<unsigned char> bytes(rgba.constBits(), rgba.constBits() + byte_count);
    return editor::snap_rgba_to_rows(bytes, rgba.width(), rgba.height(), palette);
}

} // namespace aur::tileset
