#include "playtest/QtRenderer.hpp"
#include "aur/gfx.hpp"
#include "map/TileTexture.hpp"

#include <algorithm>
#include <cmath>

namespace aur {

void QtRenderer::set_painter(QPainter* p) { painter = p; }

void QtRenderer::begin(const std::string& st) {
    Renderer::begin(st);
    if (painter) {
        painter->save();
        painter->scale(zoom, zoom);
        w = static_cast<int>(1280.0 / zoom);
        h = static_cast<int>(720.0 / zoom);
    }
}

void QtRenderer::end() {
    if (painter) painter->restore();
}

void QtRenderer::camera(int x, int y) {
    if (painter) painter->translate(-x, -y);
}

static QColor tile_color(int idx) {
    switch (idx) {
        case 0: return QColor("#2e7d32");
        case 1: return QColor("#8d6e63");
        case 2: return QColor("#1565c0");
        case 3: return QColor("#66bb6a");
        default: return QColor("#37474f");
    }
}

QImage QtRenderer::graphic_image(const std::string& gid, const std::string& frame) {
    auto key = std::make_pair(gid, frame);
    auto it = tile_cache.find(key);
    if (it != tile_cache.end()) return it->second;
    QImage img;
    if (project) {
        Json g = project->graphic(gid);
        auto [gw, gh, data] = frame_rgba(g, frame);
        if (gw > 0 && gh > 0 && !data.empty()) {
            img = QImage(data.data(), gw, gh, QImage::Format_RGBA8888).convertToFormat(QImage::Format_ARGB32);
        }
    }
    tile_cache[key] = img;
    return img;
}

void QtRenderer::tile(const std::string& tileset, int index, int x, int y, const std::string&) {
    if (!painter) return;
    const auto key = std::make_pair(tileset, index);
    auto it = tileset_cache.find(key);
    if (it == tileset_cache.end() && project) {
        it = tileset_cache.emplace(key, make_tileset_tile(*project, tileset, index)).first;
    }
    const QImage img = it == tileset_cache.end() ? QImage{} : it->second;
    if (!img.isNull()) {
        const QImage scaled = img.scaled(field_tile_size, field_tile_size,
                                         Qt::IgnoreAspectRatio, Qt::FastTransformation);
        painter->drawImage(x, y, scaled);
    } else {
        painter->fillRect(x, y, field_tile_size, field_tile_size, tile_color(index));
        painter->setPen(QColor("#222222"));
        painter->drawRect(x, y, field_tile_size, field_tile_size);
    }
}

void QtRenderer::sprite(const std::string& graphic, const std::string& frame, int x, int y, bool flip, double scale,
                        const std::string& tint, double alpha, bool, int) {
    if (!painter) return;
    QImage img = graphic_image(graphic, frame);
    if (img.isNull()) {
        QColor c = tint.empty() ? QColor("#ffb300") : QColor(QString::fromStdString(tint));
        c.setAlphaF(alpha);
        painter->fillRect(x, y, static_cast<int>(16 * scale), static_cast<int>(16 * scale), c);
        painter->setPen(Qt::black);
        painter->drawRect(x, y, static_cast<int>(16 * scale), static_cast<int>(16 * scale));
        return;
    }
    if (flip) img = img.mirrored(true, false);
    QSize scaled(static_cast<int>(img.width() * scale), static_cast<int>(img.height() * scale));
    img = img.scaled(scaled, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (alpha < 1.0) {
        QImage tmp(img.size(), QImage::Format_ARGB32);
        tmp.fill(Qt::transparent);
        QPainter tp(&tmp);
        tp.setOpacity(alpha);
        tp.drawImage(0, 0, img);
        img = tmp;
    }
    painter->drawImage(x, y, img);
}

void QtRenderer::ground_rect(int x, int y, int w, int h, const std::string& color, double alpha) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#000000") : QColor(QString::fromStdString(color));
    c.setAlphaF(alpha);
    painter->fillRect(x, y, w, h, c);
}

void QtRenderer::world_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color) {
    if (!painter) return;
    painter->setPen(QColor(std::get<0>(color), std::get<1>(color), std::get<2>(color)));
    painter->drawText(x, y + 12, QString::fromStdString(text));
}

