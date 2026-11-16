#include "StepSequencerCanvas.hpp"

#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QKeyEvent>
#include <QMouseEvent>

#include <QPainter>

#include <QScrollBar>

#include <algorithm>

namespace aur
{
    namespace
    {
        QColor cellColor(bool on, bool accent, const theme::Palette &pal)
        {
            if (!on)
                return theme::qt(pal.surface_secondary);
            return accent ? theme::qt(pal.warning) : theme::qt(pal.good);
        }
    } // namespace

    StepSequencerCanvas::StepSequencerCanvas(QWidget *parent) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setMinimumHeight(rulerHeight + rowHeight * 2);
    }

    void StepSequencerCanvas::setSong(const Json &value)
    {
        song = value;
        patterns = audio::read_step_patterns(song);
        if (cursorPattern >= static_cast<int>(patterns.size()))
            cursorPattern = static_cast<int>(patterns.size()) - 1;
        if (cursorPattern < 0)
            cursorPattern = 0;
        setMinimumHeight(contentHeight());
        update();
    }

    void StepSequencerCanvas::setHistory(editor::SongHistory *value)
    {
        history = value;
    }

    void StepSequencerCanvas::setChangeCallback(std::function<void(const Json &)> callback)
    {
        onSongChanged = std::move(callback);
    }

    void StepSequencerCanvas::setSourceTrack(int trackIndex)
    {
        sourceTrack = trackIndex;
    }

    void StepSequencerCanvas::setAuditionHook(std::function<void(int pitch, int velocity)> hook)
    {
        auditionHook = std::move(hook);
    }

    int StepSequencerCanvas::contentHeight() const
    {
        return rulerHeight + std::max(1, static_cast<int>(patterns.size())) * rowHeight + 4;
    }

    int StepSequencerCanvas::effectiveCellWidth() const
    {
        return std::max(cellWidth, (width() - labelWidth - 8) / 64);
    }

    int StepSequencerCanvas::effectiveRowHeight() const
    {
        return std::max(rowHeight,
                        (height() - rulerHeight - 8) /
                            std::max(4, static_cast<int>(patterns.size())));
    }

    int StepSequencerCanvas::rowAt(const QPoint &pos) const
    {
        const int row = (pos.y() - rulerHeight) / effectiveRowHeight();
        return row >= 0 && row < static_cast<int>(patterns.size()) ? row : -1;
    }

    int StepSequencerCanvas::cellAt(const QPoint &pos, int &row) const
    {
        row = rowAt(pos);
        if (row < 0)
            return -1;
        const int step = (pos.x() - labelWidth) / effectiveCellWidth();
        if (step < 0 || step >= maxSteps)
            return -1;
        return step;
    }

    int StepSequencerCanvas::patternIndexAt(int row) const
    {
        return row >= 0 && row < static_cast<int>(patterns.size()) ? row : -1;
    }

    const audio::StepCell *StepSequencerCanvas::cellAt(int pattern, int step) const
    {
        return const_cast<StepSequencerCanvas *>(this)->cellAtMut(pattern, step);
    }

    audio::StepCell *StepSequencerCanvas::cellAtMut(int pattern, int step)
    {
        if (pattern < 0 || pattern >= static_cast<int>(patterns.size()))
            return nullptr;
        auto &cells = patterns[static_cast<size_t>(pattern)].cells;
        for (audio::StepCell &cell : cells)
        {
            if (cell.step == step)
                return &cell;
        }
        return nullptr;
    }

    int StepSequencerCanvas::defaultPitch() const
    {
        if (sourceTrack >= 0 && song.contains("tracks") && song["tracks"].is_array() &&
            sourceTrack < static_cast<int>(song["tracks"].as_array().size()))
        {
            const Json &track = song["tracks"][static_cast<size_t>(sourceTrack)];
            if (track.contains("drum") && track["drum"].is_bool() && track["drum"].as_bool())
                return 36; // kick
            if (track.contains("channel") && track["channel"].is_number() &&
                track["channel"].as_int() == 9)
                return 38; // snare
        }
        return 60;
    }

    void StepSequencerCanvas::applyToSong()
    {
        for (size_t i = 0; i < patterns.size(); ++i)
            audio::SongDocument::set_step_pattern(song, static_cast<int>(i), patterns[i]);
    }

    void StepSequencerCanvas::commitEdit(const std::string &label)
    {
        applyToSong();
        if (history)
            history->commit_groovebox_change(song, label);
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
        update();
    }

    void StepSequencerCanvas::toggleCell(int pattern, int step, bool on)
    {
        if (pattern < 0 || pattern >= static_cast<int>(patterns.size()))
            return;
        auto &cells = patterns[static_cast<size_t>(pattern)].cells;
        const bool exists = std::any_of(cells.begin(), cells.end(),
                                        [step](const audio::StepCell &cell)
                                        { return cell.step == step; });
        if (on && !exists)
        {
            audio::StepCell cell;
            cell.step = step;
            cell.pitch = defaultPitch();
            cell.velocity = 100;
            cells.push_back(cell);
            std::stable_sort(cells.begin(), cells.end(),
                             [](const audio::StepCell &a, const audio::StepCell &b)
                             { return a.step < b.step; });
        }
        else if (!on && exists)
        {
            cells.erase(std::remove_if(cells.begin(), cells.end(),
                                       [step](const audio::StepCell &cell)
                                       { return cell.step == step; }),
                        cells.end());
        }
    }

    void StepSequencerCanvas::setAccent(int pattern, int step, bool on)
    {
        audio::StepCell *cell = cellAtMut(pattern, step);
        if (cell)
            cell->accent = on ? 1 : 0;
    }

    void StepSequencerCanvas::clearCell(int pattern, int step)
    {
        toggleCell(pattern, step, false);
    }

    void StepSequencerCanvas::paintEvent(QPaintEvent *)
    {
        using theme::qt;
        const theme::Tokens &t = theme::tokens();
        const theme::Palette &pal = t.palette;
        QPainter painter(this);
        painter.fillRect(rect(), qt(pal.background));

        const int cw = effectiveCellWidth();
        const int rh = effectiveRowHeight();
        const int ch = std::max(cellHeight, rh - 10);

        // Ruler: bar numbers every 16 steps (a bar is 16 song steps).
        painter.fillRect(QRect(0, 0, width(), rulerHeight), qt(pal.surface));
        painter.setPen(qt(pal.text_dim));
        for (int bar = 0; bar * 16 < maxSteps; ++bar)
        {
            const int x = labelWidth + bar * 16 * cw;
            if (x > width())
                break;
            painter.drawLine(x, 0, x, rulerHeight);
            painter.drawText(x + 3, rulerHeight - 6, QString::number(bar + 1));
        }
        painter.setPen(qt(pal.text_dim));
        painter.drawText(4, rulerHeight - 6, "Pattern");

        // One row per pattern.
        for (int row = 0; row < static_cast<int>(patterns.size()); ++row)
        {
            const audio::StepPattern &pattern = patterns[static_cast<size_t>(row)];
            const int yTop = rulerHeight + row * rh;
            painter.fillRect(QRect(0, yTop, width(), rh),
                             row % 2 ? qt(pal.surface) : qt(pal.surface_variant));
            painter.setPen(qt(pal.text));
            painter.drawText(QRect(4, yTop, labelWidth - 8, rh),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             QString::fromStdString(pattern.name));

            for (int step = 0; step < maxSteps; ++step)
            {
                const int x = labelWidth + step * cw;
                if (x > width())
                    break;
                const audio::StepCell *cell = cellAt(row, step);
                const bool on = cell != nullptr;
                const bool accent = on && cell->accent != 0;
                QRect box(x + 1, yTop + (rh - ch) / 2, cw - 2, ch);
                painter.fillRect(box, cellColor(on, accent, pal));
                if (on)
                {
                    painter.setPen(QColor("#10121a"));
                    painter.drawText(box, Qt::AlignCenter,
                                     QString::number(cell->pitch % 100));
                }
            }
        }

        // Cursor.
        if (cursorPattern < static_cast<int>(patterns.size()))
        {
            const int x = labelWidth + cursorStep * cw;
            const int yTop = rulerHeight + cursorPattern * rh;
            painter.setPen(QPen(qt(t.accent_bright), 2));
            painter.drawRect(x, yTop + (rh - ch) / 2, cw - 1, ch);
        }
        painter.setPen(qt(pal.divider));
        painter.drawLine(labelWidth - 1, 0, labelWidth - 1, height());
    }

    void StepSequencerCanvas::mousePressEvent(QMouseEvent *event)
    {
        int row = -1;
        const int step = cellAt(event->pos(), row);
        if (row < 0 || step < 0)
            return;
        dragging = true;
        dragStart = event->pos();
        dragStartPattern = row;
        dragStartStep = step;
        cursorPattern = row;
        cursorStep = step;

        const audio::StepCell *cell = cellAt(row, step);
        if (event->button() == Qt::RightButton)
        {
            if (history)
                history->begin_groovebox_edit(song);
            clearCell(row, step);
            commitEdit("clear step");
            emit selectionChanged(row, step);
            return;
        }
        if (event->button() != Qt::LeftButton)
            return;

        painting = true;
        paintValue = cell == nullptr;
        if (cell && (event->modifiers() & Qt::ControlModifier))
        {
            // Ctrl+click toggles the accent of an existing cell.
            if (history)
                history->begin_groovebox_edit(song);
            setAccent(row, step, cell->accent == 0);
            commitEdit("accent step");
            painting = false;
            emit selectionChanged(row, step);
            return;
        }
        if (history)
            history->begin_groovebox_edit(song);
        toggleCell(row, step, paintValue);
        applyToSong();
        emit selectionChanged(row, step);
    }

    void StepSequencerCanvas::mouseMoveEvent(QMouseEvent *event)
    {
        int row = -1;
        const int step = cellAt(event->pos(), row);
        if (row >= 0 && step >= 0)
        {
            cursorPattern = row;
            cursorStep = step;
            update();
        }
        if (!dragging)
            return;
        if (painting && row >= 0 && step >= 0)
        {
            // Drag paints across the cells it passes over.
            const audio::StepCell *cell = cellAt(row, step);
            const bool present = cell != nullptr;
            if (present != paintValue)
            {
                toggleCell(row, step, paintValue);
                applyToSong();
                if (onSongChanged)
                    onSongChanged(song);
                update();
            }
        }
        else if (!painting && (event->buttons() & Qt::LeftButton))
        {
            // A plain drag (not on a cell) moves the cursor to that cell.
            if (row >= 0 && step >= 0)
            {
                cursorPattern = row;
                cursorStep = step;
                emit selectionChanged(row, step);
            }
        }
    }

    void StepSequencerCanvas::mouseReleaseEvent(QMouseEvent *)
    {
        if (dragging && painting)
            commitEdit(paintValue ? "add steps" : "remove steps");
        dragging = false;
        painting = false;
    }

    void StepSequencerCanvas::keyPressEvent(QKeyEvent *event)
    {
        const int patternCount = static_cast<int>(patterns.size());
        switch (event->key())
        {
        case Qt::Key_Left:
            cursorStep = std::max(0, cursorStep - 1);
            update();
            return;
        case Qt::Key_Right:
            cursorStep = std::min(maxSteps - 1, cursorStep + 1);
            update();
            return;
        case Qt::Key_Up:
            cursorPattern = patternCount > 0 ? (cursorPattern + 1) % patternCount : 0;
            update();
            return;
        case Qt::Key_Down:
            cursorPattern = patternCount > 0 ? (cursorPattern + patternCount - 1) % patternCount : 0;
            update();
            return;
        case Qt::Key_Space:
        {
            if (history)
                history->begin_groovebox_edit(song);
            const bool on = cellAt(cursorPattern, cursorStep) != nullptr;
            toggleCell(cursorPattern, cursorStep, !on);
            commitEdit(on ? "remove step" : "add step");
            emit selectionChanged(cursorPattern, cursorStep);
            return;
        }
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
        {
            if (history)
                history->begin_groovebox_edit(song);
            clearCell(cursorPattern, cursorStep);
            commitEdit("clear step");
            return;
        }
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        {
            if (history)
                history->begin_groovebox_edit(song);
            audio::StepCell *cell = cellAtMut(cursorPattern, cursorStep);
            if (!cell)
                return;
            const int delta = event->key() == Qt::Key_PageUp ? 12 : -12;
            cell->pitch = std::clamp(cell->pitch + delta, 0, 127);
            commitEdit("transpose step");
            return;
        }
        default:
            break;
        }
        QWidget::keyPressEvent(event);
    }

    void StepSequencerCanvas::wheelEvent(QWheelEvent *event)
    {
        if (event->modifiers() & Qt::ControlModifier)
        {
            scrollOffset = std::max(0, scrollOffset - event->angleDelta().y() / 40);
            update();
            return;
        }
        QWidget::wheelEvent(event);
    }

} // namespace aur
