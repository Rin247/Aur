#include "theme/AurStyle.hpp"

#include "theme/AurStylePaint.hpp"
#include "theme/ThemeMetrics.hpp"

#include <QAbstractButton>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOption>

#include <algorithm>

namespace aur::theme
{
    namespace
    {
        QRect scroll_segment(const QStyleOptionSlider *sb, int from, int to)
        {
            const bool horizontal = sb->orientation == Qt::Horizontal;
            const QRect r = horizontal ? QRect(from, 0, to - from, sb->rect.height())
                                       : QRect(0, from, sb->rect.width(), to - from);
            return horizontal ? QStyle::visualRect(sb->direction, sb->rect, r) : r;
        }
    }

    AurStyle::AurStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
    }

    int AurStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                              const QWidget *widget) const
    {
        switch (metric)
        {
        case PM_ScrollBarExtent: return kScrollBarExtent;
        case PM_ScrollBarSliderMin: return kMinHitTarget;
        case PM_SplitterWidth: return kSplitterWidth;
        case PM_SmallIconSize:
        case PM_ToolBarIconSize:
        case PM_ButtonIconSize: return kMinGlyph;
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight: return kMinGlyph;
        default: return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    QSize AurStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                     const QWidget *widget) const
    {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        switch (type)
        {
        case CT_PushButton:
            return s.expandedTo(QSize(kMinHitTarget, kControlHeight)) + QSize(4, 0);
        case CT_ToolButton: return s.expandedTo(QSize(kMinHitTarget, kMinHitTarget));
        case CT_ComboBox:
        case CT_SpinBox:
        case CT_LineEdit: return s.expandedTo(QSize(0, kControlHeight));
        case CT_TabBarTab: return s.expandedTo(QSize(0, kControlHeight)) + QSize(8, 0);
        case CT_HeaderSection: return s.expandedTo(QSize(0, kControlHeight - 4));
        default: return s;
        }
    }

    QRect AurStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                   SubControl sub, const QWidget *widget) const
    {
        const auto *sb = control == CC_ScrollBar ? qstyleoption_cast<const QStyleOptionSlider *>(option)
                                                 : nullptr;
        if (!sb) return QProxyStyle::subControlRect(control, option, sub, widget);

        const bool horizontal = sb->orientation == Qt::Horizontal;
        const int length = horizontal ? sb->rect.width() : sb->rect.height();
        const int range = sb->maximum - sb->minimum;
        int slider = range > 0 ? static_cast<int>(static_cast<qint64>(sb->pageStep) * length /
                                                  (range + sb->pageStep))
                               : length;
        slider = std::clamp(slider, std::min(kMinHitTarget, length), length);
        const int pos = sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition,
                                                length - slider, sb->upsideDown);
        switch (sub)
        {
        case SC_ScrollBarGroove: return scroll_segment(sb, 0, length);
        case SC_ScrollBarSlider: return scroll_segment(sb, pos, pos + slider);
        case SC_ScrollBarSubPage: return scroll_segment(sb, 0, pos);
        case SC_ScrollBarAddPage: return scroll_segment(sb, pos + slider, length);
        case SC_ScrollBarAddLine:
        case SC_ScrollBarSubLine: return QRect();
        default: return QProxyStyle::subControlRect(control, option, sub, widget);
        }
    }

    void AurStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                 QPainter *painter, const QWidget *widget) const
    {
        switch (element)
        {
        case PE_PanelButtonCommand:
        case PE_PanelButtonTool:
        case PE_PanelButtonBevel: paint::buttonPanel(option, painter); return;
        case PE_PanelLineEdit:
            if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option))
            {
                paint::lineEditPanel(frame, painter);
                return;
            }
            break;
        case PE_IndicatorCheckBox: paint::checkBox(option, painter); return;
        case PE_IndicatorRadioButton: paint::radioButton(option, painter); return;
        case PE_FrameFocusRect:
            if (!qobject_cast<const QAbstractButton *>(widget)) paint::focusFrame(option, painter);
            return;
        case PE_FrameTabWidget: paint::tabWidgetFrame(option, painter); return;
        case PE_Frame:
            painter->save();
            painter->setPen(option->palette.color(QPalette::Mid));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5));
            painter->restore();
            return;
        default: break;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void AurStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const
    {
        switch (element)
        {
        case CE_PushButtonLabel:
            if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option))
            {
                QStyleOptionButton copy(*b);
                copy.palette = paint::buttonLabelPalette(option);
                QProxyStyle::drawControl(element, &copy, painter, widget);
                return;
            }
            break;
        case CE_ToolButtonLabel:
            if (const auto *b = qstyleoption_cast<const QStyleOptionToolButton *>(option))
            {
                QStyleOptionToolButton copy(*b);
                copy.palette = paint::buttonLabelPalette(option);
                QProxyStyle::drawControl(element, &copy, painter, widget);
                return;
            }
            break;
        case CE_TabBarTabShape:
            if (const auto *t = qstyleoption_cast<const QStyleOptionTab *>(option))
            {
                paint::tabShape(t, painter);
                return;
            }
            break;
        case CE_TabBarTabLabel:
            if (const auto *t = qstyleoption_cast<const QStyleOptionTab *>(option))
            {
                QStyleOptionTab copy(*t);
                copy.palette = paint::tabLabelPalette(t);
                QProxyStyle::drawControl(element, &copy, painter, widget);
                return;
            }
            break;
        case CE_HeaderSection:
            if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option))
            {
                paint::headerSection(h, painter);
                return;
            }
            break;
        case CE_Splitter: paint::splitter(option, painter); return;
        default: break;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    void AurStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                      QPainter *painter, const QWidget *widget) const
    {
        switch (control)
        {
        case CC_ScrollBar:
            if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option))
            {
                paint::scrollBar(sb, painter, subControlRect(control, sb, SC_ScrollBarSlider, widget));
                return;
            }
            break;
        case CC_ComboBox:
            if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option))
            {
                paint::comboBox(cb, painter, subControlRect(control, cb, SC_ComboBoxArrow, widget));
                return;
            }
            break;
        case CC_SpinBox:
            if (const auto *sp = qstyleoption_cast<const QStyleOptionSpinBox *>(option))
            {
                paint::spinBox(sp, painter, subControlRect(control, sp, SC_SpinBoxUp, widget),
                               subControlRect(control, sp, SC_SpinBoxDown, widget));
                return;
            }
            break;
        default: break;
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }

}
