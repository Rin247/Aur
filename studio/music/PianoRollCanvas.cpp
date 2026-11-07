#include "PianoRollCanvas.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>
#include <map>
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
        selectedNotes.clear();
        notifySongChanged();
    }

    void PianoRollCanvasWidget::redo()
    {
        if (history)
            history->redo(song);
        selectedNotes.clear();
        notifySongChanged();
    }

    bool PianoRollCanvasWidget::canUndo() const { return history && history->can_undo(); }

    bool PianoRollCanvasWidget::canRedo() const { return history && history->can_redo(); }

    void PianoRollCanvasWidget::setSong(const Json &value, int selectedTrack)
    {
        song = value;
        trackIndex = selectedTrack;
        selectedNotes.clear();
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
        selectedNotes.clear();
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

    bool PianoRollCanvasWidget::noteIntersectsMarquee(int index) const
    {
        if (!song.contains("tracks") || !song["tracks"].is_array() || trackIndex < 0 ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return false;
        const Json &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"];
        if (!notes.is_array() || index < 0 ||
            index >= static_cast<int>(notes.as_array().size()))
            return false;
        const Json &note = notes.as_array()[static_cast<size_t>(index)];
        if (!note.is_array() || note.as_array().size() < 3)
            return false;
        const int start = note[0].as_int();
        const int length = std::max(1, note[2].as_int());
        const QRect noteRect(keyWidth + start * stepWidth,
                             (highestPitch - note[1].as_int()) * pitchHeight,
                             length * stepWidth, pitchHeight);
        return marqueeRect.intersects(noteRect);
    }

    void PianoRollCanvasWidget::selectOnly(int index)
    {
        selectedNotes.clear();
        if (index >= 0)
            selectedNotes.insert(index);
    }

    void PianoRollCanvasWidget::deleteSelected(const char *label)
    {
        if (selectedNotes.empty() || trackIndex < 0 || !song.contains("tracks") ||
            !song["tracks"].is_array())
            return;
        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        if (history)
            history->begin_note_edit(song, trackIndex);
        for (auto it = selectedNotes.rbegin(); it != selectedNotes.rend(); ++it)
        {
            if (*it < static_cast<int>(notes.size()))
                notes.erase(notes.begin() + *it);
        }
        selectedNotes.clear();
        if (history)
            history->commit_note_edit(song, trackIndex, label);
        notifySongChanged();
    }

    void PianoRollCanvasWidget::copySelected()
    {
        if (selectedNotes.empty() || trackIndex < 0 || !song.contains("tracks") ||
            !song["tracks"].is_array() ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        const JsonArray &notes =
            song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        clipboard.clear();
        clipboardMinStep = 0;
        for (int index : selectedNotes)
        {
            if (index >= static_cast<int>(notes.size()))
                continue;
            const Json &note = notes[static_cast<size_t>(index)];
            if (!note.is_array() || note.as_array().size() < 3)
                continue;
            ClipNote clip{note[0].as_int(), note[1].as_int(),
                          std::max(1, note[2].as_int()),
                          audio::SongDocument::note_velocity(note)};
            clipboardMinStep = clipboard.empty() ? clip.step
                                                 : std::min(clipboardMinStep, clip.step);
            clipboard.push_back(clip);
        }
    }

    void PianoRollCanvasWidget::pasteClipboard()
    {
        if (clipboard.empty() || trackIndex < 0 || !song.contains("tracks") ||
            !song["tracks"].is_array() ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        if (history)
            history->begin_note_edit(song, trackIndex);
        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        selectedNotes.clear();
        for (const ClipNote &clip : clipboard)
        {
            JsonArray note{clip.step - clipboardMinStep, clip.pitch, clip.length,
                           clip.velocity};
            notes.push_back(Json(std::move(note)));
            selectedNotes.insert(static_cast<int>(notes.size()) - 1);
        }
        if (history)
            history->commit_note_edit(song, trackIndex, "paste notes");
        notifySongChanged();
    }

    void PianoRollCanvasWidget::duplicateSelection()
    {
        if (selectedNotes.empty() || trackIndex < 0 || !song.contains("tracks") ||
            !song["tracks"].is_array() ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        const JsonArray &notes =
            song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        int span = 0;
        std::vector<ClipNote> source;
        for (int index : selectedNotes)
        {
            if (index >= static_cast<int>(notes.size()))
                continue;
            const Json &note = notes[static_cast<size_t>(index)];
            if (!note.is_array() || note.as_array().size() < 3)
                continue;
            source.push_back({note[0].as_int(), note[1].as_int(),
                              std::max(1, note[2].as_int()),
                              audio::SongDocument::note_velocity(note)});
            span = std::max(span, note[0].as_int() + std::max(1, note[2].as_int()));
        }
        if (source.empty())
            return;
        int minStep = source.front().step;
        for (const ClipNote &clip : source)
            minStep = std::min(minStep, clip.step);
        const int spanOffset = span - minStep;
        if (history)
            history->begin_note_edit(song, trackIndex);
        JsonArray &mutableNotes =
            song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        selectedNotes.clear();
        for (const ClipNote &clip : source)
        {
            JsonArray note{clip.step + spanOffset, clip.pitch, clip.length,
                           clip.velocity};
            mutableNotes.push_back(Json(std::move(note)));
            selectedNotes.insert(static_cast<int>(mutableNotes.size()) - 1);
        }
        if (history)
            history->commit_note_edit(song, trackIndex, "duplicate notes");
        notifySongChanged();
        const int newContentWidth = contentWidth();
        if (newContentWidth > width())
            resize(newContentWidth, height());
    }

    int PianoRollCanvasWidget::snapped(int step) const
    {
        if (snapSteps <= 0)
            return std::max(0, step);
        if (snapSteps == 5)
        {
            // Triplet: nearest 1/3 of a bar (16 steps / 3), step base 0.
            const double third = 16.0 / 3.0;
            return std::max(0, static_cast<int>(std::lround(step / third) * third));
        }
        const int grid = std::max(1, snapSteps);
        return std::max(0, (step / grid) * grid);
    }

    void PianoRollCanvasWidget::cycleSnap()
    {
        // Off -> 1 step (16th) -> 2 (8th) -> 4 (beat) -> 5 (triplet) -> off.
        snapSteps = snapSteps == 0 ? 1 : snapSteps == 1 ? 2 : snapSteps == 2 ? 4
                   : snapSteps == 4 ? 5 : 0;
        update();
    }

    void PianoRollCanvasWidget::quantiseSelection()
    {
        if (selectedNotes.empty() || trackIndex < 0 || !song.contains("tracks") ||
            !song["tracks"].is_array() || snapSteps <= 0 ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        bool changed = false;
        for (int index : selectedNotes)
        {
            if (index >= static_cast<int>(notes.size()))
                continue;
            Json &note = notes[static_cast<size_t>(index)];
            if (!note.is_array() || note.as_array().size() < 3)
                continue;
            const int target = snapped(note[0].as_int());
            if (target != note[0].as_int())
            {
                note[0] = target;
                changed = true;
            }
        }
        if (!changed)
            return;
        if (history)
            history->commit_note_edit(song, trackIndex, "quantise");
        notifySongChanged();
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

        if (snapSteps != 1)
        {
            const QString snapLabel = snapSteps == 0 ? "snap: off"
                                      : snapSteps == 2 ? "snap: 1/8"
                                      : snapSteps == 4 ? "snap: 1/4"
                                      : snapSteps == 5 ? "snap: 1/4T"
                                                       : "snap: 1/16";
            painter.setPen(QColor("#8ab4f8"));
            painter.drawText(width() - 78, 14, snapLabel);
        }

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
            const bool selected = selectedNotes.count(static_cast<int>(i)) > 0;
            if (selected)
                fill = QColor("#f0bd58");
            painter.setPen(selected ? QPen(QColor("#fff0a8"), 2)
                                    : QPen(fill.darker(150), 1));
            painter.setBrush(fill);
            painter.drawRoundedRect(noteRect, 2, 2);
        }
        if (marqueeSelecting && !marqueeRect.isNull())
        {
            painter.setPen(QPen(QColor("#8ab4f8"), 1, Qt::DashLine));
            painter.setBrush(QColor(138, 180, 248, 30));
            painter.drawRect(marqueeRect);
        }
    }

    void PianoRollCanvasWidget::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::RightButton)
        {
            const int hit = noteAt(event->pos());
            if (hit >= 0)
            {
                if (!selectedNotes.count(hit))
                    selectOnly(hit);
                deleteSelected("delete notes");
            }
            else if (!selectedNotes.empty())
            {
                deleteSelected("delete notes");
            }
            return;
        }
        if (event->button() != Qt::LeftButton)
            return;
        setFocus();
        const int hit = noteAt(event->pos());
        if (hit < 0 && event->pos().x() >= keyWidth)
        {
            // Empty grid: Shift+click creates a note, plain click starts a
            // marquee, Ctrl adds to the existing selection while marqueeing.
            if (event->modifiers() & Qt::ShiftModifier)
            {
                const int step = firstVisibleStep +
                                 std::max(0, (event->pos().x() - keyWidth) / stepWidth);
                const int pitch = std::clamp(highestPitch - event->pos().y() / pitchHeight,
                                             lowestPitch, highestPitch);
                if (trackIndex < 0 || !song.contains("tracks") ||
                    !song["tracks"].is_array() ||
                    trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
                    return;
                if (history)
                    history->begin_note_edit(song, trackIndex);
                JsonArray note{snapped(step), pitch, 4, 90};
                JsonArray &notes =
                    song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
                notes.push_back(Json(std::move(note)));
                selectOnly(static_cast<int>(notes.size()) - 1);
                resizingNote = false;
                if (history)
                    history->commit_note_edit(song, trackIndex, "add note");
                notifySongChanged();
                const int newContentWidth = contentWidth();
                if (newContentWidth > width())
                    resize(newContentWidth, height());
                return;
            }
            marqueeSelecting = true;
            marqueeStartPos = event->pos();
            marqueeRect = QRect(event->pos(), event->pos());
            if (!(event->modifiers() & Qt::ControlModifier))
                selectedNotes.clear();
            update();
            return;
        }
        if (event->pos().x() < keyWidth)
            return;
        if (hit < 0)
        {
            // Fall-through: only reachable with an out-of-window note hit;
            // empty-grid press was already handled above.
            return;
        }

        // Hit a note: single select (or toggle with Ctrl), then drag/resize.
        if (event->modifiers() & Qt::ControlModifier)
        {
            if (selectedNotes.count(hit))
                selectedNotes.erase(hit);
            else
                selectedNotes.insert(hit);
            update();
            return;
        }
        if (!selectedNotes.count(hit))
            selectOnly(hit);

        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        Json &note = notes[static_cast<size_t>(hit)];
        dragOriginStep = note[0].as_int();
        dragOriginPitch = note[1].as_int();
        dragOriginLength = std::max(1, note[2].as_int());
        dragOriginVelocity = audio::SongDocument::note_velocity(note);
        dragStartPos = event->pos();
        dragOrigins.clear();
        for (int index : selectedNotes)
        {
            const Json &n = notes[static_cast<size_t>(index)];
            if (n.is_array() && n.as_array().size() >= 3)
                dragOrigins[index] = {n[0].as_int(), n[1].as_int(),
                                      std::max(1, n[2].as_int()),
                                      audio::SongDocument::note_velocity(n)};
        }
        velocityDrag = event->modifiers() & Qt::AltModifier;
        resizingNote = !velocityDrag && selectedNotes.size() == 1 &&
                       event->pos().x() >= keyWidth +
                                              (dragOriginStep + dragOriginLength) * stepWidth - 5;
        if (history)
            history->begin_note_edit(song, trackIndex);
        dragging = true;
        update();
    }

    void PianoRollCanvasWidget::mouseMoveEvent(QMouseEvent *event)
    {
        if (marqueeSelecting)
        {
            marqueeRect = QRect(marqueeStartPos, event->pos()).normalized();
            selectedNotes.clear();
            const int count = song.contains("tracks") && song["tracks"].is_array() &&
                                      trackIndex >= 0 &&
                                      trackIndex < static_cast<int>(song["tracks"].as_array().size())
                                  ? static_cast<int>(song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array().size())
                                  : 0;
            for (int i = 0; i < count; ++i)
            {
                const Json &note = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array()[static_cast<size_t>(i)];
                if (!note.is_array() || note.as_array().size() < 3)
                    continue;
                const int start = note[0].as_int();
                const int length = std::max(1, note[2].as_int());
                const QRect noteRect(keyWidth + start * stepWidth,
                                     (highestPitch - note[1].as_int()) * pitchHeight,
                                     length * stepWidth, pitchHeight);
                if (marqueeRect.intersects(noteRect))
                    selectedNotes.insert(i);
            }
            update();
            return;
        }
        if (!(event->buttons() & Qt::LeftButton) || selectedNotes.empty() ||
            trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array())
            return;
        JsonArray &notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        if (velocityDrag)
        {
            const int velocityDelta = (event->pos().y() - dragStartPos.y()) / pitchHeight;
            for (const auto &[index, origin] : dragOrigins)
            {
                if (index < static_cast<int>(notes.size()))
                    audio::SongDocument::set_note_velocity(
                        notes[static_cast<size_t>(index)],
                        origin.velocity - velocityDelta);
            }
            notifySongChanged();
            return;
        }
        const int stepDelta = (event->pos().x() - dragStartPos.x()) / stepWidth;
        const int pitchDelta = (dragStartPos.y() - event->pos().y()) / pitchHeight;
        if (resizingNote)
        {
            for (const auto &[index, origin] : dragOrigins)
            {
                if (index < static_cast<int>(notes.size()))
                    notes[static_cast<size_t>(index)][2] =
                        std::max(1, origin.length + stepDelta);
            }
        }
        else
        {
            for (const auto &[index, origin] : dragOrigins)
            {
                if (index >= static_cast<int>(notes.size()))
                    continue;
                Json &note = notes[static_cast<size_t>(index)];
                note[0] = std::max(0, snapped(origin.step + stepDelta));
                note[1] = std::clamp(origin.pitch + pitchDelta, lowestPitch, highestPitch);
            }
        }
        notifySongChanged();
        const int newContentWidth = contentWidth();
        if (newContentWidth > width())
            resize(newContentWidth, height());
    }

    void PianoRollCanvasWidget::mouseReleaseEvent(QMouseEvent *)
    {
        if (marqueeSelecting)
        {
            marqueeSelecting = false;
            marqueeRect = QRect();
            update();
            return;
        }
        if (dragging && history)
            history->commit_note_edit(song, trackIndex,
                                      velocityDrag ? "set velocity"
                                      : resizingNote ? "resize notes"
                                                     : "move notes");
        dragging = false;
        resizingNote = false;
        velocityDrag = false;
        dragOrigins.clear();
        emit historyChanged();
    }

    void PianoRollCanvasWidget::keyPressEvent(QKeyEvent *event)
    {
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
            !selectedNotes.empty() && trackIndex >= 0 && song.contains("tracks") &&
            song["tracks"].is_array())
        {
            deleteSelected("delete notes");
            return;
        }
        if (event->key() == Qt::Key_G && event->modifiers() & Qt::ControlModifier)
        {
            cycleSnap();
            return;
        }
        if (event->key() == Qt::Key_Q && event->modifiers() & Qt::ControlModifier)
        {
            if (history)
                history->begin_note_edit(song, trackIndex);
            quantiseSelection();
            return;
        }
        if (event->key() == Qt::Key_C && event->modifiers() & Qt::ControlModifier)
        {
            copySelected();
            return;
        }
        if (event->key() == Qt::Key_V && event->modifiers() & Qt::ControlModifier)
        {
            pasteClipboard();
            return;
        }
        if (event->key() == Qt::Key_D && event->modifiers() & Qt::ControlModifier)
        {
            duplicateSelection();
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
