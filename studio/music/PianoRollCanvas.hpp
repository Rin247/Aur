#pragma once

#include <QWidget>
#include <QPoint>
#include <functional>
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"
#include "SongHistory.hpp"
#include "SongHistory.hpp"

namespace aur
{

    class PianoRollCanvasWidget : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int keyWidth = 40;
        static constexpr int stepWidth = 16;
        static constexpr int pitchHeight = 16;
        static constexpr int lowestPitch = 48;
        static constexpr int highestPitch = 83;
        static constexpr int minimumStepCount = 128;
        static constexpr int pitchCount = highestPitch - lowestPitch + 1;

        explicit PianoRollCanvasWidget(QWidget *parent = nullptr);
        void setSong(const Json &song, int selectedTrack);
        void setSelectedTrack(int selectedTrack);
        void setVisibleStep(int firstStep);
        int contentWidth() const;
        void setChangeCallback(std::function<void(const Json &)> callback);
        void setHistory(editor::SongHistory *history);
        void undo();
        void redo();
        bool canUndo() const;
        bool canRedo() const;

    signals:
        void historyChanged();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

    private:
        Json song;
        int trackIndex = -1;
        int selectedNote = -1;
        int dragOriginStep = 0;
        int dragOriginPitch = 0;
        int dragOriginLength = 1;
        int firstVisibleStep = 0;
        QPoint dragStartPos;
        bool resizingNote = false;
        bool dragging = false; // note edit in progress; history committed on release
        bool velocityDrag = false; // Alt+drag: vertical drag adjusts velocity
        int dragOriginVelocity = 90;
        editor::SongHistory *history = nullptr;
        std::function<void(const Json &)> onSongChanged;

        int noteAt(const QPoint &pos) const;
        void notifySongChanged();
    };

} // namespace aur
