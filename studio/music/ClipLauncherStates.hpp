#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include "aur/json.hpp"

namespace aur
{

    // Session-grid launch state machine (P6.3). Pure logic, no Qt moc:
    // the grid paints from this, the mixer song-swap is driven from it
    // via std::function callbacks (moc's parser mishandles commas in
    // template member declarations, so Q_OBJECT is deliberately avoided).
    //
    // States per (scene, track) slot: idle → queued → playing.
    // A launch while the transport is playing queues the clip to start on
    // the next bar boundary (quantise = 1 bar); a launch while stopped
    // takes effect immediately. stopAll idles all playing slots.
    class ClipLauncherStates
    {
    public:
        static constexpr int kStepsPerBar = 16;

        enum class State { Idle, Queued, Playing };

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
        struct Slot
        {
            State state = State::Idle;
            std::int64_t queueStartFrames = 0;
            int targetBar = -1;
        };

        static std::int64_t slotKey(int sceneIndex, int trackIndex);
        Slot *slotAt(int sceneIndex, int trackIndex);
        std::int64_t currentFrame() const;
        int framesPerBar() const;

        const std::atomic<std::int64_t> *playheadFrames = nullptr;
        const std::atomic<int> *playheadBpm = nullptr;
        const std::atomic<bool> *playheadActive = nullptr;
        std::map<std::int64_t, Slot> slots;
    };

} // namespace aur
