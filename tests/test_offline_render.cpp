#include "mini_test.hpp"
#include "aur/audio/OfflineRenderJob.hpp"
#include "aur/audio/synth.hpp"

#include <chrono>
#include <thread>

namespace
{

    aur::Json cancel_song()
    {
        // 128 bars of eighth notes — long enough that cancelling lands mid-render.
        aur::JsonArray notes;
        for (int step = 0; step < 128 * 16; step += 2)
            notes.push_back(aur::JsonArray{step, 60 + (step % 24), 2, 100});
        return aur::JsonObject{
            {"name", "CancelProbe"}, {"tempo", 120}, {"bars", 128}, {"beats", 4}, {"tracks", aur::JsonArray{aur::JsonObject{{"name", "Lead"}, {"channel", 0}, {"program", 40}, {"notes", notes}}}}};
    }

    bool wait_finished(aur::audio::OfflineRenderJob &job, int timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeoutMs);
        while (!job.finished())
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return true;
    }

} // namespace

// Contract 7: offline render/generation runs off the calling thread with
// progress and cancel.
void test_offline_render_job()
{
    const aur::Json song = []
    {
        aur::JsonArray notes;
        for (int step = 0; step < 8 * 16; step += 4)
            notes.push_back(aur::JsonArray{step, 60 + (step % 12), 4, 100});
        return aur::JsonObject{
            {"name", "Job"}, {"tempo", 120}, {"bars", 8}, {"beats", 4}, {"tracks", aur::JsonArray{aur::JsonObject{{"name", "Lead"}, {"channel", 0}, {"program", 40}, {"notes", notes}}}}};
    }();

    // Success path: threaded result matches the synchronous render.
    const aur::StereoPcm expected = aur::synthesize_preview(
        song, aur::PreviewSynth::Gm, 8000, 30);

    aur::audio::OfflineRenderJob job;
    double lastProgress = 0.0;
    job.start(song, aur::PreviewSynth::Gm, 8000, 30,
              [&](double f)
              { lastProgress = f; });
    ASSERT(job.running() || job.finished());
    ASSERT(wait_finished(job, 30000));
    ASSERT(job.takeError().empty());
    const aur::StereoPcm got = job.takeResult();
    ASSERT(got.samples.size() == expected.samples.size());
    ASSERT(lastProgress > 0.99);
    for (size_t i = 0; i < expected.samples.size(); ++i)
        ASSERT(got.samples[i] == expected.samples[i]);

    // Cancel path: a long render is abandoned; takeResult is empty.
    aur::audio::OfflineRenderJob longJob;
    longJob.start(cancel_song(), aur::PreviewSynth::Gm, 44100, 600);
    longJob.cancel();
    ASSERT(wait_finished(longJob, 30000));
    ASSERT(longJob.takeResult().samples.empty());
    ASSERT(longJob.takeError().empty());
}
