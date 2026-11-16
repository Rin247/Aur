#pragma once

#include <QImage>
#include <QString>
#include <QWidget>
#include <string>
#include <vector>
#include "aur/json.hpp"

namespace aur::tileset {

void export_surface_png(QWidget* parent, const QImage& image, const QString& title);
JsonArray import_surface_png_rows(QWidget* parent, int target_size,
                                  const std::vector<std::string>& palette, const QString& title);

} // namespace aur::tileset
