#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <string>
#include "aur/audio/synth.hpp"

namespace aur {
namespace audio {

// Offline render on a worker thread with progress and cancel (contract 7).
// One render per instance; destruction cancels and joins.
class OfflineRenderJob {
public:
    using ProgressFn = std::function<void(double fraction)>;

    OfflineRenderJob() = default;
    ~OfflineRenderJob() { cancel(); }

    OfflineRenderJob(const OfflineRenderJob&) = delete;
    OfflineRenderJob& operator=(const OfflineRenderJob&) = delete;

    // Launches the render; replaces any running job.
    void start(const Json& song, PreviewSynth synth, int sampleRate,
               int maxSeconds, ProgressFn onProgress = {});

    // Thread-safe; cooperative — checked between voice frames.
    void cancel() { cancelled.store(true, std::memory_order_relaxed); }

    bool running() const { return running_.load(std::memory_order_relaxed); }
    bool finished() const { return isFinished.load(std::memory_order_relaxed); }

    // Valid after finished(); empty if cancelled or the render threw.
    StereoPcm takeResult() { return std::move(result); }

    // Valid after finished(); non-empty if the render threw.
    std::string takeError() { return std::move(error); }

private:
    void finishWorker();

    std::atomic<bool> cancelled{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> isFinished{false};
    StereoPcm result;
    std::string error;
    std::future<void> worker;
};

} // namespace audio
} // namespace aur
