#include "aur/audio/OfflineRenderJob.hpp"

namespace aur
{
    namespace audio
    {

        void OfflineRenderJob::start(const Json &song, PreviewSynth synth, int sampleRate,
                                     int maxSeconds, ProgressFn onProgress)
        {
            cancel();
            if (worker.valid())
                worker.wait();
            cancelled.store(false, std::memory_order_relaxed);
            running_.store(true, std::memory_order_relaxed);
            isFinished.store(false, std::memory_order_relaxed);
            error.clear();
            result = StereoPcm{};

            worker = std::async(std::launch::async, [this, song, synth, sampleRate,
                                                     maxSeconds,
                                                     progress = std::move(onProgress)]()
                                {
        try {
            StereoPcm pcm = synthesize_preview(song, synth, sampleRate, maxSeconds,
                                               &cancelled, progress);
            if (cancelled.load(std::memory_order_relaxed)) {
                finishWorker();
                return;
            }
            result = std::move(pcm);
        } catch (const std::exception& e) {
            error = e.what();
        }
        finishWorker();
    });
}

void OfflineRenderJob::finishWorker() {
    running_.store(false, std::memory_order_relaxed);
    isFinished.store(true, std::memory_order_relaxed);
}

    } // namespace audio
} // namespace aur
