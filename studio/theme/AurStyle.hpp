#pragma once

#include <QProxyStyle>

namespace aur::theme
{

    class AurStyle : public QProxyStyle
    {
    public:
        AurStyle();

        int pixelMetric(PixelMetric metric, const QStyleOption *option,
                        const QWidget *widget) const override;
        QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                               const QWidget *widget) const override;
        QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                             SubControl sub, const QWidget *widget) const override;
        void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                           const QWidget *widget) const override;
        void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                         const QWidget *widget) const override;
        void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                QPainter *painter, const QWidget *widget) const override;
    };

}
