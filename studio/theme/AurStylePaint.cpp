#include "theme/AurStylePaint.hpp"

#include "theme/ThemeManager.hpp"
#include "theme/ThemeMetrics.hpp"
#include "theme/ThemeQt.hpp"

#include <QAbstractSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOption>

namespace aur::theme::paint
{
    namespace
    {
        struct Brush
        {
            QPainter &p;
            explicit Brush(QPainter *painter) : p(*painter)
            {
                p.save();
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(Qt::NoPen);
            }
            ~Brush() { p.restore(); }
        };

        QColor mixed(const QColor &a, const QColor &b, qreal amount)
        {
            return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                                    a.greenF() + (b.greenF() - a.greenF()) * amount,
                                    a.blueF() + (b.blueF() - a.blueF()) * amount);
        }

        QRectF inset(const QRect &r)
        {
            return QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
        }

        void box(QPainter &p, const QRectF &r, qreal radius, const QColor &fill, const QColor &edge)
        {
            p.setPen(edge.isValid() ? QPen(edge, 1) : QPen(Qt::NoPen));
            p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
            p.drawRoundedRect(r, radius, radius);
        }

        QColor ink(const QStyleOption *o, bool enabled)
        {
            return o->palette.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text);
        }

        void chevron(QPainter &p, const QRect &r, const QColor &color, bool up)
        {
            const QPointF c = QRectF(r).center();
            const qreal w = 3.5;
            const qreal h = up ? -1.6 : 1.6;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(QPolygonF{QPointF(c.x() - w, c.y() - h), QPointF(c.x(), c.y() + h),
                                     QPointF(c.x() + w, c.y() - h)});
        }
    }

    void buttonPanel(const QStyleOption *o, QPainter *painter)
    {
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const bool hover = enabled && (o->state & QStyle::State_MouseOver);
        const bool pressed = o->state & QStyle::State_Sunken;
        const bool on = o->state & QStyle::State_On;
        const bool focus = o->state & QStyle::State_HasFocus;
        const bool flat = o->state & QStyle::State_AutoRaise;
        const QPalette &pal = o->palette;
        const QColor accent = pal.color(QPalette::Highlight);
        const QRectF r = inset(o->rect);

        const bool danger = o->styleObject && o->styleObject->property("dangerHover").toBool();
        if (flat && !on && !pressed && !hover && !focus) return;
        QColor fill = pal.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Button);
        QColor edge = pal.color(QPalette::Mid);
        if (!enabled) edge = mixed(edge, pal.color(QPalette::Window), 0.4);
        else if (danger && (hover || pressed))
        {
            fill = qt(tokens().palette.bad);
            edge = fill;
        }
        else if (on) fill = hover ? accent.lighter(110) : accent;
        else if (pressed) fill = mixed(fill, accent, 0.30);
        else if (hover) fill = pal.color(QPalette::Light);
        if (flat && !on && !focus) edge = QColor();
        if (enabled && (on || pressed || focus)) edge = accent;
        box(b.p, r, kButtonRadius, fill, edge);
    }

    QPalette buttonLabelPalette(const QStyleOption *o)
    {
        QPalette pal = o->palette;
        if ((o->state & QStyle::State_On) && (o->state & QStyle::State_Enabled))
            pal.setColor(QPalette::ButtonText, pal.color(QPalette::HighlightedText));
        return pal;
    }

    void lineEditPanel(const QStyleOptionFrame *o, QPainter *painter)
    {
        if (o->lineWidth == 0) return;
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const bool focus = o->state & QStyle::State_HasFocus;
        const QPalette &pal = o->palette;
        QColor fill = pal.color(QPalette::Base);
        QColor edge = focus && enabled ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
        if (!enabled) fill = mixed(fill, pal.color(QPalette::Window), 0.5);
        box(b.p, inset(o->rect), kButtonRadius, fill, edge);
    }

    void checkBox(const QStyleOption *o, QPainter *painter)
    {
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const bool on = o->state & QStyle::State_On;
        const bool partial = o->state & QStyle::State_NoChange;
        const QPalette &pal = o->palette;
        const QColor accent = pal.color(QPalette::Highlight);
        const QRectF r = inset(o->rect);
        QColor fill = on ? accent : pal.color(QPalette::Base);
        QColor edge = on || (o->state & QStyle::State_HasFocus) ? accent : pal.color(QPalette::Mid);
        if (!enabled) fill = mixed(fill, pal.color(QPalette::Window), 0.5);
        box(b.p, r, kChipRadius, fill, edge);
        if (partial)
            box(b.p, r.adjusted(4, 4, -4, -4), 2, accent, QColor());
        else if (on)
            box(b.p, r.adjusted(4.5, 4.5, -4.5, -4.5), 1.5, pal.color(QPalette::HighlightedText), QColor());
    }

    void radioButton(const QStyleOption *o, QPainter *painter)
    {
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const bool on = o->state & QStyle::State_On;
        const QPalette &pal = o->palette;
        const QColor accent = pal.color(QPalette::Highlight);
        const QRectF r = inset(o->rect);
        QColor fill = on ? accent : pal.color(QPalette::Base);
        if (!enabled) fill = mixed(fill, pal.color(QPalette::Window), 0.5);
        box(b.p, r, r.width() / 2, fill, on ? accent : pal.color(QPalette::Mid));
        if (on)
            box(b.p, r.adjusted(4.5, 4.5, -4.5, -4.5), r.width() / 2, pal.color(QPalette::HighlightedText), QColor());
    }

    void focusFrame(const QStyleOption *o, QPainter *painter)
    {
        Brush b(painter);
        box(b.p, inset(o->rect), kChipRadius, QColor(), o->palette.color(QPalette::Highlight));
    }

    void tabWidgetFrame(const QStyleOption *o, QPainter *painter)
    {
        painter->save();
        painter->setPen(o->palette.color(QPalette::Mid));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(inset(o->rect));
        painter->restore();
    }

    void tabShape(const QStyleOptionTab *o, QPainter *painter)
    {
        Brush b(painter);
        const bool selected = o->state & QStyle::State_Selected;
        const bool hover = (o->state & QStyle::State_Enabled) && (o->state & QStyle::State_MouseOver);
        if (!selected && !hover) return;
        const QPalette &pal = o->palette;
        const QColor fill = selected ? pal.color(QPalette::Highlight) : pal.color(QPalette::Light);
        box(b.p, QRectF(o->rect).adjusted(1, 2, -1, -2), kButtonRadius, fill, QColor());
    }

    QPalette tabLabelPalette(const QStyleOptionTab *o)
    {
        QPalette pal = o->palette;
        const bool selected = o->state & QStyle::State_Selected;
        const bool hover = o->state & QStyle::State_MouseOver;
        QColor ink = selected ? pal.color(QPalette::HighlightedText)
                              : (hover ? pal.color(QPalette::Text) : pal.color(QPalette::PlaceholderText));
        if (!(o->state & QStyle::State_Enabled)) ink = pal.color(QPalette::Disabled, QPalette::Text);
        pal.setColor(QPalette::WindowText, ink);
        pal.setColor(QPalette::ButtonText, ink);
        return pal;
    }

    void headerSection(const QStyleOptionHeader *o, QPainter *painter)
    {
        painter->save();
        painter->fillRect(o->rect, o->palette.color(QPalette::AlternateBase));
        painter->setPen(o->palette.color(QPalette::Mid));
        painter->drawLine(o->rect.bottomLeft(), o->rect.bottomRight());
        if (o->position != QStyleOptionHeader::End && o->position != QStyleOptionHeader::OnlyOneSection)
            painter->drawLine(o->rect.topRight(), o->rect.bottomRight());
        painter->restore();
    }

    void itemViewPanel(const QStyleOption *o, QPainter *painter)
    {
        const bool selected = o->state & QStyle::State_Selected;
        const bool hover = (o->state & QStyle::State_Enabled) && (o->state & QStyle::State_MouseOver);
        if (!selected && !hover) return;
        const Tokens &t = tokens();
        painter->save();
        if (selected)
        {
            painter->fillRect(o->rect, qt(t.accent_wash()));
            painter->fillRect(QRect(o->rect.left(), o->rect.top(), 2, o->rect.height()), qt(t.primary));
        }
        else
        {
            painter->fillRect(o->rect, o->palette.color(QPalette::Light));
        }
        painter->restore();
    }

    void splitter(const QStyleOption *o, QPainter *painter)
    {
        painter->save();
        painter->fillRect(o->rect, o->palette.color(QPalette::Window));
        const bool active = (o->state & QStyle::State_MouseOver) || (o->state & QStyle::State_Sunken);
        painter->setPen(active ? o->palette.color(QPalette::Highlight) : o->palette.color(QPalette::Mid));
        const QPoint c = o->rect.center();
        if (o->state & QStyle::State_Horizontal)
            painter->drawLine(c.x(), o->rect.top(), c.x(), o->rect.bottom());
        else
            painter->drawLine(o->rect.left(), c.y(), o->rect.right(), c.y());
        painter->restore();
    }

    void comboBox(const QStyleOptionComboBox *o, QPainter *painter, const QRect &arrow)
    {
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const bool hover = enabled && (o->state & QStyle::State_MouseOver);
        const bool open = o->state & QStyle::State_On;
        const QPalette &pal = o->palette;
        QColor fill = o->editable ? pal.color(QPalette::Base)
                                  : pal.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Button);
        if (hover && !o->editable) fill = pal.color(QPalette::Light);
        QColor edge = (open || (o->state & QStyle::State_HasFocus)) && enabled ? pal.color(QPalette::Highlight)
                                                                              : pal.color(QPalette::Mid);
        if (o->frame) box(b.p, inset(o->rect), kButtonRadius, fill, edge);
        chevron(b.p, arrow, enabled ? pal.color(QPalette::PlaceholderText) : ink(o, false), false);
    }

    void spinBox(const QStyleOptionSpinBox *o, QPainter *painter, const QRect &up, const QRect &down)
    {
        Brush b(painter);
        const bool enabled = o->state & QStyle::State_Enabled;
        const QPalette &pal = o->palette;
        const QColor edge = (o->state & QStyle::State_HasFocus) && enabled ? pal.color(QPalette::Highlight)
                                                                          : pal.color(QPalette::Mid);
        if (o->frame) box(b.p, inset(o->rect), kButtonRadius, pal.color(QPalette::Base), edge);
        const QColor arrow = enabled ? pal.color(QPalette::PlaceholderText) : ink(o, false);
        const bool can_up = o->stepEnabled & QAbstractSpinBox::StepUpEnabled;
        const bool can_down = o->stepEnabled & QAbstractSpinBox::StepDownEnabled;
        chevron(b.p, up, can_up ? arrow : ink(o, false), true);
        chevron(b.p, down, can_down ? arrow : ink(o, false), false);
    }

    void scrollBar(const QStyleOptionSlider *o, QPainter *painter, const QRect &slider)
    {
        Brush b(painter);
        const bool horizontal = o->orientation == Qt::Horizontal;
        const QPalette &pal = o->palette;
        b.p.fillRect(o->rect, pal.color(QPalette::AlternateBase));
        const bool active = (o->activeSubControls & QStyle::SC_ScrollBarSlider) &&
                            (o->state & (QStyle::State_MouseOver | QStyle::State_Sunken));
        const QColor base = mixed(pal.color(QPalette::Mid), pal.color(QPalette::PlaceholderText), 0.35);
        const QColor handle = active ? mixed(base, pal.color(QPalette::PlaceholderText), 0.5) : base;
        const QRectF r = QRectF(slider).adjusted(horizontal ? 0 : 2, horizontal ? 2 : 0,
                                                 horizontal ? 0 : -2, horizontal ? -2 : 0);
        box(b.p, r, qMin(r.width(), r.height()) / 2, handle, QColor());
    }

}
