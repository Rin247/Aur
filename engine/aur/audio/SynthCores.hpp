// SynthCores.hpp - preview rendering through the MD/SPC hardware cores.
// Owns the note-event translation from the song JSON into the cores.

#pragma once

#include <cstdint>

#include <memory>

#include <vector>

#include "aur/audio/YmFmCore.hpp"

#include "aur/audio/SnesPcmCore.hpp"

#include "aur/audio/Sf2Core.hpp"

namespace aur
{
    namespace audio
    {

        // One scheduled note event in song-step coordinates.
        struct CoreNoteEvent
        {
            std::int64_t startFrame = 0;
            std::int64_t endFrame = 0;
            int track = 0;
            int pitch = 60;
            int velocity = 90;
            int pan = 64;
            int program = 0;
        };

        // Adapter around one hardware core (YM FM or SPC sampler) for song playback.
        // Construct + setup off the audio thread; render() is allocation-free.
        class SynthCoreVoice
        {
        public:
            enum class Kind : int
            {
                MdFm = 0,
                Spc = 1,
                Sf2 = 2
            };

            void setup(Kind kind, double sampleRate);

            // Hands the Sf2 kind its bank (off the audio thread, before setup);
            // other kinds ignore it. Without a bank the Sf2 kind renders silence.
            bool setSf2Bank(const std::uint8_t *data, std::size_t size);
            Kind kind() const { return kind_; }

            // Translate scheduled notes into note-on/off events at frame boundaries.
            // consumeUpTo(frame) issues the ones whose frame has arrived.
            void setEvents(std::vector<CoreNoteEvent> events);
            // Adds events to the existing list without disturbing the cursors.
            // Used by the realtime mixer to slot in audition notes.
            void appendEvents(std::vector<CoreNoteEvent> events);
            void consumeUpTo(std::int64_t frame);
            void clearEvents();
            // Shifts every stored event frame by `shift` and rewinds the cursors.
            // Used by the realtime mixer to replay a looped song without reallocating.
            void rebaseEvents(std::int64_t shift);

            YmFmCore &fm() { return fm_; }
            SnesPcmCore &spc() { return spc_; }

            // Renders exactly one frame; returns left/right via out parameters.
            void renderFrame(float &left, float &right);

            void reset();

        private:
            void issueNoteOn(const CoreNoteEvent &e);
            void issueNoteOff(const CoreNoteEvent &e);
            void resetCursors();

            Kind kind_ = Kind::MdFm;
            double sampleRate_ = 44100;
            std::vector<CoreNoteEvent> events_;
            size_t eventCursor_ = 0;
            std::vector<std::int64_t> pendingOffs_; // endFrame for active notes, ascending
            std::vector<size_t> offIndex_; // events_ index per pending off
            size_t offCursor_ = 0;
            size_t nextEventIndex_ = 0;
            YmFmCore fm_;
            SnesPcmCore spc_;
            std::unique_ptr<Sf2Core> sf2_;
        };

    } // namespace audio
} // namespace aur
