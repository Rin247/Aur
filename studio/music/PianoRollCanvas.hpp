#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <set>
#include <functional>
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"
#include "SongHistory.hpp"

namespace aur
{

    class PianoRollCanvasWidget : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int keyWidth = 40;
        static constexpr int baseStepWidth = 16;
        static constexpr int pitchHeight = 16;
        static constexpr int lowestPitch = 12;
        static constexpr int highestPitch = 107;
        static constexpr int minimumStepCount = 128;
        static constexpr int pitchCount = highestPitch - lowestPitch + 1;
        int pitchOffset() const { return pitchOffsetRows; }

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
        void attachPlayhead(const std::atomic<std::int64_t> *frames,
                            const std::atomic<int> *bpm,
                            const std::atomic<bool> *active)
        {
            playheadFrames = frames;
            playheadBpm = bpm;
            playheadActive = active;
        }
        void setAuditionHook(std::function<void(int pitch, int velocity)> hook)
        {
            auditionHook = std::move(hook);
        }
        void setSnapSteps(int steps) { snapSteps = steps; update(); }
        int snap() const { return snapSteps; }
        int stepWidth() const { return currentStepWidth; }
        void setPitchOffset(int lowestVisiblePitch);
        void zoom(int delta);

    signals:
        void historyChanged();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

    private:
        struct NoteSnapshot
        {
            int step;
            int pitch;
            int length;
            int velocity;
        };

        Json song;
        int trackIndex = -1;
        std::set<int> selectedNotes;
        bool marqueeSelecting = false;
        QRect marqueeRect;
        QPoint marqueeStartPos;
        int dragOriginStep = 0;
        int dragOriginPitch = 0;
        int dragOriginLength = 1;
        std::map<int, NoteSnapshot> dragOrigins; // note index -> pre-drag state
        int firstVisibleStep = 0;
        QPoint dragStartPos;
        bool resizingNote = false;
        bool dragging = false; // note edit in progress; history committed on release
        bool velocityDrag = false; // Alt+drag: vertical drag adjusts velocity
        int dragOriginVelocity = 90;
        int snapSteps = 1; // 0 off, 1/2/4 steps, 5 = triplet (16/3)
        int currentStepWidth = baseStepWidth;
        static constexpr int kZoomLevels[6] = {8, 12, 16, 24, 32, 48};
        int zoomIndex = 2;
        int pitchOffsetRows = 0; // rows scrolled down from highestPitch
        const std::atomic<std::int64_t> *playheadFrames = nullptr;
        const std::atomic<int> *playheadBpm = nullptr;
        const std::atomic<bool> *playheadActive = nullptr;
        std::function<void(int pitch, int velocity)> auditionHook;
        int playheadStep = -1;
        editor::SongHistory *history = nullptr;
        std::function<void(const Json &)> onSongChanged;

        int noteAt(const QPoint &pos) const;
        bool noteIntersectsMarquee(int index) const;
        int pitchRow(int pitch) const;
        int visiblePitch(int y) const;
        int visiblePitchAt(int row) const;
        int snapped(int step) const;
        void cycleSnap();
        void quantiseSelection();
        void updatePlayhead();
        void selectOnly(int index);
        void deleteSelected(const char *label);
        void copySelected();
        void pasteClipboard();
        void duplicateSelection();
        void notifySongChanged();

        struct ClipNote { int step; int pitch; int length; int velocity; };
        std::vector<ClipNote> clipboard;
        int clipboardMinStep = 0;
        std::vector<ClipNote> duplicated; // pending paste offset for Ctrl+D
    };

} // namespace aur
