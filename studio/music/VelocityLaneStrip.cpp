#include "VelocityLaneStrip.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace aur
{
    namespace
    {
        constexpr int kStepWidth = 16;
        constexpr int kKeyWidth = 40;

        QString laneLabel(int id)
        {
            if (id == audio::SongDocument::kLaneBend)
                return "bend";
            if (id == audio::SongDocument::kLanePressure)
                return "press";
            return QString("CC %1").arg(id);
        }
    } // namespace

    VelocityLaneStrip::VelocityLaneStrip(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(stripHeight());
        setMouseTracking(true);
    }

    int VelocityLaneStrip::laneCountFor(const Json &song, int trackIndex)
    {
        if (!song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex < 0 ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return 1;
        const Json &track = song["tracks"][static_cast<size_t>(trackIndex)];
        int count = 1;
        if (track.contains("automation") && track["automation"].is_array())
        {
            for (const Json &lane : track["automation"].as_array())
            {
                if (lane.is_object() && lane.contains("cc") &&
                    lane["cc"].is_number())
                    ++count;
            }
        }
        return count;
    }

    int VelocityLaneStrip::laneId(int laneRow) const
    {
        if (laneRow <= 0 || laneRow >= static_cast<int>(lanes.size()))
            return -1;
        return lanes[static_cast<size_t>(laneRow)].id;
    }

    int VelocityLaneStrip::stripHeight() const
    {
        return std::max(minLanes, static_cast<int>(lanes.size())) * laneHeight;
    }

    void VelocityLaneStrip::setSong(const Json &value, int selectedTrack)
    {
        song = value;
        trackIndex = selectedTrack;
        lanes.clear();
        lanes.push_back({-1, 0, laneHeight});
        if (song.contains("tracks") && song["tracks"].is_array() &&
            trackIndex >= 0 &&
            trackIndex < static_cast<int>(song["tracks"].as_array().size()))
        {
            const Json &track = song["tracks"][static_cast<size_t>(trackIndex)];
            if (track.contains("automation") && track["automation"].is_array())
            {
                for (const Json &lane : track["automation"].as_array())
                {
                    if (lane.is_object() && lane.contains("cc") &&
                        lane["cc"].is_number())
                    {
                        lanes.push_back({lane["cc"].as_int(), 0, laneHeight});
                    }
                }
            }
        }
        int y = 0;
        for (LaneLayout &lane : lanes)
        {
            lane.yTop = y;
            y += lane.height;
        }
        setFixedHeight(stripHeight());
        update();
    }

    void VelocityLaneStrip::setHistory(editor::SongHistory *value)
    {
        history = value;
    }

    void VelocityLaneStrip::setChangeCallback(std::function<void(const Json &)> callback)
    {
        onSongChanged = std::move(callback);
    }

    int VelocityLaneStrip::laneAt(int y) const
    {
        for (int i = static_cast<int>(lanes.size()) - 1; i >= 0; --i)
        {
            if (y >= lanes[static_cast<size_t>(i)].yTop)
                return i;
        }
        return -1;
    }

    int VelocityLaneStrip::stepAt(const QPoint &pos) const
    {
        return std::max(0, (pos.x() - kKeyWidth) / kStepWidth);
    }

    void VelocityLaneStrip::setPoint(int laneRow, int step, int value)
    {
        if (laneRow < 0 || laneRow >= static_cast<int>(lanes.size()) ||
            trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        Json &track = song["tracks"][static_cast<size_t>(trackIndex)];
        const int id = lanes[static_cast<size_t>(laneRow)].id;
        if (id < 0)
        {
            if (!track.contains("notes") || !track["notes"].is_array())
                return;
            for (Json &note : track["notes"].as_array())
            {
                if (!note.is_array() || note.as_array().size() < 1)
                    continue;
                if (note.as_array()[0].as_int() == step)
                    audio::SongDocument::set_note_velocity(note, value);
            }
            return;
        }
        if (id == audio::SongDocument::kLaneBend)
            value = std::clamp(value, -8192, 8191);
        audio::SongDocument::set_lane_point(track, id, step, value);
    }

    void VelocityLaneStrip::notifyChanged()
    {
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
        update();
    }

    void VelocityLaneStrip::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#14161e"));

        const bool validTrack = song.contains("tracks") && song["tracks"].is_array() &&
                                trackIndex >= 0 &&
                                trackIndex < static_cast<int>(song["tracks"].as_array().size());
        if (!validTrack)
        {
            painter.setPen(QColor("#858a99"));
            painter.drawText(rect(), Qt::AlignCenter, "Velocity");
            return;
        }
        const Json &track = song["tracks"][static_cast<size_t>(trackIndex)];

        for (int row = 0; row < static_cast<int>(lanes.size()); ++row)
        {
            const LaneLayout &lane = lanes[static_cast<size_t>(row)];
            const QRect cell(0, lane.yTop, width(), lane.height);
            painter.fillRect(cell, row % 2 ? QColor("#181a23") : QColor("#1c1f2a"));

            painter.setPen(QColor("#aeb5c5"));
            painter.drawText(cell.adjusted(4, 2, -2, 0), Qt::AlignLeft,
                             lane.id < 0 ? "vel" : laneLabel(lane.id));
            painter.setPen(QColor("#2a2d39"));
            painter.drawLine(0, cell.bottom(), width(), cell.bottom());

            if (lane.id < 0)
            {
                if (!track.contains("notes") || !track["notes"].is_array())
                    continue;
                for (const Json &note : track["notes"].as_array())
                {
                    if (!note.is_array() || note.as_array().size() < 1)
                        continue;
                    const int step = note.as_array()[0].as_int();
                    const int velocity = audio::SongDocument::note_velocity(note);
                    const int h = lane.height - 14;
                    const int barHeight = std::max(2, velocity * h / 127);
                    const int x = kKeyWidth + step * kStepWidth;
                    painter.fillRect(x + 2, lane.yTop + lane.height - 2 - barHeight,
                                     kStepWidth - 4, barHeight, QColor("#48d6a0"));
                }
            }
            else
            {
                const auto points = audio::SongDocument::lane_points(track, lane.id);
                const int range = lane.id == audio::SongDocument::kLaneBend ? 8192 : 127;
                const int h = lane.height - 14;
                for (size_t i = 0; i < points.size(); ++i)
                {
                    const int x = kKeyWidth + points[i].step * kStepWidth;
                    const int valueY = points[i].value * h / range;
                    const int y = lane.id == audio::SongDocument::kLaneBend
                                      ? lane.yTop + 12 + (h / 2) - valueY / 2
                                      : lane.yTop + lane.height - 2 - valueY;
                    if (i + 1 < points.size())
                    {
                        const int nx = kKeyWidth + points[i + 1].step * kStepWidth;
                        const int nv = points[i + 1].value * h / range;
                        const int ny = lane.id == audio::SongDocument::kLaneBend
                                           ? lane.yTop + 12 + (h / 2) - nv / 2
                                           : lane.yTop + lane.height - 2 - nv;
                        painter.setPen(QPen(QColor("#6fa8dc"), 1));
                        painter.drawLine(x, y, nx, ny);
                    }
                    painter.setBrush(QColor("#6fa8dc"));
                    painter.setPen(QColor("#c8dcf0"));
                    painter.drawRect(x - 2, y - 2, 5, 5);
                }
            }
        }
        painter.setPen(QColor("#0c0d12"));
        painter.drawLine(kKeyWidth - 1, 0, kKeyWidth - 1, height());
    }

    void VelocityLaneStrip::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() != Qt::LeftButton || event->pos().x() < kKeyWidth)
            return;
        const int row = laneAt(event->pos().y());
        if (row < 0)
            return;
        if (history)
            history->begin_track_edit(song, trackIndex);
        dragLane = row;
        dragStep = stepAt(event->pos());
        dragging = true;
        setPoint(row, dragStep,
                 lanes[static_cast<size_t>(row)].id == audio::SongDocument::kLaneBend
                     ? 0
                     : 90);
        notifyChanged();
    }

    void VelocityLaneStrip::mouseMoveEvent(QMouseEvent *event)
    {
        if (!dragging || !(event->buttons() & Qt::LeftButton) || dragLane < 0)
            return;
        const LaneLayout &lane = lanes[static_cast<size_t>(dragLane)];
        const int step = stepAt(event->pos());
        const int h = lane.height - 14;
        int value;
        if (lane.id == audio::SongDocument::kLaneBend)
        {
            const int centered = std::clamp(event->pos().y() - lane.yTop - 12, 0, h);
            value = (h - centered) * 2 - h; // -h..h scaled later to bend range
            value = value * 8192 / std::max(1, h);
        }
        else
        {
            const int fromBottom = std::clamp(lane.yTop + lane.height - 2 - event->pos().y(),
                                              0, h);
            value = fromBottom * 127 / std::max(1, h);
        }
        dragStep = step;
        setPoint(dragLane, step, value);
        notifyChanged();
    }

    void VelocityLaneStrip::mouseReleaseEvent(QMouseEvent *)
    {
        if (dragging && history)
        {
            history->commit_track_change(song, trackIndex,
                                         editor::SongAction::Kind::TrackProps,
                                         dragLane >= 0 && laneId(dragLane) < 0
                                             ? "velocity"
                                             : "automation lane");
            emit historyChanged();
        }
        dragging = false;
        dragLane = -1;
        dragStep = -1;
    }

} // namespace aur
