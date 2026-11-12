#include "CreaturePortrait.hpp"

#include "BestiaryData.hpp"

#include <QPainter>

#include <algorithm>

namespace aur
{
    namespace bestiary
    {

        CreaturePortrait::CreaturePortrait(QWidget *parent) : QWidget(parent)
        {
            setMinimumHeight(120);
        }

        void CreaturePortrait::setProject(Project *value)
        {
            project = value;
            reload();
        }

        void CreaturePortrait::setGraphicId(const QString &id)
        {
            graphicId = id;
            reload();
        }

        void CreaturePortrait::reload()
        {
            image = QImage();
            if (project && !graphicId.isEmpty())
            {
                int width = 0;
                int height = 0;
                std::vector<unsigned char> pixels;
                if (bestiary::artwork(*project, graphicId.toStdString(), width, height, pixels))
                    image = QImage(pixels.data(), width, height, QImage::Format_RGBA8888)
                                .convertToFormat(QImage::Format_ARGB32);
            }
            update();
        }

        void CreaturePortrait::paintEvent(QPaintEvent *)
        {
            QPainter painter(this);
            painter.fillRect(rect(), QColor("#151a22"));
            if (image.isNull())
            {
                painter.setPen(QColor("#aeb8c2"));
                painter.drawText(rect(), Qt::AlignCenter,
                                 graphicId.isEmpty() ? "No creature selected"
                                                     : "The project has no artwork for this creature");
                return;
            }

            // The picture fits the pane it is given, and the checkerboard
            // behind it shows what is transparent.
            constexpr int margin = 12;
            const int scale = std::max(1, std::min((width() - margin * 2) / image.width(),
                                                   (height() - margin * 2) / image.height()));
            const int draw_width = image.width() * scale;
            const int draw_height = image.height() * scale;
            const QRect target((width() - draw_width) / 2, (height() - draw_height) / 2,
                               draw_width, draw_height);
            constexpr int checker = 4;
            for (int y = 0; y < image.height(); y += checker)
            {
                for (int x = 0; x < image.width(); x += checker)
                {
                    const bool alternate = ((x / checker) + (y / checker)) % 2 != 0;
                    painter.fillRect(QRect(target.left() + x * scale, target.top() + y * scale,
                                           std::min(checker, image.width() - x) * scale,
                                           std::min(checker, image.height() - y) * scale),
                                     alternate ? QColor("#cbd1d7") : QColor("#eef0f2"));
                }
            }
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.drawImage(target, image);
        }

    } // namespace bestiary
} // namespace aur
