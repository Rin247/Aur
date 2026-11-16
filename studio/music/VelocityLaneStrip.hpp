#pragma once

#include <QWidget>
#include <functional>
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"
#include "music/SongHistory.hpp"

namespace aur
{

    // Lane strip under the piano roll: per-note velocity ladder and track automation
    // lanes (CC 0..127, bend = 128, pressure = 129) over SongHistory TrackProps.
    class VelocityLaneStrip : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int laneHeight = 72;
        static constexpr int minLanes = 3; // velocity + two slots always visible

        explicit VelocityLaneStrip(QWidget *parent = nullptr);

        void setSong(const Json &song, int selectedTrack);
        void setHistory(editor::SongHistory *history);
        void setChangeCallback(std::function<void(const Json &)> callback);

        int laneCount() const { return laneCountFor(song, trackIndex); }
        int laneAt(int y) const;
        static int laneCountFor(const Json &song, int trackIndex);
        int laneId(int laneRow) const;
        int stripHeight() const;

    signals:
        void historyChanged();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        struct LaneLayout
        {
            int id; // velocity = -1, else automation lane id
            int yTop;
            int height;
        };

        int stepAt(const QPoint &pos) const;
        void setPoint(int laneRow, int step, int value);
        void notifyChanged();

        Json song;
        int trackIndex = -1;
        int firstVisibleStep = 0;
        int visibleSteps = 64;
        std::vector<LaneLayout> lanes;
        int dragLane = -1;
        int dragStep = -1;
        bool dragging = false;
        editor::SongHistory *history = nullptr;
        std::function<void(const Json &)> onSongChanged;
    };

} // namespace aur
