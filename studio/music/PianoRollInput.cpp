#include "PianoRollCanvas.hpp"

#include "NoteIndex.hpp"
#include "QtAudioOutput.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace aur
{
    void PianoRollCanvasWidget::wheelEvent(QWheelEvent *event)
    {
        if (event->modifiers() & Qt::ControlModifier)
        {
            const int magnitude = (event->modifiers() & Qt::ShiftModifier) ? 3 : 1;
            zoom(event->angleDelta().y() > 0 ? magnitude : -magnitude);
            event->accept();
            return;
        }
        // Plain vertical wheel scrolls the pitch window.
        setPitchOffset(highestPitch - visiblePitch(event->angleDelta().y() > 0 ? -pitchHeight * 3 : pitchHeight * 3));
        event->accept();
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
                                 std::max(0, (event->pos().x() - keyWidth) / currentStepWidth);
                const int pitch = std::clamp(visiblePitch(event->pos().y()),
                                             lowestPitch, highestPitch);
                if (trackIndex < 0 || !song.contains("tracks") ||
                    !song["tracks"].is_array() ||
                    trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
                    return;
                if (history)
                    history->begin_note_edit(song, trackIndex);
                JsonArray &notes =
                    song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
                JsonArray note{snapped(step), pitch, 4, 90};
                notes.push_back(Json(std::move(note)));
                audio::SongDocument::ensure_note_id(notes.back(), &nextNoteId);
                if (auditionHook)
                    auditionHook(pitch, 90);
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
                                              (dragOriginStep + dragOriginLength) * currentStepWidth - 5;
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
                const QRect noteRect(keyWidth + start * currentStepWidth,
                                     (pitchRow(note[1].as_int())) * pitchHeight,
                                     length * currentStepWidth, pitchHeight);
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
        const int stepDelta = (event->pos().x() - dragStartPos.x()) / currentStepWidth;
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
