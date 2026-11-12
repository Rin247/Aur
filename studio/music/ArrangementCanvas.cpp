#include "ArrangementCanvas.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace aur
{
    namespace
    {
        constexpr int kStepWidth = 16;
        constexpr int kRowLabelWidth = 96;
        constexpr int kTempoLaneHeight = 12;

        QColor sceneColor(int scene)
        {
            static const QColor palette[] = {
                QColor("#48d6a0"), QColor("#6fa8dc"), QColor("#e0b050"),
                QColor("#d07070"), QColor("#b080d0"), QColor("#60c0c0")};
            return palette[std::abs(scene) % 6];
        }
    } // namespace

    ArrangementCanvas::ArrangementCanvas(QWidget *parent) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    void ArrangementCanvas::setSong(const Json &value)
    {
        song = value;
        selectedStep = -1;
        selectedTrack = -1;
        // The canvas is as big as its content, so a scroll area can reach
        // the bars and the tracks beyond the window.
        setMinimumSize(canvasWidth(), canvasHeight());
        update();
    }

    void ArrangementCanvas::setHistory(editor::SongHistory *value)
    {
        history = value;
    }

    void ArrangementCanvas::setPlaceScene(int sceneIndex)
    {
        placeScene = std::max(0, sceneIndex);
        update();
    }

    void ArrangementCanvas::setChangeCallback(std::function<void(const Json &)> callback)
    {
        onSongChanged = std::move(callback);
    }

    int ArrangementCanvas::canvasHeight() const
    {
        const int rows = song.contains("tracks") && song["tracks"].is_array()
                             ? static_cast<int>(song["tracks"].as_array().size())
                             : 0;
        return rulerHeight + std::max(minRows, rows) * rowHeight + kTempoLaneHeight + 8;
    }

    int ArrangementCanvas::canvasWidth() const
    {
        const int bars = song.contains("bars") && song["bars"].is_number()
                             ? std::max(1, song["bars"].as_int())
                             : 1;
        return kRowLabelWidth + bars * 16 * kStepWidth + 24;
    }

    int ArrangementCanvas::stepAt(const QPoint &pos) const
    {
        return std::max(0, (pos.x() - kRowLabelWidth) / kStepWidth);
    }

    int ArrangementCanvas::rowAt(const QPoint &pos) const
    {
        const int row = (pos.y() - rulerHeight) / rowHeight;
        const int rows = song.contains("tracks") && song["tracks"].is_array()
                             ? static_cast<int>(song["tracks"].as_array().size())
                             : 0;
        return row >= 0 && row < rows ? row : -1;
    }

    ArrangementCanvas::Hit ArrangementCanvas::hitTest(const QPoint &pos) const
    {
        Hit hit;
        const int row = rowAt(pos);
        if (row < 0)
            return hit;
        const int step = stepAt(pos);
        const auto placed = audio::SongDocument::placements(song);
        for (int i = 0; i < static_cast<int>(placed.size()); ++i)
        {
            const auto &p = placed[static_cast<size_t>(i)];
            if (p.track == row && step >= p.step && step < p.step + p.length)
            {
                hit.placementIndex = i;
                hit.row = row;
                break;
            }
        }
        return hit;
    }

    QColor ArrangementCanvas::placementColor(int color, int scene) const
    {
        if (color >= 0 && color < 6)
            return sceneColor(color);
        return sceneColor(scene);
    }

    void ArrangementCanvas::notifyChanged()
    {
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
        update();
    }

    void ArrangementCanvas::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#14161e"));

        const int rows = song.contains("tracks") && song["tracks"].is_array()
                             ? static_cast<int>(song["tracks"].as_array().size())
                             : 0;
        const int bars = song.contains("bars") && song["bars"].is_number()
                             ? std::max(1, song["bars"].as_int())
                             : 1;
        const int totalSteps = bars * 16;

        // Ruler: bar numbers, markers, loop brace.
        const QRect ruler(0, 0, width(), rulerHeight);
        painter.fillRect(ruler, QColor("#181a23"));
        painter.setPen(QColor("#aeb5c5"));
        for (int bar = 0; bar <= bars; ++bar)
        {
            const int x = kRowLabelWidth + bar * 16 * kStepWidth;
            if (x > width())
                break;
            painter.drawLine(x, 0, x, rulerHeight);
            painter.drawText(x + 3, 12, QString::number(bar + 1));
        }
        if (song.contains("markers") && song["markers"].is_array())
        {
            painter.setPen(QColor("#e0b050"));
            for (const Json &marker : song["markers"].as_array())
            {
                if (!marker.is_object() || !marker.contains("step") ||
                    !marker.contains("name") || !marker["step"].is_number() ||
                    !marker["name"].is_string())
                    continue;
                const int x = kRowLabelWidth + marker["step"].as_int() * kStepWidth;
                painter.drawText(x + 2, rulerHeight - 22,
                                 QString::fromStdString(marker["name"].as_string()));
            }
        }
        // Loop brace.
        const int loopStart = audio::SongDocument::loop_start(song);
        const int loopEnd = audio::SongDocument::loop_end(song);
        if (loopEnd > loopStart)
        {
            const int x0 = kRowLabelWidth + loopStart * kStepWidth;
            const int x1 = kRowLabelWidth + loopEnd * kStepWidth;
            painter.setPen(QPen(QColor("#48d6a0"), 2));
            painter.drawLine(x0, rulerHeight - 4, x1, rulerHeight - 4);
            painter.drawLine(x0, rulerHeight - 10, x0, rulerHeight - 4);
            painter.drawLine(x1, rulerHeight - 10, x1, rulerHeight - 4);
        }

        // Tempo / T.S. lane.
        const int tempoY = rulerHeight + rows * rowHeight;
        painter.fillRect(0, tempoY, width(), kTempoLaneHeight, QColor("#181a23"));
        painter.setPen(QColor("#858a99"));
        painter.drawText(4, tempoY + 10, "tempo");
        painter.setPen(QColor("#e0b050"));
        if (song.contains("tempo_map") && song["tempo_map"].is_array())
        {
            for (const Json &t : song["tempo_map"].as_array())
            {
                if (!t.is_object() || !t.contains("step") || !t["step"].is_number())
                    continue;
                const int x = kRowLabelWidth + t["step"].as_int() * kStepWidth;
                painter.drawText(x + 2, tempoY + 10,
                                 QString::number(
                                     t.contains("value_a") && t["value_a"].is_number()
                                         ? t["value_a"].as_int()
                                         : 120));
            }
        }

        // Track rows + placements.
        const auto placed = audio::SongDocument::placements(song);
        for (int row = 0; row < rows; ++row)
        {
            const int yTop = rulerHeight + row * rowHeight;
            painter.fillRect(QRect(0, yTop, width(), rowHeight),
                             row % 2 ? QColor("#181a23") : QColor("#1c1f2a"));
            painter.setPen(QColor("#0c0d12"));
            painter.drawLine(0, yTop, width(), yTop);

            const Json &track = song["tracks"].as_array()[static_cast<size_t>(row)];
            painter.setPen(QColor("#aeb5c5"));
            painter.drawText(QRect(4, yTop, kRowLabelWidth - 8, rowHeight),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             QString::fromStdString(
                                 track.contains("name") && track["name"].is_string()
                                     ? track["name"].as_string()
                                     : "Track " + std::to_string(row + 1)));

            for (int i = 0; i < static_cast<int>(placed.size()); ++i)
            {
                const auto &p = placed[static_cast<size_t>(i)];
                if (p.track != row)
                    continue;
                const Json *clip = audio::SongDocument::clip_at(song, p.scene, p.track);
                const audio::SongDocument::ClipInfo info =
                    clip ? audio::SongDocument::clip_info(*clip, "Clip") : audio::SongDocument::ClipInfo{};
                QColor fill = placementColor(info.color, p.scene);
                if (selectedStep == p.step && selectedTrack == row)
                    fill = fill.lighter(140);
                const QRect block(kRowLabelWidth + p.step * kStepWidth, yTop + 4,
                                  p.length * kStepWidth - 2, rowHeight - 8);
                painter.fillRect(block, fill);
                painter.setPen(QColor("#10121a"));
                painter.drawText(block.adjusted(4, 2, -4, -2),
                                 Qt::AlignTop | Qt::AlignLeft,
                                 QString::fromStdString(info.name));
                if (info.muted)
                {
                    painter.setPen(QPen(QColor("#d07070"), 2));
                    painter.drawRect(block);
                }
            }
        }
        painter.setPen(QColor("#0c0d12"));
        painter.drawLine(kRowLabelWidth - 1, 0, kRowLabelWidth - 1, height());
    }

    void ArrangementCanvas::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() != Qt::LeftButton || event->pos().x() < kRowLabelWidth)
            return;
        const Hit hit = hitTest(event->pos());
        if (hit.placementIndex >= 0)
        {
            const auto placed = audio::SongDocument::placements(song);
            const auto &p = placed[static_cast<size_t>(hit.placementIndex)];
            selectedStep = p.step;
            selectedTrack = p.track;
            dragOrigStep = p.step;
            dragOrigTrack = p.track;
            dragging = true;
            if (history)
                history->begin_track_edit(song, p.track);
            emit selectionChanged();
            update();
            return;
        }
        const int row = rowAt(event->pos());
        if (row < 0)
            return;
        // Empty cell: place the selected scene's clip for this track, clipped
        // to that clip's actual length when it exists.
        if (!song.contains("tracks") || !song["tracks"].is_array() ||
            row >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        const Json *clip = audio::SongDocument::clip_at(song, placeScene, row);
        const audio::SongDocument::ClipInfo info =
            clip ? audio::SongDocument::clip_info(*clip, "Clip") : audio::SongDocument::ClipInfo{};
        audio::SongDocument::Placement p{placeScene, row, stepAt(event->pos()),
                                         info.length};
        if (history)
            history->begin_track_edit(song, row);
        audio::SongDocument::add_placement(song, p);
        selectedStep = p.step;
        selectedTrack = row;
        if (history)
            history->commit_track_change(song, row, editor::SongAction::Kind::TrackProps,
                                         "place clip");
        emit selectionChanged();
        notifyChanged();
    }

    void ArrangementCanvas::mouseMoveEvent(QMouseEvent *event)
    {
        if (!dragging || !(event->buttons() & Qt::LeftButton))
            return;
        const int newStep = stepAt(event->pos());
        const int newRow = rowAt(event->pos());
        if (newStep == selectedStep && newRow == selectedTrack)
            return;
        if (newRow < 0)
            return;

        // Keep the dragged placement's scene and length; rewrite its spot.
        const auto placedBefore = audio::SongDocument::placements(song);
        int scene = placeScene;
        int length = 16;
        for (const auto &p : placedBefore)
        {
            if (p.track == dragOrigTrack && p.step == dragOrigStep)
            {
                scene = p.scene;
                length = p.length;
                break;
            }
        }
        audio::SongDocument::remove_placement(song, dragOrigStep, dragOrigTrack);
        audio::SongDocument::add_placement(
            song, audio::SongDocument::Placement{scene, newRow, newStep, length});
        dragOrigStep = newStep;
        dragOrigTrack = newRow;
        selectedStep = newStep;
        selectedTrack = newRow;
        notifyChanged();
    }

    void ArrangementCanvas::mouseReleaseEvent(QMouseEvent *)
    {
        if (dragging && history && selectedTrack >= 0)
        {
            history->commit_track_change(song, selectedTrack,
                                         editor::SongAction::Kind::TrackProps,
                                         "move placement");
            emit historyChanged();
        }
        dragging = false;
    }

    void ArrangementCanvas::keyPressEvent(QKeyEvent *event)
    {
        if (event->key() == Qt::Key_Delete && selectedStep >= 0 && selectedTrack >= 0)
        {
            if (history)
                history->begin_track_edit(song, selectedTrack);
            audio::SongDocument::remove_placement(song, selectedStep, selectedTrack);
            if (history)
                history->commit_track_change(song, selectedTrack,
                                             editor::SongAction::Kind::TrackProps,
                                             "remove placement");
            selectedStep = -1;
            selectedTrack = -1;
            notifyChanged();
            return;
        }
        QWidget::keyPressEvent(event);
    }

} // namespace aur
