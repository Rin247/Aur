#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

#include <memory>

#include <mutex>

#include <vector>

#include "aur/audio/EffectChain.hpp"

#include "aur/audio/SynthCores.hpp"

#include "aur/json.hpp"

namespace aur
{
    namespace audio
    {

        // Song playback command for the real-time engine. The GUI thread enqueues;
        // the audio thread dequeues between callback fills (contract 4).
        struct PlayCommand
        {
            Json song;
            std::int64_t startFrame = 0;
        };

        // Real-time song mixer: produces interleaved stereo frames. Qt-free; the
        // audio callback pulls frames without locks or allocation.
        class RealtimeMixer
        {
        public:
            void setSampleRate(int rate) { sampleRate = rate; }
            int getSampleRate() const { return sampleRate; }

            // GUI thread: rewind the playhead and clear all scheduled audio.
            // Call before loadSong for a fresh playback, or to stop.
            void resetPlayback()
            {
                frameCounter = 0;
                nextWrapFrame = 0;
                if (coreVoice)
                    coreVoice->clearEvents();
                playheadFrames.store(0, std::memory_order_relaxed);
                playheadActive.store(false, std::memory_order_relaxed);
            }

            // GUI thread: replace the playing song. Takes effect at the next
            // command boundary, not mid-callback.
            void loadSong(const Json &song);

            // GUI thread: queue one audition note at the current frame.
            // Applied at the command boundary; ignored when idle-safe.
            void audition(int pitch, int velocity = 90, int lengthSteps = 2);

            // GUI thread: the master volume, 0..1 (1 is the mixer's own
            // level). The audio thread reads it, so it can move while the
            // song plays.
            void setMasterVolume(float gain)
            {
                masterGain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
            }
            float masterVolume() const
            {
                return masterGain.load(std::memory_order_relaxed);
            }

            // Audio thread: fill interleaved stereo frames. No locks, no allocation.
            void render(float *out, int frames);

            std::atomic<double> levelLeft{0.0};  // meter, peak over last callback
            std::atomic<double> levelRight{0.0};
            // Post-chain peaks from the last callback. Equal to levelLeft/Right
            // when no chain is active.
            std::atomic<double> chainLevelLeft{0.0};
            std::atomic<double> chainLevelRight{0.0};
            // Master-chain CPU cost: frames x active effects, last callback.
            std::atomic<std::int64_t> chainBudget{0};
            // Monotonic playback position in frames, updated after each
            // callback fill; external clocks (MIDI out) sync to this.
            std::atomic<std::int64_t> playheadFrames{0};
            std::atomic<int> playheadBpm{120};
            std::atomic<bool> playheadActive{false};

        private:
            void schedule(const Json &song, std::int64_t baseFrame);

            int sampleRate = 44100;
            std::mutex songMutex; // held by loadSong only
            Json pendingSong;     // swapped under mutex
            bool pending = false;
            std::vector<CoreNoteEvent> pendingAuditions; // applied at command boundary

            // The whole song renders through one hardware core instance
            // (YM-FM or SPC) instead of a voice list.
            std::unique_ptr<SynthCoreVoice> coreVoice;

            // Master effect chain (P8): built at the command boundary from the
            // song's "effects" key; empty chain = untouched samples.
            EffectChain masterChain;
            std::vector<float> chainScratchL, chainScratchR;

            // The master volume, read by the audio thread every callback.
            std::atomic<float> masterGain{1.0f};

            // Audio-thread state (touched only inside render / command boundary).
            std::int64_t frameCounter = 0;
            std::int64_t nextWrapFrame = 0;
            int bpm = 120;
            int loopSteps = 0; // song length in steps; 0 = no loop
        };

    } // namespace audio
} // namespace aur
