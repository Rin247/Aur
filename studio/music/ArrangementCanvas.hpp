#pragma once

#include <QWidget>
#include <functional>
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"
#include "SongHistory.hpp"

namespace aur
{

    // Arrangement timeline: one row per track, scene-clip placements painted
    // as blocks on the absolute step axis, a bar ruler with markers, the loop
    // brace and tempo/T.S. lanes from the v2 maps. Click on an empty cell
    // places the selected scene's clip for that track; drag moves a placement;
    // Delete removes the selection. Placement edits mutate the shared song
    // document through SongHistory's track-snapshot path (TrackProps).
    class ArrangementCanvas : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int rowHeight = 44;
        static constexpr int rulerHeight = 48;
        static constexpr int minRows = 4;

        explicit ArrangementCanvas(QWidget *parent = nullptr);

        void setSong(const Json &song);
        void setHistory(editor::SongHistory *history);
        void setPlaceScene(int sceneIndex);
        void setChangeCallback(std::function<void(const Json &)> callback);

        int canvasHeight() const;
        int canvasWidth() const;
        int stepAt(const QPoint &pos) const;
        int rowAt(const QPoint &pos) const;

    signals:
        void historyChanged();
        void selectionChanged();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

    private:
        struct Hit
        {
            int placementIndex = -1; // index into SongDocument::placements
            int row = -1;
        };

        Hit hitTest(const QPoint &pos) const;
        QColor placementColor(int color, int scene) const;
        void notifyChanged();

        Json song;
        int placeScene = 0;
        int selectedStep = -1;
        int selectedTrack = -1;
        int dragOrigStep = -1; // placement's original step while dragging
        int dragOrigTrack = -1;
        bool dragging = false;
        editor::SongHistory *history = nullptr;
        std::function<void(const Json &)> onSongChanged;
    };

} // namespace aur
