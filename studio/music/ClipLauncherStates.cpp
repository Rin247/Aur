#include "ClipLauncherStates.hpp"

#include <algorithm>

namespace aur
{

    void ClipLauncherStates::attachClock(const std::atomic<std::int64_t> *frames,
                                         const std::atomic<int> *bpm,
                                         const std::atomic<bool> *active)
    {
        playheadFrames = frames;
        playheadBpm = bpm;
        playheadActive = active;
    }

    std::int64_t ClipLauncherStates::currentFrame() const
    {
        if (!playheadFrames || !playheadActive ||
            !playheadActive->load(std::memory_order_relaxed))
            return -1;
        return playheadFrames->load(std::memory_order_relaxed);
    }

    int ClipLauncherStates::framesPerBar() const
    {
        const int rate = 44100; // mixer rate; playheadBpm is the transport bpm
        const int bpm = playheadBpm ? std::max(1, playheadBpm->load(std::memory_order_relaxed))
                                    : 120;
        return static_cast<int>(rate * 60.0 * kStepsPerBar / (bpm * 4.0));
    }

    std::int64_t ClipLauncherStates::slotKey(int sceneIndex, int trackIndex)
    {
        return static_cast<std::int64_t>(sceneIndex) * 4096 + trackIndex;
    }

    ClipLauncherStates::Slot *ClipLauncherStates::slotAt(int sceneIndex, int trackIndex)
    {
        auto it = slots.find(slotKey(sceneIndex, trackIndex));
        return it == slots.end() ? nullptr : &it->second;
    }

    ClipLauncherStates::State ClipLauncherStates::state(int sceneIndex, int trackIndex) const
    {
        const auto it = slots.find(slotKey(sceneIndex, trackIndex));
        return it == slots.end() ? State::Idle : it->second.state;
    }

    void ClipLauncherStates::launch(int sceneIndex, int trackIndex)
    {
        Slot &slot = slots[slotKey(sceneIndex, trackIndex)];
        const std::int64_t frame = currentFrame();
        if (frame < 0)
        {
            // Transport stopped: start immediately.
            slot.state = State::Playing;
            slot.targetBar = -1;
            if (onStateChanged)
                onStateChanged();
            return;
        }
        // Transport running: queue to the next bar boundary.
        const int barFrames = framesPerBar();
        slot.state = State::Queued;
        slot.queueStartFrames = frame;
        slot.targetBar = static_cast<int>((frame + barFrames) / barFrames);
        if (onStateChanged)
            onStateChanged();
    }

    void ClipLauncherStates::stopAll()
    {
        for (auto &[key, slot] : slots)
        {
            if (slot.state == State::Playing)
                slot.state = State::Idle; // engine notes-off handled by swap
        }
        if (onStateChanged)
            onStateChanged();
    }

    int ClipLauncherStates::queuedInSteps(int sceneIndex, int trackIndex) const
    {
        const auto it = slots.find(slotKey(sceneIndex, trackIndex));
        if (it == slots.end() || it->second.state != State::Queued)
            return 0;
        const std::int64_t frame = currentFrame();
        if (frame < 0)
            return 0;
        const int barFrames = framesPerBar();
        const std::int64_t targetFrame =
            static_cast<std::int64_t>(it->second.targetBar) * barFrames;
        const std::int64_t remaining = std::max<std::int64_t>(0, targetFrame - frame);
        return static_cast<int>(remaining * kStepsPerBar / barFrames);
    }

    bool ClipLauncherStates::tick()
    {
        bool changed = false;
        const std::int64_t frame = currentFrame();
        for (auto &[key, slot] : slots)
        {
            if (slot.state == State::Queued && frame >= 0)
            {
                const int barFrames = framesPerBar();
                if (frame >= static_cast<std::int64_t>(slot.targetBar) * barFrames)
                {
                    slot.state = State::Playing;
                    changed = true;
                }
            }
        }
        if (changed && onStateChanged)
            onStateChanged();
        return changed;
    }

} // namespace aur
