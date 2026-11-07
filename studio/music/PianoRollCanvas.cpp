#include "PianoRollCanvas.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <string>

namespace aur
{
    namespace
    {

        QString noteName(int midiPitch)
        {
            static const char *names[] = {"C", "C#", "D", "D#", "E", "F",
                                          "F#", "G", "G#", "A", "A#", "B"};
            return QString("%1%2").arg(names[midiPitch % 12]).arg(midiPitch / 12 - 1);
        }

    } // namespace

    PianoRollCanvasWidget::PianoRollCanvasWidget(QWidget *parent) : QWidget(parent)
    {
        setMinimumSize(keyWidth + minimumStepCount * stepWidth, pitchCount * pitchHeight);
        setFocusPolicy(Qt::StrongFocus);
    }

    void PianoRollCanvasWidget::setHistory(editor::SongHistory *value)
    {
        history = value;
        emit historyChanged();
    }

    void PianoRollCanvasWidget::undo()
    {
        if (history)
            history->undo(song);
        selectedNote = -1;
        notifySongChanged();
    }

    void PianoRollCanvasWidget::redo()
    {
        if (history)
            history->redo(song);
        selectedNote = -1;
        notifySongChanged();
    }

    bool PianoRollCanvasWidget::canUndo() const { return history && history->can_undo(); }

    bool PianoRollCanvasWidget::canRedo() const { return history && history->can_redo(); }

    void PianoRollCanvasWidget::setSong(const Json &value, int selectedTrack)
    {
        song = value;
        trackIndex = selectedTrack;
        selectedNote = -1;
        const int newContentWidth = contentWidth();
        if (newContentWidth > width())
            resize(newContentWidth, pitchCount * pitchHeight);
        update();
    }

