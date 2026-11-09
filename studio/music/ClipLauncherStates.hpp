#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include "aur/json.hpp"

namespace aur
{

    // Session-grid launch state machine (P6.3). Pure logic, no Qt moc:
    // the grid paints from this; song swaps are driven through a callback
    // (a Q_OBJECT member `slots` collides with Qt's own macro, so the
    // class deliberately avoids moc).
    //
    // States per (scene, track) slot: idle → queued → playing.
    // A launch while the transport is playing queues the clip to start on
    // the next bar boundary (quantise = 1 bar); a launch while stopped
    // takes effect immediately. stopAll idles all playing slots.
    struct ClipSlotState
    {
        enum class Value { Idle, Queued, Playing };
        Value value = Value::Idle;
        std::int64_t queueStartFrames = 0;
        int targetBar = -1;
    };

    class ClipLauncherStates
    {
    public:
        static constexpr int kStepsPerBar = 16;
        using State = ClipSlotState::Value;

        ClipLauncherStates() = default;

        // Transport clock source: mixer playhead atomics.
        void attachClock(const std::atomic<std::int64_t> *frames,
                         const std::atomic<int> *bpm,
                         const std::atomic<bool> *active);

        void launch(int sceneIndex, int trackIndex); // queue or start now
        void stopAll();                              // idle all playing slots
        State state(int sceneIndex, int trackIndex) const;
        // Steps until the queued launch fires (0 when playing/idle).
        int queuedInSteps(int sceneIndex, int trackIndex) const;

        // Called on a timer (~33 ms): promotes queued → playing at bar
        // boundaries. Returns true when a slot promoted (a repaint and a
        // song swap are due).
        bool tick();

        std::function<void()> onStateChanged;

    private:
        static std::int64_t slotKey(int sceneIndex, int trackIndex);
        ClipSlotState *slotAt(int sceneIndex, int trackIndex);
        std::int64_t currentFrame() const;
        int framesPerBar() const;

        const std::atomic<std::int64_t> *playheadFrames = nullptr;
        const std::atomic<int> *playheadBpm = nullptr;
        const std::atomic<bool> *playheadActive = nullptr;
        std::map<std::int64_t, ClipSlotState> slotStates;
    };

} // namespace aur
