#include "TransportIcon.hpp"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

namespace aur
{
    namespace music
    {

        QIcon transportIcon(TransportGlyph glyph)
        {
            QIcon icon;
            for (int size : {16, 32})
            {
                QPixmap pixmap(size, size);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor("#e8e1cf"));
                const qreal s = size;
                if (glyph == TransportGlyph::Stop)
                {
                    painter.drawRect(QRectF(s * 0.26, s * 0.26, s * 0.48, s * 0.48));
                }
                else if (glyph == TransportGlyph::Pause)
                {
                    painter.drawRect(QRectF(s * 0.26, s * 0.20, s * 0.17, s * 0.60));
                    painter.drawRect(QRectF(s * 0.57, s * 0.20, s * 0.17, s * 0.60));
                }
                else
                {
                    QPolygonF triangle;
                    triangle << QPointF(s * 0.30, s * 0.20) << QPointF(s * 0.30, s * 0.80)
                             << QPointF(s * 0.78, s * 0.50);
                    painter.drawPolygon(triangle);
                }
                painter.end();
                icon.addPixmap(pixmap);
            }
            return icon;
        }

    } // namespace music
} // namespace aur
