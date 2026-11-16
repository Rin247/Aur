#include "theme/ThemeIcons.hpp"

#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace aur::theme
{
    namespace
    {
        QColor glyph_color(QIcon::Mode mode, Glyph glyph)
        {
            const Tokens &t = tokens();
            const Palette &p = t.palette;
            if (mode == QIcon::Disabled) return qt(mix(p.text_dim, p.background, 0.35));
            if (mode == QIcon::Selected) return qt(t.on_accent);
            if (glyph == Glyph::Close && mode == QIcon::Active) return QColor(255, 255, 255);
            return qt(p.text);
        }

        void draw_glyph(QPainter *painter, const QRect &rect, Glyph glyph, const QColor &color)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            const QRectF r(rect);
            const qreal s = std::min(r.width(), r.height());
            const QPointF o(r.center().x() - s * 0.5, r.center().y() - s * 0.5);
            auto pt = [&](qreal x, qreal y) { return QPointF(o.x() + x * s, o.y() + y * s); };
            const qreal stroke = std::max(1.4, s * 0.105);
            QPen pen(color, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);

            switch (glyph)
            {
            case Glyph::Hamburger:
                for (qreal y : {0.26, 0.50, 0.74})
                    painter->drawLine(pt(0.20, y), pt(0.80, y));
                break;
            case Glyph::Minimize:
                painter->drawLine(pt(0.24, 0.52), pt(0.76, 0.52));
                break;
            case Glyph::Maximize:
                painter->drawRect(QRectF(pt(0.24, 0.24), pt(0.76, 0.76)));
                break;
            case Glyph::Restore:
                painter->drawRect(QRectF(pt(0.32, 0.22), pt(0.78, 0.68)));
                painter->drawRect(QRectF(pt(0.22, 0.32), pt(0.68, 0.78)));
                break;
            case Glyph::Close:
                painter->drawLine(pt(0.26, 0.26), pt(0.74, 0.74));
                painter->drawLine(pt(0.74, 0.26), pt(0.26, 0.74));
                break;
            case Glyph::Play:
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawPolygon(QPolygonF{pt(0.28, 0.20), pt(0.28, 0.80), pt(0.78, 0.50)});
                break;
            case Glyph::Pause:
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawRoundedRect(QRectF(pt(0.25, 0.20), pt(0.43, 0.80)), 1.0, 1.0);
                painter->drawRoundedRect(QRectF(pt(0.57, 0.20), pt(0.75, 0.80)), 1.0, 1.0);
                break;
            case Glyph::Stop:
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawRoundedRect(QRectF(pt(0.25, 0.25), pt(0.75, 0.75)), 1.5, 1.5);
                break;
            case Glyph::Save:
            {
                QPainterPath body;
                body.moveTo(pt(0.20, 0.20));
                body.lineTo(pt(0.68, 0.20));
                body.lineTo(pt(0.80, 0.32));
                body.lineTo(pt(0.80, 0.80));
                body.lineTo(pt(0.20, 0.80));
                body.closeSubpath();
                painter->drawPath(body);
                painter->drawRect(QRectF(pt(0.32, 0.20), pt(0.62, 0.42)));
                painter->drawRect(QRectF(pt(0.30, 0.54), pt(0.70, 0.80)));
                break;
            }
            case Glyph::Open:
            {
                QPainterPath folder;
                folder.moveTo(pt(0.16, 0.30));
                folder.lineTo(pt(0.40, 0.30));
                folder.lineTo(pt(0.48, 0.40));
                folder.lineTo(pt(0.84, 0.40));
                folder.lineTo(pt(0.84, 0.78));
                folder.lineTo(pt(0.16, 0.78));
                folder.closeSubpath();
                painter->drawPath(folder);
                break;
            }
            case Glyph::Reload:
                painter->drawArc(QRectF(pt(0.20, 0.20), pt(0.80, 0.80)), 35 * 16, 275 * 16);
                painter->drawPolyline(QPolygonF{pt(0.62, 0.18), pt(0.80, 0.30), pt(0.62, 0.40)});
                break;
            case Glyph::Appearance:
                painter->drawEllipse(QRectF(pt(0.18, 0.18), pt(0.82, 0.82)));
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawEllipse(pt(0.38, 0.38), s * 0.06, s * 0.06);
                painter->drawEllipse(pt(0.62, 0.38), s * 0.06, s * 0.06);
                painter->drawEllipse(pt(0.50, 0.62), s * 0.06, s * 0.06);
                break;
            }
            painter->restore();
        }

        class ThemeIconEngine final : public QIconEngine
        {
        public:
            explicit ThemeIconEngine(Glyph glyph) : glyph_(glyph) {}

            void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
                       QIcon::State) override
            {
                draw_glyph(painter, rect, glyph_, glyph_color(mode, glyph_));
            }

            QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
            {
                QPixmap pm(size);
                pm.fill(Qt::transparent);
                QPainter painter(&pm);
                paint(&painter, QRect(QPoint(0, 0), size), mode, state);
                return pm;
            }

            QIconEngine *clone() const override { return new ThemeIconEngine(glyph_); }

        private:
            Glyph glyph_;
        };
    }

    QIcon icon(Glyph glyph)
    {
        return QIcon(new ThemeIconEngine(glyph));
    }

}
