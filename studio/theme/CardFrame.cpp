#include "theme/CardFrame.hpp"

#include "theme/ThemeMetrics.hpp"

#include <QPainter>

namespace aur::theme
{

    CardFrame::CardFrame(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, false);
    }

    void CardFrame::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kCardRadius, kCardRadius);
    }

}
