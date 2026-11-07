#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>
#include "aur/audio/VoiceCore.hpp"
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

            // Audio thread: fill interleaved stereo frames. No locks, no allocation.
            void render(float *out, int frames);

            std::atomic<double> levelLeft{0.0};  // meter, peak over last callback
            std::atomic<double> levelRight{0.0};
            // Monotonic playback position in frames, updated after each
            // callback fill; external clocks (MIDI out) sync to this.
            std::atomic<std::int64_t> playheadFrames{0};
            std::atomic<int> playheadBpm{120};
            std::atomic<bool> playheadActive{false};

        private:
            void schedule(const Json &song, std::int64_t baseFrame);

            int sampleRate = 44100;
            Timbre timbre = Timbre::Gm;
            std::mutex songMutex; // held by loadSong only
            Json pendingSong;     // swapped under mutex
            bool pending = false;

            // Audio-thread state (touched only inside render / command boundary).
            std::deque<Voice> voices;
            std::deque<Voice> upcoming; // scheduled with absolute frames
            std::int64_t frameCounter = 0;
            std::int64_t nextWrapFrame = 0;
            int bpm = 120;
            int loopSteps = 0;             // song length in steps; 0 = no loop
            std::vector<Voice> loopVoices; // template voices for loop restarts
        };

    } // namespace audio
} // namespace aur