    void PianoRollCanvasWidget::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        if (event->size().width() > event->oldSize().width())
        {
            const int newContentWidth = contentWidth();
            if (newContentWidth > width())
                resize(newContentWidth, height());
        }
    }

    void PianoRollCanvasWidget::setSelectedTrack(int selectedTrack)
    {
        trackIndex = selectedTrack;
        selectedNote = -1;
        update();
    }

    void PianoRollCanvasWidget::setVisibleStep(int firstStep)
    {
        firstVisibleStep = std::max(0, firstStep);
        update();
    }

    int PianoRollCanvasWidget::contentWidth() const
    {
        int lastStep = minimumStepCount;
        if (song.contains("tracks") && song["tracks"].is_array())
        {
            for (const Json &track : song["tracks"].as_array())
            {
                if (!track.contains("notes") || !track["notes"].is_array())
                    continue;
                for (const Json &note : track["notes"].as_array())
                {
                    if (!note.is_array() || note.as_array().size() < 3)
                        continue;
                    const int noteEnd = std::max(0, note[0].as_int()) +
                                        std::max(1, note[2].as_int());
                    lastStep = std::max(lastStep, noteEnd);
                }
            }
        }
        const int viewportSteps = std::max(1, (width() - keyWidth) / stepWidth);
        const int requiredSteps = ((lastStep + viewportSteps - 1) / viewportSteps) * viewportSteps;
        return keyWidth + requiredSteps * stepWidth;
    }

    void PianoRollCanvasWidget::setChangeCallback(std::function<void(const Json &)> callback)
    {
        onSongChanged = std::move(callback);
    }

    int PianoRollCanvasWidget::noteAt(const QPoint &pos) const
    {
        if (!song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex < 0 || trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return -1;
        const Json &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"];
        if (!notes.is_array())
            return -1;
        const int x = pos.x() - keyWidth;
        const int pitch = highestPitch - (pos.y() / pitchHeight);
        if (x < 0 || pitch < lowestPitch || pitch > highestPitch)
            return -1;
        for (int i = static_cast<int>(notes.as_array().size()) - 1; i >= 0; --i)
        {
            const Json &note = notes[static_cast<size_t>(i)];
            if (!note.is_array() || note.as_array().size() < 3)
                continue;
            const int start = note[0].as_int();
            const int length = std::max(1, note[2].as_int());
            const int left = keyWidth + start * stepWidth;
            const int right = left + length * stepWidth;
            const int top = (highestPitch - note[1].as_int()) * pitchHeight;
            if (pos.x() >= left && pos.x() < right &&
                pos.y() >= top && pos.y() < top + pitchHeight)
                return i;
        }
        return -1;
    }

    void PianoRollCanvasWidget::notifySongChanged()
    {
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
        update();
    }

    void PianoRollCanvasWidget::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#171922"));

        for (int row = 0; row < pitchCount; ++row)
        {
            const int pitch = highestPitch - row;
            const QRect key(0, row * pitchHeight, keyWidth, pitchHeight);
            const bool blackKey = pitch % 12 == 1 || pitch % 12 == 3 ||
                                  pitch % 12 == 6 || pitch % 12 == 8 || pitch % 12 == 10;
            painter.fillRect(key, blackKey ? QColor("#292c38") : QColor("#363945"));
            painter.setPen(QColor("#858a99"));
            painter.drawText(key.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignRight,
                             noteName(pitch));
            if (pitch % 12 == 0)
            {
                painter.fillRect(QRect(keyWidth, row * pitchHeight,
                                       width() - keyWidth, pitchHeight),
                                 QColor(255, 255, 255, 8));
            }
        }

        const int displayedSteps = (width() - keyWidth) / stepWidth;
        const int firstGridStep = (firstVisibleStep / 16) * 16;
        const int lastGridStep = firstVisibleStep + displayedSteps;
        for (int step = firstGridStep; step <= lastGridStep; ++step)
        {
            const int x = keyWidth + (step - firstVisibleStep) * stepWidth;
            const int barNumber = step / 16;
            const int barBeat = step % 16;
            painter.setPen(barBeat == 0 ? QColor("#555b6d") : barBeat % 4 == 0 ? QColor("#393d4b")
                                                                               : QColor("#272a35"));
            painter.drawLine(x, 0, x, height());
            if (barBeat == 0 && step >= firstVisibleStep)
            {
                painter.setPen(QColor("#aeb5c5"));
                painter.drawText(x + 3, 12, QString::number(barNumber + 1));
            }
        }
        for (int row = 0; row <= pitchCount; ++row)
        {
            painter.setPen(QColor("#30333f"));
            painter.drawLine(keyWidth, row * pitchHeight, width(), row * pitchHeight);
        }
        painter.setPen(QColor("#0c0d12"));
        painter.drawLine(keyWidth - 1, 0, keyWidth - 1, height());

        if (!song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex < 0 || trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
        {
            painter.setPen(Qt::white);
            painter.drawText(QRect(keyWidth + 8, 8, width() - keyWidth - 16, 28),
                             Qt::AlignLeft, "Add or select a track, then click the grid to create notes.");
            return;
        }

        const Json &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"];
        if (!notes.is_array())
            return;
        const QColor noteColor("#48d6a0");
        for (size_t i = 0; i < notes.as_array().size(); ++i)
        {
            const Json &note = notes[i];
            if (!note.is_array() || note.as_array().size() < 3)
                continue;
            const int step = std::max(0, note[0].as_int());
            const int pitch = std::clamp(note[1].as_int(), lowestPitch, highestPitch);
            const int length = std::max(1, note[2].as_int());
            const int visibleStart = std::max(step, firstVisibleStep);
            const int visibleEnd = std::min(step + length, firstVisibleStep + displayedSteps);
            if (visibleStart >= visibleEnd)
                continue;
            const QRect noteRect(keyWidth + (visibleStart - firstVisibleStep) * stepWidth + 1,
                                 (highestPitch - pitch) * pitchHeight + 1,
                                 std::max(2, (visibleEnd - visibleStart) * stepWidth - 2), pitchHeight - 2);
            QColor fill = noteColor.darker(70 + (audio::SongDocument::note_velocity(note) * 90 / 127));
            if (i == static_cast<size_t>(selectedNote))
                fill = QColor("#f0bd58");
            painter.setPen(i == static_cast<size_t>(selectedNote)
                               ? QPen(QColor("#fff0a8"), 2)
                               : QPen(fill.darker(150), 1));
            painter.setBrush(fill);
            painter.drawRoundedRect(noteRect, 2, 2);
        }
    }

    void PianoRollCanvasWidget::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::RightButton)
        {
            selectedNote = noteAt(event->pos());
            if (selectedNote >= 0)
            {
                if (history)
                    history->begin_note_edit(song, trackIndex);
                JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
                notes.erase(notes.begin() + selectedNote);
                selectedNote = -1;
                if (history)
                    history->commit_note_edit(song, trackIndex, "delete note");
                notifySongChanged();
            }
            return;
        }
        if (event->button() != Qt::LeftButton || event->pos().x() < keyWidth)
            return;
        setFocus();
        const int step = firstVisibleStep + std::max(0, (event->pos().x() - keyWidth) / stepWidth);
        const int pitch = std::clamp(highestPitch - event->pos().y() / pitchHeight,
                                     lowestPitch, highestPitch);
        selectedNote = noteAt(event->pos());
        if (selectedNote < 0)
        {
            if (trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array() ||
                trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
                return;
            if (history)
                history->begin_note_edit(song, trackIndex);
            JsonArray note{step, pitch, 4, 90};
            JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
            notes.push_back(Json(std::move(note)));
            selectedNote = static_cast<int>(notes.size()) - 1;
            resizingNote = false;
            if (history)
                history->commit_note_edit(song, trackIndex, "add note");
            notifySongChanged();
            const int newContentWidth = contentWidth();
            if (newContentWidth > width())
                resize(newContentWidth, height());
            return;
        }

        Json &note = song["tracks"][static_cast<size_t>(trackIndex)]["notes"]
                         [static_cast<size_t>(selectedNote)];
        dragOriginStep = note[0].as_int();
        dragOriginPitch = note[1].as_int();
        dragOriginLength = std::max(1, note[2].as_int());
        dragOriginVelocity = audio::SongDocument::note_velocity(note);
        dragStartPos = event->pos();
        velocityDrag = event->modifiers() & Qt::AltModifier;
        resizingNote = !velocityDrag && event->pos().x() >= keyWidth +
                                               (dragOriginStep + dragOriginLength) * stepWidth - 5;
        if (history)
            history->begin_note_edit(song, trackIndex);
        dragging = true;
        update();
    }

    void PianoRollCanvasWidget::mouseMoveEvent(QMouseEvent *event)
    {
        if (!(event->buttons() & Qt::LeftButton) || selectedNote < 0 ||
            trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array())
            return;
        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        if (selectedNote >= static_cast<int>(notes.size()))
            return;
        Json &note = notes[static_cast<size_t>(selectedNote)];
        if (velocityDrag)
        {
            const int velocityDelta = (event->pos().y() - dragStartPos.y()) / pitchHeight;
            audio::SongDocument::set_note_velocity(note, dragOriginVelocity - velocityDelta);
            notifySongChanged();
            return;
        }
        const int stepDelta = (event->pos().x() - dragStartPos.x()) / stepWidth;
        if (resizingNote)
        {
            note[2] = std::max(1, dragOriginLength + stepDelta);
        }
        else
        {
            note[0] = std::max(0, dragOriginStep + stepDelta);
            const int pitchDelta = (dragStartPos.y() - event->pos().y()) / pitchHeight;
            note[1] = std::clamp(dragOriginPitch + pitchDelta, lowestPitch, highestPitch);
        }
        notifySongChanged();
        const int newContentWidth = contentWidth();
        if (newContentWidth > width())
            resize(newContentWidth, height());
    }

    void PianoRollCanvasWidget::mouseReleaseEvent(QMouseEvent *)
    {
        if (dragging && history)
            history->commit_note_edit(song, trackIndex,
                                      velocityDrag ? "set velocity"
                                      : resizingNote ? "resize note"
                                                     : "move note");
        dragging = false;
        resizingNote = false;
        velocityDrag = false;
        emit historyChanged();
    }

    void PianoRollCanvasWidget::keyPressEvent(QKeyEvent *event)
    {
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
            selectedNote >= 0 && trackIndex >= 0 && song.contains("tracks") &&
            song["tracks"].is_array())
        {
            JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
            if (selectedNote < static_cast<int>(notes.size()))
            {
                if (history)
                    history->begin_note_edit(song, trackIndex);
                notes.erase(notes.begin() + selectedNote);
                selectedNote = -1;
                if (history)
                    history->commit_note_edit(song, trackIndex, "delete note");
                notifySongChanged();
            }
            return;
        }
        if (event->key() == Qt::Key_Z && event->modifiers() & Qt::ControlModifier &&
            !(event->modifiers() & Qt::ShiftModifier))
        {
            undo();
            return;
        }
        if ((event->key() == Qt::Key_Y && event->modifiers() & Qt::ControlModifier) ||
            (event->key() == Qt::Key_Z && event->modifiers() & Qt::ShiftModifier &&
             event->modifiers() & Qt::ControlModifier))
        {
            redo();
            return;
        }
        QWidget::keyPressEvent(event);
    }

} // namespace aur