void QtRenderer::arc(int x, int y, int radius, double angle, double span_deg,
                     const std::tuple<int,int,int>& color, double alpha) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(alpha);
    painter->setPen(QPen(c, 3));
    painter->drawArc(x - radius, y - radius, radius * 2, radius * 2,
                     static_cast<int>(angle * 16 * 180 / 3.14159265), static_cast<int>(span_deg * 16));
}

void QtRenderer::ring(int x, int y, int radius, const std::tuple<int,int,int>& color, double alpha) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(alpha);
    painter->setPen(QPen(c, 3));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void QtRenderer::light(int x, int y, int radius, const std::tuple<int,int,int>& color, double intensity) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(0.35 * intensity);
    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    painter->drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void QtRenderer::ambient(const std::string& color, double darkness) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#ffffff") : QColor(QString::fromStdString(color));
    c.setAlphaF(std::clamp(darkness, 0.0, 1.0));
    painter->save();
    painter->resetTransform();
    painter->fillRect(painter->viewport(), c);
    painter->restore();
}

void QtRenderer::weather(const std::string& kind, double intensity, double t, double flash) {
    if (!painter || kind == "clear") return;
    painter->save();
    painter->resetTransform();
    const QRect area = painter->viewport();
    const int strength = qBound(0, static_cast<int>(intensity * 255.0), 255);
    if (kind == "cloudy" || kind == "storm") {
        painter->fillRect(area, QColor(35, 44, 58, strength / 5));
    }
    if (kind == "rain" || kind == "storm") {
        painter->fillRect(area, QColor(35, 50, 72, strength / 10));
        painter->setPen(QPen(QColor(170, 205, 235, strength / 2), kind == "storm" ? 2 : 1));
        const int dropCount = kind == "storm" ? 240 : 180;
        for (int i = 0; i < dropCount; ++i) {
            const int x = (i * 97 + static_cast<int>(t * 420.0)) % qMax(1, area.width());
            const int y = (i * 53 + static_cast<int>(t * 760.0)) % qMax(1, area.height());
            painter->drawLine(x, y, x - 5, y + 14);
        }
    } else if (kind == "snow") {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(238, 245, 255, strength * 3 / 4));
        for (int i = 0; i < 90; ++i) {
            const int x = (i * 113 + static_cast<int>(std::sin(t + i) * 24.0)) % qMax(1, area.width());
            const int y = (i * 67 + static_cast<int>(t * (18.0 + i % 9))) % qMax(1, area.height());
            painter->drawEllipse(QPoint(x, y), 2 + i % 2, 2 + i % 2);
        }
    } else if (kind == "fog") {
        painter->fillRect(area, QColor(185, 198, 205, strength / 4));
    }
    if (flash > 0.0) {
        painter->fillRect(area, QColor(245, 248, 255, qBound(0, static_cast<int>(flash * 180.0), 180)));
    }
    painter->restore();
}

void QtRenderer::ui_rect(int x, int y, int w, int h, const std::string& color, double alpha, const std::string&) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#000000") : QColor(QString::fromStdString(color));
    c.setAlphaF(alpha);
    painter->fillRect(x, y, w, h, c);
}

void QtRenderer::ui_graphic(const std::string&, const std::string&, int x, int y, int scale) {
    if (!painter) return;
    painter->fillRect(x, y, 16 * scale, 16 * scale, QColor("#9e9e9e"));
}

void QtRenderer::fade(double, const std::tuple<int,int,int>&) {}
void QtRenderer::flash(double, const std::tuple<int,int,int>&) {}

void QtRenderer::ui_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color, bool) {
    if (!painter) return;
    painter->setPen(QColor(std::get<0>(color), std::get<1>(color), std::get<2>(color)));
    painter->drawText(x, y + 12, QString::fromStdString(text));
}

} // namespace aur
