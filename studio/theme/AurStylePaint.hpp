#pragma once

#include <QPalette>
#include <QRect>

class QPainter;
class QStyleOption;
class QStyleOptionComboBox;
class QStyleOptionFrame;
class QStyleOptionHeader;
class QStyleOptionSlider;
class QStyleOptionSpinBox;
class QStyleOptionTab;

namespace aur::theme::paint
{

    void buttonPanel(const QStyleOption *option, QPainter *painter);
    QPalette buttonLabelPalette(const QStyleOption *option);
    void lineEditPanel(const QStyleOptionFrame *option, QPainter *painter);
    void checkBox(const QStyleOption *option, QPainter *painter);
    void radioButton(const QStyleOption *option, QPainter *painter);
    void focusFrame(const QStyleOption *option, QPainter *painter);
    void tabWidgetFrame(const QStyleOption *option, QPainter *painter);
    void tabShape(const QStyleOptionTab *option, QPainter *painter);
    QPalette tabLabelPalette(const QStyleOptionTab *option);
    void headerSection(const QStyleOptionHeader *option, QPainter *painter);
    void splitter(const QStyleOption *option, QPainter *painter);
    void comboBox(const QStyleOptionComboBox *option, QPainter *painter, const QRect &arrow);
    void spinBox(const QStyleOptionSpinBox *option, QPainter *painter, const QRect &up,
                 const QRect &down);
    void scrollBar(const QStyleOptionSlider *option, QPainter *painter, const QRect &slider);

}
