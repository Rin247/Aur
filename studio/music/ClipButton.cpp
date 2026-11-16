#include "music/ClipButton.hpp"

#include "theme/ThemeManager.hpp"
#include "theme/ThemeMetrics.hpp"
#include "theme/ThemeQt.hpp"

#include <QPainter>

namespace aur
{

    ClipButton::ClipButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
    {
        setMinimumHeight(52);
    }

    void ClipButton::setState(State state)
    {
        state_ = state;
        update();
    }

    void ClipButton::paintEvent(QPaintEvent *)
    {
        using theme::qt;
        const theme::Tokens &t = theme::tokens();
        const theme::Palette &p = t.palette;
        const bool hover = underMouse() && isEnabled();

        QColor fill = qt(p.surface);
        QColor edge = qt(p.divider);
        QColor ink = qt(p.text_dim);
        qreal edge_width = 1;
        Qt::PenStyle edge_style = Qt::SolidLine;
        switch (state_)
        {
        case State::Empty:
            edge_style = Qt::DashLine;
            break;
        case State::Clip:
            fill = qt(theme::mix(p.surface, p.good, 0.22));
            edge = qt(theme::mix(p.divider, p.good, 0.6));
            ink = qt(p.text);
            break;
        case State::Queued:
            fill = qt(theme::mix(p.surface, p.warning, 0.22));
            edge = qt(p.warning);
            ink = qt(p.text);
            edge_width = 2;
            break;
        case State::Playing:
            fill = qt(t.primary);
            edge = qt(t.accent_bright);
            ink = qt(t.on_accent);
            edge_width = 2;
            break;
        }
        if (hover && state_ != State::Playing) edge = qt(t.accent_bright);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(fill);
        painter.setPen(QPen(edge, edge_width, edge_style));
        const qreal inset = edge_width / 2.0;
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                theme::kButtonRadius, theme::kButtonRadius);
        QFont font = painter.font();
        font.setBold(state_ == State::Playing || state_ == State::Queued);
        painter.setFont(font);
        painter.setPen(ink);
        painter.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignCenter | Qt::TextWordWrap, text());
    }

}
