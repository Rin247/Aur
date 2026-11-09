#include "mini_test.hpp"
#include "music/ClipLauncherStates.hpp"

#include <atomic>

namespace
{
    using aur::ClipLauncherStates;

    struct FakeClock
    {
        std::atomic<std::int64_t> frames{0};
        std::atomic<int> bpm{120};
        std::atomic<bool> active{false};
    };

    void test_states_stopped_and_queued()
    {
        FakeClock clock;
        ClipLauncherStates launcher;
        launcher.attachClock(&clock.frames, &clock.bpm, &clock.active);

        // Stopped transport: launch plays immediately.
        launcher.launch(0, 1);
        ASSERT(launcher.state(0, 1) == ClipLauncherStates::State::Playing);
        ASSERT(launcher.state(1, 1) == ClipLauncherStates::State::Idle);
        launcher.stopAll();
        ASSERT(launcher.state(0, 1) == ClipLauncherStates::State::Idle);

        // Running transport: launch queues to the next bar.
        clock.active = true;
        // 120 bpm, 44100 Hz → frames/bar = 44100·60·16/(120·4) = 88200.
        clock.frames = 40000; // mid-bar
        launcher.launch(0, 1);
        ASSERT(launcher.state(0, 1) == ClipLauncherStates::State::Queued);
        ASSERT(launcher.queuedInSteps(0, 1) > 0);
        ASSERT(launcher.queuedInSteps(0, 1) <= ClipLauncherStates::kStepsPerBar);

        // Past the boundary: tick promotes to playing.
        clock.frames = 88201;
        ASSERT(launcher.tick());
        ASSERT(launcher.state(0, 1) == ClipLauncherStates::State::Playing);
        ASSERT(launcher.queuedInSteps(0, 1) == 0);

        // Tick before the boundary is a no-op.
        ClipLauncherStates launcher2;
        launcher2.attachClock(&clock.frames, &clock.bpm, &clock.active);
        clock.frames = 90000; // inside bar 2 (88200..176400)
        launcher2.launch(2, 3);
        ASSERT(!launcher2.tick());
        ASSERT(launcher2.state(2, 3) == ClipLauncherStates::State::Queued);
        clock.frames = 176400;
        ASSERT(launcher2.tick());
        ASSERT(launcher2.state(2, 3) == ClipLauncherStates::State::Playing);
    }
} // namespace

void test_clip_launcher()
{
    test_states_stopped_and_queued();
}
