#pragma once

#include <QWidget>
#include <functional>

#include <string>

#include <vector>

#include "aur/audio/StepSequencer.hpp"

#include "aur/json.hpp"

#include "SongHistory.hpp"

namespace aur
{

    // The 128-step grid: one row per pattern, 128 columns of cells. Click toggles a
    // cell, drag paints, right-click clears. Edits are undoable via SongHistory.
    class StepSequencerCanvas : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int maxSteps = 128;
        static constexpr int labelWidth = 132;
        static constexpr int rowHeight = 34;
        static constexpr int cellWidth = 14;
        static constexpr int cellHeight = 22;
        static constexpr int rulerHeight = 22;

        explicit StepSequencerCanvas(QWidget *parent = nullptr);

        void setSong(const Json &song);
        void setHistory(editor::SongHistory *history);
        void setChangeCallback(std::function<void(const Json &)> callback);
        // The track whose instrument the new cells take their pitch from.
        void setSourceTrack(int trackIndex);
        // Plays the cell's pitch while the pointer is over it.
        void setAuditionHook(std::function<void(int pitch, int velocity)> hook);

        int patternCount() const { return static_cast<int>(patterns.size()); }
        int effectiveCellWidth() const;
        int effectiveRowHeight() const;
        int cellAt(const QPoint &pos, int &row) const;
        int rowAt(const QPoint &pos) const;
        int contentHeight() const;

    signals:
        void historyChanged();
        void selectionChanged(int patternIndex, int step);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;

    private:
        int patternIndexAt(int row) const;
        const audio::StepCell *cellAt(int pattern, int step) const;
        audio::StepCell *cellAtMut(int pattern, int step);
        void toggleCell(int pattern, int step, bool on);
        void setAccent(int pattern, int step, bool on);
        void clearCell(int pattern, int step);
        void applyToSong();
        void commitEdit(const std::string &label);
        int defaultPitch() const;

        Json song;
        std::vector<audio::StepPattern> patterns;
        editor::SongHistory *history = nullptr;
        std::function<void(const Json &)> onSongChanged;
        std::function<void(int pitch, int velocity)> auditionHook;
        int sourceTrack = -1;
        int cursorPattern = 0;
        int cursorStep = 0;
        int scrollOffset = 0;
        bool painting = false;
        bool paintValue = false;
        bool dragging = false;
        QPoint dragStart;
        int dragStartPattern = 0;
        int dragStartStep = 0;
    };

} // namespace aur
