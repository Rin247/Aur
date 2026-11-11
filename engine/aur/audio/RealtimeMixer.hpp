#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include "aur/audio/VoiceCore.hpp"
#include "aur/audio/SynthCores.hpp"
#include "aur/audio/EffectChain.hpp"
#include "aur/audio/StepSequencer.hpp"
#include "aur/json.hpp"

namespace aur
{
    namespace audio
    {

        // Song playback command for the real-time engine. The GUI thread enqueues;
        // the audio thread dequeues between callback fills (single-producer,
        // single-consumer, guarded by a spin-free mutex held only outside the
        // callback fill — contract 4 keeps the callback itself lock-free).
        struct PlayCommand
        {
            Json song;
            std::int64_t startFrame = 0;
        };

        // Real-time song mixer: owns the scheduled voices for one playing song and
        // produces interleaved stereo frames. Qt-free; the audio callback pulls
        // frames without locks or allocation.
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
                voices.clear();
                upcoming.clear();
                playheadFrames.store(0, std::memory_order_relaxed);
                playheadActive.store(false, std::memory_order_relaxed);
            }

            // GUI thread: replace the playing song. Takes effect at the next
            // command boundary, not mid-callback.
            void loadSong(const Json &song);

            // GUI thread: queue one audition note at the current frame.
            // Applied at the command boundary; ignored when idle-safe.
            void audition(int pitch, int velocity = 90, int lengthSteps = 2);

            // Audio thread: fill interleaved stereo frames. No locks, no allocation.
            void render(float *out, int frames);

            std::atomic<double> levelLeft{0.0};  // meter, peak over last callback
            std::atomic<double> levelRight{0.0};
            // Post-chain peaks from the last callback (P8.3). Equal to
            // levelLeft/Right when no chain is active.
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
            void schedulePatternVoices(const Json &song, std::int64_t baseFrame);

            int sampleRate = 44100;
            Timbre timbre = Timbre::Gm;
            std::mutex songMutex; // held by loadSong only
            Json pendingSong;     // swapped under mutex
            bool pending = false;
            std::vector<Voice> pendingAuditions; // applied at command boundary

            // Core-synth path (md_fm / spc songs): the whole song renders
            // through one hardware core instance instead of the voice list.
            bool coreSynth = false;
            std::unique_ptr<SynthCoreVoice> coreVoice; // setup at song swap

            // Master effect chain (P8): built at the command boundary from the
            // song's "effects" key; empty chain = untouched samples. The
            // scratch is sized here (not per callback) to keep render alloc-free.
            EffectChain masterChain;
            std::vector<float> chainScratchL, chainScratchR;

            // Audio-thread state (touched only inside render / command boundary).
            std::deque<Voice> voices;
            std::deque<Voice> upcoming; // scheduled with absolute frames
            std::int64_t frameCounter = 0;
            std::int64_t nextWrapFrame = 0;
            int bpm = 120;
            int loopSteps = 0;             // song length in steps; 0 = no loop
            int patternLastStep = 0;       // last step covered by the step patterns
            std::vector<Voice> loopVoices; // template voices for loop restarts
        };

    } // namespace audio
} // namespace aur
